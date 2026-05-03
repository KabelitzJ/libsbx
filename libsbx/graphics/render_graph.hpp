// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_RENDER_GRAPH_HPP_
#define LIBSBX_GRAPHICS_RENDER_GRAPH_HPP_

#include <vector>
#include <variant>
#include <string>
#include <memory>
#include <unordered_map>
#include <optional>
#include <functional>
#include <queue>

#include <vulkan/vulkan.h>

#include <libsbx/utility/enum.hpp>
#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/hashed_string.hpp>
#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/overload.hpp>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/viewport.hpp>
#include <libsbx/graphics/draw_list.hpp>

#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/images/depth_image.hpp>

#include <libsbx/graphics/render_pass/swapchain.hpp>

namespace sbx::graphics {

enum class blend_factor : std::int32_t {
  zero = VK_BLEND_FACTOR_ZERO,
  one = VK_BLEND_FACTOR_ONE,
  source_color = VK_BLEND_FACTOR_SRC_COLOR,
  one_minus_source_color = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
  source_alpha = VK_BLEND_FACTOR_SRC_ALPHA,
  one_minus_source_alpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
  destination_color = VK_BLEND_FACTOR_DST_COLOR,
  one_minus_destination_color = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
  destination_alpha = VK_BLEND_FACTOR_DST_ALPHA,
  one_minus_destination_alpha = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
  constant_color = VK_BLEND_FACTOR_CONSTANT_COLOR,
  one_minus_constant_color = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
  constant_alpha = VK_BLEND_FACTOR_CONSTANT_ALPHA,
  one_minus_constant_alpha = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
  source_alpha_saturate = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE
}; // enum class blend_factor

enum class blend_operation : std::int32_t {
  add = VK_BLEND_OP_ADD,
  subtract = VK_BLEND_OP_SUBTRACT,
  reverse_subtract = VK_BLEND_OP_REVERSE_SUBTRACT,
  min = VK_BLEND_OP_MIN,
  max = VK_BLEND_OP_MAX
}; // enum class blend_operation

enum class color_component : std::int32_t {
  r = VK_COLOR_COMPONENT_R_BIT,
  g = VK_COLOR_COMPONENT_G_BIT,
  b = VK_COLOR_COMPONENT_B_BIT,
  a = VK_COLOR_COMPONENT_A_BIT
}; // enum class color_component

inline constexpr auto operator|(const color_component lhs, const color_component rhs) noexcept -> color_component {
  return static_cast<color_component>(static_cast<std::underlying_type_t<color_component>>(lhs) | static_cast<std::underlying_type_t<color_component>>(rhs));
}

inline constexpr auto operator&(const color_component lhs, const color_component rhs) noexcept -> color_component {
  return static_cast<color_component>(static_cast<std::underlying_type_t<color_component>>(lhs) & static_cast<std::underlying_type_t<color_component>>(rhs));
}

struct blend_state {
  blend_factor color_source{blend_factor::source_alpha};
  blend_factor color_destination{blend_factor::one_minus_source_alpha};
  blend_operation color_operation{blend_operation::add};
  blend_factor alpha_source{blend_factor::one};
  blend_factor alpha_destination{blend_factor::zero};
  blend_operation alpha_operation{blend_operation::add};
  color_component color_write_mask{color_component::r | color_component::g | color_component::b | color_component::a};
}; // struct blend_state

enum class attachment_load_operation : std::int32_t  {
  load = VK_ATTACHMENT_LOAD_OP_LOAD,
  clear = VK_ATTACHMENT_LOAD_OP_CLEAR,
  dont_care = VK_ATTACHMENT_LOAD_OP_DONT_CARE
}; // enum class color_component

class attachment {

public:

  enum class type {
    image,
    depth,
    storage,
    swapchain
  }; // enum class type

  using clear_value_type = std::variant<math::color, math::vector4i, math::vector4u>;

  attachment(const utility::hashed_string& name, type type, const clear_value_type& clear_value = math::color::black(), const format format = format::r8g8b8a8_unorm, const graphics::blend_state& blend_state = graphics::blend_state{}, const filter filter = filter::linear, const address_mode address_mode = address_mode::repeat, std::uint32_t array_layers = 1u) noexcept;

  attachment(const utility::hashed_string& name, type type, const clear_value_type& clear_value, const format format, const filter filter, const address_mode address_mode, std::uint32_t array_layers = 1u) noexcept;

  auto name() const noexcept -> const utility::hashed_string&;

  auto image_type() const noexcept -> type;

  auto format() const noexcept -> graphics::format;

  auto address_mode() const noexcept -> graphics::address_mode;

  auto clear_value() const noexcept -> const clear_value_type&;

  auto blend_state() const noexcept -> const graphics::blend_state&;

  auto array_layers() const noexcept -> std::uint32_t;

private:

  utility::hashed_string _name;
  type _type;
  clear_value_type _clear_value;
  graphics::format _format;
  graphics::filter _filter;
  graphics::address_mode _address_mode;
  graphics::blend_state _blend_state;
  std::uint32_t _array_layers;

}; // class attachment

struct attachment_description {
  attachment::type image_type;
  graphics::format format;
  graphics::blend_state blend_state;
  std::uint32_t array_layers{1u};
}; // struct attachment_description

struct attachment_handle {

  std::uint32_t index{0xFFFFFFFF};

  [[nodiscard]] auto is_valid() const noexcept -> bool { 
    return index != 0xFFFFFFFF; 
  }

}; // struct attachment_handle

struct pass_handle {
  
  std::uint32_t index{0xFFFFFFFF};

  [[nodiscard]] auto is_valid() const noexcept -> bool { 
    return index != 0xFFFFFFFF; 
  }

}; // struct pass_handle

class pass_node {

  friend class render_graph;

public:

  enum class kind : std::uint8_t {
    graphics,
    compute
  }; // enum class kind

  pass_node(const utility::hashed_string& name, const viewport& viewport, const kind kind)
  : _name{name},
    _viewport{viewport},
    _kind{kind} { }

  auto reads(const attachment_handle attachment) -> void {
    _reads.emplace_back(attachment);
  }

  template<typename... Attachments>
  requires (sizeof...(Attachments) > 1u && (std::is_same_v<std::remove_cvref_t<Attachments>, attachment_handle> && ...))
  auto reads(Attachments&&... attachments) -> void {
    (reads(attachments), ...);
  }

  auto writes(const attachment_handle attachment, const attachment_load_operation load_operation = attachment_load_operation::clear) -> void {
    _writes.emplace_back(attachment, load_operation);
  }

  auto depends_on(const pass_handle pass) -> void {
    _dependencies.emplace_back(pass);
  }

  template<typename... Passes>
  requires (sizeof...(Passes) > 1u && (std::is_same_v<std::remove_cvref_t<Passes>, pass_handle> && ...))
  auto depends_on(Passes&&... passes) -> void {
    (depends_on(passes), ...);
  }

private:

  utility::hashed_string _name;
  std::vector<attachment_handle> _reads;
  std::vector<std::pair<attachment_handle, attachment_load_operation>> _writes;
  std::vector<pass_handle> _dependencies;
  viewport _viewport;
  render_area _render_area;
  kind _kind;

}; // struct pass_node

struct transition_instruction {
  attachment_handle attachment;
  VkImageLayout new_layout;
}; // struct transition_instruction

struct pass_instruction {
  pass_handle pass;
  std::vector<std::pair<attachment_handle, attachment_load_operation>> attachments;
}; // struct pass_instruction

struct compute_instruction {
  pass_handle pass;
}; // struct compute_instruction

using instruction = std::variant<transition_instruction, pass_instruction, compute_instruction>;

struct attachment_state {
  VkImage image;
  VkImageView view;
  VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkFormat format;
  VkExtent2D extent;
  attachment::type type;
  std::uint32_t array_layers{1u};
}; // struct attachment_state

class render_graph {

public:

  struct context {
    auto graphics_pass(const utility::hashed_string& name, const viewport& viewport = viewport::window()) const -> pass_node;
    auto compute_pass(const utility::hashed_string& name) const -> pass_node;
  }; // struct context

  render_graph();

  ~render_graph();

  template<typename... Args>
  requires (std::is_constructible_v<attachment, Args...>)
  auto create_attachment(Args&&... args) -> attachment_handle;

  template<typename Callable>
  requires (std::is_invocable_r_v<pass_node, Callable, context&>)
  auto create_pass(Callable&& callable) -> pass_handle;

  auto find_attachment(const std::string& name) const -> const image2d&;

  auto attachment_descriptions(const pass_handle handle) const -> std::vector<attachment_description>;

  auto build() -> void;

  auto resize(const std::string& viewport_name) -> void;

  auto pass_kind(const pass_handle handle) const -> pass_node::kind {
    utility::assert_that(handle.is_valid() && handle.index < _passes.size(), "Invalid pass handle");

    return _passes[handle.index]._kind;
  }

  template<typename PassCallback, typename ComputeCallback>
  requires (std::is_invocable_v<PassCallback, const pass_handle&> && std::is_invocable_v<ComputeCallback, const pass_handle&>)
  auto execute(command_buffer& command_buffer, const swapchain& swapchain, PassCallback&& pass_callback, ComputeCallback&& compute_callback) -> void {
    for (auto& state : _attachment_states) {
      if (state.type == attachment::type::swapchain) {
        state.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
      }
    }

    for (const auto& instruction : _instructions) {
      std::visit(utility::overload{
        [&](const transition_instruction& instruction) { _execute_transition_instruction(command_buffer, swapchain, instruction); },
        [&](const pass_instruction& instruction) { _execute_pass_instruction(command_buffer, swapchain, instruction, std::forward<PassCallback>(pass_callback)); },
        [&](const compute_instruction& instruction) { _execute_compute_instruction(command_buffer, instruction, std::forward<ComputeCallback>(compute_callback)); },
      }, instruction);
    }
  }

private:

  auto _update_viewports() -> void;

  auto _clear_attachments(const std::string& viewport_name) -> void;

  auto _create_attachments(const pass_node& node) -> void;

  auto _pass_matches(const pass_node& pass, const std::string& viewport_name) const -> bool;

  auto _build_color_attachment_info(const attachment& attachment, const attachment_state& state, const swapchain& swapchain, const attachment_load_operation load_op) -> VkRenderingAttachmentInfo;

  auto _build_depth_attachment_info(const attachment& attachment, const attachment_state& state, const attachment_load_operation load_op) -> VkRenderingAttachmentInfo;

  template<typename Callable>
  auto _execute_pass_instruction(command_buffer& command_buffer, const swapchain& swapchain, const pass_instruction& instruction, Callable&& callable) -> void;

  template<typename Callable>
  auto _execute_compute_instruction(command_buffer& command_buffer, const compute_instruction& instruction, Callable&& callable) -> void;

  auto _execute_transition_instruction(command_buffer& command_buffer, const swapchain& swapchain, const transition_instruction& instruction) -> void;

  std::vector<attachment> _attachments;
  std::vector<pass_node> _passes;

  std::vector<image2d_handle> _color_images;
  std::vector<depth_image_handle> _depth_images;

  std::vector<attachment_state> _attachment_states;

  std::unordered_map<utility::hashed_string, std::uint32_t> _image_by_name;

  std::vector<instruction> _instructions;

}; // class render_graph

} // namespace sbx::graphics

#include <libsbx/graphics/render_graph.ipp>

#endif // LIBSBX_GRAPHICS_RENDER_GRAPH_HPP_
