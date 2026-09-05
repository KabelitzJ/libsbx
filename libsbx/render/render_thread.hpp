// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_THREAD_HPP_
#define LIBSBX_RENDER_RENDER_THREAD_HPP_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/threading_policy.hpp>
#include <libsbx/core/delegate.hpp>

namespace sbx::render {

/**
 * @brief Drives scene_renderer_module's frame handoff via an explicit idle/busy/kick state machine.
 *
 * `work` (given at construction) is whatever the owner wants run each kick; the threading policy
 * decides whether `kick()` wakes a background worker or runs it inline on the calling thread.
 */
class render_thread final : public utility::noncopyable {

public:

  enum class state : std::uint8_t {
    idle,
    busy,
    kick
  }; // enum class state

  render_thread(const core::threading_policy policy, core::delegate<void()> work);

  ~render_thread();

  /** @brief Spawns the worker thread. No-op for anything other than multi_threaded. */
  auto run() -> void;

  [[nodiscard]] auto is_running() const noexcept -> bool {
    return _is_running;
  }

  /** @brief Stops accepting/producing frames and joins the worker thread, if any. Idempotent. */
  auto terminate() -> void;

  auto wait(state wait_for) -> void;

  auto wait_and_set(state wait_for, state set_to) -> void;

  auto set(state set_to) -> void;

  auto next_frame() -> void;

  /** @brief Waits for the previously kicked frame to fully finish. */
  auto block_until_render_complete() -> void;

  /**
   * @brief multi_threaded: wakes the worker to run `work`. Anything else: runs `work()`
   * immediately, inline, on the calling thread, then returns — already idle, nothing async ever
   * happens.
   */
  auto kick() -> void;

  /** @brief Convenience: block_until_render_complete() + next_frame() + kick(). */
  auto pump() -> void;

  [[nodiscard]] static auto is_current_thread_render_thread() noexcept -> bool;

private:

  auto _worker_loop() -> void;

  core::threading_policy _policy;
  core::delegate<void()> _work;

  std::thread _thread{};

  std::mutex _mutex{};
  std::condition_variable _condition{};
  state _state{state::idle};
  bool _is_running{false};

  std::atomic<std::uint32_t> _app_thread_frame{0u};

}; // class render_thread

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_THREAD_HPP_
