// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_cooker.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include <libsbx/utility/fourcc.hpp>
#include <libsbx/utility/logger.hpp>

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>

#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>

#include <stb_vorbis.c>
#undef L
#undef C
#undef R

namespace sbx::assets {

inline constexpr auto audio_clip_magic = utility::fourcc_v<"SBAU">;

struct audio_clip_header {
  std::uint32_t magic;
  std::uint32_t version;
  std::uint32_t sample_rate;
  std::uint32_t channels;
  std::uint64_t frame_count;
  std::uint32_t encoding;
  std::uint32_t padding;
  std::uint64_t payload_size; // bytes of s16 samples or Ogg Vorbis data following the header
}; // struct audio_clip_header

struct decoded_audio {
  std::vector<std::int16_t> samples{};
  std::uint32_t channels{0u};
  std::uint32_t sample_rate{0u};
  std::uint64_t frame_count{0u};
}; // struct decoded_audio

static auto lowercase_extension(const std::filesystem::path& path) -> std::string {
  auto extension = path.extension().string();

  std::ranges::transform(extension, extension.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

  return extension;
}

template<typename Release>
static auto take_decoded(const std::int16_t* data, std::uint64_t frame_count, std::uint32_t channels, std::uint32_t sample_rate, Release&& release) -> std::optional<decoded_audio> {
  if (data == nullptr) {
    return std::nullopt;
  }

  auto result = decoded_audio{};
  result.samples.assign(data, data + frame_count * channels);
  result.channels = channels;
  result.sample_rate = sample_rate;
  result.frame_count = frame_count;

  std::forward<Release>(release)();

  if (channels == 0u || sample_rate == 0u) {
    return std::nullopt;
  }

  return result;
}

static auto decode_audio_file(const std::filesystem::path& source) -> std::optional<decoded_audio> {
  const auto extension = lowercase_extension(source);
  const auto filename = source.string();

  if (extension == ".wav") {
    auto channels = 0u;
    auto sample_rate = 0u;
    auto frame_count = drwav_uint64{0u};

    auto* data = drwav_open_file_and_read_pcm_frames_s16(filename.c_str(), &channels, &sample_rate, &frame_count, nullptr);

    return take_decoded(data, frame_count, channels, sample_rate, [data] { drwav_free(data, nullptr); });
  }

  if (extension == ".mp3") {
    auto config = drmp3_config{};
    auto frame_count = drmp3_uint64{0u};

    auto* data = drmp3_open_file_and_read_pcm_frames_s16(filename.c_str(), &config, &frame_count, nullptr);

    return take_decoded(data, frame_count, config.channels, config.sampleRate, [data] { drmp3_free(data, nullptr); });
  }

  if (extension == ".flac") {
    auto channels = 0u;
    auto sample_rate = 0u;
    auto frame_count = drflac_uint64{0u};

    auto* data = drflac_open_file_and_read_pcm_frames_s16(filename.c_str(), &channels, &sample_rate, &frame_count, nullptr);

    return take_decoded(data, frame_count, channels, sample_rate, [data] { drflac_free(data, nullptr); });
  }

  if (extension == ".ogg") {
    auto channels = 0;
    auto sample_rate = 0;
    auto* data = static_cast<short*>(nullptr);

    const auto frame_count = stb_vorbis_decode_filename(filename.c_str(), &channels, &sample_rate, &data);

    if (frame_count < 0) {
      return std::nullopt;
    }

    return take_decoded(data, static_cast<std::uint64_t>(frame_count), static_cast<std::uint32_t>(channels), static_cast<std::uint32_t>(sample_rate), [data] { std::free(data); });
  }

  return std::nullopt;
}

static auto downmix_to_mono(decoded_audio& audio) -> void {
  if (audio.channels <= 1u) {
    return;
  }

  auto mono = std::vector<std::int16_t>(audio.frame_count);

  for (auto frame = std::uint64_t{0u}; frame < audio.frame_count; ++frame) {
    auto sum = std::int32_t{0};

    for (auto channel = std::uint32_t{0u}; channel < audio.channels; ++channel) {
      sum += audio.samples[frame * audio.channels + channel];
    }

    mono[frame] = static_cast<std::int16_t>(sum / static_cast<std::int32_t>(audio.channels));
  }

  audio.samples = std::move(mono);
  audio.channels = 1u;
}

// ponytail: >2 channels (5.1, 7.1) collapse to mono; add AL_EXT_MCFORMATS output if surround clips are needed.
static auto output_channels(std::uint32_t source_channels, bool force_mono, const std::filesystem::path& source) -> std::uint32_t {
  if (force_mono || source_channels == 1u) {
    return 1u;
  }

  if (source_channels > 2u) {
    utility::logger<"assets">::warn("Cook: '{}' has {} channels; only mono and stereo are supported, downmixing to mono", source.generic_string(), source_channels);
    return 1u;
  }

  return source_channels;
}

static auto write_cooked_audio_clip(const std::filesystem::path& cooked, const audio_clip_header& header, const void* payload) -> bool {
  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write '{}'", cooked.generic_string());
    return false;
  }

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(static_cast<const char*>(payload), static_cast<std::streamsize>(header.payload_size));

  return static_cast<bool>(out);
}

auto asset_cooker::resolve_audio_clip(const std::filesystem::path& source, const std::filesystem::path& cooked, const audio_clip_import_settings& settings, bool needs_cook, bool& did_cook) -> std::optional<cooked_audio_clip_data> {
  did_cook = false;

  if (needs_cook) {
    if (!_cook_audio_clip(source, cooked, settings)) {
      utility::logger<"assets">::warn("Could not cook audio clip '{}'", source.generic_string());
      return std::nullopt;
    }
    did_cook = true;
  }

  auto data = cooked_audio_clip_data{};

  if (!_load_cooked_audio_clip(cooked, data)) {
    if (!_cook_audio_clip(source, cooked, settings) || !_load_cooked_audio_clip(cooked, data)) {
      utility::logger<"assets">::warn("Could not load cooked audio clip '{}'", cooked.generic_string());
      return std::nullopt;
    }
    did_cook = true;
  }

  return data;
}

auto asset_cooker::_cook_audio_clip(const std::filesystem::path& source, const std::filesystem::path& cooked, const audio_clip_import_settings& settings) -> bool {
  if (settings.load_type == audio_load_type::compressed_in_memory && lowercase_extension(source) == ".ogg") {
    auto in = std::ifstream{source, std::ios::binary};

    if (!in) {
      utility::logger<"assets">::warn("Cook: could not open '{}'", source.generic_string());
      return false;
    }

    const auto bytes = std::vector<char>{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};

    auto error = 0;
    auto* vorbis = stb_vorbis_open_memory(reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<int>(bytes.size()), &error, nullptr);

    if (vorbis == nullptr) {
      utility::logger<"assets">::warn("Cook: could not parse Ogg Vorbis '{}' (error {})", source.generic_string(), error);
      return false;
    }

    const auto info = stb_vorbis_get_info(vorbis);
    const auto frame_count = stb_vorbis_stream_length_in_samples(vorbis);

    stb_vorbis_close(vorbis);

    const auto header = audio_clip_header{
      audio_clip_magic,
      audio_clip_cook_version,
      info.sample_rate,
      output_channels(static_cast<std::uint32_t>(info.channels), settings.force_mono, source),
      frame_count,
      static_cast<std::uint32_t>(audio_clip_encoding::vorbis),
      0u,
      bytes.size()
    };

    if (!write_cooked_audio_clip(cooked, header, bytes.data())) {
      return false;
    }

    utility::logger<"assets">::debug("Cooked audio clip '{}' -> '{}' (vorbis, {} Hz, {} ch, {} frames)", source.generic_string(), cooked.generic_string(), header.sample_rate, header.channels, header.frame_count);

    return true;
  }

  if (settings.load_type == audio_load_type::compressed_in_memory) {
    utility::logger<"assets">::debug("Cook: '{}' isn't Ogg Vorbis, so it's decompressed on load", source.generic_string());
  }

  auto audio = decode_audio_file(source);

  if (!audio) {
    utility::logger<"assets">::warn("Cook: could not decode audio '{}'", source.generic_string());
    return false;
  }

  if (output_channels(audio->channels, settings.force_mono, source) == 1u) {
    downmix_to_mono(*audio);
  }

  const auto header = audio_clip_header{
    audio_clip_magic,
    audio_clip_cook_version,
    audio->sample_rate,
    audio->channels,
    audio->frame_count,
    static_cast<std::uint32_t>(audio_clip_encoding::pcm_s16),
    0u,
    audio->samples.size() * sizeof(std::int16_t)
  };

  if (!write_cooked_audio_clip(cooked, header, audio->samples.data())) {
    return false;
  }

  utility::logger<"assets">::debug("Cooked audio clip '{}' -> '{}' (pcm, {} Hz, {} ch, {} frames)", source.generic_string(), cooked.generic_string(), header.sample_rate, header.channels, header.frame_count);

  return true;
}

auto asset_cooker::_load_cooked_audio_clip(const std::filesystem::path& cooked, cooked_audio_clip_data& data) -> bool {
  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = audio_clip_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != audio_clip_magic || header.version != audio_clip_cook_version) {
    return false; // missing, corrupt or stale: the caller recooks
  }

  data.sample_rate = header.sample_rate;
  data.channels = header.channels;
  data.frame_count = header.frame_count;
  data.encoding = static_cast<audio_clip_encoding>(header.encoding);

  if (data.encoding == audio_clip_encoding::vorbis) {
    data.encoded.resize(header.payload_size);
    in.read(reinterpret_cast<char*>(data.encoded.data()), static_cast<std::streamsize>(header.payload_size));

    return static_cast<bool>(in);
  }

  if (header.payload_size != header.frame_count * header.channels * sizeof(std::int16_t)) {
    return false;
  }

  data.samples.resize(header.payload_size / sizeof(std::int16_t));
  in.read(reinterpret_cast<char*>(data.samples.data()), static_cast<std::streamsize>(header.payload_size));

  return static_cast<bool>(in);
}

} // namespace sbx::assets
