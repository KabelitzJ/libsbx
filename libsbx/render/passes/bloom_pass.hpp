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
 * @brief Mip-chain bloom (Sledgehammer, SIGGRAPH 2014): threshold into a half-resolution mip 0, box downsample, tent upsample additively, and hand the result to tonemap_pass.
 *
 * The render graph only tracks single-mip images, so the chain is transitioned by hand and only the final state is declared ready.
 * Bindless sampled slots are always read-only optimal, so each mip flips from general to read-only right after it is written.
 * It never skips execution, since tonemap_pass's read assumes read-only layouts; with bloom off it only transitions and tonemap_pass zeroes the contribution.
 */
class bloom_pass final : public compute_pass {

public:

  inline static constexpr auto max_mip_count = std::uint32_t{6u};

  /**
   * @brief Mip 0's resolution: half the color extent, at least 1x1.
   *
   * @param color_extent The color target extent.
   *
   * @return The extent.
   */
  [[nodiscard]] static auto extent_for(const math::vector2u& color_extent) noexcept -> math::vector2u;

  /**
   * @brief The mip count both chain images use.
   *
   * @param color_extent The color target extent.
   *
   * @return The mip count.
   */
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

  // A previous chain's views, kept until the GPU has finished with them; per-mip views and bindless indices aren't pooled by resource_registry.
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
