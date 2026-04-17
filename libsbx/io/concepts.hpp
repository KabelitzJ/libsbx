// SPDX-License-Identifier: MIT
#ifndef LIBSBX_IO_CONCEPTS_HPP_
#define LIBSBX_IO_CONCEPTS_HPP_

#include <concepts>
#include <type_traits>

namespace sbx::io {

template<typename Type, typename Serializer>
concept serializable = requires(Serializer& serializer, const Type& value) {
  { serializer << value } -> std::same_as<Serializer&>;
}; // concept serializable

template<typename Type, typename Deserializer>
concept deserializable = requires(Deserializer& deserializer, Type& value) {
  { deserializer >> value } -> std::same_as<Deserializer&>;
}; // concept deserializable

} // namespace sbx::io

#endif // LIBSBX_IO_CONCEPTS_HPP_
