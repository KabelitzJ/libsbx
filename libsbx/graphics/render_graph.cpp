// SPDX-License-Identifier: MIT
#include <libsbx/graphics/render_graph.hpp>

#include <queue>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/concepts.hpp>

#include <libsbx/math/color.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/descriptor/descriptor.hpp>

#include <libsbx/graphics/devices/debug_messenger.hpp>

namespace sbx::graphics {

attachment::attachment(const utility::hashed_string& name, type type, const clear_value_type& clear_value, const graphics::format format, const graphics::blend_state& blend_state, const graphics::filter filter, const graphics::address_mode address_mode, std::uint32_t array_layers) noexcept
: _name{name},
  _type{type},
  _clear_value{clear_value},
  _format{format},
  _filter{filter},
  _address_mode{address_mode},
  _blend_state{blend_state},
  _array_layers{array_layers} { }

attachment::attachment(const utility::hashed_string& name, type type, const clear_value_type& clear_value, const graphics::format format, const graphics::filter filter, const graphics::address_mode address_mode, std::uint32_t array_layers) noexcept
: _name{name},
  _type{type},
  _clear_value{clear_value},
  _format{format},
  _filter{filter},
  _address_mode{address_mode},
  _blend_state{},
  _array_layers{array_layers} { }

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

auto attachment::clear_value() const noexcept -> const clear_value_type& {
  return _clear_value;
}

auto attachment::blend_state() const noexcept -> const graphics::blend_state& {
  return _blend_state;
}

auto attachment::array_layers() const noexcept -> std::uint32_t {
  return _array_layers;
}

auto render_graph::context::graphics_pass(const utility::hashed_string& name, const viewport& viewport) const -> pass_node {
  return pass_node{name, viewport, pass_node::kind::graphics};
}

auto render_graph::context::compute_pass(const utility::hashed_string& name) const -> pass_node {
  return pass_node{name, viewport::window(), pass_node::kind::compute};
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

    descriptions.emplace_back(attachment.image_type(), attachment.format(), attachment.blend_state(), attachment.array_layers());
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

  for (auto i = 0u; i < pass_count; ++i) {
    const auto& pass = _passes[i];

    for (const auto& dependency : pass._dependencies) {
      utility::assert_that(dependency.is_valid() && dependency.index < pass_count, "Invalid pass handle in depends_on()");

      edges[dependency.index].emplace_back(i);
    }
  }

  auto last_writer = std::vector<std::optional<std::uint32_t>>{};
  last_writer.resize(attachment_count, std::nullopt);

  for (auto i = 0u; i < pass_count; ++i) {
    const auto& pass = _passes[i];

    for (const auto& attachment : pass._reads) {
      if (auto writer = last_writer[attachment.index]) {
        edges[*writer].push_back(i);
      }
    }

    for (const auto& [attachment, _] : pass._writes) {
      if (auto writer = last_writer[attachment.index]) {
        edges[*writer].push_back(i);
      }

      last_writer[attachment.index] = i;
    }
  }

  for (auto& adjacent : edges) {
    std::sort(adjacent .begin(), adjacent .end());
    adjacent .erase(std::unique(adjacent .begin(), adjacent .end()), adjacent .end());
  }

  auto indegrees = std::vector<std::uint32_t>{};
  indegrees.resize(pass_count, 0u);

  for (auto i = 0u; i < pass_count; ++i) {
    for (auto dependent : edges[i]) {
      indegrees[dependent]++;
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
    auto current = process_queue.front();
    process_queue.pop();

    execution_order.push_back(current);

    for (auto dependent : edges[current]) {
      utility::assert_that(indegrees[dependent] > 0u, "Invalid dependency state");

      indegrees[dependent]--;

      if (indegrees[dependent] == 0u) {
        process_queue.push(dependent);
      }
    }
  }

  if (execution_order.size() != pass_count) {
    throw utility::runtime_error("Render graph contains a dependency cycle");
  }

  _attachment_states.resize(attachment_count);

  auto planned_layouts = std::vector<VkImageLayout>{};
  planned_layouts.resize(attachment_count, VK_IMAGE_LAYOUT_UNDEFINED);

  for (auto i = 0u; i < attachment_count; ++i) {
    const auto& attachment = _attachments[i];
    auto& state = _attachment_states[i];

    state.image = nullptr;
    state.view = nullptr;
    state.current_layout = attachment.image_type() == attachment::type::swapchain ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
    state.format = VK_FORMAT_UNDEFINED;
    state.extent = VkExtent2D{0, 0};
    state.type = attachment.image_type();

    planned_layouts[i] = state.current_layout;
  }

  for (auto pass_index : execution_order) {
    const auto& pass = _passes[pass_index];

    for (const auto& handle : pass._reads) {
      auto& state = _attachment_states[handle.index];

      auto& planned_layout = planned_layouts[handle.index];

      auto required_layout = VkImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};

      if (state.type == attachment::type::depth) {
        required_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
      } else if (state.type == attachment::type::storage) {
        required_layout = VK_IMAGE_LAYOUT_GENERAL;
      } else {
        required_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      }

      if (planned_layout != required_layout) {
        _instructions.emplace_back(transition_instruction{
          .attachment = handle,
          .new_layout = required_layout
        });

        planned_layout = required_layout;
      }
    }

    auto written_attachments = std::vector<std::pair<attachment_handle, attachment_load_operation>>{};
    written_attachments.reserve(pass._writes.size());

    for (const auto& [handle, load_op] : pass._writes) {
      auto& state = _attachment_states[handle.index];

      auto& planned_layout = planned_layouts[handle.index];

      if (state.type == attachment::type::depth) {
        const auto target_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        if (planned_layout != target_layout) {
          _instructions.emplace_back(transition_instruction{
            .attachment = handle,
            .new_layout = target_layout
          });

          planned_layout = target_layout;
        }
        written_attachments.push_back({handle, load_op});

        continue; 
      }

      auto target_layout = VkImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};

      if (pass._kind == pass_node::kind::graphics) {
        utility::assert_that(state.type != attachment::type::storage, "Graphics pass cannot write a storage attachment");
        utility::assert_that(state.type == attachment::type::image || state.type == attachment::type::swapchain, "Graphics pass writes must be image/depth/swapchain");

        target_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      } else {
        utility::assert_that(pass._kind == pass_node::kind::compute, "Invalid pass kind");

        utility::assert_that(state.type != attachment::type::swapchain, "Compute pass cannot write the swapchain");
        utility::assert_that(state.type != attachment::type::depth, "Compute pass cannot write a depth attachment");

        target_layout = VK_IMAGE_LAYOUT_GENERAL;
      }

      if (planned_layout != target_layout) {
        _instructions.emplace_back(transition_instruction{
          .attachment = handle,
          .new_layout = target_layout
        });

        planned_layout = target_layout;
      }

      written_attachments.push_back({handle, load_op});
    }

    if (pass._kind == pass_node::kind::graphics) {
      _instructions.emplace_back(pass_instruction{
        .pass = pass_handle{pass_index},
        .attachments = std::move(written_attachments)
      });
    } else {
      _instructions.emplace_back(compute_instruction{
        .pass = pass_handle{pass_index}
      });
    }
  }

  for (auto i = 0u; i < attachment_count; ++i) {
    const auto& state = _attachment_states[i];

    if (state.type == attachment::type::depth) {
      const auto is_depth_stencil_attachment = planned_layouts[i] == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      const auto is_depth_stencil_read = planned_layouts[i] == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

      utility::assert_that(is_depth_stencil_attachment || is_depth_stencil_read, "Depth attachment layout mutated after initialization");
    }
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

  auto& planned_layout = planned_layouts[swapchain_attachment->index];

  if (planned_layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    _instructions.emplace_back(transition_instruction{
      .attachment = *swapchain_attachment,
      .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    });

    planned_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  }
}

auto render_graph::_pass_matches(const pass_node& pass, const std::string& viewport_name) const -> bool {
  const auto& viewport = pass._viewport;

  if (viewport.is_fixed()) {
    return viewport_name == viewport::window_name;
  }

  return viewport.name() == viewport_name;
}

auto render_graph::resize(const std::string& viewport_name) -> void {
  _update_viewports();

  _clear_attachments(viewport_name);

  _depth_images.resize(_attachments.size(), depth_image_handle{});
  _color_images.resize(_attachments.size(), image2d_handle{});

  for (const auto& pass : _passes) {
    if (!_pass_matches(pass, viewport_name)) {
      continue;
    }

    _create_attachments(pass);
  }
} 

auto render_graph::_update_viewports() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& viewports = graphics_module.viewports();

  for (auto& pass : _passes) {
    const auto& viewport = pass._viewport;
    auto& render_area = pass._render_area;

    render_area.set_offset(viewport.offset());

    const auto size = viewport.is_fixed()
      ? *viewport.size()
      : viewports.size(viewport.name());

    render_area.set_extent(math::vector2u{viewport.scale() * size});

    const auto extent_x = render_area.extent().x() == 0u ? 1u : render_area.extent().x();
    const auto extent_y = render_area.extent().y() == 0u ? 1u : render_area.extent().y();

    render_area.set_aspect_ratio(static_cast<std::float_t>(extent_x) / static_cast<std::float_t>(extent_y));
  }
}

auto render_graph::_clear_attachments(const std::string& viewport_name) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  for (const auto& pass : _passes) {
    if (!_pass_matches(pass, viewport_name)) {
      continue;
    }

    for (const auto& [attachment, _] : pass._writes) {
      auto& state = _attachment_states[attachment.index];

      switch (state.type) {
        case attachment::type::image:
        case attachment::type::storage: {
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
      state.current_layout = (state.type == attachment::type::swapchain) ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
    }
  }
}

auto render_graph::_create_attachments(const pass_node& pass) -> void {
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
        const auto needs_nearest_filter = (attachment.format() == format::r32_uint || attachment.format() == format::r64_uint || attachment.format() == format::r32g32_uint);

        const auto is_srgb = (attachment.format() == format::r8g8b8a8_srgb || attachment.format() == format::b8g8r8a8_srgb);

        auto usage = VkImageUsageFlags{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT};

        if (!is_srgb) {
          usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        }

        const auto image_handle = graphics_module.add_resource<image2d>(
          extent,
          attachment.format(),
          needs_nearest_filter ? graphics::filter::nearest : graphics::filter::linear,
          attachment.address_mode(),
          usage,
          VK_SAMPLE_COUNT_1_BIT,
          false,
          false,
          attachment.array_layers()
        );

        const auto& image = graphics_module.get_resource<image2d>(image_handle);

        _color_images[handle.index] = image_handle;

        state.image = image.handle();
        state.view = image.view();
        state.format = image.format();
        state.extent = {extent.x(), extent.y()};
        state.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        state.array_layers = attachment.array_layers();

        const auto name = fmt::format("Render Graph Color Attachment '{}'", attachment.name().str());

        graphics::set_debug_name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(state.image), name);

        break;
      }
      case attachment::type::storage: {
        const auto needs_nearest_filter = (attachment.format() == format::r32_uint || attachment.format() == format::r64_uint || attachment.format() == format::r32g32_uint);

        const auto usage = VkImageUsageFlags{
          VK_IMAGE_USAGE_STORAGE_BIT |
          VK_IMAGE_USAGE_SAMPLED_BIT
        };

        const auto image_handle = graphics_module.add_resource<image2d>(
          extent,
          attachment.format(),
          needs_nearest_filter ? graphics::filter::nearest : graphics::filter::linear,
          attachment.address_mode(),
          usage,
          VK_SAMPLE_COUNT_1_BIT,
          false,
          false,
          attachment.array_layers()
        );

        const auto& image = graphics_module.get_resource<image2d>(image_handle);

        _color_images[handle.index] = image_handle;

        state.image = image.handle();
        state.view = image.view();
        state.format = image.format();
        state.extent = {extent.x(), extent.y()};
        state.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        state.array_layers = attachment.array_layers();

        const auto name = fmt::format("Render Graph Storage Attachment '{}'", attachment.name().str());

        graphics::set_debug_name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(state.image), name);

        break;
      }
      case attachment::type::depth: {
        const auto image_handle = graphics_module.add_resource<depth_image>(extent, VK_SAMPLE_COUNT_1_BIT, attachment.array_layers());
        const auto& image = graphics_module.get_resource<depth_image>(image_handle);

        _depth_images[handle.index] = image_handle;

        state.image = image.handle();
        state.view = image.view();
        state.format = image.format();
        state.extent = {extent.x(), extent.y()};
        state.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        state.array_layers = attachment.array_layers();

        break;
      }
      case attachment::type::swapchain: {
        state.image = nullptr;
        state.view = nullptr;
        state.format = VK_FORMAT_UNDEFINED;
        state.extent = {extent.x(), extent.y()};
        state.current_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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
  
  std::visit([&](const auto& value) {
    using Type = std::decay_t<decltype(value)>;

    if constexpr (std::is_same_v<Type, math::color>) {
      info.clearValue.color.float32[0] = value.r();
      info.clearValue.color.float32[1] = value.g();
      info.clearValue.color.float32[2] = value.b();
      info.clearValue.color.float32[3] = value.a();
    } else if constexpr (std::is_same_v<Type, math::vector4i>) {
      info.clearValue.color.int32[0] = value.x();
      info.clearValue.color.int32[1] = value.y();
      info.clearValue.color.int32[2] = value.z();
      info.clearValue.color.int32[3] = value.w();
    } else if constexpr (std::is_same_v<Type, math::vector4u>) {
      info.clearValue.color.uint32[0] = value.x();
      info.clearValue.color.uint32[1] = value.y();
      info.clearValue.color.uint32[2] = value.z();
      info.clearValue.color.uint32[3] = value.w();
    } else {
      static_assert(utility::always_false<Type>, "Unsupported clear color type");
    }
  }, attachment.clear_value());

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

  const auto old_layout = state.current_layout;
  const auto new_layout = instruction.new_layout;

  if (old_layout == new_layout) {
    return;
  }

  if (state.type == attachment::type::swapchain) {
    image::transition_image_layout(command_buffer, swapchain.image(swapchain.active_image_index()), swapchain.format(), old_layout, new_layout, VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
  } else {
    auto aspect = VkImageAspectFlags{0};

    if (state.type == attachment::type::depth) {
      aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
      if (image::has_stencil_component(state.format)) {
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }
    } else {
      aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    image::transition_image_layout(command_buffer, state.image, state.format, old_layout, new_layout, aspect, 1, 0, state.array_layers, 0);
  }

  state.current_layout = new_layout;
}

} // namespace sbx::graphics
