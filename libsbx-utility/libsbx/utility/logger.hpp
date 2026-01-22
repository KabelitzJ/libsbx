// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_LOGGER_HPP_
#define LIBSBX_UTILITY_LOGGER_HPP_

#include <iostream>
#include <optional>
#include <mutex>
#include <deque>
#include <filesystem>

#include <fmt/format.h>

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/base_sink.h>

#include <libsbx/utility/target.hpp>
#include <libsbx/utility/string_literal.hpp>

namespace sbx::utility {

namespace detail {

template<typename Mutex>
class ring_buffer_sink final : public spdlog::sinks::base_sink<Mutex> {

  using base = spdlog::sinks::base_sink<Mutex>;

public:

  struct log_line {
    std::string text;
    spdlog::level::level_enum level;
  }; // struct log_line

  explicit ring_buffer_sink(const std::size_t max_lines = 512)
  : _max_lines{max_lines} {}

  [[nodiscard]] auto lines() -> std::vector<log_line> {
    auto lock = std::lock_guard<Mutex>{base::mutex_};

    return {_lines.begin(), _lines.end()};
  }

  void clear() {
    std::lock_guard<Mutex> lock(base::mutex_);
    
    _lines.clear();
  }

protected:

  void sink_it_(const spdlog::details::log_msg& msg) override {
    auto formatted = spdlog::memory_buf_t{};
    base::formatter_->format(msg, formatted);

    auto lock = std::lock_guard<Mutex>{base::mutex_};
    _lines.emplace_back(fmt::to_string(formatted), msg.level);

    if (_lines.size() > _max_lines) {
      _lines.pop_front();
    }
  }

  void flush_() override {

  }

private:

  std::size_t _max_lines;
  std::deque<log_line> _lines;

}; // class ring_buffer_sink

using ring_buffer_sink_mt = ring_buffer_sink<std::mutex>;
using ring_buffer_sink_st = ring_buffer_sink<spdlog::details::null_mutex>;

struct logger_instance {

  static auto create_logger() -> spdlog::logger {
    auto sinks = std::vector<std::shared_ptr<spdlog::sinks::sink>>{};

    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_st>("./demo/logs/sbx.log", true));

    if constexpr (utility::build_configuration_v == utility::build_configuration::debug) {
      sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_st>());
    }

    sink = std::make_shared<ring_buffer_sink_st>();

    sinks.push_back(sink);

    auto logger = spdlog::logger{"logger", std::begin(sinks), std::end(sinks)};

    logger.set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");

    if constexpr (build_configuration_v == build_configuration::debug) {
      logger.set_level(spdlog::level::trace);
    } else {
      logger.set_level(spdlog::level::info);
    }

    return logger;
  }

  inline static auto sink = std::shared_ptr<ring_buffer_sink_st>{};
  inline static auto logger = create_logger();

}; // struct logger_instance

inline auto instance() -> spdlog::logger& {
  return logger_instance::logger;
}

inline auto sink() -> std::shared_ptr<ring_buffer_sink_st>& {
  return logger_instance::sink;
}

} // namespace detail

template<string_literal Tag>
class logger {

public:

  template<typename... Args>
  using format_string_type = spdlog::format_string_t<Args...>;

  logger() = delete;

  ~logger() = default;

  template<typename... Args>
  static auto trace(format_string_type<Args...> format, Args&&... args) -> void {
    detail::instance().trace("[{}] : {}", Tag, fmt::format(format, std::forward<Args>(args)...));
  }

  template<typename Type>
  static auto trace(const Type& value) -> void {
    detail::instance().trace("[{}] : {}", Tag, value);
  }

  template<typename... Args>
  static auto debug(format_string_type<Args...> format, Args&&... args) -> void {
    detail::instance().debug("[{}] : {}", Tag, fmt::format(format, std::forward<Args>(args)...));
  }

  template<typename Type>
  static auto debug(const Type& value) -> void {
    detail::instance().debug("[{}] : {}", Tag, value);
  }

  template<typename... Args>
  static auto info(format_string_type<Args...> format, Args&&... args) -> void {
    detail::instance().info("[{}] : {}", Tag, fmt::format(format, std::forward<Args>(args)...));
  }

  template<typename Type>
  static auto info(const Type& value) -> void {
    detail::instance().info("[{}] : {}", Tag, value);
  }

  template<typename... Args>
  static auto warn(format_string_type<Args...> format, Args&&... args) -> void {
    detail::instance().warn("[{}] : {}", Tag, fmt::format(format, std::forward<Args>(args)...));
  }

  template<typename Type>
  static auto warn(const Type& value) -> void {
    detail::instance().warn("[{}] : {}", Tag, value);
  }

  template<typename... Args>
  static auto error(format_string_type<Args...> format, Args&&... args) -> void {
    detail::instance().error("[{}] : {}", Tag, fmt::format(format, std::forward<Args>(args)...));
  }

  template<typename Type>
  static auto error(const Type& value) -> void {
    detail::instance().error("[{}] : {}", Tag, value);
  }

  template<typename... Args>
  static auto critical(format_string_type<Args...> format, Args&&... args) -> void {
    detail::instance().critical("[{}] : {}", Tag, fmt::format(format, std::forward<Args>(args)...));
  }

  template<typename Type>
  static auto critical(const Type& value) -> void {
    detail::instance().critical("[{}] : {}", Tag, value);
  }

}; // class logger

} // namespace sbx::utility

// [NOTE] KAJ 2024-01-19 : Enable formatting to underlying type for all enums
template<typename Type>
requires (std::is_enum_v<Type>)
struct fmt::formatter<Type> : public fmt::formatter<std::underlying_type_t<Type>> {

  using base_type = fmt::formatter<std::underlying_type_t<Type>>;

  template<typename FormatContext>
  auto format(const Type& value, FormatContext& context) const -> decltype(auto) {
    return base_type::format(static_cast<std::underlying_type_t<Type>>(value), context);
  }

}; // struct fmt::formatter

// [NOTE] KAJ 2025-05-27 : Enable formatting for optionals
template<typename Type>
struct fmt::formatter<std::optional<Type>> : public fmt::formatter<Type> {

  using base_type = fmt::formatter<Type>;

  template<typename FormatContext>
  auto format(const std::optional<Type>& value, FormatContext& context) -> decltype(auto) {
    if (value) {
      return base_type::format(*value, context);
    }

    return fmt::format_to(context.out(), "[empty optional]");
  }

}; // struct fmt::formatter<Type>

template<>
struct fmt::formatter<std::filesystem::path> : public fmt::formatter<std::filesystem::path::string_type> {

  using base_type = fmt::formatter<std::filesystem::path::string_type>;

  template<typename FormatContext>
  auto format(const std::filesystem::path& value, FormatContext& context) -> decltype(auto) {
    return fmt::format_to(context.out(), "{}", value.string());
  }

}; // struct fmt::formatter<Type>


#endif // LIBSBX_UTILITY_LOGGER_HPP_
