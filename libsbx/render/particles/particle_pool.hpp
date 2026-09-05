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
 * @brief One shared GPU particle pool for the whole scene, serving every emitter of one blend mode.
 *
 * `particles`/`alive_list` are device_local. `dead_list`/`counters`/`emitter_instances` are host_write
 * (persistently mapped) to avoid the upload_context staging path, which would race the first
 * @ref particle_simulate_pass submission. `dispatch_args`/`draw_args` are device_local, rewritten
 * every frame by build_dispatch_args.slang/prepare_indirect_draw.slang.
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
   * @brief Overwrites one emitter instance slot with a full record; call every frame while the slot is active.
   */
  auto write_emitter_instance(std::uint32_t slot, const emitter_instance& data) -> void;

  /**
   * @brief Claims a free emitter_instances slot, or std::nullopt if the pool is exhausted.
   *
   * Call once per instance on its first active frame; reuse the slot via @ref keep_alive afterward
   * instead of claiming again. Exhaustion is logged once, not every frame.
   */
  [[nodiscard]] auto claim_slot() -> std::optional<std::uint32_t>;

  /**
   * @brief Marks `slot` as in use this frame and refreshes the lifetime @ref tick drains it over once released.
   *
   * Call once per frame for every slot still actively spawning.
   */
  auto keep_alive(std::uint32_t slot, std::float_t lifetime_max) -> void;

  /**
   * @brief Recycles emitter-instance slots that stopped being claimed this frame.
   *
   * Call after every @ref keep_alive each frame. An unclaimed slot drains for its lifetime_max
   * seconds before reuse — recycling sooner would let simulate.slang/draw.slang read a new owner's
   * `emitters[emitter_slot]` for still in-flight old particles, corrupting them.
   */
  auto tick(std::float_t delta_time) -> void;

  /**
   * @brief Immediately discards every live particle and emitter-instance slot, resetting the pool to its just-constructed state.
   *
   * Used by the editor's Stop button; unlike @ref tick, this is an instant reset with no drain.
   */
  auto clear() -> void;

private:

  // Writes dead_list = 0..max_particles-1 and counters = {dead_count: max_particles, alive_count:
  // {0, 0}} — the pool's "nothing alive yet" state, shared by the constructor and clear().
  auto _write_initial_state() -> void;

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
