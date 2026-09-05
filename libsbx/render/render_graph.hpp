// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_GRAPH_HPP_
#define LIBSBX_RENDER_RENDER_GRAPH_HPP_

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <vulkan/vulkan.h> // compiled_group only — see its comment; the pass-authoring API above it is Vulkan-free

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/render/render_pass.hpp>

namespace sbx::render {

struct graph_resources {
  math::vector2u extent{};

  graphics::image_handle depth{};
  graphics::image_handle color{};
  graphics::image_handle color_msaa{};
  graphics::image_handle final_image{};
  graphics::image_handle accumulator{};
  graphics::image_handle accumulator_msaa{};
  graphics::image_handle revealage{};
  graphics::image_handle revealage_msaa{};
  graphics::image_handle bloom_downsample{};
  graphics::image_handle bloom_upsample{};
  std::array<graphics::image_handle, shadow_cascade_count> shadow_maps{};

  graphics::buffer_handle frame_buffer{};
  graphics::buffer_handle cluster_aabb_buffer{};
  graphics::buffer_handle cluster_range_buffer{};
  graphics::buffer_handle cluster_light_index_buffer{};
  graphics::buffer_handle cluster_counter_buffer{};
}; // struct graph_resources

struct color_attachment_slot {
  graphics::image_handle image{};
  graphics::pipeline_stage stage_mask{graphics::pipeline_stage::color_attachment_output};
  graphics::access access_mask{graphics::access::color_attachment_write};
  graphics::attachment_store_op store_op{graphics::attachment_store_op::store};
  math::color clear_value{};
  graphics::image_handle resolve_image{}; // invalid => no MSAA resolve
}; // struct color_attachment_slot

struct depth_attachment_slot {
  graphics::image_handle image{};
  graphics::pipeline_stage stage_mask{graphics::pipeline_stage::early_fragment_tests | graphics::pipeline_stage::late_fragment_tests};
  graphics::access access_mask{graphics::access::depth_stencil_attachment_read};
  graphics::attachment_store_op store_op{graphics::attachment_store_op::dont_care};
  graphics::depth_stencil_clear_value clear_value{1.0f, 0u};
}; // struct depth_attachment_slot

struct render_attachment_group {
  math::vector2u extent{};
  std::vector<color_attachment_slot> colors{};
  std::optional<depth_attachment_slot> depth{};
}; // struct render_attachment_group

namespace detail {

enum class operation_kind : std::uint8_t {
  color_attachment,
  depth_attachment,
  read_image,
  write_image,
  read_buffer,
  write_buffer,
  declare_image_ready,
  declare_buffer_ready,
  transition_after
}; // enum class operation_kind

struct recorded_operation {
  operation_kind kind{};
  std::uint32_t group_index{0u};

  graphics::image_handle image{};
  graphics::image_handle resolve_image{};
  graphics::buffer_handle buffer{};

  graphics::pipeline_stage stage{graphics::pipeline_stage::none};
  graphics::access access{graphics::access::none};
  graphics::image_layout layout{graphics::image_layout::undefined};

  graphics::attachment_store_op store_op{graphics::attachment_store_op::store};
  math::color clear_color{};
  graphics::depth_stencil_clear_value clear_depth{1.0f, 0u};
}; // struct recorded_operation

class resource_builder {

public:

  auto reads_image(graphics::image_handle image, graphics::pipeline_stage stage, graphics::access access, graphics::image_layout layout, std::uint32_t group_index = 0u) -> void;

  auto writes_image(graphics::image_handle image, graphics::pipeline_stage stage, graphics::access access, graphics::image_layout layout, std::uint32_t group_index = 0u) -> void;

  auto reads_buffer(graphics::buffer_handle buffer, graphics::pipeline_stage stage, graphics::access access, std::uint32_t group_index = 0u) -> void;

  auto writes_buffer(graphics::buffer_handle buffer, graphics::pipeline_stage stage, graphics::access access, std::uint32_t group_index = 0u) -> void;

  auto declares_buffer_ready(graphics::buffer_handle buffer, graphics::pipeline_stage stage, graphics::access access) -> void;

  auto declares_image_ready(graphics::image_handle image, graphics::pipeline_stage stage, graphics::access access, graphics::image_layout layout, std::uint32_t group_index = 0u) -> void;

  [[nodiscard]] auto operations() const noexcept -> const std::vector<recorded_operation>& {
    return _operations;
  }

protected:

  std::vector<recorded_operation> _operations{};

}; // class resource_builder

} // namespace detail

class compute_pass_builder final : public detail::resource_builder, public utility::noncopyable {

public:

  compute_pass_builder() = default;

}; // class compute_pass_builder

class graphics_pass_builder final : public detail::resource_builder, public utility::noncopyable {

public:

  graphics_pass_builder() = default;

  auto add_group(const render_attachment_group& group) -> std::uint32_t;

  auto transitions_after(std::uint32_t group_index, graphics::image_handle image, graphics::pipeline_stage dst_stage, graphics::access dst_access, graphics::image_layout dst_layout) -> void;

  [[nodiscard]] auto group_count() const noexcept -> std::uint32_t {
    return _group_count;
  }

  [[nodiscard]] auto group_extent(std::uint32_t group_index) const -> math::vector2u {
    return _group_extents[group_index];
  }

private:

  std::uint32_t _group_count{0u};
  std::vector<math::vector2u> _group_extents{};

}; // class graphics_pass_builder

enum class pass_kind : std::uint8_t {
  graphics,
  compute
}; // enum class pass_kind

class graph_pass : public utility::noncopyable {

public:

  virtual ~graph_pass() = default;

  [[nodiscard]] virtual auto name() const -> std::string_view = 0;

  [[nodiscard]] virtual auto kind() const -> pass_kind = 0;

protected:

  friend class render_graph;

  virtual auto declare_resources(graphics_pass_builder* graphics_builder, compute_pass_builder* compute_builder, const graph_resources& resources) -> void = 0;

  [[nodiscard]] virtual auto is_group_enabled(const render_context& context, std::uint32_t group) const -> bool = 0;

  virtual auto execute_group(render_context& context, std::uint32_t group) -> void = 0;

}; // class graph_pass

class graphics_pass : public graph_pass {

public:

  virtual auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void = 0;

  virtual auto execute(render_context& context, std::uint32_t group) -> void = 0;

  [[nodiscard]] virtual auto should_execute([[maybe_unused]] const render_context& context, [[maybe_unused]] std::uint32_t group) const -> bool {
    return true;
  }

private:

  [[nodiscard]] auto kind() const -> pass_kind final {
    return pass_kind::graphics;
  }

  auto declare_resources(graphics_pass_builder* graphics_builder, compute_pass_builder*, const graph_resources& resources) -> void final {
    declare(*graphics_builder, resources);
  }

  [[nodiscard]] auto is_group_enabled(const render_context& context, std::uint32_t group) const -> bool final {
    return should_execute(context, group);
  }

  auto execute_group(render_context& context, std::uint32_t group) -> void final {
    execute(context, group);
  }

}; // class graphics_pass

class compute_pass : public graph_pass {

public:

  virtual auto declare(compute_pass_builder& builder, const graph_resources& resources) -> void = 0;

  virtual auto execute(render_context& context) -> void = 0;

  [[nodiscard]] virtual auto should_execute([[maybe_unused]] const render_context& context) const -> bool {
    return true;
  }

private:

  [[nodiscard]] auto kind() const -> pass_kind final {
    return pass_kind::compute;
  }

  auto declare_resources(graphics_pass_builder*, compute_pass_builder* compute_builder, const graph_resources& resources) -> void final {
    declare(*compute_builder, resources);
  }

  [[nodiscard]] auto is_group_enabled(const render_context& context, std::uint32_t) const -> bool final {
    return should_execute(context);
  }

  auto execute_group(render_context& context, std::uint32_t) -> void final {
    execute(context);
  }

}; // class compute_pass

class render_graph final : public utility::noncopyable {

public:

  render_graph() = default;

  template<typename Pass, typename... Args>
  auto add_pass(Args&&... args) -> Pass& {
    static_assert(std::is_base_of_v<graph_pass, Pass>, "Pass must derive from graphics_pass or compute_pass.");

    auto owned = std::make_unique<Pass>(std::forward<Args>(args)...);
    auto& ref = *owned;

    _passes.push_back(std::move(owned));

    return ref;
  }

  auto compile(const graph_resources& resources) -> void;

  /** @brief Walks the compiled instruction list for one frame. */
  auto execute(render_context& context) -> void;

private:

  struct compiled_group {
    bool has_rendering{false};
    math::vector2u extent{};

    std::array<VkRenderingAttachmentInfo, 2u> color_attachments{};
    std::uint32_t color_attachment_count{0u};

    VkRenderingAttachmentInfo depth_attachment{};
    bool has_depth{false};

    std::vector<graphics::command_buffer::image_transition_data> entry_image_barriers{};
    std::vector<VkMemoryBarrier2> entry_buffer_barriers{};
    std::vector<graphics::command_buffer::image_transition_data> exit_image_barriers{};
  }; // struct compiled_group

  struct compiled_entry {
    memory::observer_ptr<graph_pass> pass{};
    std::vector<compiled_group> groups{};
  }; // struct compiled_entry

  std::vector<std::unique_ptr<graph_pass>> _passes{};
  std::vector<compiled_entry> _compiled{};

}; // class render_graph

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_GRAPH_HPP_
