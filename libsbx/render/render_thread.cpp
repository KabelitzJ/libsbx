// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/render_thread.hpp>

#include <utility>

#include <libsbx/utility/profiler.hpp>

namespace sbx::render {

namespace {

auto _render_thread_id = std::atomic<std::thread::id>{};

} // namespace

render_thread::render_thread(core::threading_policy policy, core::delegate<void()> work)
: _policy{policy},
  _work{std::move(work)} { }

render_thread::~render_thread() {
  terminate();
}

auto render_thread::run() -> void {
  _is_running = true;

  if (_policy != core::threading_policy::multi_threaded) {
    _render_thread_id.store(std::this_thread::get_id(), std::memory_order_relaxed);

    return;
  }

  _thread = std::thread{[this]() { _worker_loop(); }};

  _render_thread_id.store(_thread.get_id(), std::memory_order_relaxed);
}

auto render_thread::terminate() -> void {
  if (!_is_running) {
    return;
  }

  if (_policy != core::threading_policy::multi_threaded) {
    _is_running = false;

    return;
  }

  {
    auto lock = std::unique_lock{_mutex};
    _is_running = false;
  }

  _condition.notify_all();

  if (_thread.joinable()) {
    _thread.join();
  }
}

auto render_thread::wait(state wait_for) -> void {
  if (_policy != core::threading_policy::multi_threaded) {
    return;
  }

  auto lock = std::unique_lock{_mutex};

  _condition.wait(lock, [this, wait_for]() { return _state == wait_for || !_is_running; });
}

auto render_thread::wait_and_set(state wait_for, state set_to) -> void {
  if (_policy != core::threading_policy::multi_threaded) {
    _state = set_to;

    return;
  }

  auto lock = std::unique_lock{_mutex};

  _condition.wait(lock, [this, wait_for]() { return _state == wait_for || !_is_running; });

  _state = set_to;

  lock.unlock();

  _condition.notify_all();
}

auto render_thread::set(state set_to) -> void {
  if (_policy != core::threading_policy::multi_threaded) {
    _state = set_to;

    return;
  }

  {
    auto lock = std::unique_lock{_mutex};
    _state = set_to;
  }

  _condition.notify_all();
}

auto render_thread::next_frame() -> void {
  _app_thread_frame.fetch_add(1u, std::memory_order_relaxed);
}

auto render_thread::block_until_render_complete() -> void {
  wait(state::idle);
}

auto render_thread::kick() -> void {
  if (_policy != core::threading_policy::multi_threaded) {
    _render_thread_id.store(std::this_thread::get_id(), std::memory_order_relaxed);

    _state = state::busy;

    if (_work) {
      std::invoke(_work);
    }

    _state = state::idle;

    return;
  }

  set(state::kick);
}

auto render_thread::pump() -> void {
  block_until_render_complete();
  next_frame();
  kick();
}

auto render_thread::is_current_thread_render_thread() noexcept -> bool {
  return std::this_thread::get_id() == _render_thread_id.load(std::memory_order_relaxed);
}

auto render_thread::_worker_loop() -> void {
  SBX_PROFILE_THREAD_NAME("Render thread");

  while (true) {
    {
      auto lock = std::unique_lock{_mutex};

      _condition.wait(lock, [this]() { return _state == state::kick || !_is_running; });

      if (_state != state::kick && !_is_running) {
        break;
      }

      _state = state::busy;
    }

    if (_work) {
      std::invoke(_work);
    }

    {
      auto lock = std::unique_lock{_mutex};
      _state = state::idle;
    }

    _condition.notify_all();
  }
}

} // namespace sbx::render
