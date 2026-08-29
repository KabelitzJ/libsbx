// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCRIPTING_SCRIPT_COMPILER_HPP_
#define LIBSBX_SCRIPTING_SCRIPT_COMPILER_HPP_

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/scripting/managed/runtime.hpp>

namespace sbx::scripting {

/**
 * @brief Compiles every `*.cs` file under the project's assets directory into one assembly
 * (compile_if_stale), caching the result under `.sbx/library/scripts/` — mirrors
 * asset_cooker::_is_cooked_stale's mtime-fast-path/hash-fallback manifest pattern
 * (libsbx/assets/asset_cooker.cpp), just as one N:1 (sources -> assembly) manifest instead of
 * per-asset 1:1 entries. Owned by scripting_module, which loads output_path() into its game
 * assembly_load_context once compilation succeeds.
 *
 * A failed compile never touches the previously-built output_path() — see compile_if_stale's
 * doc comment — so the engine keeps running on the last-known-good assembly, matching Unity's
 * "compile errors don't wipe out what was working" behavior.
 */
class script_compiler final : public utility::noncopyable {

public:

  /**
   * @brief Recompiles if any `*.cs` source or @p core_assembly_path (Sbx.Core.dll, the compile
   * reference — its own changes force a recompile too, since the interop ABI it exposes may have
   * moved) changed since the last successful compile, or output_path() doesn't exist yet.
   * No-op (keeps the existing output_path()) if nothing changed. Compiles to a scratch file and
   * only replaces output_path() on success — a compile error leaves the last-good assembly in
   * place; see last_compile_succeeded().
   */
  auto compile_if_stale(managed::runtime& runtime, const std::filesystem::path& core_assembly_path) -> void;

  /** @brief Whether the most recent compile_if_stale() call (or the absence of any *.cs files) left a usable assembly at output_path(). */
  [[nodiscard]] auto last_compile_succeeded() const noexcept -> bool {
    return _last_compile_succeeded;
  }

  /** @brief `.sbx/library/scripts/Game.dll` — where the compiled game assembly lives once compile_if_stale() has succeeded at least once. */
  [[nodiscard]] auto output_path() const -> std::filesystem::path;

private:

  struct source_entry {
    std::uint64_t hash{0u};
    std::int64_t mtime{0};
  }; // struct source_entry

  /**
   * @brief (Re)writes assets_directory/Game.csproj — an SDK-style project referencing
   * core_assembly_path (Sbx.Core.dll) purely so an IDE (VS Code/OmniSharp, Rider, Visual Studio)
   * resolves the Sbx.Core namespace for autocomplete. Never built by the engine — it globs the
   * same *.cs sources compile_if_stale() itself walks and compiles in-process via Roslyn. Skips
   * the write if the file's content wouldn't change, so an IDE watching its mtime isn't nudged
   * into reloading the project on every engine start.
   */
  auto _write_ide_project(const std::filesystem::path& assets_directory, const std::filesystem::path& core_assembly_path) -> void;

  [[nodiscard]] auto _manifest_path() const -> std::filesystem::path;

  [[nodiscard]] auto _is_stale(const std::vector<std::filesystem::path>& sources, std::uint64_t core_assembly_hash) -> bool;

  auto _record_manifest(const std::vector<std::filesystem::path>& sources, std::uint64_t core_assembly_hash) -> void;

  bool _manifest_loaded{false};
  std::uint32_t _manifest_compiler_version{0u};
  std::uint64_t _manifest_core_assembly_hash{0u};
  std::unordered_map<std::string, source_entry> _manifest_sources; // keyed by path relative to assets_directory()

  bool _last_compile_succeeded{true}; // no sources yet -> trivially "succeeded", doesn't block Play

}; // class script_compiler

} // namespace sbx::scripting

#endif // LIBSBX_SCRIPTING_SCRIPT_COMPILER_HPP_
