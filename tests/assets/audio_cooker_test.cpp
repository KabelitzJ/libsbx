// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include <libsbx/assets/asset_cooker.hpp>

using namespace sbx::assets;

// Interleaved stereo: (L, R) per frame.
constexpr auto stereo_frames = std::array<std::int16_t, 8u>{1000, 3000, -2000, 2000, 400, 400, -100, -300};

static auto write_wav(const std::filesystem::path& path) -> void {
  const auto put = [](std::ofstream& out, auto value) { out.write(reinterpret_cast<const char*>(&value), sizeof(value)); };

  const auto data_size = static_cast<std::uint32_t>(sizeof(stereo_frames));

  auto out = std::ofstream{path, std::ios::binary};
  out.write("RIFF", 4);
  put(out, std::uint32_t{36u + data_size});
  out.write("WAVEfmt ", 8);
  put(out, std::uint32_t{16u});     // fmt chunk size
  put(out, std::uint16_t{1u});      // PCM
  put(out, std::uint16_t{2u});      // channels
  put(out, std::uint32_t{44100u});  // sample rate
  put(out, std::uint32_t{44100u * 4u}); // byte rate
  put(out, std::uint16_t{4u});      // block align
  put(out, std::uint16_t{16u});     // bits per sample
  out.write("data", 4);
  put(out, data_size);
  out.write(reinterpret_cast<const char*>(stereo_frames.data()), data_size);
}

TEST(audio_cooker_test, wav_cooks_to_pcm_and_force_mono_averages_channels) {
  const auto directory = std::filesystem::temp_directory_path() / "libsbx_audio_cooker_test";
  std::filesystem::create_directories(directory);

  const auto source = directory / "clip.wav";
  write_wav(source);

  auto did_cook = false;

  const auto stereo = asset_cooker::resolve_audio_clip(source, directory / "stereo.sbxaud", audio_clip_import_settings{}, true, did_cook);

  ASSERT_TRUE(stereo.has_value());
  EXPECT_TRUE(did_cook);
  EXPECT_EQ(stereo->encoding, audio_clip_encoding::pcm_s16);
  EXPECT_EQ(stereo->sample_rate, 44100u);
  EXPECT_EQ(stereo->channels, 2u);
  EXPECT_EQ(stereo->frame_count, 4u);
  EXPECT_TRUE(std::ranges::equal(stereo->samples, stereo_frames));

  // Compressed-in-memory only applies to Ogg Vorbis, so a WAV still decompresses.
  const auto mono = asset_cooker::resolve_audio_clip(source, directory / "mono.sbxaud", audio_clip_import_settings{audio_load_type::compressed_in_memory, true}, true, did_cook);

  ASSERT_TRUE(mono.has_value());
  EXPECT_EQ(mono->encoding, audio_clip_encoding::pcm_s16);
  EXPECT_EQ(mono->channels, 1u);
  EXPECT_EQ(mono->frame_count, 4u);
  EXPECT_TRUE(std::ranges::equal(mono->samples, std::array<std::int16_t, 4u>{2000, 0, 400, -200}));

  // A second resolve without needs_cook reads the cached blob.
  const auto cached = asset_cooker::resolve_audio_clip(source, directory / "mono.sbxaud", audio_clip_import_settings{audio_load_type::compressed_in_memory, true}, false, did_cook);

  ASSERT_TRUE(cached.has_value());
  EXPECT_FALSE(did_cook);
  EXPECT_EQ(cached->samples, mono->samples);

  std::filesystem::remove_all(directory);
}
