// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_EXPECTED_HPP_
#define LIBSBX_UTILITY_EXPECTED_HPP_

#include <memory>
#include <variant>
#include <stdexcept>

namespace sbx::utility {

template<typename Type>
class unexpected {

public:

  constexpr unexpected(const Type& value) noexcept
  : _value{value} { }

  constexpr ~unexpected() = default;

  constexpr auto value() const noexcept -> const Type& {
    return _value;
  }

private:

  Type _value;

}; // class unexpected

struct bad_expected_access : public std::exception {

  bad_expected_access() noexcept = default;

  ~bad_expected_access() = default;

  auto what() const noexcept -> const char* override {
    return "bad expected access";
  }

}; // struct bad_expected_access

template<typename Type, typename Error>
class expected {

public:

  using value_type = Type;
  using error_type = Error;
  using unexpected_type = unexpected<Error>;

  template<typename Other>
  using rebind = expected<Other, Error>;

  constexpr expected(const Type& value) noexcept
  : _value{value} { }

  constexpr expected(const unexpected_type& unexpected) noexcept
  : _value{unexpected.value()} { }

  constexpr auto has_value() const noexcept -> bool {
    return std::holds_alternative<Type>(_value);
  }

  constexpr operator bool() const noexcept {
    return has_value();
  }

  constexpr auto value() const -> const Type& {
    if (!has_value()) {
      throw bad_expected_access{};
    }

    return std::get<Type>(_value);
  }

  constexpr auto operator*() const -> const Type& {
    return value();
  }

  constexpr auto operator->() const -> const Type* {
    return &value();
  }

  constexpr auto error() const -> const Error& {
    if (has_value()) {
      throw bad_expected_access{};
    }

    return std::get<Error>(_value);
  }

private:

  std::variant<Type, Error> _value;

}; // class expected

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_EXPECTED_HPP_
