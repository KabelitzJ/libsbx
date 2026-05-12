// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CORE_PROFILER_HPP_
#define LIBSBX_CORE_PROFILER_HPP_

#include <string>
#include <string_view>
#include <unordered_map>
#include <ranges>
#include <chrono>

#include <libsbx/units/time.hpp>

#include <libsbx/utility/assert.hpp>

#include <libsbx/utility/hashed_string.hpp>
#include <libsbx/utility/iterator.hpp>
#include <libsbx/utility/exception.hpp>

namespace sbx::core {

template<typename Type>
class sampler {

public:

  using value_type = Type;
  using size_type = std::size_t;

  sampler(const std::size_t capacity)
  : _data{utility::make_reserved_vector<value_type>(capacity)},
    _index{0},
    _sum{0} { }

  sampler(const sampler& other)
  : _data{}, 
    _index{other._index}, 
    _sum{other._sum} {
    _data.reserve(other._data.capacity());
    _data.insert(_data.end(), other._data.begin(), other._data.end());
  }

  auto record(const value_type value) -> void {
    if (_data.size() < _data.capacity()) {
      _data.push_back(value);
      _sum += value;
    } else {
      _sum = _sum - _data[_index] + value;
      _data[_index] = value;
    }

    _index = (_index + 1u) % _data.capacity();
  }

  template<typename Other>
  [[nodiscard]] auto average_as() const -> Other {
    return _data.size() == 0u ? static_cast<Other>(0) : static_cast<Other>(_sum) / static_cast<Other>(_data.size());
  }

  [[nodiscard]] auto size() const -> size_type {
    return _data.size();
  }

  [[nodiscard]] auto data() const -> const value_type* {
    return _data.data();
  }

  void clear() {
    _data.clear();
    _index = 0u;
    _sum = static_cast<value_type>(0);
  }

  auto write_in_order(std::span<value_type> target) const -> void {
    utility::assert_that(target.size() >= _data.capacity(), "Insufficient buffer size");

    if (_data.size() < _data.capacity()) {
      for (auto i = 0u; i < _data.size(); ++i) {
        target[i] = _data[i];
      }

      for (auto i = _data.size(); i < _data.capacity(); ++i) {
        target[i] = static_cast<value_type>(0);
      }
    } else {
      auto position = 0u;

      for (auto i = _index; i < _data.capacity(); ++i) {
        target[position++] = _data[i];
      }

      for (auto i = 0u; i < _index; ++i) {
        target[position++] = _data[i];
      }
    }
  }

private:

  std::vector<value_type> _data;
  size_type _index;
  value_type _sum;

}; // class sampler

struct scope_info {

  using node_id = std::uint64_t;

  inline static constexpr auto null_node = static_cast<node_id>(-1);
  inline static constexpr auto max_nodes = std::uint64_t{516u};

  inline static constexpr auto null_time = std::chrono::microseconds{static_cast<std::uint64_t>(-1)};

  std::string_view label;
  std::string_view file;
  std::string_view function;

  std::uint32_t line;

  std::chrono::microseconds time;

  node_id id;
  node_id parent_id;

  std::uint32_t depth;

}; // struct scope_info

namespace detail {

struct database {

  std::array<scope_info, scope_info::max_nodes> nodes;

  scope_info::node_id next_node_id = 0u;
  scope_info::node_id current_node_id = scope_info::null_node;

  std::uint32_t current_depth = 0u;

  [[nodiscard]] auto create_node(const std::string_view label, const std::string_view file, const std::string_view function, const std::uint32_t line) -> scope_info& {
    if (next_node_id >= scope_info::max_nodes) [[unlikely]] {
      throw utility::runtime_error("Profiler exceeded maximum number of nodes ({})", scope_info::max_nodes);
    }

    const auto id = next_node_id++;

    nodes[id] = scope_info{
      .label = label,
      .file = file,
      .function = function,
      .line = line,
      .time = scope_info::null_time,
      .id = id,
      .parent_id = scope_info::null_node,
      .depth = current_depth,
    };

    return nodes[id];
  }

  inline static auto instance() -> database& {
    static thread_local auto instance = database{};
    return instance;
  }

}; // struct database

struct scope_guard {

  using clock = std::chrono::steady_clock;

  scope_info& info;
  clock::time_point start_time;
  scope_info::node_id previous_node_id;


  explicit scope_guard(scope_info& info)
  : info{info},
    start_time{clock::now()},
    previous_node_id{database::instance().current_node_id} {
    info.parent_id = database::instance().current_node_id;
    database::instance().current_node_id = info.id;
    database::instance().current_depth = info.depth + 1u;
  }

  ~scope_guard() {
    info.time = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start_time);
    database::instance().current_node_id = previous_node_id;
    database::instance().current_depth = info.depth;
  }
  
}; // struct scope_guard

} // namespace detail

} // namespace sbx::core

#endif // LIBSBX_CORE_PROFILER_HPP_
