// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSET_RESIDENCY_HPP_
#define LIBSBX_ASSETS_ASSET_RESIDENCY_HPP_

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/skeleton.hpp>
#include <libsbx/assets/animation_clip.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/environment_map.hpp>
#include <libsbx/assets/particle_effect.hpp>
#include <libsbx/assets/asset_cooker.hpp>
#include <libsbx/assets/ibl_baker.hpp>

namespace sbx::assets {

/**
 * @brief Turns cooked asset data into GPU-resident textures/meshes/materials/environment-maps:
 * upload queues, bindless registration, the material UBO, and the default fallback textures.
 * Depends on @ref asset_cooker for cooked data and @ref ibl_baker for environment baking — both
 * held by reference, owned by whoever constructs this (see @ref assets_module).
 */
class asset_residency final : public utility::noncopyable {

public:

  asset_residency(asset_cooker& cooker, ibl_baker& baker);

  /** @brief Loads a texture from a UUID or project-relative path; returns the existing handle if already loaded. */
  auto load_texture(const math::uuid& id, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture_handle;

  auto load_texture(const std::filesystem::path& path, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture_handle;

  /** @brief Loads a mesh from a UUID or project-relative path; returns the existing handle if already loaded. */
  auto load_mesh(const math::uuid& id, const mesh_import_options& options = {}) -> mesh_handle;

  auto load_mesh(const std::filesystem::path& path, const mesh_import_options& options = {}) -> mesh_handle;

  /** @brief Loads a skeleton cooked as a side effect of a mesh import; returns the existing handle if already loaded. Pure CPU data -- resolves immediately, no upload queue involved. */
  auto load_skeleton(const math::uuid& id) -> skeleton_handle;

  /** @brief Loads an animation clip cooked as a side effect of a mesh import; returns the existing handle if already loaded. Pure CPU data -- resolves immediately, no upload queue involved. */
  auto load_animation_clip(const math::uuid& id) -> animation_clip_handle;

  auto load_material(const math::uuid& id) -> material_handle;

  auto load_material(const std::filesystem::path& path) -> material_handle;

  auto create_material(const material::create_info& create_info) -> material_handle;

  /**
   * @brief Overwrites an existing material's fields in place. Every material_handle already
   * pointing at this record observes the change immediately, since they all share the same
   * underlying object. Does not touch identity (index/uuid) or persist to disk.
   */
  auto update_material(material_handle& material, const material::create_info& create_info) -> void;

  /**
   * @brief Writes a material to a `.material` file and (re-)registers it as a first-class asset.
   * @param path Destination path relative to the active project's assets directory.
   * @return The material's canonical uuid (also written back onto the material record itself).
   */
  auto save_material(material_handle& material, const std::filesystem::path& path) -> math::uuid;

  auto load_environment_map(const math::uuid& id) -> environment_map_handle;

  auto load_environment_map(const std::filesystem::path& path) -> environment_map_handle;

  auto load_particle_effect(const math::uuid& id) -> particle_effect_handle;

  auto load_particle_effect(const std::filesystem::path& path) -> particle_effect_handle;

  auto create_particle_effect(const particle_effect::create_info& create_info) -> particle_effect_handle;

  /**
   * @brief Overwrites an existing particle_effect's emitters in place. Every particle_effect_handle
   * already pointing at this record observes the change immediately. Does not touch identity
   * (uuid) or persist to disk.
   */
  auto update_particle_effect(particle_effect_handle& effect, const particle_effect::create_info& create_info) -> void;

  /**
   * @brief Writes a particle_effect to a `.particle_effect` file and (re-)registers it as a first-class asset.
   * @param path Destination path relative to the active project's assets directory.
   * @return The effect's canonical uuid (also written back onto the record itself).
   */
  auto save_particle_effect(particle_effect_handle& effect, const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Turns queued texture loads into GPU images and bindless writes.
   *
   * Runs on the render thread; copies are recorded by the caller's subsequent @ref upload_context::flush.
   */
  auto process_uploads(std::uint64_t frame_index) -> void;

  [[nodiscard]] auto is_resident(const texture_handle& texture) const -> bool;

  [[nodiscard]] auto is_resident(const mesh_handle& mesh) const -> bool;

  [[nodiscard]] auto is_resident(const material_handle& material) const -> bool;

  [[nodiscard]] auto is_resident(const environment_map_handle& environment) const -> bool;

  /**
   * @brief The image view backing a resident texture's bindless slot, or VK_NULL_HANDLE if the
   * texture isn't valid or its upload hasn't been processed yet (see is_resident). Used by the
   * editor/UI layer to blit a real preview into ImGui (see ui_module::texture_id), the same way
   * the viewport blits the scene's final image.
   */
  [[nodiscard]] auto image_view_of(const texture_handle& texture) const -> VkImageView;

  [[nodiscard]] auto white_texture() const noexcept -> texture_handle {
    return _white;
  }

  [[nodiscard]] auto normal_texture() const noexcept -> texture_handle {
    return _normal;
  }

  [[nodiscard]] auto black_texture() const noexcept -> texture_handle {
    return _black;
  }

  [[nodiscard]] auto magenta_texture() const noexcept -> texture_handle {
    return _magenta;
  }

  [[nodiscard]] auto material_buffer_address() const noexcept -> graphics::buffer::address_type {
    return _material_address;
  }

private:

  inline static constexpr auto material_capacity = std::uint32_t{1024u};

  struct pending_texture_upload {
    std::uint32_t index;
    std::vector<std::byte> pixels;
    std::uint32_t width;
    std::uint32_t height;
    graphics::format format;
  }; // struct pending_texture_upload

  struct pending_mesh_upload {
    std::shared_ptr<mesh> record;
    std::vector<vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<skin_vertex> skin_vertices{}; // empty when the mesh has no skin data
  }; // struct pending_mesh_upload

  struct pending_material_upload {
    std::shared_ptr<material> record;
  }; // struct pending_material_upload

  auto _create_default_texture(std::array<std::uint8_t, 4u> color) -> texture_handle;

  auto _register_material(std::shared_ptr<material> record) -> material_handle;

  /**
   * @brief Turns one gltf-embedded material description into a real, standalone `.material` asset
   * next to the mesh (models/<name>/materials/<material name>.material), reusing one already there
   * instead of overwriting it. Passed to @ref asset_cooker::resolve_mesh as its material_resolver
   * when `mesh_import_options::extract_materials` is set.
   */
  auto _extract_gltf_material(const material_description& description, const std::filesystem::path& relative_source) -> math::uuid;

  asset_cooker& _cooker;
  ibl_baker& _ibl;

  mutable std::mutex _mutex{};

  std::unordered_map<std::string, std::shared_ptr<texture>> _textures{};
  std::vector<pending_texture_upload> _pending_textures{};
  std::unordered_map<std::uint32_t, graphics::image_handle> _images{};
  std::unordered_map<std::uint32_t, std::uint64_t> _resident_frame{};

  std::unordered_map<math::uuid, std::shared_ptr<mesh>> _meshes{};
  std::vector<pending_mesh_upload> _pending_meshes{};

  // Pure CPU data -- no GPU buffer/index, unlike _meshes above.
  std::unordered_map<math::uuid, std::shared_ptr<skeleton>> _skeletons{};
  std::unordered_map<math::uuid, std::shared_ptr<animation_clip>> _animation_clips{};

  std::vector<std::shared_ptr<material>> _materials{};
  std::vector<pending_material_upload> _pending_materials{};
  graphics::buffer_handle _material_buffer{};
  graphics::buffer::address_type _material_address{0u};
  std::uint32_t _material_count{0u};
  std::unordered_map<math::uuid, std::shared_ptr<material>> _material_files{};

  std::unordered_map<math::uuid, std::shared_ptr<environment_map>> _environment_maps{};

  // Pure CPU data — no GPU buffer/index, unlike _materials above.
  std::unordered_map<math::uuid, std::shared_ptr<particle_effect>> _particle_effect_files{};

  texture_handle _white{};
  texture_handle _normal{};
  texture_handle _black{};
  texture_handle _magenta{};

}; // class asset_residency

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_RESIDENCY_HPP_
