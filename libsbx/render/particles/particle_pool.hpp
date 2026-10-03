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

#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/resources/buffer.hpp>

#include <libsbx/render/particles/particle_data.hpp>

namespace sbx::render {

/**
 * @brief One scene-wide GPU particle pool serving every emitter of one blend mode.
 *
 * Host-written buffers are persistently mapped instead of going through upload_context, whose staging would race the first simulate submission.
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

  [[nodiscard]] auto emitter_instances(std::uint32_t frame_slot) const noexcept -> graphics::buffer_handle {
    return _emitter_instances[frame_slot];
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

  /**
   * @brief The emitter instance buffer for a frame slot.
   *
   * @param frame_slot The current frame-in-flight index.
   *
   * @return The buffer's address.
   */
  [[nodiscard]] auto emitter_instances_address(std::uint32_t frame_slot) const noexcept -> graphics::buffer::address_type {
    return _emitter_instances_addresses[frame_slot];
  }

  /**
   * @brief Overwrites one emitter instance record; call every frame while the slot is active.
   *
   * @param frame_slot The current frame-in-flight index.
   * @param slot The emitter instance slot.
   * @param data The record.
   */
  auto write_emitter_instance(std::uint32_t frame_slot, std::uint32_t slot, const emitter_instance& data) -> void;

  /**
   * @brief Claims a free emitter instance slot; call once on an instance's first active frame, then keep_alive it.
   *
   * @return The slot, or nullopt if the pool is exhausted (logged once).
   */
  [[nodiscard]] auto claim_slot() -> std::optional<std::uint32_t>;

  /**
   * @brief Marks @p slot in use this frame and refreshes how long it drains after release. Call every frame while spawning.
   *
   * @param slot The slot.
   * @param lifetime_max The longest particle lifetime the slot can have in flight.
   */
  auto keep_alive(std::uint32_t slot, std::float_t lifetime_max) -> void;

  /**
   * @brief Recycles slots not kept alive this frame, after a drain of lifetime_max, so in-flight particles never read a new owner's emitter data.
   *
   * @param delta_time The time step.
   */
  auto tick(std::float_t delta_time) -> void;

  /** @brief Discards every live particle and slot immediately, e.g. when play mode stops. */
  auto clear() -> void;

private:

  // The empty state: every particle dead, nothing alive. Shared by the constructor and clear().
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

  // One buffer per frame in flight: CPU writes here while an earlier frame's compute may still read it, and GPU barriers don't order host writes.
  std::array<graphics::buffer_handle, graphics::swapchain::max_frames_in_flight> _emitter_instances{};

  graphics::buffer::address_type _particles_address{};
  graphics::buffer::address_type _dead_list_address{};
  std::array<graphics::buffer::address_type, 2u> _alive_list_addresses{};
  graphics::buffer::address_type _counters_address{};
  graphics::buffer::address_type _dispatch_args_address{};
  graphics::buffer::address_type _draw_args_address{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _emitter_instances_addresses{};

  std::vector<std::uint32_t> _free_list{};
  std::vector<std::float_t> _drain_timer{};
  std::vector<std::float_t> _lifetime_max{};
  std::vector<bool> _claimed_this_frame{};
  std::vector<bool> _claimed_last_frame{};
  bool _exhaustion_logged{false};

}; // class particle_pool

} // namespace sbx::render

#endif // LIBSBX_RENDER_PARTICLE_POOL_HPP_
