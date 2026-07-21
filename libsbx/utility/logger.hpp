// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_LOGGER_HPP_
#define LIBSBX_UTILITY_LOGGER_HPP_

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/base_sink.h>

#include <libsbx/containers/ring_buffer.hpp>

#include <libsbx/utility/target.hpp>
#include <libsbx/utility/string_literal.hpp>

namespace sbx::utility {

namespace detail {

/**
 * @brief Keeps the last lines in memory for an in-engine console (editor).
 */
template<typename Mutex>
class ring_buffer_sink final : public spdlog::sinks::base_sink<Mutex> {

  using base = spdlog::sinks::base_sink<Mutex>;

public:

  using MutexType = Mutex;
  using log_level_type = spdlog::level::level_enum;

  struct log_line {
    std::string text;
    log_level_type level;
  }; // struct log_line

  explicit ring_buffer_sink(const std::size_t max_lines = 256u)
  : _lines{max_lines} { }

  [[nodiscard]] auto lines() -> std::vector<log_line> {
    auto lock = std::lock_guard<Mutex>{base::mutex_};

    return {_lines.begin(), _lines.end()};
  }

  auto clear() -> void {
    auto lock = std::lock_guard<Mutex>{base::mutex_};

    _lines.clear();
  }

protected:

  auto sink_it_(const spdlog::details::log_msg& msg) -> void override {
    auto formatted = spdlog::memory_buf_t{};

    base::formatter_->format(msg, formatted);

    _lines.emplace(fmt::to_string(formatted), msg.level);
  }

  auto flush_() -> void override { }

private:

  containers::ring_buffer<log_line> _lines;

}; // class ring_buffer_sink

using ring_buffer_sink_mt = ring_buffer_sink<std::mutex>;

/**
 * @brief Sinks shared by every tagged logger. Created lazily on first log
 * call (thread-safe magic static) — never during static initialization.
 */
class logger_context {

public:

  static auto instance() -> logger_context& {
    static auto context = logger_context{};

    return context;
  }

  auto sinks() const -> const std::vector<spdlog::sink_ptr>& {
    return _sinks;
  }

  auto ring_buffer() const -> const std::shared_ptr<ring_buffer_sink_mt>& {
    return _ring_buffer;
  }

  auto level() const -> spdlog::level::level_enum {
    return _level;
  }

private:

  logger_context() {
    const auto log_path = std::filesystem::path{"logs/sbx.log"};

    if (log_path.has_parent_path()) {
      std::filesystem::create_directories(log_path.parent_path());
    }

    _sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true));

    if constexpr (build_type_v == build_type::debug) {
      _sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    _ring_buffer = std::make_shared<ring_buffer_sink_mt>();
    _sinks.push_back(_ring_buffer);

    _level = (build_type_v == build_type::debug) ? spdlog::level::trace : spdlog::level::info;
  }

  std::vector<spdlog::sink_ptr> _sinks{};
  std::shared_ptr<ring_buffer_sink_mt> _ring_buffer{};
  spdlog::level::level_enum _level{};

}; // class logger_context

inline auto make_logger(std::string name) -> spdlog::logger {
  auto& context = logger_context::instance();

  const auto& sinks = context.sinks();

  auto logger = spdlog::logger{std::move(name), sinks.begin(), sinks.end()};

  logger.set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
  logger.set_level(context.level());
  logger.flush_on(spdlog::level::warn);

  return logger;
}

} // namespace detail

/**
 * @brief Lines held by the in-memory ring buffer sink (for an editor console).
 */
[[nodiscard]] inline auto logged_lines() -> std::vector<detail::ring_buffer_sink_mt::log_line> {
  return detail::logger_context::instance().ring_buffer()->lines();
}

/**
 * @brief A tagged logger. Every tag owns a lightweight spdlog::logger named
 * after it; all of them share the global sinks. The tag is rendered by the
 * sink pattern (%n), so messages are formatted exactly once and only when the
 * level is enabled.
 */
template<string_literal Tag>
class logger {

public:

  template<typename... Args>
  using format_string_type = spdlog::format_string_t<Args...>;

  logger() = delete;

  ~logger() = default;

  template<typename... Args>
  static auto trace(format_string_type<Args...> format, Args&&... args) -> void {
    _instance().trace(format, std::forward<Args>(args)...);
  }

  template<typename Type>
  static auto trace(const Type& value) -> void {
    _instance().trace(value);
  }

  template<typename... Args>
  static auto debug(format_string_type<Args...> format, Args&&... args) -> void {
    _instance().debug(format, std::forward<Args>(args)...);
  }

  template<typename Type>
  static auto debug(const Type& value) -> void {
    _instance().debug(value);
  }

  template<typename... Args>
  static auto info(format_string_type<Args...> format, Args&&... args) -> void {
    _instance().info(format, std::forward<Args>(args)...);
  }

  template<typename Type>
  static auto info(const Type& value) -> void {
    _instance().info(value);
  }

  template<typename... Args>
  static auto warn(format_string_type<Args...> format, Args&&... args) -> void {
    _instance().warn(format, std::forward<Args>(args)...);
  }

  template<typename Type>
  static auto warn(const Type& value) -> void {
    _instance().warn(value);
  }

  template<typename... Args>
  static auto error(format_string_type<Args...> format, Args&&... args) -> void {
    _instance().error(format, std::forward<Args>(args)...);
  }

  template<typename Type>
  static auto error(const Type& value) -> void {
    _instance().error(value);
  }

  template<typename... Args>
  static auto critical(format_string_type<Args...> format, Args&&... args) -> void {
    _instance().critical(format, std::forward<Args>(args)...);
  }

  template<typename Type>
  static auto critical(const Type& value) -> void {
    _instance().critical(value);
  }

private:

  static auto _instance() -> spdlog::logger& {
    static auto instance = detail::make_logger(std::string{Tag.data(), Tag.size()});

    return instance;
  }

}; // class logger

} // namespace sbx::utility

template<typename Type>
requires (std::is_enum_v<Type>)
struct fmt::formatter<Type> : public fmt::formatter<std::underlying_type_t<Type>> {

  using base_type = fmt::formatter<std::underlying_type_t<Type>>;

  template<typename FormatContext>
  auto format(const Type& value, FormatContext& context) const -> decltype(auto) {
    return base_type::format(static_cast<std::underlying_type_t<Type>>(value), context);
  }

}; // struct fmt::formatter

template<typename Type>
struct fmt::formatter<std::optional<Type>> : public fmt::formatter<Type> {

  using base_type = fmt::formatter<Type>;

  template<typename FormatContext>
  auto format(const std::optional<Type>& value, FormatContext& context) const -> decltype(auto) {
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
  auto format(const std::filesystem::path& value, FormatContext& context) const -> decltype(auto) {
    return fmt::format_to(context.out(), "{}", value.string());
  }

}; // struct fmt::formatter<Type>

#endif // LIBSBX_UTILITY_LOGGER_HPP_
