// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SIGNAL_OBJECT_PTR_HPP_
#define LIBSBX_SIGNAL_OBJECT_PTR_HPP_

#include <type_traits>

#include <libsbx/signals/to_weak.hpp>

namespace sbx::signals {

using object_ptr = const void*;

template<typename Type>
auto get_object_ptr(const Type& value) -> object_ptr;

template<typename Type, typename = void>
struct object_pointer {
  static auto get(const Type&) -> object_ptr {
    return nullptr;
  }
}; // struct object_pointer

template<typename Type>
struct object_pointer<Type*, std::enable_if_t<std::is_pointer_v<Type*>>> {
  static auto get(memory::observer_ptr<const Type> value) -> object_ptr {
    return reinterpret_cast<object_ptr>(value);
  }
}; // struct object_pointer

template<typename Type>
struct object_pointer<Type, std::enable_if_t<is_weak_ptr_v<Type>>> {
  static auto get(const Type& value) -> object_ptr {
    auto object = value.lock();

    return get_object_ptr(object);
  }
}; // struct object_pointer

template<typename Type>
struct object_pointer<Type, std::enable_if_t<!std::is_pointer_v<Type> && !is_weak_ptr_v<Type> && is_weak_ptr_compatible_v<Type>>> {
  static auto get(const Type& value) -> object_ptr {
    return value ? reinterpret_cast<object_ptr>(value.get()) : nullptr;
  }
}; // struct object_pointer

template<typename Type>
auto get_object_ptr(const Type& value) -> object_ptr {
  return object_pointer<Type>::get(value);
}

} // namespace sbx::signals

#endif // LIBSBX_SIGNAL_OBJECT_PTR_HPP_
