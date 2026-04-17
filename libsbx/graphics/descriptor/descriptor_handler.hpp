// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DESCRIPTOR_DESCRIPTOR_HANDLER_HPP_
#define LIBSBX_GRAPHICS_DESCRIPTOR_DESCRIPTOR_HANDLER_HPP_

#include <memory>
#include <map>
#include <vector>
#include <optional>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/render_pass/swapchain.hpp>

#include <libsbx/graphics/buffers/uniform_handler.hpp>
#include <libsbx/graphics/buffers/storage_handler.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>

#include <libsbx/graphics/pipeline/pipeline.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/graphics/descriptor/descriptor.hpp>
#include <libsbx/graphics/descriptor/descriptor_set.hpp>

namespace sbx::graphics {

class descriptor_handler {

public:

  inline static constexpr auto global_set_id = std::uint32_t{0u};
  inline static constexpr auto per_draw_call_set_id = std::uint32_t{1u};

  descriptor_handler(std::uint32_t set);

  explicit descriptor_handler(const pipeline& pipeline, std::uint32_t set);

  explicit descriptor_handler(const graphics_pipeline_handle& handle, std::uint32_t set);

  descriptor_handler(const descriptor_handler& other) = delete;

  descriptor_handler(descriptor_handler&&) noexcept = default;

  ~descriptor_handler();

  auto operator=(const descriptor_handler& other) -> descriptor_handler& = delete;

  auto operator=(descriptor_handler&& other) -> descriptor_handler& = default;

  template<typename Descriptor>
  requires (std::is_base_of_v<descriptor, Descriptor>)
  auto push(const std::string& name, const Descriptor& descriptor) -> void {
    if (_pipeline) {
      _push(name, descriptor);
    }
  }

  template<typename Descriptor>
  requires (std::is_base_of_v<descriptor, Descriptor>)
  auto push(const std::string& name, const Descriptor& descriptor, write_descriptor_set&& write_descriptor_set) -> void {
    if (!_pipeline) {
      return;
    }

    auto binding = _pipeline->find_descriptor_binding(name, _set);

    if (!binding) {
      throw std::runtime_error(fmt::format("Failed to find descriptor binding for descriptor '{}'", name));
    }

    _descriptors.insert_or_assign(name, descriptor_entry{std::addressof(descriptor), std::move(write_descriptor_set), *binding});
    _dirty_frames = swapchain::max_frames_in_flight;
  }

  auto push(const std::string& name, uniform_handler& uniform_handler) -> void;

  auto push(const std::string& name, storage_handler& storage_handler) -> void;

  auto bind_descriptors(command_buffer& command_buffer) -> void;

  auto descriptor_set() const noexcept -> VkDescriptorSet;

  auto update(const pipeline& pipeline) -> bool;

private:

  struct descriptor_entry {
    memory::observer_ptr<const graphics::descriptor> descriptor;
    graphics::write_descriptor_set write_descriptor_set;
    std::uint32_t binding;
  }; // struct descriptor_entry

  template<typename Descriptor>
  requires (std::is_base_of_v<descriptor, Descriptor>)
  auto _push(const std::string& name, const Descriptor& descriptor) -> void {
    auto binding = _pipeline->find_descriptor_binding(name, _set);

    if (!binding) {
      throw std::runtime_error(fmt::format("Failed to find descriptor binding for descriptor '{}'", name));
    }

    auto descriptor_type = _pipeline->find_descriptor_type_at_binding(_set, *binding);

    if (!descriptor_type) {

      throw std::runtime_error(fmt::format("Failed to find descriptor type for descriptor '{}' set: {} binding {}", name, _set, *binding));
    }

    auto write_descriptor_set = descriptor.write_descriptor_set(*binding, *descriptor_type);

    if (write_descriptor_set) {
      _descriptors.insert_or_assign(name, descriptor_entry{std::addressof(descriptor), std::move(write_descriptor_set), *binding});
      _dirty_frames = graphics::swapchain::max_frames_in_flight;
    }

  }

  auto _recreate_descriptor_sets() -> void;

  std::uint32_t _set;

  memory::observer_ptr<const pipeline> _pipeline;

  std::array<std::unique_ptr<graphics::descriptor_set>, graphics::swapchain::max_frames_in_flight> _descriptor_sets{};

  std::map<std::string, descriptor_entry> _descriptors{};
  std::uint32_t _dirty_frames{0};

}; // class descriptor_handler

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DESCRIPTOR_DESCRIPTOR_HANDLER_HPP_
