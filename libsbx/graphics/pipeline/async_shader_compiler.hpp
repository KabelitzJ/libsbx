// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_ASYNC_SHADER_COMPILER_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_ASYNC_SHADER_COMPILER_HPP_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::graphics {

/**
 * @brief Compiles Slang source off the render thread, for callers (shader-graph previews) that
 * need to recompile on every structural edit without blocking on shader_compiler's own
 * render-thread-only compile (see shader_cache's doc comment for why that one can't move).
 *
 * Owns its own shader_compiler instance -- its own Slang global session, entirely separate from
 * graphics_module's -- so callers never have to reason about whether Slang's session objects are
 * safe to use from two threads at once: none ever are.
 *
 * Requests are coalesced by key: submit() replaces any not-yet-picked-up pending request for the
 * same key rather than queuing a duplicate, so a burst of edits to the same target only ever
 * compiles the latest one once the worker gets to it. Mirrors asset_loader's own
 * thread+mutex-guarded-deque+atomic-abort shape (libsbx/assets/asset_loader.hpp) -- same proven
 * pattern, just single-request-type and coalesced instead of per-asset-type and FIFO.
 */
class async_shader_compiler final : public utility::noncopyable {

public:

  using key_type = std::uint64_t;

  struct request {
    key_type key;
    std::filesystem::path path; // where `source` is written before compiling -- must sit under a "shaders" tree, see shader_compiler.cpp's _shaders_root
    std::string source;
    std::vector<shader_compiler::entry_point_request> entry_points;
  }; // struct request

  struct result {
    key_type key{0u};
    bool success{false};
    std::string error{};                                         // valid only if !success
    std::vector<shader_compiler::compiled_entry_point> compiled{}; // valid only if success
  }; // struct result

  async_shader_compiler();

  ~async_shader_compiler();

  auto submit(request compile_request) -> void;

  [[nodiscard]] auto take_results(std::size_t max_count = 8u) -> std::vector<result>;

private:

  auto _abort() -> void;

  auto _worker_loop() -> void;

  std::atomic<bool> _aborted{false};

  std::mutex _request_mutex{};
  std::condition_variable _request_condition{};
  std::unordered_map<key_type, request> _pending{};

  std::mutex _result_mutex{};
  std::vector<result> _results{};

  std::thread _thread{};

  shader_compiler _compiler{};

}; // class async_shader_compiler

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_ASYNC_SHADER_COMPILER_HPP_
