// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_CONSOLE_SINK_HPP_
#define EDITOR_CONSOLE_SINK_HPP_

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <spdlog/sinks/base_sink.h>

#include <libsbx/containers/ring_buffer.hpp>

#include <libsbx/utility/logger.hpp>

namespace editor {

struct log_line {
  std::string text;
  spdlog::level::level_enum level;
}; // struct log_line

/**
 * @brief Keeps the last lines in memory for the editor's Console panel. Editor-only:
 * nothing in the engine itself needs a queryable log history, so this doesn't live in
 * libsbx::utility.
 */
class console_sink final : public spdlog::sinks::base_sink<std::mutex> {

public:

  explicit console_sink(const std::size_t max_lines = 256u)
  : _lines{max_lines} { }

  [[nodiscard]] auto lines() -> std::vector<log_line> {
    auto lock = std::lock_guard{mutex_};

    return {_lines.begin(), _lines.end()};
  }

  auto clear() -> void {
    auto lock = std::lock_guard{mutex_};

    _lines.clear();
  }

protected:

  auto sink_it_(const spdlog::details::log_msg& msg) -> void override {
    auto formatted = spdlog::memory_buf_t{};

    formatter_->format(msg, formatted);

    _lines.push(log_line{fmt::to_string(formatted), msg.level});
  }

  auto flush_() -> void override { }

private:

  sbx::containers::ring_buffer<log_line> _lines;

}; // class console_sink

/** @brief The editor's single console sink instance. */
[[nodiscard]] inline auto console_sink_instance() -> const std::shared_ptr<console_sink>& {
  static auto instance = std::make_shared<console_sink>();

  return instance;
}

/**
 * @brief Registers the console sink so it starts receiving log messages. Must be called
 * before the first log call of the process — see sbx::utility::add_sink. Call this first
 * thing in main(), before the engine (and its modules) are constructed.
 */
inline auto install_console_sink() -> void {
  sbx::utility::add_sink(console_sink_instance());
}

} // namespace editor

#endif // EDITOR_CONSOLE_SINK_HPP_
