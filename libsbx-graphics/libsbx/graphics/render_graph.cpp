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

auto render_graph::context::graphics_pass(const utility::hashed_string& name, const viewport& viewport) const -> pass_node {
  return pass_node{name, viewport};
}

render_graph::render_graph() {

}

render_graph::~render_graph() {
  auto& rendering_module = core::engine::get_module<graphics::graphics_module>();

  for (auto& image_handle : _color_images) {
    if (image_handle.is_valid()) {
      rendering_module.remove_resource<image2d>(image_handle);
    }
  }

  for (auto& depth_image_handle : _depth_images) {
    if (depth_image_handle.is_valid()) {
      rendering_module.remove_resource<depth_image>(depth_image_handle);
    }
  }
}

auto render_graph::find_attachment(const std::string& name) const -> const image2d& {
  auto entry = _image_by_name.find(name);

  if (entry == _image_by_name.end()) {
    throw utility::runtime_error{"Render graph does not have attachment '{}", name};
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  return graphics_module.get_resource<image2d>(_color_images[entry->second]);
}

auto render_graph::attachment_descriptions(const pass_handle handle) const -> std::vector<attachment_description> {
  utility::assert_that(handle.is_valid() && handle.index < _passes.size(), "Invalid pass handle");

  const auto& pass = _passes[handle.index]; 

  auto descriptions = std::vector<attachment_description>{};
  descriptions.reserve(pass._writes.size());

  for (const auto& [attachment_handle, load_op] : pass._writes) {
    auto& attachment = _attachments[attachment_handle.index];

    descriptions.emplace_back(attachment.image_type(), attachment.format(), attachment.blend_state());
  }

  return descriptions;
}

auto render_graph::build() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  _instructions.clear();
  _attachment_states.clear();

  const auto pass_count = static_cast<std::uint32_t>(_passes.size());
  const auto attachment_count = static_cast<std::uint32_t>(_attachments.size());

  utility::assert_that(pass_count > 0u, "Render graph must have at least one pass");
  utility::assert_that(attachment_count > 0u, "Render graph must have at least one attachment");

  for (const auto& pass : _passes) {
    for (auto read : pass._reads) {
      utility::assert_that(read.is_valid() && read.index < attachment_count, "Invalid attachment handle in reads()");
    }
    for (const auto& [write, _] : pass._writes) {
      utility::assert_that(write.is_valid() && write.index < attachment_count, "Invalid attachment handle in writes()");
    }
  }

  auto edges = std::vector<std::vector<std::uint32_t>>{};
  edges.resize(pass_count);

  auto indegrees = std::vector<std::uint32_t>{};
  indegrees.resize(pass_count, 0u);

  for (auto i = 0u; i < pass_count; ++i) {
    const auto& pass = _passes[i];

    for (const auto& dependency : pass._dependencies) {
      utility::assert_that(dependency.is_valid() && dependency.index < pass_count, "Invalid pass handle in depends_on()");

      edges[dependency.index].emplace_back(i);
      indegrees[i]++;
    }
  }

  auto last_writer = std::vector<std::optional<std::uint32_t>>{};
  last_writer.resize(attachment_count, std::nullopt);

  for (auto i = 0u; i < pass_count; ++i) {
    const auto& pass = _passes[i];

    for (const auto& attachment : pass._reads) {
      if (auto writer = last_writer[attachment.index]) {
        edges[*writer].push_back(i);
        indegrees[i]++;
      }
    }

    for (const auto& [attachment, _] : pass._writes) {
      if (auto writer = last_writer[attachment.index]) {
        edges[*writer].push_back(i);
        indegrees[i]++;
      }

      last_writer[attachment.index] = i;
    }
  }

  auto process_queue = std::queue<std::uint32_t>{};

  for (auto i = 0u; i < pass_count; ++i) {
    if (indegrees[i] == 0u) {
      process_queue.push(i);
    }
  }

  auto execution_order = std::vector<std::uint32_t>{};
  execution_order.reserve(pass_count);

  while (!process_queue.empty()) {
    uint32_t current = process_queue.front();
    process_queue.pop();

    execution_order.push_back(current);

    for (uint32_t dependent : edges[current]) {
      utility::assert_that(indegrees[dependent] > 0, "Invalid dependency state");

      if (--indegrees[dependent] == 0) {
        process_queue.push(dependent);
      }
    }
  }

  if (execution_order.size() != pass_count) {
    throw utility::runtime_error("Render graph contains a dependency cycle");
  }

  _attachment_states.resize(attachment_count);

  for (auto i = 0u; i < attachment_count; ++i) {
    const auto& attachment = _attachments[i];
    auto& state = _attachment_states[i];

    state.image = nullptr;
    state.view = nullptr;
    state.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    state.format = VK_FORMAT_UNDEFINED;
    state.extent = VkExtent2D{0, 0};
    state.type = attachment.image_type();
  }

  for (auto pass_index : execution_order) {
    const auto& pass = _passes[pass_index];

    for (const auto& attachment : pass._reads) {
      auto& state = _attachment_states[attachment.index];

      const auto required_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      if (state.current_layout != required_layout) {
        _instructions.emplace_back(transition_instruction{
          .attachment = attachment,
          .old_layout = state.current_layout,
          .new_layout = required_layout
        });

        state.current_layout = required_layout;
      }
    }

    auto written_attachments = std::vector<std::pair<attachment_handle, attachment_load_operation>>{};
    written_attachments.reserve(pass._writes.size());

    for (const auto& [attachment, load_op] : pass._writes) {
      auto& state = _attachment_states[attachment.index];

      auto target_layout = (state.type == attachment::type::depth) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      if (state.current_layout != target_layout) {
        _instructions.emplace_back(transition_instruction{
          .attachment = attachment,
          .old_layout = state.current_layout,
          .new_layout = target_layout
        });

        state.current_layout = target_layout;
      }

      written_attachments.push_back({attachment, load_op});
    }

    _instructions.emplace_back(pass_instruction{
      .pass = pass_handle{pass_index},
      .attachments = std::move(written_attachments)
    });
  }

  auto swapchain_attachment = std::optional<attachment_handle>{};

  for (auto i = 0u; i < attachment_count; ++i) {
    const auto& attachment = _attachments[i];
    
    if (attachment.image_type() == attachment::type::swapchain) {
      swapchain_attachment = attachment_handle{i};
      break;
    }
  }

  if (!swapchain_attachment) {
    throw utility::runtime_error("Render graph must have a swapchain attachment");
  }

  auto& state = _attachment_states[swapchain_attachment->index];

  _instructions.emplace_back(transition_instruction{
    .attachment = *swapchain_attachment,
    .old_layout = state.current_layout,
    .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  });

  state.current_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

auto render_graph::resize(const viewport::type flags) -> void {
  _update_viewports();

  _clear_attachments(flags);

  _depth_images.resize(_attachments.size(), depth_image_handle{});
  _color_images.resize(_attachments.size(), image2d_handle{});

  for (const auto& pass : _passes) {
    _create_attachments(flags, pass);
  }
}

auto render_graph::_update_viewports() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& surface = graphics_module.surface();
  const auto surface_extent = math::vector2u{surface.current_extent().width, surface.current_extent().height};

  const auto viewport_extent = graphics_module.viewport();

  for (auto& pass : _passes) {
    const auto& viewport = pass._viewport;
    auto& render_area = pass._render_area;

    render_area.set_offset(viewport.offset());

    const auto size = viewport.is_fixed() ? *viewport.size() : (viewport.is_window() ? surface_extent : viewport_extent);

    render_area.set_extent(math::vector2u{viewport.scale() * size});

    render_area.set_aspect_ratio(static_cast<std::float_t>(render_area.extent().x()) / static_cast<std::float_t>(render_area.extent().y()));
    render_area.set_extent(render_area.extent() + render_area.offset());
  }
}

auto render_graph::_clear_attachments(const viewport::type flags) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  for (const auto& pass : _passes) {
    const auto& viewport = pass._viewport;

    if (!viewport.is_type(flags) && !viewport.is_fixed()) {
      continue;
    }

    for (const auto& [attachment, _] : pass._writes) {
      auto& state = _attachment_states[attachment.index];

      switch (state.type) {
        case attachment::type::image: {
          if (state.image != VK_NULL_HANDLE) {
            graphics_module.remove_resource<image2d>(_color_images[attachment.index]);
            _color_images[attachment.index] = {};
          }
          break;
        }
        case attachment::type::depth: {
          if (state.image != VK_NULL_HANDLE) {
            graphics_module.remove_resource<depth_image>(_depth_images[attachment.index]);
            _depth_images[attachment.index] = {};
          }
          break;
        }
        case attachment::type::swapchain: {
          break;
        }
        default: {
          throw utility::runtime_error("Invalid attachment type in resize clear");
        }
      }

      state.image = nullptr;
      state.view = nullptr;
    }
  }
}

auto render_graph::_create_attachments(const viewport::type flags, const pass_node& pass) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto extent = pass._render_area.extent();

  for (const auto& [handle, load_op] : pass._writes) {
    auto& state = _attachment_states[handle.index];
    const auto& attachment = _attachments[handle.index];

    if (state.image) {
      continue;
    }

    switch (attachment.image_type()) {
      case attachment::type::image: {
        VkFilter filter = VK_FILTER_LINEAR;

        if (attachment.format() == format::r32_uint || attachment.format() == format::r64_uint || attachment.format() == format::r32g32_uint) {
          filter = VK_FILTER_NEAREST;
        }

        const auto usage = VkImageUsageFlags{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT};

        const auto image_handle = graphics_module.add_resource<image2d>(extent, attachment.format(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, usage, filter, to_vk_enum<VkSamplerAddressMode>(attachment.address_mode()), VK_SAMPLE_COUNT_1_BIT);
        const auto& image = graphics_module.get_resource<image2d>(image_handle);

        _color_images[handle.index] = image_handle;

        state.image = image.handle();
        state.view = image.view();
        state.format = image.format();
        state.extent = {extent.x(), extent.y()};

        break;
      }
      case attachment::type::depth: {
        const auto image_handle = graphics_module.add_resource<depth_image>(extent, VK_SAMPLE_COUNT_1_BIT);
        const auto& image = graphics_module.get_resource<depth_image>(image_handle);

        _depth_images[handle.index] = image_handle;

        state.image = image.handle();
        state.view = image.view();
        state.format = image.format();
        state.extent = {extent.x(), extent.y()};
        break;
      }
      case attachment::type::swapchain: {
        state.image = nullptr;
        state.view = nullptr;
        state.format = VK_FORMAT_UNDEFINED;
        state.extent = {extent.x(), extent.y()};
        break;
      }
      default: {
        throw utility::runtime_error("Unsupported attachment type in resize create");
      }
    }
  }
}

auto render_graph::_build_color_attachment_info(const attachment& attachment, const attachment_state& state, const swapchain& swapchain, const attachment_load_operation load_op) -> VkRenderingAttachmentInfo {
  auto info = VkRenderingAttachmentInfo{};
  info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  info.imageView = (state.type == attachment::type::swapchain) ? swapchain.image_view(swapchain.active_image_index()) : state.view;
  info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  info.loadOp = to_vk_enum<VkAttachmentLoadOp>(load_op);
  info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  info.clearValue.color.float32[0] = attachment.clear_color().r();
  info.clearValue.color.float32[1] = attachment.clear_color().g();
  info.clearValue.color.float32[2] = attachment.clear_color().b();
  info.clearValue.color.float32[3] = attachment.clear_color().a();

  return info;
}

auto render_graph::_build_depth_attachment_info(const attachment& attachment, const attachment_state& state, const attachment_load_operation load_op) -> VkRenderingAttachmentInfo {
  auto info = VkRenderingAttachmentInfo{};
  info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  info.imageView = state.view;
  info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  info.loadOp = to_vk_enum<VkAttachmentLoadOp>(load_op);
  info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  info.clearValue.depthStencil.depth = 1.0f;
  info.clearValue.depthStencil.stencil = 0;

  return info;
}

auto render_graph::_execute_transition_instruction(command_buffer& command_buffer, const swapchain& swapchain, const transition_instruction& instruction) -> void {
  auto& state = _attachment_states[instruction.attachment.index];

  if (state.type == attachment::type::swapchain) {
    image::transition_image_layout(command_buffer, swapchain.image(swapchain.active_image_index()), swapchain.format(), instruction.old_layout, instruction.new_layout, VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
  } else {
    const auto aspect = (state.type == attachment::type::depth) ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_COLOR_BIT;

    image::transition_image_layout(command_buffer, state.image, state.format, instruction.old_layout, instruction.new_layout, aspect, 1, 0, 1, 0);
  }
}

} // namespace sbx::graphics
