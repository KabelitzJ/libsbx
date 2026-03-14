// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_MESH_HPP_
#define LIBSBX_MODELS_MESH_HPP_

#include <filesystem>

#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/enum.hpp>
#include <libsbx/utility/crc32.hpp>

#include <libsbx/math/volume.hpp>
#include <libsbx/math/sphere.hpp>

#include <libsbx/io/loader_factory.hpp>

#include <libsbx/graphics/pipeline/mesh.hpp>

#include <libsbx/models/vertex3d.hpp>

namespace sbx::models {

class mesh : public graphics::mesh<vertex3d>, public io::loader_factory<mesh, graphics::mesh<vertex3d>::mesh_data> {

  using base = graphics::mesh<vertex3d>;

public:

  using mesh_data = graphics::mesh<vertex3d>::mesh_data;

  using base::mesh;

  mesh(const std::filesystem::path& path, std::uint32_t lod_count = 1u);

  ~mesh() override;

private:

  static constexpr auto file_magic = utility::make_magic<std::uint64_t>("SBXSTMSH");
  static constexpr auto file_version = std::uint16_t{2u};
  static constexpr auto binary_file_extention = std::string_view{".sbxstmsh"};

  enum class file_flags : std::uint16_t {
    none = 0,
    compressed = utility::bit_v<0>,  // reserved for future zstd block compression
    quantized = utility::bit_v<1>,  // reserved for future vertex quantization
  }; // enum class file_flags

  struct alignas(8) file_header {
    std::uint64_t magic;
    std::uint16_t version;
    std::uint16_t flags;
    std::uint32_t vertex_count;
    std::uint32_t index_count;
    std::uint32_t submesh_count;
    std::uint32_t vertex_stride;
    std::uint32_t index_stride;
    std::uint32_t uncompressed_size;
    std::uint32_t compressed_size;
  }; // struct file_header

  static_assert(sizeof(file_header) == 40u, "file_header layout changed");

  struct alignas(4) file_bounds {
    std::float_t aabb_min[3];
    std::float_t aabb_max[3];
    std::float_t sphere_center[3];
    std::float_t sphere_radius;
  }; // struct file_bounds

  static_assert(sizeof(file_bounds) == 40u, "file_bounds layout changed");

  struct alignas(8) file_submesh {
    char name[64];
    std::uint32_t material_index;
    std::uint32_t vertex_offset;
    std::uint32_t vertex_count;
    std::uint32_t index_offset;
    std::uint32_t index_count;
    std::float_t aabb_min[3];
    std::float_t aabb_max[3];
    std::float_t local_transform[16];
    std::uint32_t lod_level;
    std::uint32_t lod_group;
  }; // struct file_submesh
 
  static_assert(sizeof(file_submesh) == 184u, "file_submesh layout changed");

  struct alignas(2) file_vertex {
    std::int16_t position[3];
    std::int16_t normal[2];
    std::int16_t uv[2];
    std::int16_t tangent[2];
    std::int8_t tangent_w;
    std::uint8_t _pad;
  }; // struct file_vertex

  static_assert(sizeof(file_vertex) == 20u, "file_vertex layout changed");

  static auto _load(const std::filesystem::path& path, std::uint32_t lod_count) -> mesh_data;

  static auto _generate_lods(mesh_data& data, std::uint32_t lod_count) -> void;

  static auto _load_binary(const std::filesystem::path& path) -> mesh_data;

  static auto _process(const std::filesystem::path& path, const mesh_data& data) -> void;

}; // class mesh

} // namespace sbx::models

#endif // LIBSBX_MODELS_MESH_HPP_
