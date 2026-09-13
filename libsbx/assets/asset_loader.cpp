// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_loader.hpp>

#include <utility>

#include <libsbx/utility/profiler.hpp>

namespace sbx::assets {

asset_loader::asset_loader() {
  _thread = std::thread{[this] { _worker_loop(); }};
}

asset_loader::~asset_loader() {
  abort();
}

auto asset_loader::submit(texture_request request) -> void {
  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  {
    auto lock = std::lock_guard{_request_mutex};
    _requests.push_back(std::move(request));
  }

  _request_condition.notify_one();
}

auto asset_loader::submit(mesh_request request) -> void {
  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  {
    auto lock = std::lock_guard{_request_mutex};
    _requests.push_back(std::move(request));
  }

  _request_condition.notify_one();
}

auto asset_loader::submit(font_request request) -> void {
  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  {
    auto lock = std::lock_guard{_request_mutex};
    _requests.push_back(std::move(request));
  }

  _request_condition.notify_one();
}

auto asset_loader::submit(material_request request) -> void {
  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  {
    auto lock = std::lock_guard{_request_mutex};
    _requests.push_back(std::move(request));
  }

  _request_condition.notify_one();
}

auto asset_loader::submit(particle_effect_request request) -> void {
  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  {
    auto lock = std::lock_guard{_request_mutex};
    _requests.push_back(std::move(request));
  }

  _request_condition.notify_one();
}

auto asset_loader::submit(animation_graph_request request) -> void {
  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  {
    auto lock = std::lock_guard{_request_mutex};
    _requests.push_back(std::move(request));
  }

  _request_condition.notify_one();
}

auto asset_loader::submit(skeleton_request request) -> void {
  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  {
    auto lock = std::lock_guard{_request_mutex};
    _requests.push_back(std::move(request));
  }

  _request_condition.notify_one();
}

auto asset_loader::submit(animation_clip_request request) -> void {
  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  {
    auto lock = std::lock_guard{_request_mutex};
    _requests.push_back(std::move(request));
  }

  _request_condition.notify_one();
}

auto asset_loader::abort() -> void {
  const auto already_aborted = _aborted.exchange(true, std::memory_order_relaxed);

  if (already_aborted) {
    if (_thread.joinable()) {
      _thread.join();
    }

    return;
  }

  {
    auto lock = std::lock_guard{_request_mutex};
    _requests.clear();
  }

  _request_condition.notify_one();

  if (_thread.joinable()) {
    _thread.join();
  }
}

auto asset_loader::_worker_loop() -> void {
  SBX_PROFILE_THREAD_NAME("Asset thread");

  while (true) {
    auto next = request{};

    {
      auto lock = std::unique_lock{_request_mutex};

      _request_condition.wait(lock, [this] { return _aborted.load(std::memory_order_relaxed) || !_requests.empty(); });

      if (_aborted.load(std::memory_order_relaxed)) {
        return;
      }

      next = std::move(_requests.front());
      _requests.pop_front();
    }

    std::visit([this](const auto& value) { _resolve(value); }, next);
  }
}

auto asset_loader::_resolve(const texture_request& request) -> void {
  SBX_PROFILE_SCOPE("asset_loader::_resolve texture");

  auto did_cook = false;
  auto data = asset_cooker::resolve_texture(request.source, request.cooked, request.needs_cook, did_cook);

  if (_aborted.load(std::memory_order_relaxed)) {
    return; // shutting down -- nothing will ever drain this, discard rather than push it
  }

  auto lock = std::lock_guard{_result_mutex};
  _resolved_textures.push_back(texture_result{request, std::move(data), did_cook});
}

auto asset_loader::_resolve(const mesh_request& request) -> void {
  SBX_PROFILE_SCOPE("asset_loader::_resolve mesh");

  auto did_cook = false;
  auto data = asset_cooker::resolve_mesh(request.source, request.id, request.cooked, request.needs_cook, did_cook);

  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  auto lock = std::lock_guard{_result_mutex};
  _resolved_meshes.push_back(mesh_result{request, std::move(data), did_cook});
}

auto asset_loader::_resolve(const font_request& request) -> void {
  SBX_PROFILE_SCOPE("asset_loader::_resolve font");

  auto did_cook = false;
  auto data = asset_cooker::resolve_font(request.source, request.cooked, request.needs_cook, did_cook);

  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  auto lock = std::lock_guard{_result_mutex};
  _resolved_fonts.push_back(font_result{request, std::move(data), did_cook});
}

auto asset_loader::_resolve(const material_request& request) -> void {
  SBX_PROFILE_SCOPE("asset_loader::_resolve material");

  auto data = (!request.source.empty() && request.source.extension() == ".material") ? asset_cooker::parse_material_file(request.source) : asset_cooker::resolve_cooked_material(request.id);

  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  auto lock = std::lock_guard{_result_mutex};
  _resolved_materials.push_back(material_result{request, std::move(data)});
}

auto asset_loader::_resolve(const particle_effect_request& request) -> void {
  SBX_PROFILE_SCOPE("asset_loader::_resolve particle_effect");

  auto data = asset_cooker::parse_particle_effect_file(request.source);

  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  auto lock = std::lock_guard{_result_mutex};
  _resolved_particle_effects.push_back(particle_effect_result{request, std::move(data)});
}

auto asset_loader::_resolve(const animation_graph_request& request) -> void {
  SBX_PROFILE_SCOPE("asset_loader::_resolve animation_graph");

  auto data = asset_cooker::parse_animation_graph_file(request.source);

  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  auto lock = std::lock_guard{_result_mutex};
  _resolved_animation_graphs.push_back(animation_graph_result{request, std::move(data)});
}

auto asset_loader::_resolve(const skeleton_request& request) -> void {
  SBX_PROFILE_SCOPE("asset_loader::_resolve skeleton");

  auto data = asset_cooker::resolve_skeleton(request.id);

  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  auto lock = std::lock_guard{_result_mutex};
  _resolved_skeletons.push_back(skeleton_result{request, std::move(data)});
}

auto asset_loader::_resolve(const animation_clip_request& request) -> void {
  SBX_PROFILE_SCOPE("asset_loader::_resolve animation_clip");

  auto data = asset_cooker::resolve_animation_clip(request.id);

  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  auto lock = std::lock_guard{_result_mutex};
  _resolved_animation_clips.push_back(animation_clip_result{request, std::move(data)});
}

auto asset_loader::take_resolved_textures(std::size_t max_count) -> std::vector<texture_result> {
  SBX_PROFILE_SCOPE("asset_loader::take_resolved_textures");

  auto result = std::vector<texture_result>{};

  auto lock = std::lock_guard{_result_mutex};

  while (max_count > 0u && !_resolved_textures.empty()) {
    result.push_back(std::move(_resolved_textures.front()));
    _resolved_textures.pop_front();
    --max_count;
  }

  return result;
}

auto asset_loader::take_resolved_meshes(std::size_t max_count) -> std::vector<mesh_result> {
  SBX_PROFILE_SCOPE("asset_loader::take_resolved_meshes");

  auto result = std::vector<mesh_result>{};

  auto lock = std::lock_guard{_result_mutex};

  while (max_count > 0u && !_resolved_meshes.empty()) {
    result.push_back(std::move(_resolved_meshes.front()));
    _resolved_meshes.pop_front();
    --max_count;
  }

  return result;
}

auto asset_loader::take_resolved_fonts(std::size_t max_count) -> std::vector<font_result> {
  SBX_PROFILE_SCOPE("asset_loader::take_resolved_fonts");

  auto result = std::vector<font_result>{};

  auto lock = std::lock_guard{_result_mutex};

  while (max_count > 0u && !_resolved_fonts.empty()) {
    result.push_back(std::move(_resolved_fonts.front()));
    _resolved_fonts.pop_front();
    --max_count;
  }

  return result;
}

auto asset_loader::take_resolved_materials(std::size_t max_count) -> std::vector<material_result> {
  SBX_PROFILE_SCOPE("asset_loader::take_resolved_materials");

  auto result = std::vector<material_result>{};

  auto lock = std::lock_guard{_result_mutex};

  while (max_count > 0u && !_resolved_materials.empty()) {
    result.push_back(std::move(_resolved_materials.front()));
    _resolved_materials.pop_front();
    --max_count;
  }

  return result;
}

auto asset_loader::take_resolved_particle_effects(std::size_t max_count) -> std::vector<particle_effect_result> {
  SBX_PROFILE_SCOPE("asset_loader::take_resolved_particle_effects");

  auto result = std::vector<particle_effect_result>{};

  auto lock = std::lock_guard{_result_mutex};

  while (max_count > 0u && !_resolved_particle_effects.empty()) {
    result.push_back(std::move(_resolved_particle_effects.front()));
    _resolved_particle_effects.pop_front();
    --max_count;
  }

  return result;
}

auto asset_loader::take_resolved_animation_graphs(std::size_t max_count) -> std::vector<animation_graph_result> {
  SBX_PROFILE_SCOPE("asset_loader::take_resolved_animation_graphs");

  auto result = std::vector<animation_graph_result>{};

  auto lock = std::lock_guard{_result_mutex};

  while (max_count > 0u && !_resolved_animation_graphs.empty()) {
    result.push_back(std::move(_resolved_animation_graphs.front()));
    _resolved_animation_graphs.pop_front();
    --max_count;
  }

  return result;
}

auto asset_loader::take_resolved_skeletons(std::size_t max_count) -> std::vector<skeleton_result> {
  SBX_PROFILE_SCOPE("asset_loader::take_resolved_skeletons");

  auto result = std::vector<skeleton_result>{};

  auto lock = std::lock_guard{_result_mutex};

  while (max_count > 0u && !_resolved_skeletons.empty()) {
    result.push_back(std::move(_resolved_skeletons.front()));
    _resolved_skeletons.pop_front();
    --max_count;
  }

  return result;
}

auto asset_loader::take_resolved_animation_clips(std::size_t max_count) -> std::vector<animation_clip_result> {
  SBX_PROFILE_SCOPE("asset_loader::take_resolved_animation_clips");

  auto result = std::vector<animation_clip_result>{};

  auto lock = std::lock_guard{_result_mutex};

  while (max_count > 0u && !_resolved_animation_clips.empty()) {
    result.push_back(std::move(_resolved_animation_clips.front()));
    _resolved_animation_clips.pop_front();
    --max_count;
  }

  return result;
}

} // namespace sbx::assets
