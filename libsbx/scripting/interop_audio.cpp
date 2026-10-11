// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/interop.hpp>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <libsbx/utility/logger.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/audio/audio_module.hpp>

#include <libsbx/scenes/components.hpp>

namespace sbx::scripting {

template<typename Component>
static auto audio_component_of(std::uint64_t uuid, std::string_view call) -> memory::observer_ptr<Component> {
  auto& scene = core::engine::get_module<scenes::scenes_module>().active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !node.has_component<Component>()) {
    utility::logger<"scripting">::error("{}: node {} has no {}", call, uuid, utility::type_name<Component>());
    return nullptr;
  }

  return memory::make_observer(node.get_component<Component>());
}

static auto load_audio_clip(std::uint64_t clip_uuid) -> assets::audio_clip_handle {
  if (clip_uuid == 0u) {
    return assets::audio_clip_handle{};
  }

  return core::engine::get_module<assets::assets_module>().load_audio_clip(math::uuid::from_value(clip_uuid));
}

static auto find_mixer_group(std::string_view name) -> std::optional<math::uuid> {
  const auto& groups = core::engine::project().mixer_groups();

  const auto entry = std::ranges::find(groups, name, &core::mixer_group::name);

  return entry != groups.end() ? std::optional{entry->id} : std::nullopt;
}

auto interop::audio_clip_load(managed::string path) -> std::uint64_t {
  const auto clip = core::engine::get_module<assets::assets_module>().load_audio_clip(std::filesystem::path{std::string{path}});

  return clip.is_valid() ? clip->id().value() : 0u;
}

auto interop::audio_clip_is_loaded(std::uint64_t clip_uuid) -> managed::bool32 {
  return load_audio_clip(clip_uuid).is_loaded();
}

auto interop::audio_clip_get_length(std::uint64_t clip_uuid) -> std::float_t {
  const auto clip = load_audio_clip(clip_uuid);

  return clip.is_loaded() ? clip->duration() : 0.0f;
}

auto interop::audio_clip_get_channels(std::uint64_t clip_uuid) -> std::uint32_t {
  const auto clip = load_audio_clip(clip_uuid);

  return clip.is_loaded() ? clip->channels() : 0u;
}

auto interop::audio_clip_get_frequency(std::uint64_t clip_uuid) -> std::uint32_t {
  const auto clip = load_audio_clip(clip_uuid);

  return clip.is_loaded() ? clip->sample_rate() : 0u;
}

auto interop::audio_source_get_settings(std::uint64_t uuid, audio_source_data* out_value) -> void {
  const auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource settings");

  if (!source || !out_value) {
    return;
  }

  *out_value = audio_source_data{
    source->volume,
    source->pitch,
    source->min_distance,
    source->max_distance,
    source->doppler_level,
    source->priority,
    static_cast<std::uint32_t>(source->rolloff),
    source->loop,
    source->play_on_awake,
    source->spatial
  };
}

auto interop::audio_source_set_settings(std::uint64_t uuid, const audio_source_data* value) -> void {
  auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource settings");

  if (!source || !value) {
    return;
  }

  source->volume = std::max(value->volume, 0.0f);
  source->pitch = std::max(value->pitch, 0.01f);
  source->min_distance = std::max(value->min_distance, 0.01f);
  source->max_distance = std::max(value->max_distance, source->min_distance);
  source->doppler_level = std::max(value->doppler_level, 0.0f);
  source->priority = static_cast<std::uint8_t>(std::min(value->priority, 255u));
  source->rolloff = value->rolloff == static_cast<std::uint32_t>(scenes::audio_rolloff::linear) ? scenes::audio_rolloff::linear : scenes::audio_rolloff::inverse;
  source->loop = value->loop != 0u;
  source->play_on_awake = value->play_on_awake != 0u;
  source->spatial = value->spatial != 0u;
}

auto interop::audio_source_get_clip(std::uint64_t uuid) -> std::uint64_t {
  const auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource.Clip");

  return source && source->clip.is_valid() ? source->clip->id().value() : 0u;
}

auto interop::audio_source_set_clip(std::uint64_t uuid, std::uint64_t clip_uuid) -> void {
  if (auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource.Clip")) {
    source->clip = load_audio_clip(clip_uuid);
  }
}

auto interop::audio_source_get_output_group(std::uint64_t uuid) -> managed::string {
  const auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource.OutputGroup");
  const auto& groups = core::engine::project().mixer_groups();

  const auto group = source ? std::ranges::find(groups, source->mixer_group, &core::mixer_group::id) : groups.end();

  return managed::string::create(group != groups.end() ? group->name.c_str() : groups.front().name.c_str());
}

auto interop::audio_source_set_output_group(std::uint64_t uuid, managed::string name) -> managed::bool32 {
  auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource.OutputGroup");

  if (!source) {
    return false;
  }

  const auto group_name = std::string{name};
  const auto group = find_mixer_group(group_name);

  if (!group) {
    utility::logger<"scripting">::error("AudioSource.OutputGroup: no mixer group named '{}'", group_name);
    return false;
  }

  source->mixer_group = *group;

  return true;
}

auto interop::audio_source_play(std::uint64_t uuid) -> void {
  if (auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource.Play")) {
    source->playback = scenes::audio_playback_state::playing;
    source->restart = true;
  }
}

auto interop::audio_source_pause(std::uint64_t uuid) -> void {
  if (auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource.Pause"); source && source->playback == scenes::audio_playback_state::playing) {
    source->playback = scenes::audio_playback_state::paused;
  }
}

auto interop::audio_source_unpause(std::uint64_t uuid) -> void {
  if (auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource.UnPause"); source && source->playback == scenes::audio_playback_state::paused) {
    source->playback = scenes::audio_playback_state::playing;
  }
}

auto interop::audio_source_stop(std::uint64_t uuid) -> void {
  if (auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource.Stop")) {
    source->playback = scenes::audio_playback_state::stopped;
  }
}

auto interop::audio_source_get_is_playing(std::uint64_t uuid) -> managed::bool32 {
  const auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource.IsPlaying");

  return source && source->playback == scenes::audio_playback_state::playing;
}

auto interop::audio_source_get_time(std::uint64_t uuid) -> std::float_t {
  const auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource.Time");

  return source ? source->time : 0.0f;
}

auto interop::audio_source_set_time(std::uint64_t uuid, std::float_t seconds) -> void {
  if (auto source = audio_component_of<scenes::audio_source>(uuid, "AudioSource.Time")) {
    source->time = std::max(seconds, 0.0f);
    source->seek = true;
  }
}

auto interop::audio_source_play_one_shot(std::uint64_t uuid, std::uint64_t clip_uuid, std::float_t volume_scale) -> void {
  auto& scene = core::engine::get_module<scenes::scenes_module>().active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !node.has_component<scenes::audio_source>()) {
    utility::logger<"scripting">::error("AudioSource.PlayOneShot: node {} has no audio_source", uuid);
    return;
  }

  const auto& source = node.get_component<scenes::audio_source>();
  const auto& world = node.world_matrix();

  core::engine::get_module<audio::audio_module>().play_one_shot(load_audio_clip(clip_uuid), math::vector3{world[3][0], world[3][1], world[3][2]}, source.volume * volume_scale, source.mixer_group, source.spatial);
}

auto interop::audio_listener_get_volume(std::uint64_t uuid) -> std::float_t {
  const auto listener = audio_component_of<scenes::audio_listener>(uuid, "AudioListener.Volume");

  return listener ? listener->volume : 0.0f;
}

auto interop::audio_listener_set_volume(std::uint64_t uuid, std::float_t volume) -> void {
  if (auto listener = audio_component_of<scenes::audio_listener>(uuid, "AudioListener.Volume")) {
    listener->volume = std::max(volume, 0.0f);
  }
}

auto interop::audio_play_clip_at_point(std::uint64_t clip_uuid, math::vector3* position, std::float_t volume) -> void {
  if (!position) {
    return;
  }

  core::engine::get_module<audio::audio_module>().play_one_shot(load_audio_clip(clip_uuid), *position, volume);
}

auto interop::audio_set_mixer_group_volume(managed::string name, std::float_t volume) -> managed::bool32 {
  const auto group_name = std::string{name};
  const auto group = find_mixer_group(group_name);

  if (!group) {
    utility::logger<"scripting">::error("Audio.SetMixerGroupVolume: no mixer group named '{}'", group_name);
    return false;
  }

  core::engine::get_module<audio::audio_module>().set_mixer_group_volume(*group, std::max(volume, 0.0f));

  return true;
}

auto interop::audio_get_mixer_group_volume(managed::string name) -> std::float_t {
  const auto group = find_mixer_group(std::string{name});

  return group ? core::engine::get_module<audio::audio_module>().mixer_group_volume(*group) : 1.0f;
}

auto interop::audio_reset_mixer_group_volume(managed::string name) -> void {
  if (const auto group = find_mixer_group(std::string{name})) {
    core::engine::get_module<audio::audio_module>().reset_mixer_group_volume(*group);
  }
}

} // namespace sbx::scripting
