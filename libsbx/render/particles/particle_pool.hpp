// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PARTICLE_POOL_HPP_
#define LIBSBX_RENDER_PARTICLE_POOL_HPP_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/resources/buffer.hpp>

#include <libsbx/render/particles/particle_data.hpp>

namespace sbx::render {

/**
 * @brief One shared GPU particle pool for the whole scene, serving every emitter of one blend
 * mode — see render_module (owns pool[0] = additive, pool[1] = alpha blend) and the plan at
 * /home/kaj/.claude/plans/i-want-to-implement-memoized-journal.md for why the split is by blend
 * mode rather than one pool per emitter instance.
 *
 * Buffers fall into two groups:
 *  - `particles`/`alive_list[2]` are pure GPU-to-GPU state: never touched from the CPU after
 *    construction (no init needed — a slot is only ever read once emit.slang has written it, and
 *    the very first frame's alive_count[read] is 0 so nothing reads alive_list before it's been
 *    written), so they're device_local.
 *  - `dead_list`/`counters` need a one-time CPU-side init (dead_list = 0..max_particles-1,
 *    counters.dead_count = max_particles) and are thereafter exclusively GPU read-modify-written
 *    via atomics; `emitter_instances` is rewritten wholesale from the CPU every frame. All three
 *    are host_write (persistently mapped, HOST_COHERENT) — same choice render_module already
 *    makes for _light_buffer/_transform_buffer. This trades a little GPU-side bandwidth (host
 *    visible memory is typically BAR-limited on discrete GPUs) for avoiding the upload_context
 *    staging path entirely, which would otherwise race the very first
 *    particle_simulate_pass submission (that submission runs before frame_context::begin_frame,
 *    i.e. before the main command buffer's upload_context::flush this same frame — see
 *    particle_simulate_pass.hpp). At the pool sizes M1 targets (max_particles in the thousands,
 *    so tens of KB per buffer) this is a non-issue; revisit only if profiling says otherwise.
 *
 * `dispatch_args`/`draw_args` hold one VkDispatchIndirectCommand / VkDrawIndirectCommand each,
 * rewritten by build_dispatch_args.slang / prepare_indirect_draw.slang every frame — device_local,
 * no CPU init needed.
 */
class particle_pool : public utility::noncopyable {

public:

  struct create_info {
    std::uint32_t max_particles{4096u};
    std::uint32_t max_emitter_instances{64u};
    std::string name{"Particle Pool"};
  }; // struct create_info

  explicit particle_pool(const create_info& create_info);

  [[nodiscard]] auto max_particles() const noexcept -> std::uint32_t {
    return _max_particles;
  }

  [[nodiscard]] auto max_emitter_instances() const noexcept -> std::uint32_t {
    return _max_emitter_instances;
  }

  [[nodiscard]] auto particles() const noexcept -> graphics::buffer_handle {
    return _particles;
  }

  [[nodiscard]] auto dead_list() const noexcept -> graphics::buffer_handle {
    return _dead_list;
  }

  [[nodiscard]] auto alive_list(std::uint32_t index) const noexcept -> graphics::buffer_handle {
    return _alive_list[index];
  }

  [[nodiscard]] auto counters() const noexcept -> graphics::buffer_handle {
    return _counters;
  }

  [[nodiscard]] auto dispatch_args() const noexcept -> graphics::buffer_handle {
    return _dispatch_args;
  }

  [[nodiscard]] auto draw_args() const noexcept -> graphics::buffer_handle {
    return _draw_args;
  }

  [[nodiscard]] auto emitter_instances() const noexcept -> graphics::buffer_handle {
    return _emitter_instances;
  }

  [[nodiscard]] auto particles_address() const noexcept -> graphics::buffer::address_type {
    return _particles_address;
  }

  [[nodiscard]] auto dead_list_address() const noexcept -> graphics::buffer::address_type {
    return _dead_list_address;
  }

  [[nodiscard]] auto alive_list_address(std::uint32_t index) const noexcept -> graphics::buffer::address_type {
    return _alive_list_addresses[index];
  }

  [[nodiscard]] auto counters_address() const noexcept -> graphics::buffer::address_type {
    return _counters_address;
  }

  [[nodiscard]] auto dispatch_args_address() const noexcept -> graphics::buffer::address_type {
    return _dispatch_args_address;
  }

  [[nodiscard]] auto draw_args_address() const noexcept -> graphics::buffer::address_type {
    return _draw_args_address;
  }

  [[nodiscard]] auto emitter_instances_address() const noexcept -> graphics::buffer::address_type {
    return _emitter_instances_address;
  }

  /**
   * @brief Rewrites one emitter instance slot. Called every frame from render_module for every
   * active emitter instance using this pool — same "wholesale CPU rewrite" pattern as
   * render_module's _light_buffer/_transform_buffer.
   */
  auto write_emitter_instance(std::uint32_t slot, const emitter_instance& data) -> void;

  /**
   * @brief Claims a free emitter_instances slot, or std::nullopt if the pool is exhausted (logged
   * once, not every frame, so a full pool fails loud without spamming). Call once per emitter
   * instance the first frame it becomes active; hold the returned slot and call @ref keep_alive
   * every frame it stays active instead of claiming again.
   */
  [[nodiscard]] auto claim_slot() -> std::optional<std::uint32_t>;

  /**
   * @brief Marks `slot` as still in use this frame and refreshes the lifetime @ref tick will drain
   * it for once it stops being claimed. Call once per frame for every slot an emitter instance is
   * still actively spawning from.
   */
  auto keep_alive(std::uint32_t slot, std::float_t lifetime_max) -> void;

  /**
   * @brief Call once per frame, after every @ref keep_alive for that frame. A slot claimed last
   * frame but not this frame just lost its emitter (stopped, or the owning entity/component was
   * destroyed) and starts draining for its last known lifetime_max seconds — long enough that any
   * particle it spawned has aged out — before it's recycled onto the free-list. Slots already
   * draining have their timer decremented and are recycled once it reaches zero.
   *
   * The delay matters: particles read `emitters[emitter_slot]` every frame (in both simulate.slang
   * and draw.slang), so handing the slot to a new owner while old particles are still alive would
   * visibly corrupt them mid-flight.
   */
  auto tick(std::float_t delta_time) -> void;

private:

  std::uint32_t _max_particles;
  std::uint32_t _max_emitter_instances;
  std::string _name;

  graphics::buffer_handle _particles{};
  graphics::buffer_handle _dead_list{};
  std::array<graphics::buffer_handle, 2u> _alive_list{};
  graphics::buffer_handle _counters{};
  graphics::buffer_handle _dispatch_args{};
  graphics::buffer_handle _draw_args{};
  graphics::buffer_handle _emitter_instances{};

  graphics::buffer::address_type _particles_address{};
  graphics::buffer::address_type _dead_list_address{};
  std::array<graphics::buffer::address_type, 2u> _alive_list_addresses{};
  graphics::buffer::address_type _counters_address{};
  graphics::buffer::address_type _dispatch_args_address{};
  graphics::buffer::address_type _draw_args_address{};
  graphics::buffer::address_type _emitter_instances_address{};

  // Emitter-instance slot allocator — see claim_slot/keep_alive/tick. Sized to
  // _max_emitter_instances at construction.
  std::vector<std::uint32_t> _free_list{};
  std::vector<std::float_t> _drain_timer{};
  std::vector<std::float_t> _lifetime_max{};
  std::vector<bool> _claimed_this_frame{};
  std::vector<bool> _claimed_last_frame{};
  bool _exhaustion_logged{false};

}; // class particle_pool

} // namespace sbx::render

#endif // LIBSBX_RENDER_PARTICLE_POOL_HPP_
