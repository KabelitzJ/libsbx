// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSET_LOADER_HPP_
#define LIBSBX_ASSETS_ASSET_LOADER_HPP_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>
#include <variant>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/graphics/types.hpp>

#include <libsbx/assets/asset_cooker.hpp>

namespace sbx::assets {

class asset_loader final : public utility::noncopyable {

public:

  struct texture_request { 
    math::uuid id; 
    graphics::format format; 
    std::filesystem::path source; 
    std::filesystem::path cooked; 
    bool needs_cook{false}; 
  }; // struct texture_request

  struct mesh_request { 
    math::uuid id; 
    mesh_import_options options; 
    std::filesystem::path source; 
    std::filesystem::path cooked; 
    bool needs_cook{false}; 
  }; // struct mesh_request

  struct font_request { 
    math::uuid id; 
    std::filesystem::path source; 
    std::filesystem::path cooked; 
    bool needs_cook{false}; 
  }; // struct font_request

  struct material_request { 
    math::uuid id; 
    std::filesystem::path source; 
  }; // struct material_request

  struct particle_effect_request { 
    math::uuid id; 
    std::filesystem::path source; 
  }; // struct particle_effect_request

  struct animation_graph_request { 
    math::uuid id; 
    std::filesystem::path source; 
  }; // struct animation_graph_request

  struct skeleton_request { 
    math::uuid id; 
  }; // struct skeleton_request

  struct animation_clip_request { 
    math::uuid id; 
  }; // struct animation_clip_request

  struct texture_result {
    texture_request request;
    std::optional<pixel_data> data;
    bool did_cook{false};
  }; // struct texture_result

  struct mesh_result {
    mesh_request request;
    std::optional<cooked_mesh_data> data;
    bool did_cook{false};
  }; // struct mesh_result

  struct font_result {
    font_request request;
    std::optional<cooked_font_data> data;
    bool did_cook{false};
  }; // struct font_result

  struct material_result {
    material_request request;
    std::optional<material_description> data;
  }; // struct material_result

  struct particle_effect_result {
    particle_effect_request request;
    std::optional<particle_effect_description> data;
  }; // struct particle_effect_result

  struct animation_graph_result {
    animation_graph_request request;
    std::optional<animation_graph::create_info> data;
  }; // struct animation_graph_result

  struct skeleton_result {
    skeleton_request request;
    std::optional<std::vector<skeleton::joint>> data;
  }; // struct skeleton_result

  struct animation_clip_result {
    animation_clip_request request;
    std::optional<animation_clip_data> data;
  }; // struct animation_clip_result

  asset_loader();

  ~asset_loader();

  auto submit(texture_request request) -> void;

  auto submit(mesh_request request) -> void;

  auto submit(font_request request) -> void;

  auto submit(material_request request) -> void;

  auto submit(particle_effect_request request) -> void;

  auto submit(animation_graph_request request) -> void;

  auto submit(skeleton_request request) -> void;

  auto submit(animation_clip_request request) -> void;

  auto abort() -> void;

  [[nodiscard]] auto take_resolved_textures(std::size_t max_count) -> std::vector<texture_result>;

  [[nodiscard]] auto take_resolved_meshes(std::size_t max_count) -> std::vector<mesh_result>;

  [[nodiscard]] auto take_resolved_fonts(std::size_t max_count) -> std::vector<font_result>;

  [[nodiscard]] auto take_resolved_materials(std::size_t max_count) -> std::vector<material_result>;

  [[nodiscard]] auto take_resolved_particle_effects(std::size_t max_count) -> std::vector<particle_effect_result>;

  [[nodiscard]] auto take_resolved_animation_graphs(std::size_t max_count) -> std::vector<animation_graph_result>;

  [[nodiscard]] auto take_resolved_skeletons(std::size_t max_count) -> std::vector<skeleton_result>;

  [[nodiscard]] auto take_resolved_animation_clips(std::size_t max_count) -> std::vector<animation_clip_result>;

private:

  using request = std::variant<texture_request, mesh_request, font_request, material_request, particle_effect_request, animation_graph_request, skeleton_request, animation_clip_request>;

  auto _worker_loop() -> void;

  auto _resolve(const texture_request& request) -> void;

  auto _resolve(const mesh_request& request) -> void;

  auto _resolve(const font_request& request) -> void;

  auto _resolve(const material_request& request) -> void;

  auto _resolve(const particle_effect_request& request) -> void;

  auto _resolve(const animation_graph_request& request) -> void;

  auto _resolve(const skeleton_request& request) -> void;

  auto _resolve(const animation_clip_request& request) -> void;

  std::atomic<bool> _aborted{false};

  std::mutex _request_mutex{};
  std::condition_variable _request_condition{};
  std::deque<request> _requests{};

  std::mutex _result_mutex{};
  std::deque<texture_result> _resolved_textures{};
  std::deque<mesh_result> _resolved_meshes{};
  std::deque<font_result> _resolved_fonts{};
  std::deque<material_result> _resolved_materials{};
  std::deque<particle_effect_result> _resolved_particle_effects{};
  std::deque<animation_graph_result> _resolved_animation_graphs{};
  std::deque<skeleton_result> _resolved_skeletons{};
  std::deque<animation_clip_result> _resolved_animation_clips{};

  std::thread _thread{};

}; // class asset_loader

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_LOADER_HPP_
