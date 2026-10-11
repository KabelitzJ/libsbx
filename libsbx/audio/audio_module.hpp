// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_AUDIO_AUDIO_MODULE_HPP_
#define LIBSBX_AUDIO_AUDIO_MODULE_HPP_

#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include <AL/al.h>
#include <AL/alc.h>

#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/audio_clip.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scenes_module.hpp>

namespace sbx::audio {

/**
 * @brief Plays every @ref scenes::audio_source in the active scene through OpenAL Soft, from a fixed pool of voices.
 *
 * Scene audio only advances while @ref scenes::scenes_module::is_simulating(), like physics and particles; pausing the simulation pauses it.
 * Asset previews ignore that gate. Without an audio device the module logs once and stays silent.
 */
class audio_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<assets::assets_module, scenes::scenes_module>;

  audio_module();

  ~audio_module();

  /** @brief Runs after scenes_module::late_update, so world transforms are current. */
  auto late_update() -> void;

  /**
   * @brief Plays @p clip once without a source component, e.g. for impacts or UI clicks.
   *
   * Waits for the clip to finish loading. Pauses with the simulation like any scene audio.
   *
   * @param spatial Positions the sound at @p position; otherwise it plays at the listener.
   */
  auto play_one_shot(const assets::audio_clip_handle& clip, const math::vector3& position, std::float_t volume = 1.0f, const math::uuid& mixer_group = math::uuid::nil(), bool spatial = true) -> void;

  /** @brief Plays @p clip non-spatially for auditioning in the editor, replacing any running preview. Ignores the simulation gate. */
  auto preview(const assets::audio_clip_handle& clip) -> void;

  auto stop_preview() -> void;

  [[nodiscard]] auto is_previewing() const -> bool;

  /** @brief Overrides a mixer group's volume for this session without touching the project file. */
  auto set_mixer_group_volume(const math::uuid& group, std::float_t volume) -> void;

  /** @brief Drops a set_mixer_group_volume override, returning to the project's volume. */
  auto reset_mixer_group_volume(const math::uuid& group) -> void;

  /** @brief The group's volume including any session override, not multiplied by its parents. */
  [[nodiscard]] auto mixer_group_volume(const math::uuid& group) const -> std::float_t;

  [[nodiscard]] auto has_device() const noexcept -> bool {
    return _context != nullptr;
  }

private:

  inline static constexpr auto stream_buffer_count = std::size_t{4u};
  inline static constexpr auto stream_buffer_frames = std::size_t{16384u};

  enum class voice_owner : std::uint8_t {
    none,
    source,
    one_shot,
    preview
  }; // enum class voice_owner

  struct vorbis_deleter {
    auto operator()(stb_vorbis* decoder) const -> void {
      stb_vorbis_close(decoder);
    }
  }; // struct vorbis_deleter

  struct stream_state {
    std::unique_ptr<stb_vorbis, vorbis_deleter> decoder{};
    std::array<ALuint, stream_buffer_count> buffers{};
    std::deque<std::uint32_t> queued_frames{}; // frames in each queued buffer, oldest first
    std::uint64_t consumed_frames{0u}; // frames fully played, for the playback position
    bool ended{false};
  }; // struct stream_state

  struct voice {
    ALuint source{0u};
    std::uint32_t generation{0u};
    voice_owner owner{voice_owner::none};
    bool touched{false};
    bool paused_by_simulation{false};
    bool loop{false};
    std::uint8_t priority{128u};
    assets::audio_clip_handle clip{};
    std::uint64_t clip_generation{0u};
    math::vector3 position{0.0f, 0.0f, 0.0f};
    math::vector3 last_position{0.0f, 0.0f, 0.0f};
    bool has_last_position{false};
    stream_state stream{};
  }; // struct voice

  struct clip_buffer {
    ALuint buffer{0u};
    std::uint64_t generation{0u};
  }; // struct clip_buffer

  struct pending_one_shot {
    assets::audio_clip_handle clip;
    math::vector3 position;
    std::float_t volume;
    math::uuid mixer_group;
    bool spatial;
  }; // struct pending_one_shot

  struct device_deleter {
    auto operator()(ALCdevice* device) const -> void {
      alcCloseDevice(device);
    }
  }; // struct device_deleter

  struct context_deleter {
    auto operator()(ALCcontext* context) const -> void {
      alcMakeContextCurrent(nullptr);
      alcDestroyContext(context);
    }
  }; // struct context_deleter

  auto _update_mixer_gains() -> void;

  [[nodiscard]] auto _group_gain(const math::uuid& group) const -> std::float_t;

  auto _update_listener(scenes::scene& scene, std::float_t delta_time) -> void;

  auto _update_source(scenes::audio_source& source, const math::vector3& position, std::float_t delta_time, std::float_t simulation_delta_time, bool simulating) -> void;

  /** @brief Starts pending one-shots and the pending preview once loaded, and frees finished ones. */
  auto _update_one_shots(bool simulating) -> void;

  auto _start_one_shot(const pending_one_shot& request) -> void;

  auto _set_scene_voices_paused(bool paused) -> void;

  /** @brief A free voice, or one taken from a less important sound. Never hands out the preview voice. */
  [[nodiscard]] auto _acquire(std::uint8_t priority, const math::vector3& position) -> voice*;

  auto _release(voice& target) -> void;

  [[nodiscard]] auto _resolve(const scenes::audio_voice_ref& reference) -> voice*;

  [[nodiscard]] auto _reference_of(const voice& target) const -> scenes::audio_voice_ref;

  /** @brief Binds @p clip to @p target and starts it @p offset seconds in. */
  auto _start(voice& target, const assets::audio_clip_handle& clip, bool loop, std::float_t offset) -> bool;

  [[nodiscard]] auto _buffer_for(const assets::audio_clip_handle& clip) -> ALuint;

  auto _fill_stream_buffer(voice& target, ALuint buffer) -> bool;

  auto _update_stream(voice& target) -> void;

  [[nodiscard]] auto _playback_time(voice& target) const -> std::float_t;

  auto _apply_spatial(voice& target, bool spatial, const math::vector3& position, std::float_t delta_time, std::float_t doppler_level) -> void;

  std::unique_ptr<ALCdevice, device_deleter> _device{};
  std::unique_ptr<ALCcontext, context_deleter> _context{};

  std::vector<voice> _voices{}; // the last voice is reserved for previews
  std::unordered_map<const assets::audio_clip*, clip_buffer> _clip_buffers{};
  std::vector<pending_one_shot> _pending_one_shots{};
  assets::audio_clip_handle _pending_preview{};
  std::vector<std::int16_t> _stream_scratch{};

  std::unordered_map<math::uuid, std::float_t> _group_gains{};
  std::unordered_map<math::uuid, std::float_t> _group_volume_overrides{};

  math::vector3 _listener_position{0.0f, 0.0f, 0.0f};
  math::vector3 _listener_last_position{0.0f, 0.0f, 0.0f};
  bool _listener_has_last_position{false};

  bool _was_simulating{false};

}; // class audio_module

} // namespace sbx::audio

#endif // LIBSBX_AUDIO_AUDIO_MODULE_HPP_
