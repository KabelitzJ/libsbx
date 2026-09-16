// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/pipeline/async_shader_compiler.hpp>

#include <fstream>
#include <system_error>
#include <utility>

#include <fmt/format.h>

#include <libsbx/utility/profiler.hpp>

namespace sbx::graphics {

async_shader_compiler::async_shader_compiler() {
  _thread = std::thread{[this] { _worker_loop(); }};
}

async_shader_compiler::~async_shader_compiler() {
  _abort();
}

auto async_shader_compiler::submit(request compile_request) -> void {
  if (_aborted.load(std::memory_order_relaxed)) {
    return;
  }

  {
    auto lock = std::lock_guard{_request_mutex};
    _pending[compile_request.key] = std::move(compile_request); // coalesce: replaces any not-yet-picked-up entry for this key
  }

  _request_condition.notify_one();
}

auto async_shader_compiler::take_results(std::size_t max_count) -> std::vector<result> {
  auto out = std::vector<result>{};

  auto lock = std::lock_guard{_result_mutex};

  while (max_count > 0u && !_results.empty()) {
    out.push_back(std::move(_results.back()));
    _results.pop_back();
    --max_count;
  }

  return out;
}

auto async_shader_compiler::_abort() -> void {
  const auto already_aborted = _aborted.exchange(true, std::memory_order_relaxed);

  if (already_aborted) {
    if (_thread.joinable()) {
      _thread.join();
    }

    return;
  }

  {
    auto lock = std::lock_guard{_request_mutex};
    _pending.clear();
  }

  _request_condition.notify_one();

  if (_thread.joinable()) {
    _thread.join();
  }
}

auto async_shader_compiler::_worker_loop() -> void {
  SBX_PROFILE_THREAD_NAME("Shader Preview Compile");

  while (true) {
    auto current = request{};

    {
      auto lock = std::unique_lock{_request_mutex};

      _request_condition.wait(lock, [this] { return _aborted.load(std::memory_order_relaxed) || !_pending.empty(); });

      if (_aborted.load(std::memory_order_relaxed)) {
        return;
      }

      auto entry = _pending.begin();
      current = std::move(entry->second);
      _pending.erase(entry);
    }

    auto out = result{current.key};

    auto error = std::error_code{};

    if (const auto parent = current.path.parent_path(); !parent.empty()) {
      std::filesystem::create_directories(parent, error);
    }

    if (auto file = std::ofstream{current.path, std::ios::binary}; file) {
      file << current.source;
      file.close();

      try {
        out.compiled = _compiler.compile(current.path, current.entry_points);
        out.success = true;
      } catch (const std::exception& exception) {
        out.error = exception.what();
      }
    } else {
      out.error = fmt::format("could not write '{}'", current.path.generic_string());
    }

    if (_aborted.load(std::memory_order_relaxed)) {
      continue; // shutting down -- nothing will ever drain this, discard rather than push it
    }

    auto lock = std::lock_guard{_result_mutex};
    _results.push_back(std::move(out));
  }
}

} // namespace sbx::graphics
