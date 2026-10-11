// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_THUMBNAIL_CACHE_HPP_
#define EDITOR_THUMBNAIL_CACHE_HPP_

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include <imgui.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/pipeline/async_shader_compiler.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/assets/asset_type.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/material.hpp>

namespace editor {

/**
 * @brief Rendered previews for mesh, material and prefab assets, like the Content Browser thumbnails in Unity or Hazel.
 *
 * get() is cheap and never blocks: it returns the last rendered image and queues a render when the asset's content changed. update() renders at most one per frame.
 * Textures aren't handled here; their tiles sample the texture itself.
 */
class thumbnail_cache final : public sbx::utility::noncopyable {

public:

  thumbnail_cache();

  ~thumbnail_cache();

  /**
   * @brief The asset's thumbnail, queueing a (re)render if it's missing or stale.
   *
   * @param id The asset's uuid.
   * @param type Mesh, material or prefab; other types return nullopt.
   *
   * @return The ImGui texture, or nullopt until the first render finished.
   */
  [[nodiscard]] auto get(const sbx::math::uuid& id, sbx::assets::asset_type type) -> std::optional<ImTextureID>;

  /** @brief Renders the next queued thumbnail whose mesh and materials are resident; call once per frame. */
  auto update() -> void;

  /** @brief Whether the type has rendered thumbnails. */
  [[nodiscard]] static auto supports(sbx::assets::asset_type type) -> bool;

private:

  struct entry {
    sbx::assets::mesh_handle mesh{};
    sbx::assets::material_handle material_override{}; // material thumbnails draw the sphere with this
    std::uint64_t rendered_key{0u};
    sbx::graphics::image_handle image{};
    bool is_queued{false};
  }; // struct entry

  [[nodiscard]] auto _content_key(const entry& entry) const -> std::uint64_t;

  [[nodiscard]] auto _is_ready(const entry& entry) const -> bool;

  auto _ensure_pipeline() -> bool;

  auto _render(entry& entry) -> void;

  inline static constexpr auto _extent = std::uint32_t{128u};

  std::unordered_map<sbx::math::uuid, entry> _entries{};
  std::deque<sbx::math::uuid> _queue{};

  sbx::graphics::async_shader_compiler _compiler{};
  std::unique_ptr<sbx::graphics::shader> _shader{};
  std::unique_ptr<sbx::graphics::graphics_pipeline> _pipeline{};
  bool _compile_requested{false};

  sbx::graphics::image_handle _depth{};
  std::uint32_t _sampler_index{0u};

}; // class thumbnail_cache

} // namespace editor

#endif // EDITOR_THUMBNAIL_CACHE_HPP_
