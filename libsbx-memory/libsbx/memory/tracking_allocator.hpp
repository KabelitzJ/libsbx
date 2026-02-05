#ifndef LIBSBX_MEMORY_TRACKING_ALLOCATOR_HPP_
#define LIBSBX_MEMORY_TRACKING_ALLOCATOR_HPP_

#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <list>
#include <map>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <source_location>

#include <magic_enum/magic_enum.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/enum.hpp>
#include <libsbx/utility/target.hpp>

namespace sbx::memory {

struct is_memory_tracking_enabled {

#if defined(SBX_MEMORY_TRACKING)
  inline static constexpr auto value = (SBX_MEMORY_TRACKING != 0);
#else
  inline static constexpr auto value = utility::is_build_configuration_debug_v;
#endif

}; // struct is_memory_tracking_enabled

inline constexpr auto is_memory_tracking_enabled_v = is_memory_tracking_enabled::value;

enum class allocation_category : std::uint8_t {
  general,
  container,
  string
}; // enum class allocation_category

constexpr auto to_string(const allocation_category category) noexcept -> std::string_view {
  switch (category) {
    case allocation_category::general: return "General";
    case allocation_category::container: return "Container";
    case allocation_category::string: return "String";
    default: return "Unknown";
  }
}

struct allocation_statistics_snapshot {

  std::size_t allocation_count{0};
  std::size_t deallocation_count{0};
  std::size_t bytes_allocated{0};
  std::size_t bytes_freed{0};
  std::size_t peak_bytes{0};

  [[nodiscard]] auto current_bytes() const noexcept -> std::size_t {
    return bytes_allocated - bytes_freed;
  }

  [[nodiscard]] auto current_allocations() const noexcept -> std::size_t {
    return allocation_count - deallocation_count;
  }

}; // struct allocation_statistics_snapshot

struct allocation_statistics {

  std::atomic<std::size_t> allocation_count{0};
  std::atomic<std::size_t> deallocation_count{0};
  std::atomic<std::size_t> bytes_allocated{0};
  std::atomic<std::size_t> bytes_freed{0};
  std::atomic<std::size_t> peak_bytes{0};

  [[nodiscard]] auto snapshot() const noexcept -> allocation_statistics_snapshot {
    auto out = allocation_statistics_snapshot{};

    out.allocation_count = allocation_count.load(std::memory_order_relaxed);
    out.deallocation_count = deallocation_count.load(std::memory_order_relaxed);
    out.bytes_allocated = bytes_allocated.load(std::memory_order_relaxed);
    out.bytes_freed = bytes_freed.load(std::memory_order_relaxed);
    out.peak_bytes = peak_bytes.load(std::memory_order_relaxed);

    return out;
  }

  [[nodiscard]] auto current_bytes() const noexcept -> std::size_t {
    return bytes_allocated.load(std::memory_order_relaxed) - bytes_freed.load(std::memory_order_relaxed);
  }

  [[nodiscard]] auto current_allocations() const noexcept -> std::size_t {
    return allocation_count.load(std::memory_order_relaxed) - deallocation_count.load(std::memory_order_relaxed);
  }

}; // struct allocation_statistics

struct alignas(std::max_align_t) allocation_header {

  inline static constexpr auto magic_allocated = std::uint32_t{0xABCD1234};
  inline static constexpr auto magic_freed = std::uint32_t{0xDEADBEEF};

  std::size_t size;
  std::size_t alignment;
  allocation_category category;
  std::uint32_t source_line;
  const char* source_file;
  allocation_header* previous;
  allocation_header* next;
  std::uint32_t magic;

}; // struct allocation_header

class memory_tracker {
public:

  static auto instance() noexcept -> memory_tracker& {
    static auto tracker = memory_tracker{};

    return tracker;
  }

  void record_allocation(allocation_header* header) noexcept {
    auto& statistics = _statistics[utility::to_underlying(header->category)];

    statistics.allocation_count.fetch_add(1, std::memory_order_relaxed);
  
    auto old_bytes = statistics.bytes_allocated.fetch_add(header->size, std::memory_order_relaxed);
    
    auto current = old_bytes + header->size - statistics.bytes_freed.load(std::memory_order_relaxed);
    auto peak = statistics.peak_bytes.load(std::memory_order_relaxed);

    while (current > peak &&  !statistics.peak_bytes.compare_exchange_weak(peak, current, std::memory_order_relaxed)) { }

    {
      auto lock = std::lock_guard{_statistics_mutex};

      header->previous = nullptr;
      header->next = _head;

      if (_head) {
        _head->previous = header;
      }

      _head = header;
    }
  }

  void record_deallocation(allocation_header* header) noexcept {
    auto& statistics = _statistics[utility::to_underlying(header->category)];

    statistics.deallocation_count.fetch_add(1, std::memory_order_relaxed);
    statistics.bytes_freed.fetch_add(header->size, std::memory_order_relaxed);

    {
      auto lock = std::lock_guard{_statistics_mutex};

      if (header->previous) {
        header->previous->next = header->next;
      } else {
        _head = header->next;
      }
      if (header->next) {
        header->next->previous = header->previous;
      }
    }
  }

  [[nodiscard]] auto statistics(const allocation_category category) const noexcept -> allocation_statistics_snapshot {
    return _statistics[utility::to_underlying(category)].snapshot();
  }

  [[nodiscard]] auto total_statistics() const noexcept -> allocation_statistics_snapshot {
    auto total = allocation_statistics_snapshot{};

    for (const auto& statistics : _statistics) {
      auto snapshot = statistics.snapshot();

      total.allocation_count += snapshot.allocation_count;
      total.deallocation_count += snapshot.deallocation_count;
      total.bytes_allocated += snapshot.bytes_allocated;
      total.bytes_freed += snapshot.bytes_freed;
      total.peak_bytes = std::max(total.peak_bytes, snapshot.peak_bytes);
    }

    return total;
  }

  auto report_leaks() const -> void {
    auto lock = std::lock_guard{_statistics_mutex};

    if (!_head) {
      return;
    }

    auto leak_count = std::size_t{0};
    auto leak_bytes = std::size_t{0};

    for (auto* header = _head; header; header = header->next) {
      ++leak_count;
      leak_bytes += header->size;
      
      utility::logger<"memory">::warn("Leak: {} bytes at {}:{}", header->size, header->source_file, header->source_line);
    }

    utility::logger<"memory">::warn("Memory leaks detected: {} allocations, {} bytes", leak_count, leak_bytes);
  }

private:

  memory_tracker() = default;

  memory_tracker(const memory_tracker&) = delete;

  ~memory_tracker() {
    report_leaks();
  }

  auto operator=(const memory_tracker&) -> memory_tracker& = delete;

  std::array<allocation_statistics, magic_enum::enum_count<allocation_category>()> _statistics{};
  mutable std::mutex _statistics_mutex;

  allocation_header* _head{nullptr};

}; // class memory_tracker

namespace detail {

[[nodiscard]] inline auto aligned_allocate(std::size_t size, std::size_t alignment, const allocation_category category, std::string_view file, std::int32_t line) -> void* {
  auto effective_alignment = alignment;

  effective_alignment = std::max(effective_alignment, alignof(void*));
  effective_alignment = std::max(effective_alignment, alignof(std::max_align_t));

  if (!std::has_single_bit(effective_alignment)) {
    effective_alignment = std::bit_ceil(effective_alignment);
  }

  auto overhead = sizeof(allocation_header) + sizeof(allocation_header*) + (effective_alignment - 1);
  auto total_size = size + overhead;

  auto* base = std::malloc(total_size);

  if (!base) {
    throw std::bad_alloc{};
  }

  auto* base_bytes = static_cast<std::byte*>(base);

  auto* header = std::construct_at(reinterpret_cast<allocation_header*>(base_bytes));
  header->size = size;
  header->alignment = effective_alignment;
  header->category = category;
  header->source_line = static_cast<std::uint32_t>(line);
  header->source_file = file.data();
  header->previous = nullptr;
  header->next = nullptr;
  header->magic = allocation_header::magic_allocated;

  auto* user_unaligned = base_bytes + sizeof(allocation_header) + sizeof(allocation_header*);

  auto user_addr = reinterpret_cast<std::uintptr_t>(user_unaligned);
  auto aligned_addr = (user_addr + effective_alignment - 1) & ~(static_cast<std::uintptr_t>(effective_alignment) - 1);

  auto* user_ptr = reinterpret_cast<std::byte*>(aligned_addr);

  auto* header_slot = reinterpret_cast<allocation_header**>(user_ptr - sizeof(allocation_header*));
  *header_slot = header;

  memory_tracker::instance().record_allocation(header);

  return static_cast<void*>(user_ptr);
}

inline auto aligned_deallocate(void* ptr) noexcept -> void {
  if (!ptr) {
    return;
  }

  auto* user_bytes = static_cast<std::byte*>(ptr);
  auto* header_slot = reinterpret_cast<allocation_header**>(user_bytes - sizeof(allocation_header*));
  auto* header = *header_slot;

  if (!header) {
    std::terminate();
  }

  if (header->magic != allocation_header::magic_allocated) {
    std::terminate();
  }

  header->magic = allocation_header::magic_freed;

  memory_tracker::instance().record_deallocation(header);

  std::destroy_at(header);
  std::free(static_cast<void*>(header));
}

} // namespace detail

template<typename Type, allocation_category Category = allocation_category::container>
class tracking_allocator {

public:

  using value_type = Type;
  using pointer = Type*;
  using const_pointer = const Type*;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  template<typename OtherType>
  struct rebind {
    using other = tracking_allocator<OtherType, Category>;
  }; // struct rebind

  using is_always_equal = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;

  tracking_allocator() noexcept = default;

  template<typename OtherType, allocation_category OtherCategory>
  tracking_allocator(const tracking_allocator<OtherType, OtherCategory>&) noexcept {}

  [[nodiscard]] auto allocate(size_type n, const std::source_location& loc = std::source_location::current()) -> pointer {
    if (n > std::numeric_limits<size_type>::max() / sizeof(Type)) {
      throw std::bad_array_new_length{};
    }

    return static_cast<pointer>(detail::aligned_allocate(n * sizeof(Type), alignof(Type), Category, loc.file_name(), static_cast<int>(loc.line())));
  }

  auto deallocate(pointer ptr, size_type) noexcept -> void {
    detail::aligned_deallocate(ptr);
  }

}; // class tracking_allocator

template<typename LhsType, allocation_category LhsCategory, typename RhsType, allocation_category RhsCategory>
[[nodiscard]] constexpr auto operator==(const tracking_allocator<LhsType, LhsCategory>&, const tracking_allocator<RhsType, RhsCategory>&) noexcept -> bool {
  return true;
}

template<typename Type>
using general_tracking_allocator = tracking_allocator<Type, allocation_category::general>;

namespace detail {

template<typename Type, allocation_category Category = allocation_category::container>
using allocator_type = std::conditional_t<is_memory_tracking_enabled_v, tracking_allocator<Type, Category>, std::allocator<Type>>;

[[nodiscard]] inline auto header_from_user_ptr(void* ptr) noexcept -> allocation_header* {
  if (!ptr) {
    return nullptr;
  }

  auto* user_bytes = static_cast<std::byte*>(ptr);
  auto* header_slot = reinterpret_cast<allocation_header**>(user_bytes - sizeof(allocation_header*));

  return *header_slot;
}

template<typename Type>
[[nodiscard]] inline auto element_count_from_header(const allocation_header* header) noexcept -> std::size_t {
  if (!header) {
    return 0u;
  }

  if (header->size == 0u) {
    return 0u;
  }

  return header->size / sizeof(Type);
}

template<typename Type, allocation_category Category = allocation_category::general>
struct tracked_delete {

  auto operator()(Type* ptr) const noexcept -> void {
    if (!ptr) {
      return;
    }

    if constexpr (!std::is_trivially_destructible_v<Type>) {
      std::destroy_at(ptr);
    }

    detail::aligned_deallocate(static_cast<void*>(ptr));
  }

}; // struct tracked_delete

template<typename Type, allocation_category Category>
struct tracked_delete<Type[], Category> {

  auto operator()(Type* ptr) const noexcept -> void {
    if (!ptr) {
      return;
    }

    auto* header = detail::header_from_user_ptr(static_cast<void*>(ptr));
    auto count = detail::element_count_from_header<Type>(header);

    if constexpr (!std::is_trivially_destructible_v<Type>) {
      if (count != 0u) {
        std::destroy_n(ptr, count);
      }
    }

    detail::aligned_deallocate(static_cast<void*>(ptr));
  }

}; // struct tracked_delete

template<typename Type>
using deleter_type = std::conditional_t<is_memory_tracking_enabled_v, tracked_delete<Type>, std::default_delete<Type>>;

}; // namespace detail

template<typename Type>
using vector = std::vector<Type, detail::allocator_type<Type>>;

template<typename Type>
using list = std::list<Type, detail::allocator_type<Type>>;

template<typename Key, typename Value, typename Compare = std::less<Key>>
using map = std::map<Key, Value, Compare, detail::allocator_type<std::pair<const Key, Value>>>;

template<typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
using unordered_map = std::unordered_map<Key, Value, Hash, KeyEqual, detail::allocator_type<std::pair<const Key, Value>>>;

using string = std::basic_string<char, std::char_traits<char>, detail::allocator_type<char, allocation_category::string>>;

template<typename Type>
using unique_ptr = std::unique_ptr<Type, detail::deleter_type<Type>>;

template<typename Type, allocation_category Category = allocation_category::general, typename... Args>
[[nodiscard]] auto make_unique(Args&&... args) -> unique_ptr<Type> {
  if constexpr (is_memory_tracking_enabled_v) {
    auto allocator = tracking_allocator<Type, Category>{};
    auto* ptr = allocator.allocate(1u);

    try {
      std::construct_at(ptr, std::forward<Args>(args)...);
    } catch (...) {
      allocator.deallocate(ptr, 1u);
      throw;
    }

    return unique_ptr<Type>{ptr};
  } else {
    return std::make_unique<Type>(std::forward<Args>(args)...);
  }
}

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_TRACKING_ALLOCATOR_HPP_
