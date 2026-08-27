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

#include <libsbx/render/render_pass.hpp> // render_context, shadow_cascade_count

namespace sbx::render {

/**
 * @brief The stable, extent/handle-bearing state one @ref render_graph::compile needs. Rebuilt by
 * render_module from its own members and handed to compile() once at startup (the first real
 * _resize_targets) and again every time it actually resizes — extent-dependent image handles get
 * new resource_handle values every such resize, so a compiled instruction stream is only valid for
 * one "resource generation"; buffers and shadow maps, created once and never resized, stay valid
 * across every recompile.
 */
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
  std::array<graphics::image_handle, shadow_cascade_count> shadow_maps{};

  graphics::buffer_handle frame_buffer{};
  graphics::buffer_handle cluster_aabb_buffer{};
  graphics::buffer_handle cluster_range_buffer{};
  graphics::buffer_handle cluster_light_index_buffer{};
  graphics::buffer_handle cluster_counter_buffer{};
}; // struct graph_resources

/**
 * @brief One color attachment slot inside a @ref render_attachment_group. @ref stage_mask /
 * @ref access_mask describe this pass's own write to @p image (and, if set, @ref resolve_image) —
 * use the read+write combination when blending onto content a prior pass already wrote this frame,
 * or write-only for a fresh clear (matches e.g. skybox_pass's continuation write vs. opaque_pass's
 * fresh one).
 *
 * There is no load_op field: render_graph::compile derives it itself from the same per-resource
 * "have we touched this yet this compile" tracking it already uses for barriers — a resource's
 * first declared touch always clears (to @ref clear_value), every later touch always loads. This
 * is exactly the CLEAR/LOAD split every pass in this codebase already followed by hand, so it
 * needs no per-pass declaration; it also means clear_value only ever matters when this slot turns
 * out to be the first touch, and is a genuinely static, compile-time-only value — never patched
 * per frame.
 */
struct color_attachment_slot {
  graphics::image_handle image{};
  graphics::pipeline_stage stage_mask{graphics::pipeline_stage::color_attachment_output};
  graphics::access access_mask{graphics::access::color_attachment_write};
  graphics::attachment_store_op store_op{graphics::attachment_store_op::store};
  math::color clear_value{};
  graphics::image_handle resolve_image{}; // invalid => no MSAA resolve
}; // struct color_attachment_slot

/** @brief Same first-touch-clears/later-touch-loads rule as @ref color_attachment_slot applies to
 * @ref clear_value here too. */
struct depth_attachment_slot {
  graphics::image_handle image{};
  graphics::pipeline_stage stage_mask{graphics::pipeline_stage::early_fragment_tests | graphics::pipeline_stage::late_fragment_tests};
  graphics::access access_mask{graphics::access::depth_stencil_attachment_read};
  graphics::attachment_store_op store_op{graphics::attachment_store_op::dont_care};
  graphics::depth_stencil_clear_value clear_value{1.0f, 0u};
}; // struct depth_attachment_slot

/** @brief One "begin_rendering/end_rendering instance" — 0-2 color slots plus an optional depth
 * slot, all sized to @ref extent. A graphics_pass registers one of these per distinct render target
 * set it needs (most passes need exactly one; shadow_pass needs 4, one per cascade). */
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

/**
 * @brief One declaration recorded by a builder call, in call order. render_graph::compile replays
 * these in the exact order they were recorded — which is also the exact order the corresponding
 * hand-written barrier would have appeared in today's execute() — against a per-resource state
 * tracker, so the compiled barrier list reproduces today's hand-tuned behavior byte-for-byte.
 */
struct recorded_operation {
  operation_kind kind{};
  std::uint32_t group_index{0u};

  graphics::image_handle image{};
  graphics::image_handle resolve_image{}; // color_attachment only
  graphics::buffer_handle buffer{};

  graphics::pipeline_stage stage{graphics::pipeline_stage::none};
  graphics::access access{graphics::access::none};
  graphics::image_layout layout{graphics::image_layout::undefined};

  graphics::attachment_store_op store_op{graphics::attachment_store_op::store};
  math::color clear_color{};
  graphics::depth_stencil_clear_value clear_depth{1.0f, 0u};
}; // struct recorded_operation

/** @brief Shared recording surface behind both @ref graphics_pass_builder and @ref compute_pass_builder —
 * the buffer/image read/write vocabulary is identical for both; only attachment groups are
 * graphics-only. Not part of the pass-authoring API on its own. */
class resource_builder {

public:

  auto reads_image(graphics::image_handle image, graphics::pipeline_stage stage, graphics::access access, graphics::image_layout layout, std::uint32_t group_index = 0u) -> void;

  auto writes_image(graphics::image_handle image, graphics::pipeline_stage stage, graphics::access access, graphics::image_layout layout, std::uint32_t group_index = 0u) -> void;

  auto reads_buffer(graphics::buffer_handle buffer, graphics::pipeline_stage stage, graphics::access access, std::uint32_t group_index = 0u) -> void;

  auto writes_buffer(graphics::buffer_handle buffer, graphics::pipeline_stage stage, graphics::access access, std::uint32_t group_index = 0u) -> void;

  /** @brief "I already fully synchronized this myself, via a hand-written barrier inside my own
   * execute()." Updates the compiler's tracked state for @p buffer to (stage, access,
   * last_was_write=false) — as if already read-synced — without emitting anything. Used by passes
   * with pass-local multi-dispatch barrier chains (light_culling_pass, particle_simulate_pass) to
   * publish their overall exit guarantee, so later declared reads of the same buffer elide cleanly
   * instead of emitting a redundant barrier the pass already made unnecessary. */
  auto declares_buffer_ready(graphics::buffer_handle buffer, graphics::pipeline_stage stage, graphics::access access) -> void;

  /** @brief Same idea as @ref declares_buffer_ready, for an image. */
  auto declares_image_ready(graphics::image_handle image, graphics::pipeline_stage stage, graphics::access access, graphics::image_layout layout, std::uint32_t group_index = 0u) -> void;

  [[nodiscard]] auto ops() const noexcept -> const std::vector<recorded_operation>& {
    return _operations;
  }

protected:

  std::vector<recorded_operation> _operations{};

}; // class resource_builder

} // namespace detail

/** @brief Setup-time interface a @ref compute_pass declares its (whole-pass, ungrouped) resource
 * usage through. Deliberately narrower than @ref graphics_pass_builder — no attachments, since a compute
 * pass has exactly one implicit, non-rendering group. */
class compute_pass_builder final : public detail::resource_builder, public utility::noncopyable {

public:

  compute_pass_builder() = default;

}; // class compute_pass_builder

/**
 * @brief Setup-time interface a @ref graphics_pass declares its resource usage through. Every call
 * feeds the same linear per-resource state tracker render_graph::compile walks the fixed pass list
 * with; the compiler derives old_layout/src_stage/src_access from tracked state and elides
 * read-after-read barriers automatically.
 */
class graphics_pass_builder final : public detail::resource_builder, public utility::noncopyable {

public:

  graphics_pass_builder() = default;

  /** @brief Registers one attachment group ("one begin_rendering/end_rendering instance"). Returns
   * a stable group_index matching what graphics_pass::execute/should_execute will be called with,
   * in registration order. */
  auto add_group(const render_attachment_group& group) -> std::uint32_t;

  /** @brief An EXIT-side transition for one of @p group_index's own attachment images, recorded
   * after that group's end_rendering() (Vulkan forbids transitioning an attachment's layout while
   * still inside its own render scope). Needed when nothing else in the fixed pass list declares a
   * consuming read of the image (e.g. shadow_pass's per-cascade hand-off to a bindless sample no
   * consumer explicitly declares) — the producer must self-transition. */
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

/**
 * @brief Internal base every graph-managed pass shares, giving render_graph a single uniform
 * dispatch surface (no RTTI/casts) while graphics_pass/compute_pass expose distinct, ergonomic
 * public shapes to pass authors — the "graphics vs compute distinct in the type system" split.
 */
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

/**
 * @brief A dynamic-rendering pass: declares one or more attachment groups plus any non-attachment
 * accesses at setup time; frame-varying execute() only binds pipelines, writes push constants, and
 * draws — no barriers, no VkRenderingInfo construction.
 */
class graphics_pass : public graph_pass {

public:

  virtual auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void = 0;

  virtual auto execute(render_context& context, std::uint32_t group) -> void = 0;

  [[nodiscard]] virtual auto should_execute(const render_context& context, std::uint32_t group) const -> bool {
    (void)context;
    (void)group;
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

/**
 * @brief A compute-only pass: no attachments, exactly one implicit group. Pass-local dispatch-to-
 * dispatch barrier chains stay hand-written inside execute() — only the pass's overall entry
 * requirements and exit guarantees are declared to the graph.
 */
class compute_pass : public graph_pass {

public:

  virtual auto declare(compute_pass_builder& builder, const graph_resources& resources) -> void = 0;

  virtual auto execute(render_context& context) -> void = 0;

  [[nodiscard]] virtual auto should_execute(const render_context& context) const -> bool {
    (void)context;
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

/**
 * @brief Owns the fixed pass list and its compiled instruction stream. Compiles once (see
 * render_module::_resize_targets) and again only when render targets are actually recreated on
 * resize. Per-frame work (@ref execute) is limited to walking the compiled list and issuing
 * precomputed barriers / begin+end_rendering / each pass's frame-varying execute() — no
 * recomputation, no allocation.
 */
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

  /** @brief (Re)compiles the fixed instruction list against @p resources. Idempotent — safe to call
   * repeatedly; only actually needed once at startup and once per real resize. */
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
