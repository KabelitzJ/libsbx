// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_THREAD_POOL_HPP_
#define LIBSBX_ASSETS_THREAD_POOL_HPP_

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <functional>
#include <atomic>
#include <future>
#include <memory>
#include <ranges>
#include <concepts>

#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>

namespace sbx::assets {

class thread_pool final {

public:

  thread_pool(const std::size_t size = std::thread::hardware_concurrency())
  : _is_running{true} {
    _workers.reserve(size);

    for ([[maybe_unused]] auto i : std::views::iota(0u, size)) {
      _workers.emplace_back([this](){ _worker(); });
    }
  }

  thread_pool(const thread_pool&) = delete;
  thread_pool(thread_pool&&) = delete;

  ~thread_pool() {
    _is_running = false;
    _condition.notify_all();

    for (auto& worker : _workers) {
      worker.join();
    }
  }

  auto operator=(const thread_pool&) -> thread_pool& = delete;
  auto operator=(thread_pool&&) -> thread_pool& = delete;

  template<typename Function, typename... Args>
  requires (std::is_invocable_v<Function, Args...>)
  auto submit(Function&& function, Args&&... args) -> std::future<std::invoke_result_t<Function, Args...>> {
    using result_type = std::invoke_result_t<Function, Args...>;

    auto promise = std::make_shared<std::promise<result_type>>();
    auto future = promise->get_future();

    {
      auto lock = std::scoped_lock{_mutex};

      _tasks.emplace([promise, function = std::forward<Function>(function), ...args = std::forward<Args>(args)](){
        try {
          if constexpr (std::is_void_v<result_type>) {
            std::invoke(function, std::forward<Args>(args)...);
            promise->set_value();
          } else {
            promise->set_value(std::invoke(function, std::forward<Args>(args)...));
          }
        } catch (...) {
          try {
            promise->set_exception(std::current_exception());
          } catch (...) {
            utility::logger<"assets">::error("Failed to set exception for promise");
          }
        }
      });
    }

    _condition.notify_one();

    return future;
  }


private:

  auto _worker() -> void {
    while (true) {
      auto lock = std::unique_lock{_mutex};

      _condition.wait(lock, [this](){ return !_tasks.empty() || !_is_running; });

      if (!_is_running) {
        break;
      }

      auto task = std::move(_tasks.front());
      _tasks.pop();

      lock.unlock();

      std::invoke(task);
    }
  }

  std::vector<std::thread> _workers;
  std::queue<std::function<void()>> _tasks;

  std::mutex _mutex;
  std::condition_variable _condition;

  std::atomic_bool _is_running;

}; // class thread_pool

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_THREAD_POOL_HPP_
