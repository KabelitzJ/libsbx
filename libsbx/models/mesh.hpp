// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_MESH_HPP_
#define LIBSBX_MODELS_MESH_HPP_

#include <array>
#include <filesystem>
#include <span>

#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/enum.hpp>
#include <libsbx/utility/crc32.hpp>

#include <libsbx/math/volume.hpp>
#include <libsbx/math/sphere.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/io/loader_factory.hpp>

#include <libsbx/graphics/pipeline/mesh.hpp>
#include <libsbx/graphics/buffers/buffer.hpp>

#include <libsbx/models/vertex3d.hpp>
#include <libsbx/models/vertex_stream.hpp>

namespace sbx::models {

class mesh : public graphics::mesh<vertex3d>, public io::loader_factory<mesh, graphics::mesh<vertex3d>::mesh_data> {

  using base = graphics::mesh<vertex3d>;

public:

  using mesh_data = graphics::mesh<vertex3d>::mesh_data;

  mesh(const std::vector<vertex3d>& vertices, const std::vector<std::uint32_t>& indices, const math::volume& bounds = math::volume{});

  mesh(std::vector<vertex3d>&& vertices, std::vector<std::uint32_t>&& indices, const math::volume& bounds = math::volume{});

  mesh(mesh_data&& data);

  mesh(const std::filesystem::path& path, std::uint32_t lod_count = 1u);

  ~mesh() override;

  auto set_stream(vertex_stream stream, std::span<const math::vector4> data) -> void;

  auto stream_address(vertex_stream stream) const -> graphics::buffer::address_type;

  auto available_streams() const noexcept -> const utility::bit_field<vertex_stream>&;

  auto has_streams(const utility::bit_field<vertex_stream>& required) const noexcept -> bool;

private:

  auto _upload_streams(std::array<std::vector<math::vector4>, vertex_stream_count>& streams) -> void;

  static constexpr auto file_magic = utility::make_magic<std::uint64_t>("SBXSTMSH");
  static constexpr auto file_version = std::uint16_t{4u};
  static constexpr auto binary_file_extention = std::string_view{".sbxstmsh"};

  enum class file_flags : std::uint16_t {
    none = 0,
    compressed = utility::bit_v<0>,  // reserved for future zstd block compression
    quantized = utility::bit_v<1>,  // reserved for future vertex quantization
    has_streams = utility::bit_v<2>, // per-vertex stream payload follows the vertex/index block
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
    std::int16_t uv0[2];
    std::int16_t uv1[2];
    std::int16_t tangent[2];
    std::int8_t tangent_w;
    std::uint8_t _pad;
  }; // struct file_vertex

  static_assert(sizeof(file_vertex) == 24u, "file_vertex layout changed");

  static auto _load(const std::filesystem::path& path, std::uint32_t lod_count) -> mesh_data;

  static auto _generate_lods(mesh_data& data, std::uint32_t lod_count) -> void;

  static auto _load_binary(const std::filesystem::path& path) -> mesh_data;

  static auto _process(const std::filesystem::path& path, const mesh_data& data) -> void;

  std::array<graphics::buffer_handle, vertex_stream_count> _stream_buffers{};
  utility::bit_field<vertex_stream> _available_streams{};

}; // class mesh

struct mesh_handle : public math::uuid {

  constexpr mesh_handle()
  : math::uuid{math::uuid::nil()} { }

  constexpr explicit mesh_handle(const math::uuid& id)
  : math::uuid{id} { }

  constexpr auto is_valid() const noexcept -> bool {
    return (*this) != math::uuid::nil();
  }

  constexpr auto operator==(const mesh_handle& other) const noexcept -> bool = default;

}; // struct mesh_handle

} // namespace sbx::models

#endif // LIBSBX_MODELS_MESH_HPP_
