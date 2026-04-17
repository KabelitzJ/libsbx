// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MEMORY_ALIGNED_STORAGE_HPP_
#define LIBSBX_MEMORY_ALIGNED_STORAGE_HPP_

#include <memory>
#include <type_traits>
#include <cinttypes>

namespace sbx::memory {

template<std::size_t Size, std::size_t Alignment>
struct aligned_storage {
  struct type {
    alignas(Alignment) std::byte data[Size];
  }; // union type
}; // struct aligned_storage

template<std::size_t Size, std::size_t Alignment>
using aligned_storage_t = typename aligned_storage<Size, Alignment>::type;

template<typename Type>
struct storage_for {
  // using type = alignas(alignof(Type)) std::byte[sizeof(Type)];
  struct alignas(alignof(Type)) type {
    std::byte data[sizeof(Type)];
  }; // struct type
}; // struct storage_for

template<typename Type>
using storage_for_t = typename storage_for<Type>::type;

/**
 * @brief A class that allows for the manual construction and destruction of an object of type Type 
 * 
 * @tparam Type The type of the object
 * 
 * @warning This class does not manage the lifetime of the object, it only provides a way to construct and destroy it manually. 
 * The user is responsible for ensuring that the object is constructed before it is used and destroyed when it is no longer needed. 
 */
template<typename Type>
class constructible {

public:

  using value_type = Type;

  constructible() = default;

  constructible(const constructible& other) = delete;
  constructible(constructible&& other) = delete;

  ~constructible() = default;

  auto operator=(const constructible& other) -> constructible& = delete;
  auto operator=(constructible&& other) -> constructible& = delete;

  template<typename... Args>
  requires (std::is_constructible_v<Type, Args...>)
  auto construct(Args&&... args) -> Type* {
    return std::construct_at(_ptr(), std::forward<Args>(args)...);
  }

  auto destroy() noexcept -> void {
    std::destroy_at(_ptr());
  }

  auto get() noexcept -> Type* {
    return _ptr();
  }

  auto get() const noexcept -> const Type* {
    return _ptr();
  }

  auto operator*() noexcept -> Type& {
    return *_ptr();
  }

  auto operator*() const noexcept -> const Type& {
    return *_ptr();
  }

private:

  auto _ptr() noexcept -> Type* {
    return std::launder(reinterpret_cast<Type*>(&_storage));
  }

  auto _ptr() const noexcept -> const Type* {
    return std::launder(reinterpret_cast<const Type*>(&_storage));
  }

  storage_for_t<Type> _storage;

}; // class optional

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_ALIGNED_STORAGE_HPP_
