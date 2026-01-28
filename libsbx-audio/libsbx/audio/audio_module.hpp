// SPDX-License-Identifier: MIT
#ifndef LIBSBX_AUDIO_AUDIO_MODULE_HPP_
#define LIBSBX_AUDIO_AUDIO_MODULE_HPP_

#include <cstdint>

#include <vector>
#include <stdexcept>

#include <dr_wav.h>

#include <AL/al.h>
#include <AL/alc.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace sbx::audio {

struct wav_data {
  std::vector<std::int16_t> samples;
  std::uint32_t sample_rate;
  std::uint16_t channels;
}; // struct wav_data

auto load_wav(const char* filepath) -> wav_data {
  drwav wav{};

  if (!drwav_init_file(&wav, filepath, nullptr)) {
    throw std::runtime_error("Failed to load WAV");
  }

  wav_data out;
  out.sample_rate = wav.sampleRate;
  out.channels = wav.channels;

  out.samples.resize(wav.totalPCMFrameCount * wav.channels);
  drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, out.samples.data());

  drwav_uninit(&wav);
  return out;
}

class audio_module final : public core::module<audio_module> {

  inline static const auto is_registered = register_module(stage::normal, dependencies<assets::assets_module>{});

public:

  audio_module() {
    _device = alcOpenDevice(nullptr);

    if (!_device) {
      throw std::runtime_error{"Failed to open OpenAL device."};
    }

    _context = alcCreateContext(_device, nullptr);

    if (!_context) {
      alcCloseDevice(_device);
      throw std::runtime_error{"Failed to create OpenAL context."};
    }

    if (!alcMakeContextCurrent(_context)) {
      alcDestroyContext(_context);
      alcCloseDevice(_device);
      throw std::runtime_error{"Failed to make OpenAL context current."};
    }

    alGenSources(1, &_source);

    if (alGetError() != AL_NO_ERROR) {
      alcMakeContextCurrent(nullptr);
      alcDestroyContext(_context);
      alcCloseDevice(_device);

      throw std::runtime_error{"Failed to generate OpenAL source."};
    }

    alSourcef(_source, AL_PITCH, 1.0f);
    alSourcef(_source, AL_GAIN, 1.0f);
  }

  ~audio_module() override {
    alSourceStop(_source);
    alDeleteSources(1, &_source);
    alDeleteBuffers(1, &_buffer);

    alcMakeContextCurrent(nullptr);
    alcDestroyContext(_context);
    alcCloseDevice(_device);
  }

  auto start() -> void {
    auto& assets_module = core::engine::get_module<assets::assets_module>();

    auto wav = load_wav(assets_module.resolve_path("res://audio/forest.wav").string().c_str());

    auto format = ALenum{};

    if (wav.channels == 1) {
      format = AL_FORMAT_MONO16;
    } else if (wav.channels == 2) {
      format = AL_FORMAT_STEREO16;
    } else {
      throw std::runtime_error{"Unsupported number of channels in WAV file."};
    }

    utility::logger<"audio">::debug("Loaded WAV: {} Hz, {} channels, {} samples", wav.sample_rate, wav.channels, wav.samples.size());

    alGenBuffers(1, &_buffer);

    if (alGetError() != AL_NO_ERROR) {
      throw std::runtime_error{"Failed to generate OpenAL buffer."};
    }

    alBufferData(_buffer, format, wav.samples.data(), static_cast<ALsizei>(wav.samples.size() * sizeof(std::int16_t)), static_cast<ALsizei>(wav.sample_rate));

    alSourcei(_source, AL_BUFFER, static_cast<ALint>(_buffer));
    alSourcei(_source, AL_LOOPING, AL_TRUE);

    alSourcePlay(_source);
  }

  auto update() -> void override {

  }

private:

  ALCdevice* _device;
  ALCcontext* _context;

  ALuint _source;
  ALuint _buffer;

}; // class audio_module

} // namespace sbx::audio

#endif // LIBSBX_AUDIO_AUDIO_MODULE_HPP_
