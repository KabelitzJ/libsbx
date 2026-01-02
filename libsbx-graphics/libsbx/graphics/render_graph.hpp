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

#include <libsbx/math/color.hpp>

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

  attachment(const utility::hashed_string& name, type type, const math::color& clear_color = math::color::black(), const format format = format::r8g8b8a8_unorm, const graphics::blend_state& blend_state = graphics::blend_state{}, const address_mode address_mode = address_mode::repeat) noexcept;

  attachment(const utility::hashed_string& name, type type, const math::color& clear_color, const format format, const address_mode address_mode) noexcept;

  auto name() const noexcept -> const utility::hashed_string&;

  auto image_type() const noexcept -> type;

  auto format() const noexcept -> graphics::format;

  auto address_mode() const noexcept -> graphics::address_mode;

  auto clear_color() const noexcept -> const math::color&;

  auto blend_state() const noexcept -> const graphics::blend_state&;

private:

  utility::hashed_string _name;
  type _type;
  math::color _clear_color;
  graphics::format _format;
  graphics::address_mode _address_mode;
  graphics::blend_state _blend_state;

}; // class attachment

struct attachment_description {
  attachment::type image_type;
  graphics::format format;
  graphics::blend_state blend_state;
}; // struct attachment_description

struct attachment_handle {

  std::uint32_t index{0xFFFFFFFF};

  [[nodiscard]] auto is_valid() const noexcept -> bool { 
    return index != 0xFFFFFFFF; 
  }

}; // struct attachment_handle

class buffer_resource {

public:

  enum class type {
    storage
  }; // enum class type

  buffer_resource(const utility::hashed_string& name, type type, VkDeviceSize size);

  auto name() const noexcept -> const utility::hashed_string&;

  auto buffer_type() const noexcept -> type;

  auto size() const noexcept -> VkDeviceSize;

private:

  utility::hashed_string _name;
  type _type;
  VkDeviceSize _size;

}; // class buffer_resource

struct buffer_resource_handle {

  std::uint32_t index{0xFFFFFFFF};

  [[nodiscard]] auto is_valid() const noexcept -> bool { 
    return index != 0xFFFFFFFF; 
  }

}; // struct buffer_resource

enum class buffer_access {
  read,
  write,
  read_write
}; // enum class buffer_access

struct buffer_usage {
  buffer_resource_handle buffer;
  buffer_access access;
}; // struct buffer_usage

struct pass_handle {
  
  std::uint32_t index{0xFFFFFFFF};

  [[nodiscard]] auto is_valid() const noexcept -> bool { 
    return index != 0xFFFFFFFF; 
  }

}; // struct pass_handle

class pass_node {

  friend class render_graph;

public:

  enum class type : std::uint8_t {
    graphics,
    compute
  }; // enum class type

  pass_node(type type, const utility::hashed_string& name, const viewport& viewport)
  : _type{type},
    _name{name},
    _viewport{viewport} { }

  auto reads(const attachment_handle attachment) -> void {
    _reads.emplace_back(attachment);
  }

  template<typename... Attachments>
  requires (sizeof...(Attachments) > 1u && (std::is_same_v<std::remove_cvref_t<Attachments>, attachment_handle> && ...))
  auto reads(Attachments&&... attachments) -> void {
    _reads.reserve(_reads.size() + sizeof...(Attachments));
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
    _dependencies.reserve(_dependencies.size() + sizeof...(Passes));
    (depends_on(passes), ...);
  }

  auto reads(const buffer_resource_handle buffer) -> void {
    _buffer_usages.push_back({buffer, buffer_access::read});
  }

  auto writes(const buffer_resource_handle buffer) -> void {
    _buffer_usages.push_back({buffer, buffer_access::write});
  }

  auto reads_writes(const buffer_resource_handle buffer) -> void {
    _buffer_usages.push_back({buffer, buffer_access::read_write});
  }

private:

  type _type;
  utility::hashed_string _name;
  std::vector<attachment_handle> _reads;
  std::vector<std::pair<attachment_handle, attachment_load_operation>> _writes;
  std::vector<buffer_usage> _buffer_usages;
  std::vector<pass_handle> _dependencies;
  viewport _viewport;
  render_area _render_area;

}; // struct pass_node

struct transition_instruction {
  attachment_handle attachment;
  VkImageLayout old_layout;
  VkImageLayout new_layout;
}; // struct transition_instruction

struct buffer_barrier_instruction {
  buffer_resource_handle buffer;
  VkPipelineStageFlags source_stages;
  VkPipelineStageFlags destination_stages;
  VkAccessFlags source_access;
  VkAccessFlags destination_access;
}; // struct buffer_barrier_instruction

struct graphics_pass_instruction {
  pass_handle pass;
  std::vector<std::pair<attachment_handle, attachment_load_operation>> attachments;
}; // struct graphics_pass_instruction

struct compute_pass_instruction {
  pass_handle pass;
}; // struct compute_pass_instruction

using instruction = std::variant<transition_instruction, buffer_barrier_instruction, graphics_pass_instruction, compute_pass_instruction>;

template<typename... Callables>
struct overload : Callables... {
  using Callables::operator()...;
};

template<typename... Callables>
overload(Callables...) -> overload<Callables...>;

struct attachment_state {
  VkImage image;
  VkImageView view;
  VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkFormat format;
  VkExtent2D extent;
  attachment::type type;
}; // struct attachment_state

struct buffer_state {
  VkBuffer buffer;
  bool has_writer{false};
  bool last_access_was_read{false};
}; // struct buffer_state

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

  template<typename... Args>
  requires (std::is_constructible_v<buffer_resource, Args...>)
  auto create_buffer_resource(Args&&... args) -> buffer_resource_handle;

  template<typename Callable>
  requires (std::is_invocable_r_v<pass_node, Callable, context&>)
  auto create_pass(Callable&& callable) -> pass_handle;

  auto find_attachment(const std::string& name) const -> const image2d&;

  auto attachment_descriptions(const pass_handle handle) const -> std::vector<attachment_description>;

  auto build() -> void;

  auto resize(const viewport::type flags) -> void;

  template<typename Callable>
  requires (std::is_invocable_v<Callable, const pass_handle&>)
  auto execute(command_buffer& command_buffer, const swapchain& swapchain, Callable&& callable) -> void {
    for (const auto& instruction : _instructions) {
      std::visit(overload{
        [&](const transition_instruction& instruction) { _execute_transition_instruction(command_buffer, swapchain, instruction); },
        [&](const compute_pass_instruction& instruction) { _execute_compute_pass_instruction(command_buffer, swapchain, instruction); },
        [&](const buffer_barrier_instruction& instruction) { _execute_buffer_barrier_instruction(command_buffer, swapchain, instruction); },
        [&](const graphics_pass_instruction& instruction) { _execute_graphics_pass_instruction(command_buffer, swapchain, instruction, std::forward<Callable>(callable)); }
      }, instruction);
    }
  }

private:

  auto _update_viewports() -> void;

  auto _clear_attachments(const viewport::type flags) -> void;  

  auto _create_attachments(const viewport::type flags, const pass_node& node) -> void;

  auto _build_color_attachment_info(const attachment& attachment, const attachment_state& state, const swapchain& swapchain, const attachment_load_operation load_op) -> VkRenderingAttachmentInfo;

  auto _build_depth_attachment_info(const attachment& attachment, const attachment_state& state, const attachment_load_operation load_op) -> VkRenderingAttachmentInfo;

  template<typename Callable>
  auto _execute_graphics_pass_instruction(command_buffer& command_buffer, const swapchain& swapchain, const graphics_pass_instruction& instruction, Callable&& callable) -> void;

  auto _execute_compute_pass_instruction(command_buffer& command_buffer, const swapchain& swapchain, const compute_pass_instruction& instruction) -> void;

  auto _execute_transition_instruction(command_buffer& command_buffer, const swapchain& swapchain, const transition_instruction& instruction) -> void;

  auto _execute_buffer_barrier_instruction(command_buffer& command_buffer, const swapchain& swapchain, const buffer_barrier_instruction& instruction) -> void;

  auto _buffer_access(const buffer_usage& usage) const -> std::pair<VkPipelineStageFlags, VkAccessFlags>;

  std::vector<attachment> _attachments;
  std::vector<attachment_state> _attachment_states;
  std::vector<image2d_handle> _color_images;
  std::vector<depth_image_handle> _depth_images;

  std::vector<buffer_resource> _buffer_resources;
  std::vector<buffer_state> _buffer_states;
  std::vector<storage_buffer_handle> _storage_buffers;

  std::vector<pass_node> _passes;

  std::unordered_map<utility::hashed_string, std::uint32_t> _image_by_name;

  std::vector<instruction> _instructions;

}; // class render_graph

} // namespace sbx::graphics

#include <libsbx/graphics/render_graph.ipp>

#endif // LIBSBX_GRAPHICS_RENDER_GRAPH_HPP_
