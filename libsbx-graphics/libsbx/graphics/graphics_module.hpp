#ifndef LIBSBX_GRAPHICS_GRAPHICS_MODULE_HPP_
#define LIBSBX_GRAPHICS_GRAPHICS_MODULE_HPP_

#include <memory>
#include <unordered_map>
#include <vector>
#include <typeindex>

#include <libsbx/core/module.hpp>
#include <libsbx/core/delegate.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/devices/devices_module.hpp>

#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/concepts.hpp>

#include <libsbx/signals/signal.hpp>

#include <libsbx/graphics/devices/instance.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>
#include <libsbx/graphics/devices/allocator.hpp>
#include <libsbx/graphics/devices/surface.hpp>
#include <libsbx/graphics/devices/query_pool.hpp>

#include <libsbx/graphics/commands/command_pool.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/render_pass/swapchain.hpp>

#include <libsbx/graphics/pipeline/pipeline.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>
#include <libsbx/graphics/pipeline/compiler.hpp>

#include <libsbx/graphics/buffers/buffer.hpp>
#include <libsbx/graphics/buffers/storage_buffer.hpp>

#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/images/cube_image.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>
#include <libsbx/graphics/images/depth_image.hpp>

#include <libsbx/graphics/renderer.hpp>

#include <libsbx/graphics/resource_storage.hpp>

namespace sbx::graphics {

/**
 * @brief Checks the @ref VkResult and throws an exception if it is an error
 * @param result 
 * @throws @see std::runtime_error 
 */
auto validate(VkResult result) -> void;

template<typename VkEnum, typename Enum>
requires ((std::is_enum_v<VkEnum> || std::is_same_v<VkEnum, VkFlags>) && std::is_enum_v<Enum>)
constexpr auto to_vk_enum(Enum value) -> VkEnum {
  return static_cast<VkEnum>(value);
}

/**
 * @brief Module for managing rendering specific tasks
 * 
 * @extends @ref sbx::core::module<T>
 */
class graphics_module final : public core::module<graphics_module> {

  inline static const auto is_registered = register_module(stage::rendering, dependencies<devices::devices_module>{});

  inline static constexpr auto max_deletion_queue_size = std::size_t{16u};

public:

  graphics_module();

  ~graphics_module() override;

  auto update() -> void override;

  auto instance() -> instance&;

  auto physical_device() -> physical_device&;

  auto logical_device() -> logical_device&;

  auto surface() -> surface&;

  auto command_pool(VkQueueFlagBits queue_type = VK_QUEUE_GRAPHICS_BIT, const std::thread::id& thread_id = std::this_thread::get_id()) -> const std::shared_ptr<command_pool>&;

  auto swapchain() -> swapchain&;

  template<utility::implements<renderer> Renderer, typename... Args>
  requires (std::is_constructible_v<Renderer, Args...>)
  auto set_renderer(Args&&... args) -> void {
    _renderer = std::make_unique<Renderer>(std::forward<Args>(args)...);
    _recreate_swapchain();
  }

  // auto render_stage(const pipeline::stage& stage) -> graphics::render_stage&;
  
  auto current_frame() const noexcept -> std::uint32_t {
    return _current_frame;
  }

  auto attachment(const std::string& name) const -> const descriptor&;

  template<typename Type, typename... Args>
  requires (std::is_constructible_v<Type, Args...>)
  auto add_resource(Args&&... args) -> resource_handle<Type> {
    return _storage<Type>().emplace(std::forward<Args>(args)...);
  }

  template<typename Type>
  auto get_resource(const resource_handle<Type>& handle) -> Type& {
    return _storage<Type>().get(handle);
  }

  template<typename Type>
  auto get_resource(const resource_handle<Type>& handle) const -> const Type& {
    return _storage<Type>().get(handle);
  }

  template<typename Type>
  auto remove_resource(const resource_handle<Type>& handle) -> void {
    return _storage<Type>().remove(handle);
  }

  auto allocator() const noexcept -> const graphics::allocator& {
    return _allocator;
  }

  template<queue::type Source, queue::type Destination, typename Type>
  requires (std::is_same_v<Type, graphics::buffer> || std::is_same_v<Type, graphics::storage_buffer>)
  auto transfer_ownership(const resource_handle<Type>& handle, const VkPipelineStageFlagBits2 stage = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT) -> void {
    auto& buffer = get_resource<Type>(handle);

    _release_ownership_data.push_back(command_buffer::release_ownership_data{
      .src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = _logical_device->queue<Source>().family(),
      .dst_queue_family = _logical_device->queue<Destination>().family(),
      .buffer = buffer
    });

    _acquire_ownership_data.push_back(command_buffer::acquire_ownership_data{
      .dst_stage_mask = stage,
      .dst_access_mask = _access_mask_from_stage(stage),
      .src_queue_family = _logical_device->queue<Source>().family(),
      .dst_queue_family = _logical_device->queue<Destination>().family(),
      .buffer = buffer
    });
  }

  template<typename Callable>
  requires (std::is_invocable_r_v<math::vector2u, Callable>)
  auto set_dynamic_size_callback(Callable&& callback) -> void {
    _dynamic_size_callback = std::forward<Callable>(callback);
  }

  auto dynamic_viewport() const -> const math::vector2u& {
    return _viewport;
  }

  auto on_viewport_changed() -> signals::signal<const math::vector2u&>& {
    return _on_viewport_changed;
  }

  auto compiler() -> graphics::compiler& {
    return _compiler;
  }

private:

  static constexpr auto _access_mask_from_stage(VkPipelineStageFlagBits2 stage) -> VkAccessFlagBits2 {
    switch (stage) {
      case VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT: {
        return VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
      }
      case VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT:
      case VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT:
      case VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT:
      case VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT:
      case VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT: {
        return VK_ACCESS_2_SHADER_READ_BIT;
      }
      case VK_PIPELINE_STAGE_2_TRANSFER_BIT: {
        return VK_ACCESS_2_TRANSFER_READ_BIT;
      }
      default: {
        return VK_ACCESS_2_MEMORY_READ_BIT;
      }
    }
  }

  // auto _start_render_pass(const utility::hashed_string& pass, graphics::command_buffer& command_buffer) -> void;

  // auto _end_render_pass(const utility::hashed_string& pass, graphics::command_buffer& command_buffer) -> void;

  auto _reset_render_stages() -> void;

  auto _recreate_viewport() -> void;

  auto _recreate_swapchain() -> void;

  auto _recreate_per_frame_data() -> void;

  auto _recreate_per_image_data() -> void;

  auto _recreate_command_buffers() -> void;

  auto _recreate_attachments() -> void;

  struct per_frame_data {
    // graphics
    VkSemaphore image_available_semaphore{};
    VkFence graphics_in_flight_fence{};
    // compute
    VkSemaphore compute_finished_semaphore{};
    VkFence compute_in_flight_fence{};
  }; // struct per_frame_data
  
  struct per_image_data {
    VkSemaphore render_finished_semaphore{};
  }; // struct per_image_data

  struct command_pool_key {
    VkQueueFlagBits queue_type;
    std::thread::id thread_id;
  }; // struct command_pool_key

  struct command_pool_key_hash {
    auto operator()(const command_pool_key& key) const noexcept -> std::size_t {
      auto hast = std::size_t{0};
      utility::hash_combine(hast, key.queue_type, key.thread_id);
      return hast;
    }
  }; // struct command_pool_key_hash

  struct command_pool_key_equality {
    auto operator()(const command_pool_key& lhs, const command_pool_key& rhs) const noexcept -> bool {
      return lhs.queue_type == rhs.queue_type && lhs.thread_id == rhs.thread_id;
    }
  }; // struct command_pool_key_equal


  static_assert(std::is_class_v<graphics::graphics_pipeline>, "graphics_pipeline is not a class in sbx::graphics");

  template<typename Type>
  auto _storage() -> resource_storage<Type>& {
    if constexpr (std::is_same_v<Type, shader>) {
      return _shaders;
    } else if constexpr (std::is_same_v<Type, graphics_pipeline>) {
      return _graphics_pipelines;
    } else if constexpr (std::is_same_v<Type, compute_pipeline>) {
      return _compute_pipelines;
    } else if constexpr (std::is_same_v<Type, buffer>) {
      return _buffers;
    } else if constexpr (std::is_same_v<Type, storage_buffer>) {
      return _storage_buffers;
    } else if constexpr (std::is_same_v<Type, uniform_buffer>) {
      return _uniform_buffers;
    } if constexpr (std::is_same_v<Type, image2d>) {
      return _images;
    } else if constexpr (std::is_same_v<Type, depth_image>) {
      return _depth_images;
    } else if constexpr (std::is_same_v<Type, cube_image>) {
      return _cube_images;
    } else if constexpr (std::is_same_v<Type, sampler_state>) {
      return _sampler_states;
    }

    utility::assert_that(false, "Invalid resource type");
  }

  template<typename Type>
  auto _storage() const -> const resource_storage<Type>& {
    if constexpr (std::is_same_v<Type, shader>) {
      return _shaders;
    } else if constexpr (std::is_same_v<Type, graphics_pipeline>) {
      return _graphics_pipelines;
    } else if constexpr (std::is_same_v<Type, compute_pipeline>) {
      return _compute_pipelines;
    } else if constexpr (std::is_same_v<Type, buffer>) {
      return _buffers;
    } else if constexpr (std::is_same_v<Type, storage_buffer>) {
      return _storage_buffers;
    } else if constexpr (std::is_same_v<Type, uniform_buffer>) {
      return _uniform_buffers;
    } if constexpr (std::is_same_v<Type, image2d>) {
      return _images;
    } else if constexpr (std::is_same_v<Type, depth_image>) {
      return _depth_images;
    } else if constexpr (std::is_same_v<Type, cube_image>) {
      return _cube_images;
    } else if constexpr (std::is_same_v<Type, sampler_state>) {
      return _sampler_states;
    }

    utility::assert_that(false, "Invalid resource type");
  }

  std::unique_ptr<graphics::instance> _instance{};
  std::unique_ptr<graphics::physical_device> _physical_device{};
  std::unique_ptr<graphics::logical_device> _logical_device{};

  std::unordered_map<command_pool_key, std::shared_ptr<graphics::command_pool>, command_pool_key_hash, command_pool_key_equality> _command_pools{};

  std::map<std::string, memory::observer_ptr<const descriptor>> _attachments{};

  std::unique_ptr<graphics::surface> _surface{};

  std::unique_ptr<graphics::swapchain> _swapchain{};

  std::vector<per_frame_data> _per_frame_data{};
  std::vector<per_image_data> _per_image_data{};
  std::vector<graphics::command_buffer> _graphics_command_buffers{};
  std::vector<graphics::command_buffer> _compute_command_buffers{};

  std::unique_ptr<graphics::renderer> _renderer{};

  resource_storage<graphics::shader> _shaders;
  resource_storage<graphics::graphics_pipeline> _graphics_pipelines;
  resource_storage<graphics::compute_pipeline> _compute_pipelines;
  resource_storage<graphics::buffer> _buffers;
  resource_storage<graphics::storage_buffer> _storage_buffers;
  resource_storage<graphics::uniform_buffer> _uniform_buffers;
  resource_storage<graphics::image2d> _images;
  resource_storage<graphics::depth_image> _depth_images;
  resource_storage<graphics::cube_image> _cube_images;
  resource_storage<graphics::sampler_state> _sampler_states;

  graphics::allocator _allocator;
  graphics::query_pool _query_pool;

  graphics::compiler _compiler;

  std::vector<command_buffer::acquire_ownership_data> _acquire_ownership_data;
  std::vector<command_buffer::release_ownership_data> _release_ownership_data;

  std::uint32_t _current_frame{};
  bool _is_framebuffer_resized{};
  bool _is_viewport_resized{};

  math::vector2u _viewport{};

  signals::signal<const math::vector2u&> _on_viewport_changed;

  core::delegate<math::vector2u()> _dynamic_size_callback;

}; // class graphics_module

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_GRAPHICS_MODULE_HPP_
