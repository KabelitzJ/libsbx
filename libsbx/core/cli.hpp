// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CORE_CLI_HPP_
#define LIBSBX_CORE_CLI_HPP_

#include <string>
#include <string_view>
#include <span>
#include <vector>
#include <string_view>
#include <concepts>
#include <cinttypes>
#include <optional>
#include <charconv>

#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>

namespace sbx::core {

template<typename Type>
concept argument = (std::is_same_v<Type, bool> || std::is_integral_v<Type> || std::is_floating_point_v<Type> || std::is_same_v<Type, std::string>);

class cli {

  friend class engine;

public:

  cli(std::span<std::string_view> args) {
    for (const auto& arg : args) {
      if (arg.substr(0, 2) != "--") {
        continue;
      }
      
      const auto pos = arg.find_first_of("=");

      if (pos == std::string::npos) {
        utility::logger<"core">::warn("Could not parse argument: '{}'", arg);
        continue;
      }

      const auto key = std::string{arg.substr(2, pos - 2u)};
      const auto value = std::string{arg.substr(pos + 1u)};

      _arguments[key] = std::move(value);
    }
  }

  template<argument Type>
  auto argument(const std::string& name) const -> std::optional<Type> {
    if (auto entry = _arguments.find(name); entry != _arguments.end()) {
      const auto& value = entry->second;

      if constexpr (std::is_same_v<Type, bool>) {
        if (value == "true") {
          return true;
        } else if (value == "false") {
          return false;
        } else {
          return std::nullopt;
        }
      } else if constexpr (std::is_floating_point_v<Type> || std::is_integral_v<Type>) {
        auto result = Type{};

        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);

        if (ec == std::errc::invalid_argument || ec == std::errc::result_out_of_range) {
          return std::nullopt;
        }

        return result;
      } else if constexpr (std::is_same_v<Type, std::string>) {
        return value;
      }
    }

    return std::nullopt;
  }

private:

  auto _argument_value(const std::string& name) -> std::optional<std::string> {
    if (auto entry = _arguments.find(name); entry != _arguments.end()) {
      return entry->second;
    }

    return std::nullopt;
  }

  std::unordered_map<std::string, std::string> _arguments;

}; // class cli

} // namespace sbx::core

#endif // LIBSBX_CORE_CLI_HPP_
