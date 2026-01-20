#ifndef DEMO_TERRAIN_SUBRENDERER_HPP_
#define DEMO_TERRAIN_SUBRENDERER_HPP_

#include <libsbx/math/vector3.hpp>

#include <libsbx/graphics/graphics.hpp>

#include <libsbx/scenes/scenes.hpp>

#include <demo/dual_grid.hpp>
#include <demo/data.hpp>

namespace demo {

class terrain_subrenderer : public sbx::graphics::subrenderer {

  class pipeline : public sbx::graphics::graphics_pipeline {

    inline static const auto pipeline_definition = sbx::graphics::pipeline_definition{
      .depth = sbx::graphics::depth::read_write,
      .uses_transparency = true,
      .rasterization_state = sbx::graphics::rasterization_state{
        .polygon_mode = sbx::graphics::polygon_mode::fill,
        .cull_mode = sbx::graphics::cull_mode::none,
        .front_face = sbx::graphics::front_face::counter_clockwise
      }
    };

    using base = sbx::graphics::graphics_pipeline;
  public:

    pipeline(const std::filesystem::path& path, const std::vector<sbx::graphics::attachment_description>& attachments)
    : base{path, attachments, pipeline_definition} { }

    ~pipeline() override = default;

  }; // class pipeline

public:

  struct grid_vertex_data {
    sbx::math::vector3 position;
    std::uint32_t flags;
  }; // struct grid_vertex_data

  struct grid_quad_data {
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;
    std::uint32_t d;
  }; // struct grid_quad_data

  struct transform_data {
    sbx::math::matrix4x4 model;
    sbx::math::matrix4x4 normal;
  }; // struct transform_data

  struct instance_data {
    std::uint32_t transform_index;
    std::uint32_t grid_quad_index;
    std::float_t height;
    std::float_t red;
  
    std::float_t green;
    std::float_t blue;
    std::uint32_t rotation_steps;
    std::uint32_t padding0;
  }; // struct instance_data

  terrain_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path);

  ~terrain_subrenderer() override = default;

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override;

  auto update_dual_grid_data(const dual_grid<grid_data>& grid) -> void;

private:

  template<typename Type>
  auto _update_buffer(sbx::graphics::storage_buffer& buffer, const std::vector<Type>& data) -> void {
    const auto required_size = static_cast<VkDeviceSize>(data.size() * sizeof(Type));

    if (required_size > buffer.size()) {
      buffer.resize(static_cast<VkDeviceSize>(required_size * 1.5f));
    }

    buffer.update(data.data(), data.size() * sizeof(Type));
  }

  pipeline _pipeline;

  sbx::graphics::push_handler _push_handler;
  sbx::graphics::descriptor_handler _descriptor_handler;

  sbx::graphics::storage_buffer_handle _grid_vertex_buffer;
  sbx::graphics::storage_buffer_handle _grid_quad_buffer;
  sbx::graphics::storage_buffer_handle _transform_buffer;
  sbx::graphics::storage_buffer_handle _instance_buffer;

}; // class terrain_subrenderer

} // namespace demo

#endif // DEMO_TERRAIN_SUBRENDERER_HPP_
