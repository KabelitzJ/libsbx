// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/scenes_module.hpp>

#include <algorithm>
#include <chrono>
#include <utility>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/profiler.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/scene_serializer.hpp>

namespace sbx::scenes {

scenes_module::scenes_module() {

}

scenes_module::~scenes_module() { 

}

auto scenes_module::pre_update() -> void {
  apply_pending_operations();
  _update_async_loads();
}

auto scenes_module::load_scene(const std::filesystem::path& path, load_scene_mode mode) -> queued_load {
  const auto operation = pending_operation{_next_operation_id++, operation_kind::load, _scene_set.next_handle++, path, mode};

  _pending_operations.push_back(operation);

  return queued_load{operation.id, operation.handle};
}

auto scenes_module::new_scene() -> queued_load {
  return load_scene({}, load_scene_mode::single);
}

auto scenes_module::unload_scene(std::uint32_t handle) -> std::uint32_t {
  const auto operation = pending_operation{_next_operation_id++, operation_kind::unload, handle};

  _pending_operations.push_back(operation);

  return operation.id;
}

auto scenes_module::is_operation_done(std::uint32_t operation) const -> bool {
  return operation < _next_operation_id
    && std::ranges::none_of(_pending_operations, [&](const pending_operation& pending) { return pending.id == operation; })
    && std::ranges::none_of(_async_loads, [&](const async_load& pending) { return pending.operation.id == operation; });
}

auto scenes_module::load_scene_async(const std::filesystem::path& path, load_scene_mode mode) -> queued_load {
  auto pending = async_load{};
  pending.operation = pending_operation{_next_operation_id++, operation_kind::load, _scene_set.next_handle++, path, mode};

  const auto queued = queued_load{pending.operation.id, pending.operation.handle};
  const auto file = core::engine::project().assets_directory() / path;

  // Rejected up front, which leaves the operation done with nothing loaded.
  if (path.extension() != ".scene" || !std::filesystem::exists(file)) {
    utility::logger<"scenes">::error("Cannot load scene '{}': no such scene file", path.generic_string());
    return queued;
  }

  if (!_is_loadable(pending.operation)) {
    return queued;
  }

  pending.parse = std::async(std::launch::async, [file] { return YAML::LoadFile(file.string()); });

  _async_loads.push_back(std::move(pending));

  return queued;
}

auto scenes_module::operation_progress(std::uint32_t operation) const -> std::float_t {
  if (is_operation_done(operation)) {
    return 1.0f;
  }

  const auto pending = std::ranges::find(_async_loads, operation, [](const async_load& load) { return load.operation.id; });

  return pending != _async_loads.end() ? pending->progress : 0.0f;
}

auto scenes_module::set_allow_scene_activation(std::uint32_t operation, bool allow) -> void {
  if (const auto pending = std::ranges::find(_async_loads, operation, [](const async_load& load) { return load.operation.id; }); pending != _async_loads.end()) {
    pending->allow_activation = allow;
  }
}

auto scenes_module::allow_scene_activation(std::uint32_t operation) const -> bool {
  const auto pending = std::ranges::find(_async_loads, operation, [](const async_load& load) { return load.operation.id; });

  return pending == _async_loads.end() || pending->allow_activation;
}

auto scenes_module::_update_async_loads() -> void {
  for (auto& pending : _async_loads) {
    if (pending.failed) {
      continue;
    }

    if (!pending.parsed) {
      if (pending.parse.wait_for(std::chrono::seconds{0}) != std::future_status::ready) {
        continue;
      }

      try {
        pending.snapshot = pending.parse.get();
        pending.parsed = true;
      } catch (const YAML::Exception& exception) {
        utility::logger<"scenes">::error("Cannot load scene '{}': {}", pending.operation.path.generic_string(), exception.what());
        pending.failed = true;
        continue;
      }

      _request_dependencies(pending);
    }

    const auto total = pending.dependencies.size();
    const auto ready = static_cast<std::size_t>(std::ranges::count_if(pending.dependencies, [](const auto& is_ready) { return is_ready(); }));

    pending.dependencies_ready = ready == total;
    pending.progress = 0.1f + 0.8f * (total == 0u ? 1.0f : static_cast<std::float_t>(ready) / static_cast<std::float_t>(total));
  }

  _finish_async_loads(false);
}

auto scenes_module::_finish_async_loads(bool force) -> void {
  while (!_async_loads.empty()) {
    auto& front = _async_loads.front();

    if (front.failed) {
      _async_loads.erase(_async_loads.begin());
      continue;
    }

    if (force && !front.parsed) {
      try {
        front.snapshot = front.parse.get();
        front.parsed = true;
      } catch (const YAML::Exception& exception) {
        utility::logger<"scenes">::error("Cannot load scene '{}': {}", front.operation.path.generic_string(), exception.what());
        _async_loads.erase(_async_loads.begin());
        continue;
      }
    }

    // In start order: one still loading, or held back, holds back those after it.
    if (!force && (!front.parsed || !front.dependencies_ready || !front.allow_activation)) {
      break;
    }

    // Moved out first: activation runs scripts, which may start further async loads.
    auto finished = std::move(front);
    _async_loads.erase(_async_loads.begin());

    _activate_load(finished.operation, finished.snapshot);
  }
}

auto scenes_module::_request_dependencies(async_load& pending) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto assets_node = pending.snapshot["assets"];

  if (!assets_node) {
    return;
  }

  const auto request = [&](const char* category, auto&& load_asset) {
    if (const auto table = assets_node[category]) {
      for (const auto entry : table) {
        pending.dependencies.push_back([handle = load_asset(entry["uuid"].template as<math::uuid>())] {
          return !handle.is_valid() || handle.is_loaded();
        });
      }
    }
  };

  request("static_meshes", [&](const math::uuid& id) { return assets_module.load_mesh(id); });
  request("materials", [&](const math::uuid& id) { return assets_module.load_material(id); });
  request("particle_effects", [&](const math::uuid& id) { return assets_module.load_particle_effect(id); });
  request("animation_graphs", [&](const math::uuid& id) { return assets_module.load_animation_graph(id); });
  request("textures", [&](const math::uuid& id) { return assets_module.load_texture(id); });
  request("fonts", [&](const math::uuid& id) { return assets_module.load_font(id); });
  request("audio_clips", [&](const math::uuid& id) { return assets_module.load_audio_clip(id); });
  request("prefabs", [&](const math::uuid& id) { return assets_module.load_prefab(id); });
}

auto scenes_module::apply_pending_operations() -> void {
  // Swapped out first: a scene_loaded handler (a script's OnCreate) may queue the next load.
  const auto operations = std::exchange(_pending_operations, {});

  for (const auto& operation : operations) {
    _apply(operation);
  }
}

auto scenes_module::find_loaded_scene(std::uint32_t handle) const -> memory::observer_ptr<const loaded_scene> {
  const auto entry = std::ranges::find(_scene_set.scenes, handle, &loaded_scene::handle);

  return entry != _scene_set.scenes.end() ? memory::make_observer<const loaded_scene>(*entry) : nullptr;
}

auto scenes_module::set_scene_path(std::uint32_t handle, const std::filesystem::path& path) -> void {
  if (const auto entry = std::ranges::find(_scene_set.scenes, handle, &loaded_scene::handle); entry != _scene_set.scenes.end()) {
    const auto build_index = core::engine::project().build_index_of(path);

    entry->path = path;
    entry->name = path.stem().string();
    entry->build_index = build_index ? static_cast<std::int32_t>(*build_index) : -1;
  }
}

auto scenes_module::restore_scene_set(scene_set set) -> void {
  // A load a script queued in its last Play frame must not land in edit mode.
  _pending_operations.clear();
  _async_loads.clear();
  _scene_set = std::move(set);
  _scene.set_active_membership(_scene_set.active);
}

auto scenes_module::set_active_scene(std::uint32_t handle) -> bool {
  if (handle == persistent_scene_handle || !find_loaded_scene(handle)) {
    utility::logger<"scenes">::error("Cannot make scene {} active: it isn't loaded", handle);
    return false;
  }

  if (handle != _scene_set.active) {
    _change_active(handle);
  }

  return true;
}

auto scenes_module::move_node_to_scene(node target, std::uint32_t handle) -> bool {
  if (!target.is_valid() || !find_loaded_scene(handle)) {
    return false;
  }

  // Scenes own top-level nodes, so a nested node moves together with its whole top-level ancestor.
  auto top_level = target;

  while (true) {
    const auto parent = _scene.node_of(top_level.get_component<relationship>().parent);

    if (parent == _scene.root()) {
      break;
    }

    top_level = parent;
  }

  top_level.get_or_add_component<scene_membership>().handle = handle;

  return true;
}

auto scenes_module::dont_destroy_on_load(node target) -> void {
  _ensure_persistent_scene();
  move_node_to_scene(target, persistent_scene_handle);
}

auto scenes_module::_ensure_persistent_scene() -> void {
  if (!find_loaded_scene(persistent_scene_handle)) {
    _scene_set.scenes.insert(_scene_set.scenes.begin(), loaded_scene{persistent_scene_handle, {}, "DontDestroyOnLoad", -1});
  }
}

auto scenes_module::_change_active(std::uint32_t handle) -> void {
  const auto previous = _scene_set.active;

  _scene_set.active = handle;
  _scene.set_active_membership(handle);

  if (const auto record = find_loaded_scene(handle)) {
    _scene.set_name(record->name);
  }

  // The camera may have gone with an unloaded scene: fall back to any camera left in the world.
  if (!_scene.has_active_camera()) {
    for (const auto entity : _scene.query<scenes::camera>(ecs::exclude<inactive>)) {
      _scene.set_active_camera(_scene.node_of(entity));
      break;
    }
  }

  _on_active_scene_changed(active_scene_changed_event{previous, handle});
}

auto scenes_module::_apply(const pending_operation& operation) -> void {
  if (operation.kind == operation_kind::unload) {
    _apply_unload(operation);
  } else {
    _apply_load(operation);
  }
}

auto scenes_module::_apply_unload(const pending_operation& operation) -> void {
  SBX_PROFILE_SCOPE("scenes_module::_apply_unload");

  const auto record = find_loaded_scene(operation.handle);

  if (!record || operation.handle == persistent_scene_handle) {
    utility::logger<"scenes">::error("Cannot unload scene {}: it isn't a loaded scene", operation.handle);
    return;
  }

  const auto remaining = std::ranges::count_if(_scene_set.scenes, [&](const loaded_scene& loaded) { return loaded.handle != persistent_scene_handle && loaded.handle != operation.handle; });

  if (remaining == 0) {
    utility::logger<"scenes">::error("Cannot unload scene '{}': it's the only loaded scene", record->name);
    return;
  }

  const auto unloaded = *record;

  _on_scene_unloading(unloaded.handle);
  _scene.destroy_scene_nodes(unloaded.handle);

  if (_scene_set.active == unloaded.handle) {
    const auto next = std::ranges::find_if(_scene_set.scenes, [&](const loaded_scene& loaded) { return loaded.handle != persistent_scene_handle && loaded.handle != unloaded.handle; });
    _change_active(next->handle);
  } else if (!_scene.has_active_camera()) {
    _change_active(_scene_set.active); // re-picks a camera
  }

  _on_scene_unloaded(unloaded);

  std::erase_if(_scene_set.scenes, [&](const loaded_scene& loaded) { return loaded.handle == unloaded.handle; });

  utility::logger<"scenes">::info("Unloaded scene '{}'", unloaded.name);
}

auto scenes_module::_apply_load(const pending_operation& operation) -> void {
  // Like Unity: a synchronous load first completes every async load queued before it, even ones held back by allow_scene_activation.
  _finish_async_loads(true);

  if (!_is_loadable(operation)) {
    return;
  }

  auto snapshot = YAML::Node{};

  // Resolved before anything is unloaded, so a bad path leaves the current scenes alone.
  if (!operation.path.empty()) {
    const auto asset = core::engine::get_module<assets::assets_module>().load_scene(operation.path);

    if (!asset.is_valid()) {
      utility::logger<"scenes">::error("Cannot load scene '{}': not a scene asset", operation.path.generic_string());
      return;
    }

    snapshot = asset->snapshot();
  }

  _activate_load(operation, snapshot);
}

auto scenes_module::_is_loadable(const pending_operation& operation) const -> bool {
  if (_require_scene_list && !operation.path.empty() && !core::engine::project().build_index_of(operation.path)) {
    utility::logger<"scenes">::error("Cannot load scene '{}': it isn't in the project's scene list (Project Settings > Scenes)", operation.path.generic_string());
    return false;
  }

  return true;
}

auto scenes_module::_activate_load(const pending_operation& operation, const YAML::Node& snapshot) -> void {
  SBX_PROFILE_SCOPE("scenes_module::_activate_load");

  const auto build_index = operation.path.empty() ? std::nullopt : core::engine::project().build_index_of(operation.path);

  if (operation.mode == load_scene_mode::single) {
    for (const auto& loaded : _scene_set.scenes) {
      if (loaded.handle != persistent_scene_handle) {
        _on_scene_unloading(loaded.handle);
      }
    }

    for (const auto& loaded : _scene_set.scenes) {
      if (loaded.handle != persistent_scene_handle) {
        _scene.destroy_scene_nodes(loaded.handle);
      }
    }

    // Announced while still listed, like an additive unload; copied, since handlers may read the list.
    const auto unloaded = _scene_set.scenes;

    for (const auto& loaded : unloaded) {
      if (loaded.handle != persistent_scene_handle) {
        _on_scene_unloaded(loaded);
      }
    }

    std::erase_if(_scene_set.scenes, [](const loaded_scene& loaded) { return loaded.handle != persistent_scene_handle; });
  }

  const auto handle = operation.handle;
  const auto name = operation.path.empty() ? std::string{"Scene"} : operation.path.stem().string();

  _scene_set.scenes.push_back(loaded_scene{handle, operation.path, name, build_index ? static_cast<std::int32_t>(*build_index) : -1});

  if (operation.path.empty()) {
    const auto previous = _scene.active_membership();
    _scene.set_active_membership(handle);

    auto camera = _scene.create_node("Camera");
    camera.add_component<scenes::camera>();
    _scene.set_active_camera(camera);

    _scene.set_active_membership(previous);
  } else {
    scene_serializer::load_into(_scene, snapshot, handle, operation.mode == load_scene_mode::single);
  }

  if (operation.mode == load_scene_mode::single) {
    _change_active(handle);
  }

  utility::logger<"scenes">::info("Loaded scene '{}' ({})", name, reflection::to_string(operation.mode));

  _on_scene_loaded(scene_loaded_event{handle, operation.mode});
}

auto scenes_module::late_update() -> void {
  SBX_PROFILE_SCOPE("scenes_module::late_update");

  _simulation_time += simulation_delta_time();

  _scene.update();

  // Resyncs any prefab_instance whose source has been edited since this scene last saw it — see
  // scene_serializer::sync_prefab_instances. Runs every frame, editor and runtime alike, same as
  // the transform update above.
  scene_serializer::sync_prefab_instances(_scene);
}

auto scenes_module::release_instance_buffer(graphics::buffer_handle handle) -> void {
  auto lock = std::lock_guard{_released_instance_buffers_mutex};
  _released_instance_buffers.push_back(handle);
}

auto scenes_module::collect_released_instance_buffers() -> void {
  auto lock = std::lock_guard{_released_instance_buffers_mutex};

  if (_released_instance_buffers.empty()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  const auto frame_index = graphics_module.frame_context().frame_index();

  for (const auto handle : _released_instance_buffers) {
    registry.retire(handle, frame_index);
  }

  _released_instance_buffers.clear();
}

} // namespace sbx::scenes
