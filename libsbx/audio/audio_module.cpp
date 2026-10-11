// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/audio/audio_module.hpp>

#include <algorithm>
#include <optional>

#include <AL/alext.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/profiler.hpp>

#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/core/project.hpp>

namespace sbx::audio {

inline constexpr auto one_shot_priority = std::uint8_t{128u};
inline constexpr auto one_shot_min_distance = 1.0f;
inline constexpr auto one_shot_max_distance = 50.0f;

static auto al_format(std::uint32_t channels) -> ALenum {
  return channels == 1u ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
}

static auto column(const math::matrix4x4& matrix, std::size_t index) -> math::vector3 {
  return math::vector3{matrix[index][0], matrix[index][1], matrix[index][2]};
}

static auto source_state(ALuint source) -> ALint {
  auto state = ALint{AL_INITIAL};
  alGetSourcei(source, AL_SOURCE_STATE, &state);
  return state;
}

static auto set_distance(ALuint source, std::float_t min_distance, std::float_t max_distance, scenes::audio_rolloff rolloff) -> void {
  const auto reference = std::max(min_distance, 0.01f);

  alSourcef(source, AL_REFERENCE_DISTANCE, reference);
  alSourcef(source, AL_MAX_DISTANCE, std::max(max_distance, reference));
  alSourcef(source, AL_ROLLOFF_FACTOR, 1.0f);
  alSourcei(source, AL_DISTANCE_MODEL, rolloff == scenes::audio_rolloff::linear ? AL_LINEAR_DISTANCE_CLAMPED : AL_INVERSE_DISTANCE_CLAMPED);
}

audio_module::audio_module() {
  auto device = std::unique_ptr<ALCdevice, device_deleter>{alcOpenDevice(nullptr)};

  if (!device) {
    utility::logger<"audio">::warn("No audio device available; audio is disabled");
    return;
  }

  auto context = std::unique_ptr<ALCcontext, context_deleter>{alcCreateContext(device.get(), nullptr)};

  if (!context || !alcMakeContextCurrent(context.get())) {
    utility::logger<"audio">::warn("Could not create an OpenAL context; audio is disabled");
    return;
  }

  alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

  if (alIsExtensionPresent("AL_EXT_source_distance_model")) {
    alEnable(AL_SOURCE_DISTANCE_MODEL);
  }

  // One extra voice, always last, is reserved for editor previews.
  _voices.resize(core::engine::project().max_voices() + 1u);

  for (auto& target : _voices) {
    alGenSources(1, &target.source);

    if (alGetError() != AL_NO_ERROR) {
      target.source = 0u;
    }
  }

  std::erase_if(_voices, [](const voice& target) { return target.source == 0u; });

  _stream_scratch.resize(stream_buffer_frames * 2u);

  utility::logger<"audio">::info("Opened audio device '{}' with {} voices", alcGetString(device.get(), ALC_DEVICE_SPECIFIER), _voices.size());

  _device = std::move(device);
  _context = std::move(context);
}

audio_module::~audio_module() {
  if (!_context) {
    return;
  }

  for (auto& target : _voices) {
    _release(target);

    if (target.stream.buffers.front() != 0u) {
      alDeleteBuffers(static_cast<ALsizei>(target.stream.buffers.size()), target.stream.buffers.data());
    }

    alDeleteSources(1, &target.source);
  }

  for (auto& [clip, entry] : _clip_buffers) {
    alDeleteBuffers(1, &entry.buffer);
  }
}

auto audio_module::late_update() -> void {
  SBX_PROFILE_SCOPE("audio_module::late_update");

  if (!_context) {
    return;
  }

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  const auto simulating = scenes_module.is_simulating();
  const auto delta_time = core::engine::delta_time().value();
  const auto simulation_delta_time = scenes_module.simulation_delta_time().value();

  if (simulating != _was_simulating) {
    _set_scene_voices_paused(!simulating);
    _was_simulating = simulating;
  }

  _update_mixer_gains();
  _update_listener(scene, delta_time);

  for (auto& target : _voices) {
    target.touched = false;
  }

  for (auto&& [entity, world, source] : scene.query<scenes::world_transform, scenes::audio_source>(ecs::exclude<scenes::inactive>).each()) {
    _update_source(source, column(world.matrix, 3u), delta_time, simulation_delta_time, simulating);
  }

  // Frees voices whose source was destroyed, deactivated, lost its component or was reloaded (e.g. leaving Play mode).
  for (auto& target : _voices) {
    if (target.owner == voice_owner::source && !target.touched) {
      _release(target);
    }
  }

  _update_one_shots(simulating);

  for (auto& target : _voices) {
    if (target.owner != voice_owner::none && target.stream.decoder && !target.paused_by_simulation) {
      _update_stream(target);
    }
  }
}

auto audio_module::play_one_shot(const assets::audio_clip_handle& clip, const math::vector3& position, std::float_t volume, const math::uuid& mixer_group, bool spatial) -> void {
  if (!_context || !clip.is_valid()) {
    return;
  }

  _pending_one_shots.push_back(pending_one_shot{clip, position, volume, mixer_group, spatial});
}

auto audio_module::preview(const assets::audio_clip_handle& clip) -> void {
  if (!_context || _voices.empty() || !clip.is_valid()) {
    return;
  }

  _release(_voices.back());
  _pending_preview = clip;
}

auto audio_module::stop_preview() -> void {
  _pending_preview = assets::audio_clip_handle{};

  if (!_voices.empty()) {
    _release(_voices.back());
  }
}

auto audio_module::is_previewing() const -> bool {
  return _pending_preview.is_valid() || (!_voices.empty() && _voices.back().owner == voice_owner::preview);
}

auto audio_module::set_mixer_group_volume(const math::uuid& group, std::float_t volume) -> void {
  _group_volume_overrides[group] = volume;
}

auto audio_module::reset_mixer_group_volume(const math::uuid& group) -> void {
  _group_volume_overrides.erase(group);
}

auto audio_module::mixer_group_volume(const math::uuid& group) const -> std::float_t {
  if (const auto entry = _group_volume_overrides.find(group); entry != _group_volume_overrides.end()) {
    return entry->second;
  }

  const auto& groups = core::engine::project().mixer_groups();

  const auto entry = std::ranges::find(groups, group, &core::mixer_group::id);

  return entry != groups.end() ? entry->volume : 1.0f;
}

auto audio_module::_update_mixer_gains() -> void {
  const auto& groups = core::engine::project().mixer_groups();

  _group_gains.clear();

  // ponytail: O(groups * depth) every frame; cache on a project change signal if mixers grow to hundreds of groups.
  for (const auto& group : groups) {
    auto gain = 1.0f;
    auto current = groups.begin() + (&group - groups.data());

    // project::set_mixer_groups guarantees every chain reaches Master; the step limit is only a backstop.
    for (auto steps = std::size_t{0u}; steps <= groups.size() && current != groups.end(); ++steps) {
      gain *= current->muted ? 0.0f : mixer_group_volume(current->id);

      if (current->parent == math::uuid::nil()) {
        break;
      }

      current = std::ranges::find(groups, current->parent, &core::mixer_group::id);
    }

    _group_gains[group.id] = gain;
  }
}

auto audio_module::_group_gain(const math::uuid& group) const -> std::float_t {
  if (const auto entry = _group_gains.find(group); entry != _group_gains.end()) {
    return entry->second;
  }

  if (const auto master = _group_gains.find(core::master_mixer_group_id); master != _group_gains.end()) {
    return master->second;
  }

  return 1.0f;
}

auto audio_module::_update_listener(scenes::scene& scene, std::float_t delta_time) -> void {
  auto matrix = std::optional<math::matrix4x4>{};
  auto volume = 1.0f;

  for (auto&& [entity, world, listener] : scene.query<scenes::world_transform, scenes::audio_listener>(ecs::exclude<scenes::inactive>).each()) {
    matrix = world.matrix;
    volume = listener.volume;
    break;
  }

  if (!matrix && scene.has_active_camera()) {
    if (auto camera = scene.active_camera(); camera.is_valid()) {
      matrix = camera.world_matrix();
    }
  }

  const auto& world = matrix ? *matrix : math::matrix4x4::identity;

  _listener_position = column(world, 3u);

  auto velocity = math::vector3{0.0f, 0.0f, 0.0f};

  if (_listener_has_last_position && delta_time > 0.0f) {
    velocity = (_listener_position - _listener_last_position) / delta_time;
  }

  _listener_last_position = _listener_position;
  _listener_has_last_position = true;

  const auto forward = math::vector3::normalized(-column(world, 2u));
  const auto up = math::vector3::normalized(column(world, 1u));

  const auto orientation = std::array<ALfloat, 6u>{forward.x(), forward.y(), forward.z(), up.x(), up.y(), up.z()};

  alListener3f(AL_POSITION, _listener_position.x(), _listener_position.y(), _listener_position.z());
  alListener3f(AL_VELOCITY, velocity.x(), velocity.y(), velocity.z());
  alListenerfv(AL_ORIENTATION, orientation.data());
  alListenerf(AL_GAIN, volume);
}

auto audio_module::_update_source(scenes::audio_source& source, const math::vector3& position, std::float_t delta_time, std::float_t simulation_delta_time, bool simulating) -> void {
  auto* current = _resolve(source.voice);

  if (current != nullptr) {
    current->touched = true;
  }

  if (!simulating) {
    return;
  }

  if (!source.awoken) {
    source.awoken = true;

    if (source.play_on_awake && source.clip.is_valid()) {
      source.playback = scenes::audio_playback_state::playing;
      source.restart = true;
    }
  }

  if (source.playback == scenes::audio_playback_state::stopped) {
    if (current != nullptr) {
      _release(*current);
    }

    source.voice = scenes::audio_voice_ref{};
    source.time = 0.0f;
    source.restart = false;

    return;
  }

  if (!source.clip.is_valid()) {
    if (current != nullptr) {
      _release(*current);
    }

    source.voice = scenes::audio_voice_ref{};

    return;
  }

  if (!source.clip.is_loaded()) {
    return;
  }

  const auto& clip = *source.clip;

  // A swapped or reimported clip starts over.
  if (current != nullptr && (current->clip.get() != source.clip.get() || current->clip_generation != clip.generation())) {
    _release(*current);
    current = nullptr;
    source.time = 0.0f;
  }

  if (source.restart) {
    source.restart = false;
    source.seek = false;
    source.time = 0.0f;

    if (current != nullptr) {
      _release(*current);
      current = nullptr;
    }
  }

  // Rebinding is the simplest seek that works for both buffers and streams; the voice is reacquired below at source.time.
  if (source.seek) {
    source.seek = false;

    if (current != nullptr) {
      _release(*current);
      current = nullptr;
    }
  }

  if (current == nullptr) {
    if (source.playback == scenes::audio_playback_state::paused) {
      return;
    }

    current = _acquire(source.priority, position);

    if (current != nullptr && !_start(*current, source.clip, source.loop, source.time)) {
      _release(*current);
      current = nullptr;
    }

    if (current == nullptr) {
      // Virtual: inaudible, but the clock keeps running so it resumes in sync once a voice frees up.
      source.voice = scenes::audio_voice_ref{};
      source.time += simulation_delta_time * source.pitch;

      if (clip.duration() > 0.0f && source.time >= clip.duration()) {
        if (source.loop) {
          source.time = std::fmod(source.time, clip.duration());
        } else {
          source.playback = scenes::audio_playback_state::stopped;
          source.time = 0.0f;
        }
      }

      return;
    }

    current->owner = voice_owner::source;
    current->touched = true;
    source.voice = _reference_of(*current);
  }

  const auto handle = current->source;

  current->priority = source.priority;
  current->loop = source.loop;

  alSourcef(handle, AL_GAIN, std::max(source.volume, 0.0f) * _group_gain(source.mixer_group));
  alSourcef(handle, AL_PITCH, std::max(source.pitch, 0.01f));

  if (!current->stream.decoder) {
    alSourcei(handle, AL_LOOPING, source.loop ? AL_TRUE : AL_FALSE);
  }

  set_distance(handle, source.min_distance, source.max_distance, source.rolloff);
  _apply_spatial(*current, source.spatial, position, delta_time, source.doppler_level);

  const auto state = source_state(handle);

  if (source.playback == scenes::audio_playback_state::paused) {
    if (state == AL_PLAYING) {
      alSourcePause(handle);
    }
  } else if (state == AL_INITIAL || state == AL_PAUSED) {
    alSourcePlay(handle);
  } else if (state == AL_STOPPED && (!current->stream.decoder || current->stream.ended)) {
    _release(*current);
    source.voice = scenes::audio_voice_ref{};
    source.playback = scenes::audio_playback_state::stopped;
    source.time = 0.0f;

    return;
  }

  source.time = _playback_time(*current);
}

auto audio_module::_update_one_shots(bool simulating) -> void {
  if (_pending_preview.is_valid() && _pending_preview.is_loaded() && !_voices.empty()) {
    auto& target = _voices.back();

    if (_start(target, _pending_preview, false, 0.0f)) {
      target.owner = voice_owner::preview;
      alSourcef(target.source, AL_GAIN, 1.0f);
      alSourcef(target.source, AL_PITCH, 1.0f);
      _apply_spatial(target, false, _listener_position, 0.0f, 0.0f);
      alSourcePlay(target.source);
    } else {
      _release(target);
    }

    _pending_preview = assets::audio_clip_handle{};
  }

  if (simulating) {
    std::erase_if(_pending_one_shots, [this](const pending_one_shot& request) {
      if (!request.clip.is_loaded()) {
        return false;
      }

      _start_one_shot(request);

      return true;
    });
  }

  for (auto& target : _voices) {
    if (target.owner != voice_owner::one_shot && target.owner != voice_owner::preview) {
      continue;
    }

    if (target.paused_by_simulation) {
      continue;
    }

    if (source_state(target.source) == AL_STOPPED && (!target.stream.decoder || target.stream.ended)) {
      _release(target);
    }
  }
}

auto audio_module::_start_one_shot(const pending_one_shot& request) -> void {
  auto* target = _acquire(one_shot_priority, request.position);

  if (target == nullptr) {
    return; // every voice is busy with something at least as important
  }

  if (!_start(*target, request.clip, false, 0.0f)) {
    _release(*target);
    return;
  }

  target->owner = voice_owner::one_shot;
  target->priority = one_shot_priority;

  alSourcef(target->source, AL_GAIN, std::max(request.volume, 0.0f) * _group_gain(request.mixer_group));
  alSourcef(target->source, AL_PITCH, 1.0f);
  set_distance(target->source, one_shot_min_distance, one_shot_max_distance, scenes::audio_rolloff::inverse);
  _apply_spatial(*target, request.spatial, request.position, 0.0f, 0.0f);

  alSourcePlay(target->source);
}

auto audio_module::_set_scene_voices_paused(bool paused) -> void {
  for (auto& target : _voices) {
    if (target.owner != voice_owner::source && target.owner != voice_owner::one_shot) {
      continue;
    }

    if (paused) {
      if (source_state(target.source) == AL_PLAYING) {
        alSourcePause(target.source);
        target.paused_by_simulation = true;
      }
    } else if (target.paused_by_simulation) {
      target.paused_by_simulation = false;
      alSourcePlay(target.source);
    }
  }
}

auto audio_module::_acquire(std::uint8_t priority, const math::vector3& position) -> voice* {
  if (_voices.size() < 2u) {
    return nullptr;
  }

  const auto pool = std::span{_voices}.first(_voices.size() - 1u);

  if (const auto idle = std::ranges::find(pool, voice_owner::none, &voice::owner); idle != pool.end()) {
    return &*idle;
  }

  // Takes the least important voice, the farthest one among equals, but only from a sound that matters less than the request.
  auto* victim = static_cast<voice*>(nullptr);
  auto victim_distance = 0.0f;

  for (auto& candidate : pool) {
    const auto distance = (candidate.position - _listener_position).length_squared();

    if (victim == nullptr || candidate.priority > victim->priority || (candidate.priority == victim->priority && distance > victim_distance)) {
      victim = &candidate;
      victim_distance = distance;
    }
  }

  const auto requester_distance = (position - _listener_position).length_squared();

  if (victim->priority < priority || (victim->priority == priority && victim_distance <= requester_distance)) {
    return nullptr;
  }

  _release(*victim);

  return victim;
}

auto audio_module::_release(voice& target) -> void {
  alSourceStop(target.source);
  alSourcei(target.source, AL_BUFFER, 0);

  target.stream.decoder.reset();
  target.stream.queued_frames.clear();
  target.stream.consumed_frames = 0u;
  target.stream.ended = false;

  target.owner = voice_owner::none;
  target.clip = assets::audio_clip_handle{};
  target.clip_generation = 0u;
  target.paused_by_simulation = false;
  target.has_last_position = false;
  target.loop = false;

  ++target.generation;
}

auto audio_module::_resolve(const scenes::audio_voice_ref& reference) -> voice* {
  if (reference.index >= _voices.size()) {
    return nullptr;
  }

  auto& target = _voices[reference.index];

  return target.generation == reference.generation && target.owner == voice_owner::source ? &target : nullptr;
}

auto audio_module::_reference_of(const voice& target) const -> scenes::audio_voice_ref {
  return scenes::audio_voice_ref{static_cast<std::uint32_t>(&target - _voices.data()), target.generation};
}

auto audio_module::_start(voice& target, const assets::audio_clip_handle& clip, bool loop, std::float_t offset) -> bool {
  const auto& data = *clip;

  if (data.frame_count() == 0u || data.sample_rate() == 0u) {
    return false;
  }

  alGetError();
  alSourceRewind(target.source);

  offset = data.duration() > 0.0f ? std::fmod(std::max(offset, 0.0f), data.duration()) : 0.0f;

  if (data.encoding() == assets::audio_clip_encoding::pcm_s16) {
    const auto buffer = _buffer_for(clip);

    if (buffer == 0u) {
      return false;
    }

    target.clip = clip;
    target.clip_generation = data.generation();
    target.loop = loop;

    alSourcei(target.source, AL_BUFFER, static_cast<ALint>(buffer));
    alSourcei(target.source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    alSourcef(target.source, AL_SEC_OFFSET, offset);

    return alGetError() == AL_NO_ERROR;
  }

  auto error = 0;
  const auto encoded = data.encoded();

  target.stream.decoder.reset(stb_vorbis_open_memory(reinterpret_cast<const unsigned char*>(encoded.data()), static_cast<int>(encoded.size()), &error, nullptr));

  if (!target.stream.decoder) {
    utility::logger<"audio">::warn("Could not open Ogg Vorbis stream for clip {} (error {})", data.id(), error);
    return false;
  }

  if (target.stream.buffers.front() == 0u) {
    alGenBuffers(static_cast<ALsizei>(target.stream.buffers.size()), target.stream.buffers.data());

    if (alGetError() != AL_NO_ERROR) {
      target.stream.buffers.fill(0u);
      return false;
    }
  }

  target.clip = clip;
  target.clip_generation = data.generation();
  target.loop = loop;

  const auto start_frame = static_cast<std::uint32_t>(offset * static_cast<std::float_t>(data.sample_rate()));

  if (start_frame > 0u) {
    stb_vorbis_seek(target.stream.decoder.get(), start_frame);
  }

  target.stream.consumed_frames = start_frame;
  target.stream.queued_frames.clear();
  target.stream.ended = false;

  alSourcei(target.source, AL_LOOPING, AL_FALSE);

  for (auto buffer : target.stream.buffers) {
    if (!_fill_stream_buffer(target, buffer)) {
      break;
    }

    alSourceQueueBuffers(target.source, 1, &buffer);
  }

  return !target.stream.queued_frames.empty() && alGetError() == AL_NO_ERROR;
}

auto audio_module::_buffer_for(const assets::audio_clip_handle& clip) -> ALuint {
  const auto& data = *clip;

  auto& entry = _clip_buffers[clip.get()];

  if (entry.buffer != 0u && entry.generation == data.generation()) {
    return entry.buffer;
  }

  if (entry.buffer != 0u) {
    // Reimported: a buffer can't be deleted while attached, so detach every voice still playing the old content.
    for (auto& target : _voices) {
      if (target.clip.get() == clip.get()) {
        _release(target);
      }
    }

    alDeleteBuffers(1, &entry.buffer);
    entry.buffer = 0u;
  }

  const auto samples = data.samples();

  alGetError();
  alGenBuffers(1, &entry.buffer);
  alBufferData(entry.buffer, al_format(data.channels()), samples.data(), static_cast<ALsizei>(samples.size_bytes()), static_cast<ALsizei>(data.sample_rate()));

  if (alGetError() != AL_NO_ERROR) {
    utility::logger<"audio">::warn("Could not upload audio clip {}", data.id());

    alDeleteBuffers(1, &entry.buffer);
    entry.buffer = 0u;

    return 0u;
  }

  entry.generation = data.generation();

  return entry.buffer;
}

auto audio_module::_fill_stream_buffer(voice& target, ALuint buffer) -> bool {
  const auto& data = *target.clip;
  const auto channels = static_cast<int>(data.channels());
  const auto capacity = static_cast<int>(stream_buffer_frames);

  auto frames = 0;
  auto looped_without_progress = false;

  while (frames < capacity) {
    // Decodes and, for a mono clip from a stereo source, downmixes to the requested channel count.
    const auto decoded = stb_vorbis_get_samples_short_interleaved(target.stream.decoder.get(), channels, _stream_scratch.data() + frames * channels, (capacity - frames) * channels);

    if (decoded > 0) {
      frames += decoded;
      looped_without_progress = false;
      continue;
    }

    if (!target.loop || looped_without_progress) {
      target.stream.ended = true;
      break;
    }

    stb_vorbis_seek_start(target.stream.decoder.get());
    looped_without_progress = true;
  }

  if (frames == 0) {
    return false;
  }

  alBufferData(buffer, al_format(data.channels()), _stream_scratch.data(), static_cast<ALsizei>(static_cast<std::size_t>(frames * channels) * sizeof(std::int16_t)), static_cast<ALsizei>(data.sample_rate()));

  target.stream.queued_frames.push_back(static_cast<std::uint32_t>(frames));

  return true;
}

auto audio_module::_update_stream(voice& target) -> void {
  auto processed = ALint{0};
  alGetSourcei(target.source, AL_BUFFERS_PROCESSED, &processed);

  for (; processed > 0; --processed) {
    auto buffer = ALuint{0u};
    alSourceUnqueueBuffers(target.source, 1, &buffer);

    if (!target.stream.queued_frames.empty()) {
      target.stream.consumed_frames += target.stream.queued_frames.front();
      target.stream.queued_frames.pop_front();
    }

    if (!target.stream.ended && _fill_stream_buffer(target, buffer)) {
      alSourceQueueBuffers(target.source, 1, &buffer);
    }
  }

  auto queued = ALint{0};
  alGetSourcei(target.source, AL_BUFFERS_QUEUED, &queued);

  // Starved: the frame took longer than the queued audio. Resume; the gap is audible but brief.
  if (queued > 0 && source_state(target.source) == AL_STOPPED) {
    alSourcePlay(target.source);
  }
}

auto audio_module::_playback_time(voice& target) const -> std::float_t {
  const auto& data = *target.clip;

  if (!target.stream.decoder) {
    auto seconds = ALfloat{0.0f};
    alGetSourcef(target.source, AL_SEC_OFFSET, &seconds);
    return seconds;
  }

  auto offset = ALint{0};
  alGetSourcei(target.source, AL_SAMPLE_OFFSET, &offset);

  const auto seconds = static_cast<std::float_t>(target.stream.consumed_frames + static_cast<std::uint64_t>(offset)) / static_cast<std::float_t>(data.sample_rate());

  return data.duration() > 0.0f ? std::fmod(seconds, data.duration()) : 0.0f;
}

auto audio_module::_apply_spatial(voice& target, bool spatial, const math::vector3& position, std::float_t delta_time, std::float_t doppler_level) -> void {
  const auto handle = target.source;

  // AL_SOFT_source_spatialize: stereo clips are downmixed and positioned too when spatial, so force_mono is only a memory saving.
  alSourcei(handle, AL_SOURCE_SPATIALIZE_SOFT, spatial ? AL_TRUE : AL_FALSE);
  alSourcei(handle, AL_SOURCE_RELATIVE, spatial ? AL_FALSE : AL_TRUE);

  if (!spatial) {
    target.position = _listener_position;
    target.has_last_position = false;

    alSource3f(handle, AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSource3f(handle, AL_VELOCITY, 0.0f, 0.0f, 0.0f);

    return;
  }

  auto velocity = math::vector3{0.0f, 0.0f, 0.0f};

  if (target.has_last_position && delta_time > 0.0f) {
    velocity = (position - target.last_position) / delta_time * doppler_level;
  }

  target.position = position;
  target.last_position = position;
  target.has_last_position = true;

  alSource3f(handle, AL_POSITION, position.x(), position.y(), position.z());
  alSource3f(handle, AL_VELOCITY, velocity.x(), velocity.y(), velocity.z());
}

} // namespace sbx::audio
