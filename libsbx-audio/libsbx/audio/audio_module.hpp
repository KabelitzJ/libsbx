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

struct sound_listener {
  std::float_t gain = 1.0f;
}; // struct sound_listener

struct sound_clip_reference {
  std::string uri;
}; // struct sound_clip_reference

enum class playback : std::uint8_t { 
  stopped, 
  playing, 
  paused 
}; // enum class playback

struct sound_source {
  sound_clip_reference clip;

  std::float_t gain  = 1.0f;
  std::float_t pitch = 1.0f;
  bool looping = false;

  bool spatial = true;
  std::float_t reference_distance = 1.0f;
  std::float_t max_distance = 100.0f;
  std::float_t rolloff = 1.0f;

  playback desired = playback::stopped;

  // Runtime (owned by audio system)
  ALuint al_source = 0;         // 0 => not allocated yet
  ALuint bound_buffer = 0;      // last buffer bound (avoid redundant calls)
  bool dirty_params = true;     // when any param changes
}; // struct sound_source

class audio_module final : public core::module<audio_module> {

  inline static const auto is_registered = register_module(stage::normal, dependencies<assets::assets_module>{});

  
public:

  struct sound_clip {
    ALuint buffer = 0;
    ALenum format = 0;
    ALsizei sample_rate = 0;
  }; // struct sound_clip

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
  }

  ~audio_module() override {
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(_context);
    alcCloseDevice(_device);
  }

  auto get_or_load_clip_buffer(const std::string& uri) -> sound_clip& {
    if (auto it = _clip_cache.find(uri); it != _clip_cache.end()) {
      return it->second;
    }

    auto& assets = core::engine::get_module<assets::assets_module>();

    auto path = assets.resolve_path(uri).string();

    auto wav = _load_wav(path);

    auto format = ALenum{};

    if (wav.channels == 1) {
      format = AL_FORMAT_MONO16;
    } else if (wav.channels == 2) {
      format = AL_FORMAT_STEREO16;
    } else {
      throw std::runtime_error{"Unsupported WAV channel count"};
    }

    auto clip = sound_clip{};
    clip.format = format;
    clip.sample_rate = static_cast<ALsizei>(wav.sample_rate);

    alGenBuffers(1, &clip.buffer);

    if (alGetError() != AL_NO_ERROR) {
      throw std::runtime_error{"alGenBuffers failed."};
    }

    alBufferData(clip.buffer, clip.format, wav.samples.data(), static_cast<ALsizei>(wav.samples.size() * sizeof(std::int16_t)), clip.sample_rate);

    if (alGetError() != AL_NO_ERROR) {
      alDeleteBuffers(1, &clip.buffer);

      throw std::runtime_error{"alBufferData failed."};
    }

    auto [it, _] = _clip_cache.emplace(uri, clip);

    return it->second;
  }

  auto update() -> void override {
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

    auto camera_node = environment.camera();

    const auto& camera_transform = graph.get_component<scenes::transform>(camera_node);

    alListener3f(AL_POSITION, camera_transform.position().x(), camera_transform.position().y(), camera_transform.position().z());
    alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);

    if (graph.has_component<audio::sound_listener>(camera_node)) {
      const auto& listener = graph.get_component<audio::sound_listener>(camera_node);

      alListenerf(AL_GAIN, listener.gain);
    }

    const auto forward = camera_transform.forward();
    const auto up = camera_transform.up();

    auto orientation = std::array<std::float_t, 6u>{
      forward.x(), forward.y(), forward.z(),
      up.x(), up.y(), up.z()
    };

    alListenerfv(AL_ORIENTATION, orientation.data());
  }

private:

  struct wav_data {
    std::vector<std::int16_t> samples;
    std::uint32_t sample_rate;
    std::uint16_t channels;
  }; // struct wav_data

  auto _load_wav(const std::string& filepath) -> wav_data {
    auto wav = drwav{};

    if (!drwav_init_file(&wav, filepath.c_str(), nullptr)) {
      throw std::runtime_error("Failed to load WAV");
    }

    auto out = wav_data{};
    out.sample_rate = wav.sampleRate;
    out.channels = wav.channels;

    out.samples.resize(wav.totalPCMFrameCount * wav.channels);
    drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, out.samples.data());

    drwav_uninit(&wav);

    return out;
  }

  ALCdevice* _device;
  ALCcontext* _context;

  std::unordered_map<std::string, sound_clip> _clip_cache;

}; // class audio_module

} // namespace sbx::audio

#endif // LIBSBX_AUDIO_AUDIO_MODULE_HPP_
