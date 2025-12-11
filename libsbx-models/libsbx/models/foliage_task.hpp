#ifndef LIBSBX_MODELS_FOLIAGE_TASK_HPP_
#define LIBSBX_MODELS_FOLIAGE_TASK_HPP_

#include <numbers>

#include <libsbx/graphics/task.hpp>
#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/components/transform.hpp>
#include <libsbx/math/random.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/graphics/buffers/uniform_handler.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>
#include <libsbx/graphics/buffers/storage_handler.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/global_transform.hpp>

namespace sbx::models {

struct grass_blade {
  math::vector4 position_bend;
  math::vector4 size_animation_pitch;
}; // struct grass_blade

class foliage_task final : public graphics::task {

  using base = graphics::task;

  inline static constexpr auto count = 1u << 16u;

public:

  foliage_task(const std::filesystem::path& path)
  : _pipeline{path},
    _push_handler{_pipeline},
    _blades{_generate_blades(math::vector3::zero, 20.0f, count)} {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    _grass_input_buffer = graphics_module.add_resource<graphics::storage_buffer>(_blades.size() * sizeof(grass_blade), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, _blades.data());
    _grass_output_buffer = graphics_module.add_resource<graphics::storage_buffer>(_blades.size() * sizeof(grass_blade), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    _draw_command_buffer = graphics_module.add_resource<graphics::storage_buffer>(sizeof(VkDrawIndirectCommand), (VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT));
  }

  ~foliage_task() override = default;

  auto grass_output_buffer() -> graphics::storage_buffer_handle {
    return _grass_output_buffer;
  }

  auto draw_command_buffer() -> graphics::storage_buffer_handle {
    return _draw_command_buffer;
  }

  auto execute(graphics::command_buffer& command_buffer) -> void override {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

    auto& logical_device = graphics_module.logical_device();

    auto& grass_input_buffer = graphics_module.get_resource<graphics::storage_buffer>(_grass_input_buffer);
    auto& grass_output_buffer = graphics_module.get_resource<graphics::storage_buffer>(_grass_output_buffer);
    auto& draw_command_buffer = graphics_module.get_resource<graphics::storage_buffer>(_draw_command_buffer);

    auto& scene = scenes_module.scene();

    auto camera_node = scene.camera();

    auto& transform = scene.get_component<scenes::transform>(camera_node);
    auto& global_transform = scene.get_component<scenes::global_transform>(camera_node);

    const auto view = math::matrix4x4::inverted(scene.world_transform(camera_node));

    auto& camera = scene.get_component<scenes::camera>(camera_node);

    const auto& projection = camera.projection();

    _pipeline.bind(command_buffer);

    _push_handler.push("in_blades", grass_input_buffer.address());
    _push_handler.push("out_blades", grass_output_buffer.address());
    _push_handler.push("draw_command", draw_command_buffer.address());
    _push_handler.push("view_projection", projection * view);
    _push_handler.push("blade_count", count);

    _push_handler.bind(command_buffer);

    auto draw_command = VkDrawIndirectCommand{};
    draw_command.vertexCount = 3u;
    draw_command.instanceCount = 0u;
    draw_command.firstVertex = 0u;
    draw_command.firstInstance = 0u;

    draw_command_buffer.update(&draw_command, sizeof(VkDrawIndirectCommand), 0);

    const auto blade_count = static_cast<std::uint32_t>(_blades.size());
    const auto local_size_x = 64u;
    const auto workgroup_count = (blade_count + local_size_x - 1u) / local_size_x;

    _pipeline.dispatch(command_buffer, {workgroup_count, 1u, 1u});
  }

private:

  auto _generate_blades(const math::vector3& center, std::float_t radius, std::size_t count, std::float_t min_height = 0.2f, std::float_t max_height = 0.8f) -> std::vector<grass_blade> {
    auto blades = std::vector<grass_blade>{};
    blades.reserve(count);

    for (auto i = 0; i < count; ++i) {
      auto position = center + math::vector3{math::random::next<std::float_t>(-radius, radius), 0.0f, math::random::next<std::float_t>(-radius, radius)};

      auto blade = grass_blade{
        .position_bend = math::vector4{position, math::random::next<std::float_t>(0.01f, 0.3f)},
        .size_animation_pitch = math::vector4{
          math::random::next<std::float_t>(0.03f, 0.07f),                                 // width
          math::random::next<std::float_t>(min_height, max_height),                       // height
          math::random::next<std::float_t>(-0.4f, 0.4f),                                  // animation
          math::random::next<std::float_t>(0.0f, 2.0f * std::numbers::pi_v<std::float_t>) // pitch
        }
      };

      blades.push_back(blade);
    }

    return blades;
  }

  graphics::compute_pipeline _pipeline;

  // graphics::buffer_handle _bounding_box_buffer;
  // graphics::buffer_handle _draw_data_buffer;
  graphics::storage_buffer_handle _grass_input_buffer;
  graphics::storage_buffer_handle _grass_output_buffer;
  graphics::storage_buffer_handle _draw_command_buffer;  
  // graphics::buffer_handle _frustum_buffer;

  graphics::push_handler _push_handler;

  std::vector<grass_blade> _blades;

}; // class frustum_culling_task

} // namespace sbx::models

#endif // LIBSBX_MODELS_FOLIAGE_TASK_HPP_
