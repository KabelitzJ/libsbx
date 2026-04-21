#ifndef LIBSBX_MEMORY_COUNTING_RESOURCE_HPP_
#define LIBSBX_MEMORY_COUNTING_RESOURCE_HPP_

#include <memory_resource>
#include <atomic>
#include <cstddef>
#include <algorithm>

namespace sbx::memory {

template<typename Type>
struct is_atomic : std::false_type { };

template<typename Type>
struct is_atomic<std::atomic<Type>> : std::true_type { };

template<typename Type>
inline constexpr bool is_atomic_v = is_atomic<Type>::value;

template<typename Counter>
struct counter_value;

template<typename Type>
struct counter_value<std::atomic<Type>> { using type = Type; };

template<typename Type>
struct counter_value { using type = Type; };

template<typename Counter>
using counter_value_t = typename counter_value<Counter>::type;

namespace detail {

template<typename Counter>
constexpr auto load(const Counter& counter) noexcept -> counter_value_t<Counter> {
  if constexpr (is_atomic_v<Counter>) {
    return counter.load(std::memory_order_relaxed);
  } else {
    return counter;
  }
}

template<typename Counter>
constexpr void store(Counter& counter, counter_value_t<Counter> value) noexcept {
  if constexpr (is_atomic_v<Counter>) {
    counter.store(value, std::memory_order_relaxed);
  } else {
    counter = value;
  }
}

template<typename Counter>
constexpr auto add(Counter& counter, counter_value_t<Counter> value) noexcept -> counter_value_t<Counter> {
  if constexpr (is_atomic_v<Counter>) {
    return counter.fetch_add(value, std::memory_order_relaxed) + value;
  } else {
    counter += value;

    return counter;
  }
}

template<typename Counter>
constexpr void subtract(Counter& counter, counter_value_t<Counter> value) noexcept {
  if constexpr (is_atomic_v<Counter>) {
    counter.fetch_sub(value, std::memory_order_relaxed);
  } else {
    counter -= value;
  }
}

template<typename Counter>
constexpr void increment(Counter& counter) noexcept {
  if constexpr (is_atomic_v<Counter>) {
    counter.fetch_add(1, std::memory_order_relaxed);
  } else {
    ++counter;
  }
}

template<typename Counter>
constexpr void update_peak(Counter& peak, counter_value_t<Counter> now) noexcept {
  if constexpr (is_atomic_v<Counter>) {
    auto current = peak.load(std::memory_order_relaxed);

    while (now > current && !peak.compare_exchange_weak(current, now, std::memory_order_relaxed, std::memory_order_relaxed)) { }
  } else {
    if (now > peak) {
      peak = now;
    }
  }
}

} // namespace detail

template<typename Counter>
requires (std::is_unsigned_v<counter_value_t<Counter>>)
class basic_counting_resource final : public std::pmr::memory_resource {

public:

  using counter_type = Counter;
  using value_type = counter_value_t<counter_type>;

  explicit basic_counting_resource(std::pmr::memory_resource* upstream) 
  : _upstream{upstream} {}

  void reset() noexcept {
    detail::store(_bytes_outstanding, 0);
    detail::store(_peak_outstanding, 0);
    detail::store(_alloc_count, 0);
    detail::store(_dealloc_count, 0);
  }

  auto outstanding() const noexcept -> value_type { 
    return detail::load(_bytes_outstanding); 
  }

  auto peak() const noexcept -> value_type { 
    return detail::load(_peak_outstanding); 
  }

  auto allocs() const noexcept -> value_type { 
    return detail::load(_alloc_count); 
  }

  auto deallocs() const noexcept -> value_type { 
    return detail::load(_dealloc_count); 
  }

private:

  auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override {
    void* ptr = _upstream->allocate(bytes, alignment);

    detail::increment(_alloc_count);

    const auto count = static_cast<value_type>(bytes);
    const auto now = detail::add(_bytes_outstanding, count);

    detail::update_peak(_peak_outstanding, now);

    return ptr;
  }

  auto do_deallocate(void* ptr, std::size_t bytes, std::size_t alignment) -> void override {
    detail::increment(_dealloc_count);
    detail::subtract(_bytes_outstanding, static_cast<value_type>(bytes));

    _upstream->deallocate(ptr, bytes, alignment);
  }

  auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override {
    return this == &other;
  }

  std::pmr::memory_resource* _upstream;

  counter_type _bytes_outstanding{0};
  counter_type _peak_outstanding{0};
  counter_type _alloc_count{0};
  counter_type _dealloc_count{0};

}; // struct counting_resource

template<std::unsigned_integral Type>
using basic_atomic_counting_resource = basic_counting_resource<std::atomic<Type>>;

using counting_resource = basic_counting_resource<std::size_t>;
using atomic_counting_resource = basic_atomic_counting_resource<std::size_t>;

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_COUNTING_RESOURCE_HPP_