#include <libsbx/graphics/render_graph.hpp>

#include <queue>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/exception.hpp>

#include <libsbx/math/color.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/descriptor/descriptor.hpp>

namespace sbx::graphics {

attachment::attachment(const utility::hashed_string& name, type type, const math::color& clear_color, const graphics::format format, const graphics::blend_state& blend_state, const graphics::address_mode address_mode) noexcept
: _name{name}, 
  _type{type},
  _clear_color{clear_color},
  _format{format}, 
  _address_mode{address_mode},
  _blend_state{blend_state} { }

attachment::attachment(const utility::hashed_string& name, type type, const math::color& clear_color, const graphics::format format, const graphics::address_mode address_mode) noexcept
: _name{name}, 
  _type{type},
  _clear_color{clear_color},
  _format{format}, 
  _address_mode{address_mode},
  _blend_state{} { }

auto attachment::name() const noexcept -> const utility::hashed_string& {
  return _name;
}

auto attachment::image_type() const noexcept -> type {
  return _type;
}

auto attachment::format() const noexcept -> graphics::format {
  return _format;
}

auto attachment::address_mode() const noexcept -> graphics::address_mode {
  return _address_mode;
}

auto attachment::clear_color() const noexcept -> const math::color& {
  return _clear_color;
}

auto attachment::blend_state() const noexcept -> const graphics::blend_state& {
  return _blend_state;
}

namespace detail {

graphics_node::graphics_node(const utility::hashed_string& name, const viewport& viewport)
: _name{name},
  _viewport{viewport} { }

compute_node::compute_node(const utility::hashed_string& name)
: _name{name} { }

auto graph_base::reserve(const std::size_t graphics, const std::size_t compute) -> void {
  _graphics_nodes.reserve(graphics);
  _compute_nodes.reserve(compute);
}

auto graphics_pass::name() const -> const utility::hashed_string& {
  return _node._name;
}

auto graphics_pass::inputs() const -> const std::vector<utility::hashed_string>& {
  return _node._inputs;
}

auto graphics_pass::outputs() const -> const std::vector<attachment>& {
  return _node._outputs;
}

// auto graphics_pass::draw_list(const utility::hashed_string& name) const -> const std::unique_ptr<graphics::draw_list>& {
//   if (auto entry = _graph._draw_lists.find(name); entry != _graph._draw_lists.end()) {
//     return entry->second;
//   }

//   throw utility::runtime_error{"Draw list with name '{}' not found in graphics pass '{}'", name.str(), _node._name.str()};
// }

graphics_pass::graphics_pass(graph_base& graph, graphics_node& node)
: _graph{graph},
  _node{node} { }

compute_pass::compute_pass(compute_node& node)
: _node{node} { }

auto context::graphics_pass(const utility::hashed_string& name, const viewport& viewport) -> detail::graphics_pass {
  return detail::graphics_pass{_graph, _graph.emplace_back<detail::graphics_node>(name, viewport)};
}

auto context::compute_pass(const utility::hashed_string& name) -> detail::compute_pass {
  return detail::compute_pass{_graph.emplace_back<detail::compute_node>(name)};
}

context::context(graph_base& graph)
: _graph{graph} { }

graph_builder::graph_builder(graph_base& graph)
: _graph{graph} { }

auto graph_builder::build() -> void {
  auto attachment_producers = std::unordered_map<utility::hashed_string, utility::hashed_string>{};
  auto pass_dependencies = std::unordered_map<utility::hashed_string, std::unordered_set<utility::hashed_string>>{};

  for (const auto& node : _graph._graphics_nodes) {
    for (const auto& output : node._outputs) {
      attachment_producers[output.name()] = node._name;
    }
  }

  for (const auto& node : _graph._graphics_nodes) {
    for (const auto& input : node._inputs) {
      if (auto entry = attachment_producers.find(input); entry != attachment_producers.end()) {
        pass_dependencies[node._name].insert(entry->second);
      } else {
        throw utility::runtime_error{"No producer for attachment '{}'", input.str()};
      }
    }
  }

  auto dependents = std::unordered_map<utility::hashed_string, std::uint32_t>{};

  for (const auto& [pass, dependencies] : pass_dependencies) {
    dependents[pass] = dependencies.size();
  }

  for (const auto& node : _graph._graphics_nodes) {
    // This only inserts 0 is the key does not already exist
    dependents.emplace(node._name, 0u);
  }

  auto ready = std::queue<utility::hashed_string>{};

  for (const auto& [pass, dependencies] : dependents) {
    if (dependencies == 0u) {
      ready.push(pass);
    }
  }

  auto sorted_passes = std::vector<utility::hashed_string>{};

  while (!ready.empty()) {
    auto current = ready.front();
    ready.pop();
    
    sorted_passes.push_back(current);

    for (auto& [pass, dependencies] : pass_dependencies) {
      if (dependencies.erase(current) && --dependents[pass] == 0) {
        ready.push(pass);
      }
    }
  }

  auto swapchain_attachment_name = utility::hashed_string{};

  for (const auto& pass_name : sorted_passes) {
    auto node = std::ranges::find_if(_graph._graphics_nodes, [&](const auto& n) {
      return n._name == pass_name;
    });

    if (node == _graph._graphics_nodes.end()) {
      continue;
    }

    for (const auto& input : node->_inputs) {
      const auto required_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      auto& attachment_state = _attachment_states[input];

      if (attachment_state.current_layout != required_layout) {
        _instructions.emplace_back(transition_instruction{
          .attachment = input,
          .old_layout = attachment_state.current_layout,
          .new_layout = required_layout
        });

        attachment_state.current_layout = required_layout;
      }
    }

    auto attachments = std::vector<utility::hashed_string>{};

    for (const auto& output : node->_outputs) {
      const auto target_layout = output.image_type() == attachment::type::depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      auto& attachment_state = _attachment_states[output.name()];

      if (attachment_state.current_layout != target_layout) {
        _instructions.emplace_back(transition_instruction{
          .attachment = output.name(),
          .old_layout = attachment_state.current_layout,
          .new_layout = target_layout
        });

        attachment_state.current_layout = target_layout;
      } 
      
      if (output.image_type() == attachment::type::swapchain) {
        swapchain_attachment_name = output.name();
      }

      attachments.push_back(output.name());
    }

    _instructions.emplace_back(pass_instruction{
      .node = *node,
      .attachments = attachments
    });
  }

  if (swapchain_attachment_name.is_empty()) {
    throw utility::runtime_error("Render graph does not containe swapchain attachment");
  }

  _instructions.emplace_back(transition_instruction{
    .attachment = swapchain_attachment_name,
    .old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  });
}

auto graph_builder::resize(const viewport::type flags) -> void {
  _update_viewports();

  _clear_attachments(flags);

  for (const auto& node : _graph._graphics_nodes) {
    _create_attachments(flags, node);
  }
}

auto graph_builder::attachment(const std::string& name) const -> const descriptor& {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (auto entry = _color_images.find(name); entry != _color_images.end()) {
    return graphics_module.get_resource<image2d>(entry->second);
  } else if (auto entry = _depth_images.find(name); entry != _depth_images.end()) {
    return graphics_module.get_resource<depth_image>(entry->second);
  }

  throw utility::runtime_error{"No Attachment '{}' found", name};
}

auto graph_builder::_update_viewports() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& surface = graphics_module.surface();
  const auto surface_extent = math::vector2u{surface.current_extent().width, surface.current_extent().height};

  const auto viewport_extent = graphics_module.dynamic_viewport();

  for (auto& node : _graph._graphics_nodes) {
    const auto& viewport = node._viewport;
    auto& render_area = node._render_area;

    render_area.set_offset(viewport.offset());

    const auto size = viewport.is_fixed() ? *viewport.size() : (viewport.is_window() ? surface_extent : viewport_extent);

    render_area.set_extent(math::vector2u{viewport.scale() * size});

    render_area.set_aspect_ratio(static_cast<std::float_t>(render_area.extent().x()) / static_cast<std::float_t>(render_area.extent().y()));
    render_area.set_extent(render_area.extent() + render_area.offset());
  }
}

auto graph_builder::_clear_all_attachments() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  for (const auto& [key, handle] : _color_images) {
    graphics_module.remove_resource<image2d>(handle);
  }

  _color_images.clear();

  for (const auto& [key, handle] : _depth_images) {
    graphics_module.remove_resource<depth_image>(handle);
  }

  _depth_images.clear();

  _attachment_states.clear();
}

auto graph_builder::_clear_attachments(const viewport::type flags) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  for (auto& node : _graph._graphics_nodes) {
    const auto& viewport = node._viewport;

    if (!viewport.is_type(flags) && !viewport.is_fixed()) {
      continue;
    }

    for (const auto& attachment : node._outputs) {
      switch (attachment.image_type()) {
        case attachment::type::image: {
          auto entry = _color_images.find(attachment.name());

          if (entry != _color_images.end()) {
            graphics_module.remove_resource<image2d>(entry->second);
            _color_images.erase(entry);
            _attachment_states.erase(attachment.name());
          }

          break;
        }
        case attachment::type::depth: {
          auto entry = _depth_images.find(attachment.name());

          if (entry != _depth_images.end()) {
            graphics_module.remove_resource<depth_image>(entry->second);
            _depth_images.erase(entry);
            _attachment_states.erase(attachment.name());
          }

          break;
        }
        case attachment::type::storage: {
          utility::logger<"graphics">::warn("Storage image attachments not implemented");
          break;
        }
        case attachment::type::swapchain: {
          _attachment_states.erase(attachment.name());
          break;
        }
        default: {
          throw utility::runtime_error{"Invalid image type '{}", utility::to_underlying(attachment.image_type())};
        }
      }
    }
  }
}

auto graph_builder::_create_attachments(const viewport::type flags, const graphics_node& node) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  _pass_render_areas[node._name] = node._render_area;

  const auto extent = node._render_area.extent();

  for (const auto& attachment : node._outputs) {
    switch (attachment.image_type()) {
      case attachment::type::image: {
        if (_color_images.contains(attachment.name())) {
          break;
        }

        auto filter = VK_FILTER_LINEAR;

        if (attachment.format() == format::r32_uint || attachment.format() == format::r64_uint || attachment.format() == format::r32g32_uint) {
          filter = VK_FILTER_NEAREST;
        }

        const auto usage = VkImageUsageFlags{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT};

        const auto handle = graphics_module.add_resource<image2d>(extent, attachment.format(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, usage, filter, to_vk_enum<VkSamplerAddressMode>(attachment.address_mode()), VK_SAMPLE_COUNT_1_BIT);
        const auto& image = graphics_module.get_resource<image2d>(handle);
        
        _color_images.emplace(attachment.name(), handle);

        _attachment_states.insert_or_assign(attachment.name(), attachment_state{
          .image = image.handle(),
          .view = image.view(),
          .current_layout = VK_IMAGE_LAYOUT_UNDEFINED,
          // .format = to_vk_enum<VkFormat>(attachment.format()),
          .format = image.format(),
          .extent = VkExtent2D{extent.x(), extent.y()},
          .type = attachment::type::image,
          .is_first_use = false
        });

        _clear_values.insert_or_assign(attachment.name(), VkClearValue{
          .color = {
            .float32 = {
              attachment.clear_color().r(), 
              attachment.clear_color().g(), 
              attachment.clear_color().b(), 
              attachment.clear_color().a()
            }
          }
        });

        utility::logger<"graphics">::debug("Created color attachment '{}' with extent {}x{}", attachment.name().str(), extent.x(), extent.y());

        break;
      }
      case attachment::type::depth: {
        if (_depth_images.contains(attachment.name())) {
          break;
        }

        const auto handle = graphics_module.add_resource<depth_image>(node._render_area.extent(), VK_SAMPLE_COUNT_1_BIT);
        const auto& image = graphics_module.get_resource<depth_image>(handle);
        
        _depth_images.emplace(attachment.name(), handle);

        _attachment_states.insert_or_assign(attachment.name(), attachment_state{
          .image = image.handle(),
          .view = image.view(),
          .current_layout = VK_IMAGE_LAYOUT_UNDEFINED,
          // .format = to_vk_enum<VkFormat>(attachment.format()),
          .format = image.format(),
          .extent = VkExtent2D{extent.x(), extent.y()},
          .type = attachment::type::depth,
          .is_first_use = false
        });

        _clear_values.insert_or_assign(attachment.name(), VkClearValue{
          .depthStencil {
            .depth = 1.0f,
            .stencil = 0
          }
        });

        utility::logger<"graphics">::debug("Created depth attachment '{}' with extent {}x{}", attachment.name().str(), extent.x(), extent.y());

        break;
      }
      case attachment::type::storage: {
        utility::logger<"graphics">::warn("Storage image attachments not implemented");
        break;
      }
      case attachment::type::swapchain: {
        _attachment_states.emplace(attachment.name(), attachment_state{
          .image = nullptr,
          .view = nullptr,
          .current_layout = VK_IMAGE_LAYOUT_UNDEFINED,
          // .format = to_vk_enum<VkFormat>(attachment.format()),
          .format = VK_FORMAT_UNDEFINED,
          .extent = VkExtent2D{extent.x(), extent.y()},
          .type = attachment::type::swapchain,
          .is_first_use = false
        });
        
        _clear_values.emplace(attachment.name(), VkClearValue{
          .color = {
            .float32 = {
              attachment.clear_color().r(), 
              attachment.clear_color().g(), 
              attachment.clear_color().b(), 
              attachment.clear_color().a()
            }
          }
        });

        utility::logger<"graphics">::debug("Created swapchain attachment '{}' with extent {}x{}", attachment.name().str(), extent.x(), extent.y());

        break;
      }
      default: {
        throw utility::runtime_error{"Invalid image type '{}", utility::to_underlying(attachment.image_type())};
      }
    }
  }
}

// auto graph_builder::_execute_instruction(command_buffer& command_buffer, const transition_instruction& instruction) -> void {
//   auto& state = _attachment_states.at(instruction.attachment);

//   if (state.current_layout == instruction.new_layout) {
//     utility::logger<"graphics">::warn("Transition instruction found where none was needed");
//     return;
//   }

//   auto barrier = VkImageMemoryBarrier{};
//   barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//   barrier.oldLayout = instruction.old_layout;
//   barrier.newLayout = instruction.new_layout;
//   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//   barrier.image = state.image;
//   barrier.subresourceRange.aspectMask = state.is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
//   barrier.subresourceRange.baseMipLevel = 0;
//   barrier.subresourceRange.levelCount = 1;
//   barrier.subresourceRange.baseArrayLayer = 0;
//   barrier.subresourceRange.layerCount = 1;

//   // You can refine this per case if you track pass type
//   barrier.srcAccessMask = 0;
//   barrier.dstAccessMask = state.is_depth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

//   vkCmdPipelineBarrier(
//     command_buffer,
//     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // srcStage (can refine later)
//     state.is_depth ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStage
//     0,
//     0, nullptr,
//     0, nullptr,
//     1, &barrier
//   );

//   state.current_layout = instruction.new_layout;
// }

// auto graph_builder::_execute_instruction(command_buffer& command_buffer, const pass_instruction& instruction) -> void {
  
// }

} // namespace detail

render_graph::render_graph() 
: base{_graph} { }

} // namespace sbx::graphics
