// SPDX-License-Identifier: MIT
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/devices/debug_messenger.hpp>

namespace sbx::graphics {

descriptor_handler::descriptor_handler(std::uint32_t set)
: _set{set},
  _has_changed{true} { }

descriptor_handler::descriptor_handler(const pipeline& pipeline, std::uint32_t set)
: _pipeline{&pipeline},
  _set{set},
  _has_changed{true} {
  _recreate_descriptor_sets();
}

static auto _get_pipeline(const graphics_pipeline_handle& handle) -> memory::observer_ptr<const pipeline> {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  return &graphics_module.get_resource<graphics::graphics_pipeline>(handle);
}

descriptor_handler::descriptor_handler(const graphics_pipeline_handle& handle, std::uint32_t set)
: _pipeline{_get_pipeline(handle)},
  _set{set},
  _has_changed{true} {
  _recreate_descriptor_sets();
}

descriptor_handler::~descriptor_handler() {
  for (auto& descriptor_set : _descriptor_sets) {
    descriptor_set.reset();
  }
}

auto descriptor_handler::push(const std::string& name, uniform_handler& uniform_handler) -> void {
  if (_pipeline) {
    uniform_handler.update(_pipeline->descriptor_block(name, _set));
    _push(name, uniform_handler.uniform_buffer());
  }
}

auto descriptor_handler::push(const std::string& name, storage_handler& storage_handler) -> void {
  if (_pipeline) {
    storage_handler.update(_pipeline->descriptor_block(name, _set));
    _push(name, storage_handler.storage_buffer());
  }
}

auto descriptor_handler::bind_descriptors(command_buffer& command_buffer) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto current_frame = graphics_module.current_frame();

  _descriptor_sets[current_frame]->bind(command_buffer);
}

auto descriptor_handler::descriptor_set() const noexcept -> VkDescriptorSet {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto current_frame = graphics_module.current_frame();

  return *_descriptor_sets[current_frame];
}

auto descriptor_handler::update(const pipeline& pipeline) -> bool {
  if (_pipeline.get() != &pipeline) {
    _pipeline = &pipeline;

    _descriptors.clear();

    _recreate_descriptor_sets();

    return false;
  }

  if (_has_changed) {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    const auto current_frame = graphics_module.current_frame();

    auto& descriptor_set = _descriptor_sets[current_frame];

    auto write_descriptor_sets = std::vector<VkWriteDescriptorSet>{};
    write_descriptor_sets.reserve(_descriptors.size());

    for (const auto& [name, descriptor] : _descriptors) {
      auto write_descriptor_set = descriptor.write_descriptor_set.handle();
      write_descriptor_set.dstSet = *descriptor_set;

      write_descriptor_sets.push_back(write_descriptor_set);
    }

    graphics::descriptor_set::update(write_descriptor_sets);

    _has_changed = false;
  }


  return true;
}

auto descriptor_handler::_recreate_descriptor_sets() -> void {
  _descriptors.clear();

  for (auto& descriptor_set : _descriptor_sets) {
    descriptor_set = std::make_unique<graphics::descriptor_set>(*_pipeline, _set);
  }

  _has_changed = false;
}

} // namespace sbx::graphics
