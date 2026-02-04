// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_MEMORY_TRACKER_HPP_
#define LIBSBX_UTILITY_MEMORY_TRACKER_HPP_

#include <cstddef>
#include <string_view>
#include <memory>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <print>
#include <format>

#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>

namespace sbx::utility {

class memory_tracker {

public:

  struct config {
    bool track_allocations = true;
    bool track_deallocations = true;
    bool capture_callstacks = false;
    std::size_t budget_warning_threshold = 0;
  }; // struct config

  struct scope {
    std::string module;
    std::string component;
  }; // struct scope

  struct record {
    std::size_t size = 0;
    scope allocation_scope;
    std::uint64_t frame_id;
  }; // struct record

  struct statistics {

    std::atomic<size_t> current_bytes{0};
    std::atomic<size_t> peak_bytes{0};
    std::atomic<size_t> total_allocations{0};
    std::atomic<size_t> active_allocations{0};

    auto record_allocation(std::size_t size) -> void {
      auto current = current_bytes.fetch_add(size) + size;
      total_allocations.fetch_add(1);
      active_allocations.fetch_add(1);

      auto peak = peak_bytes.load();
      while (current > peak && !peak_bytes.compare_exchange_weak(peak, current)) {

      }
    }

    void record_deallocation(std::size_t size) {
      current_bytes.fetch_sub(size);
      active_allocations.fetch_sub(1);
    }

  }; // struct statistics

  struct snapshot {
    std::size_t current_bytes = 0;
    std::size_t peak_bytes = 0;
    std::size_t total_allocations = 0;
    std::size_t active_allocations = 0;
  }; // struct snapshot

  static auto instance() -> memory_tracker& {
    static auto tracker = memory_tracker{};
    return tracker;
  }

  auto initialize(const config& configuration) -> void {
    _config = configuration;
    _is_initialized = true;
  }

  auto shutdown() -> void {
    if (_config.track_allocations) {
      report_leaks();
    }

    _is_initialized = false;
  }

  auto set_frame_id(std::uint64_t frame_id) -> void {
    _current_frame_id = frame_id;
  }

  auto push_scope(const std::string& module, const std::string& component) -> void {
    scope_stack().emplace_back(scope{module, component});
  }

  auto pop_scope() -> void {
    if (!scope_stack().empty()) {
      scope_stack().pop_back();
    }
  }

  auto current_scope() -> scope {
    if (scope_stack().empty()) {
      return scope{"<unknown>", "<unknown>"};
    }
    return scope_stack().back();
  }

  auto record_allocation(void* address, std::size_t size) -> void {
    if (!_is_initialized || _is_in_recursion) {
      return;
    }

    auto guard = recursion_guard{_is_in_recursion};

    auto lock = std::lock_guard{_allocation_records_mutex};

    auto scope = current_scope();

    _allocation_records[address] = record{
      .size = size,
      .allocation_scope = scope,
      .frame_id = _current_frame_id.load()
    };

    _global_statistics.record_allocation(size);

    get_module_statistics(scope.module).record_allocation(size);
    get_component_statistics(scope.component).record_allocation(size);
  }

  auto record_deallocation(void* address) -> void {
    if (!address || !_is_initialized || _is_in_recursion) {
      return;
    }

    auto guard = recursion_guard{_is_in_recursion};

    auto lock = std::lock_guard{_allocation_records_mutex};

    auto entry = _allocation_records.find(address);

    if (entry != _allocation_records.end()) {
      auto& [address, record] = *entry;

      _global_statistics.record_deallocation(record.size);
      get_module_statistics(record.allocation_scope.module).record_deallocation(record.size);
      get_component_statistics(record.allocation_scope.component).record_deallocation(record.size);

      _allocation_records.erase(entry);
    }
  }

  auto get_global_statistics() -> const statistics& {
    return _global_statistics;
  }

  auto get_module_statistics(const std::string& module) -> statistics& {
    auto lock = std::lock_guard{_statistics_mutex};

    return _module_statistics[module];
  }

  auto get_component_statistics(const std::string& component) -> statistics& {
    auto lock = std::lock_guard{_statistics_mutex};

    return _component_statistics[component];
  }

  auto report_statistics() -> void {
    auto lock = std::lock_guard{_statistics_mutex};

    logger<"utility">::info("===== Memory Tracker Statistics =====");
    logger<"utility">::info("Global Statistics:");
    logger<"utility">::info("  Current Bytes: {} MB", _global_statistics.current_bytes.load() / (1024 * 1024));
    logger<"utility">::info("  Peak Bytes: {} MB", _global_statistics.peak_bytes.load() / (1024 * 1024));
    logger<"utility">::info("  Total Allocations: {}", _global_statistics.total_allocations.load());
    logger<"utility">::info("  Active Allocations: {}", _global_statistics.active_allocations.load());

    logger<"utility">::info("\nPer-Module Statistics:");

    for (const auto& [module, stats] : _module_statistics) {
      logger<"utility">::info("Module: {}", module);
      logger<"utility">::info("  Current Bytes: {} MB", stats.current_bytes.load() / (1024 * 1024));
      logger<"utility">::info("  Peak Bytes: {} MB", stats.peak_bytes.load() / (1024 * 1024));
      logger<"utility">::info("  Total Allocations: {}", stats.total_allocations.load());
      logger<"utility">::info("  Active Allocations: {}", stats.active_allocations.load());
    }

    logger<"utility">::info("\nPer-Component Statistics:");

    for (const auto& [component, stats] : _component_statistics) {
      logger<"utility">::info("Component: {}", component);
      logger<"utility">::info("  Current Bytes: {} MB", stats.current_bytes.load() / (1024 * 1024));
      logger<"utility">::info("  Peak Bytes: {} MB", stats.peak_bytes.load() / (1024 * 1024));
      logger<"utility">::info("  Total Allocations: {}", stats.total_allocations.load());
      logger<"utility">::info("  Active Allocations: {}", stats.active_allocations.load());
    }
  }

  auto report_leaks() -> void {
    auto lock = std::lock_guard{_allocation_records_mutex};

    if (_allocation_records.empty()) {
      logger<"utility">::info("No memory leaks detected.");
      return;
    }

    logger<"utility">::info("===== Memory Leaks Detected =====");
    logger<"utility">::info("Total Leaks: {}", _allocation_records.size());

    auto leaks_by_module = std::unordered_map<std::string, std::size_t>{};

    for (const auto& [address, record] : _allocation_records) {
      leaks_by_module[record.allocation_scope.module] += record.size;
    }

    for (const auto& [module, total_size] : leaks_by_module) {
      logger<"utility">::info("Module: {}, Leaked Bytes: {}", module, total_size);
    }
  }

  auto get_module_snapshot() -> std::vector<std::pair<std::string, snapshot>> {
    auto guard = recursion_guard{_is_in_recursion};
    auto lock = std::lock_guard{_statistics_mutex};

    auto snapshots = std::vector<std::pair<std::string, snapshot>>{};

    for (const auto& [module, stats] : _module_statistics) {
      snapshots.emplace_back(
        module,
        snapshot{
          .current_bytes = stats.current_bytes.load(),
          .peak_bytes = stats.peak_bytes.load(),
          .total_allocations = stats.total_allocations.load(),
          .active_allocations = stats.active_allocations.load()
        }
      );
    }

    return snapshots;
  }

  auto get_component_snapshot() -> std::vector<std::pair<std::string, snapshot>> {
    auto guard = recursion_guard{_is_in_recursion};
    auto lock = std::lock_guard{_statistics_mutex};

    auto snapshots = std::vector<std::pair<std::string, snapshot>>{};

    for (const auto& [component, stats] : _component_statistics) {
      snapshots.emplace_back(
        component,
        snapshot{
          .current_bytes = stats.current_bytes.load(),
          .peak_bytes = stats.peak_bytes.load(),
          .total_allocations = stats.total_allocations.load(),
          .active_allocations = stats.active_allocations.load()
        }
      );
    }

    return snapshots;
  }

  auto get_components_for_module(const std::string& module) -> std::vector<std::pair<std::string, snapshot>> {
    auto guard = recursion_guard{_is_in_recursion};
    auto lock = std::lock_guard{_allocation_records_mutex};

    auto snapshots = std::vector<std::pair<std::string, snapshot>>{};

    auto prefix = std::format("{}::", module); 
  
    for (const auto& [component, stats] : _component_statistics) {
      if (component.starts_with(prefix)) {
        snapshots.emplace_back(
          component,
          snapshot{
            .current_bytes = stats.current_bytes.load(),
            .peak_bytes = stats.peak_bytes.load(),
            .total_allocations = stats.total_allocations.load(),
            .active_allocations = stats.active_allocations.load()
          }
        );
      }
    }

    return snapshots;
  }

private:

  struct recursion_guard {
    
    bool& flag;

    recursion_guard(bool& flag_reference) 
    : flag(flag_reference) {
      flag = true;
    }

    ~recursion_guard() {
      flag = false;
    }

  }; // struct recursion_guard

  memory_tracker() = default;

  static auto scope_stack() -> std::vector<scope>& {
    thread_local auto stack = std::vector<scope>{};
    return stack;
  }

  config _config{};
  std::atomic<bool> _is_initialized{false};
  std::atomic<std::uint64_t> _current_frame_id{0};

  inline thread_local static bool _is_in_recursion = false;

  statistics _global_statistics{};
  std::unordered_map<std::string, statistics> _module_statistics{};
  std::unordered_map<std::string, statistics> _component_statistics{};
  std::mutex _statistics_mutex{};

  std::unordered_map<void*, record> _allocation_records{};
  std::mutex _allocation_records_mutex{};

}; // class memory_tracker

struct tracker_initializer {

  tracker_initializer(const memory_tracker::config& configuration) {
    memory_tracker::instance().initialize(configuration);
  }

  ~tracker_initializer() {
    memory_tracker::instance().shutdown();
  }

}; // struct tracker_initializer

struct tracker_scope {

  tracker_scope(const std::string& module, const std::string& component) {
    memory_tracker::instance().push_scope(module, component);
  }

  tracker_scope(const tracker_scope&) = delete;

  ~tracker_scope() {
    memory_tracker::instance().pop_scope();
  }

  auto operator=(const tracker_scope&) -> tracker_scope& = delete;

}; // struct tracker_scope

} // namespace sbx::utility

auto operator new(std::size_t size) -> void*;

auto operator new[](std::size_t size) -> void*;

auto operator delete(void* ptr) noexcept -> void;

auto operator delete(void* ptr, std::size_t) noexcept -> void;

auto operator delete[](void* ptr) noexcept -> void;

auto operator delete[](void* ptr, std::size_t) noexcept -> void;

#define _CONCATENATE_DETAIL(x, y) x##y
#define _CONCATENATE(x, y) _CONCATENATE_DETAIL(x, y)

#define SBX_MEMORY_TRACKER_INITIALIZE(module, component, config) \
  static auto _CONCATENATE(_memory_tracker_initializer_, __LINE__) = sbx::utility::tracker_initializer{config}; \
  auto _CONCATENATE(_memory_tracker_scope_, __LINE__) = sbx::utility::tracker_scope{module, component};

#define SBX_MEMORY_TRACKER_SCOPE(module, component) \
  auto _CONCATENATE(_memory_tracker_scope_, __LINE__) = sbx::utility::tracker_scope{module, component};

#endif // LIBSBX_UTILITY_MEMORY_TRACKER_HPP_
