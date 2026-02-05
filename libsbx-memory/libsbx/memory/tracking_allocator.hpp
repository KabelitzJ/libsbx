#ifndef LIBSBX_MEMORY_TRACKING_ALLOCATOR_HPP_
#define LIBSBX_MEMORY_TRACKING_ALLOCATOR_HPP_

#include <memory>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>

namespace sbx::memory {

struct allocation_header {
  std::size_t size;
  const char* file;
  int line;
}; // struct allocation_header

template<typename Type>
class tracking_allocator {

public:

  using value_type = Type;
  using pointer = Type*;

  struct rebind {
    using other = tracking_allocator<Type>;
  }; // struct rebind

  using is_always_equal = std::true_type;

  tracking_allocator() noexcept = default;

  template<typename OtherType>
  tracking_allocator(const tracking_allocator<OtherType>&) noexcept { }

  [[nodiscard]] auto allocate(const std::size_t n) -> pointer {
    auto total_size = n * sizeof(Type) + sizeof(allocation_header);

    auto* raw_memory = static_cast<std::byte*>(std::malloc(total_size));

    if (!raw_memory) {
      throw std::bad_alloc{};
    }

    auto* header = reinterpret_cast<allocation_header*>(raw_memory);
    header->size = n * sizeof(Type);
    header->file = "unknown";
    header->line = 0;

    return reinterpret_cast<Type*>(raw_memory + sizeof(allocation_header));
  }

  void deallocate(pointer ptr, const std::size_t) noexcept {
    if (!ptr) {
      return;
    }

    auto* raw_memory = reinterpret_cast<std::byte*>(ptr) - sizeof(allocation_header);
    auto* header = reinterpret_cast<allocation_header*>(raw_memory);

    std::free(raw_memory);
  }



private:

}; // class tracking_allocator

template<typename Type, typename OtherType>
auto operator==(const tracking_allocator<Type>&, const tracking_allocator<OtherType>&) noexcept -> bool {
  return true;
}

template<typename Type>
using tracking_allocator_t = tracking_allocator<Type>;

template<typename Type>
using vector = std::vector<Type, tracking_allocator<Type>>;

template<typename Type>
using list = std::list<Type, tracking_allocator<Type>>;

template<typename Key, typename Value, typename Compare = std::less<Key>>
using map = std::map<Key, Value, Compare, tracking_allocator<std::pair<const Key, Value>>>;

template<typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
using unordered_map = std::unordered_map<Key, Value, Hash, KeyEqual, tracking_allocator<std::pair<const Key, Value>>>;

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_TRACKING_ALLOCATOR_HPP_
