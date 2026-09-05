// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PASSES_BLOOM_PASS_HPP_
#define LIBSBX_RENDER_PASSES_BLOOM_PASS_HPP_

#include <cstdint>
#include <deque>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief Mip-chain bloom (Call of Duty/Sledgehammer SIGGRAPH 2014 style): threshold-prefilter the
 * HDR scene color into a half-resolution mip0, box-downsample into a mip chain, tent-upsample back
 * with additive combine, and hand the half-resolution result to tonemap_pass to fold in before ACES.
 *
 * Owns two private mip-chain images (graph_resources::bloom_downsample/bloom_upsample) that no
 * other pass touches. Two subtleties fall out of that:
 *
 *  - The render graph's automatic barrier tracking only understands single-mip images (see
 *    render_graph.hpp's operation model); a multi-mip chain must be transitioned by hand inside
 *    execute(), with only the final promised state told to the compiler via declares_image_ready.
 *
 *  - bindless_table's sampled-image slots are permanently declared at VK_IMAGE_LAYOUT_SHADER_
 *    READ_ONLY_OPTIMAL, so a mip that is written as a storage image and later read as a sampled
 *    texture needs its own layout flip (general -> read-only) right after it's produced, not one
 *    transition at the end -- see the per-mip transitions in execute().
 *
 * Never skips its own execution via should_execute(): the chain images would otherwise never leave
 * VK_IMAGE_LAYOUT_UNDEFINED while tonemap_pass's declared read assumes shader_read_only_optimal.
 * When bloom_enabled is off, execute() still performs the layout dance (cheap, no dispatches) so
 * that promise keeps holding; tonemap_pass zeroes the contribution instead (see its push constant).
 *
 * Runs after transparent_resolve_pass/particle_pass (so it sees the fully composited HDR scene) and
 * before tonemap_pass.
 */
class bloom_pass final : public compute_pass {

public:

  inline static constexpr auto max_mip_count = std::uint32_t{6u};

  /** @brief mip0's resolution for a given color target extent -- half res, floored at 1x1. */
  [[nodiscard]] static auto extent_for(const math::vector2u& color_extent) noexcept -> math::vector2u;

  /** @brief The mip count both chain images should be created with for a given color target extent. */
  [[nodiscard]] static auto mip_count_for(const math::vector2u& color_extent) noexcept -> std::uint32_t;

  bloom_pass();

  ~bloom_pass() override;

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Bloom";
  }

  auto declare(compute_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context) -> void override;

private:

  // One mip level's private views/indices, valid for as long as the owning image handle is.
  struct mip_view {
    VkImageView view{};
    std::uint32_t sampled_index{0xFFFFFFFFu};
    std::uint32_t storage_index{0xFFFFFFFFu};
  }; // struct mip_view

  // A previous chain's views, kept alive until frame_index has caught up to a value the GPU has
  // certainly finished by -- same contract resource_pool::retire/collect uses, hand-rolled here
  // because per-mip VkImageViews/bindless indices aren't buffers or images resource_registry pools.
  struct retired_chain {
    std::uint64_t frame_index;
    std::vector<mip_view> views;
  }; // struct retired_chain

  auto _rebuild_views(const graph_resources& resources) -> void;

  auto _drain_retired() -> void;

  auto _destroy(const mip_view& mip) -> void;

  memory::observer_ptr<graphics::compute_pipeline> _prefilter_pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _downsample_pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _upsample_pipeline{};

  graphics::image_handle _cached_downsample{};
  graphics::image_handle _cached_upsample{};

  std::uint32_t _mip_count{0u};

  std::vector<mip_view> _downsample_mips{};
  std::vector<mip_view> _upsample_mips{};

  std::deque<retired_chain> _retired{};

}; // class bloom_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_PASSES_BLOOM_PASS_HPP_
