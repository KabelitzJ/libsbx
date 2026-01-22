// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_ENABLE_PRIVATE_CONSTRUCTOR_HPP_
#define LIBSBX_UTILITY_ENABLE_PRIVATE_CONSTRUCTOR_HPP_

namespace sbx::utility {

template<typename Derived>
struct enable_private_constructor : public Derived {
  template<typename... Args>
  enable_private_constructor(Args&&... args) : Derived{std::forward<Args>(args)...} {}
}; // struct enable_private_constructor

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_ENABLE_PRIVATE_CONSTRUCTOR_HPP_
