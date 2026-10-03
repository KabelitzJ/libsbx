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
 * @brief Compiles every `*.cs` file under the project's assets directory into one cached assembly under `.sbx/library/scripts/`.
 *
 * A failed compile never replaces the previous output, so the engine keeps running on the last good assembly.
 */
class script_compiler final : public utility::noncopyable {

public:

  /**
   * @brief Recompiles if any source, @p core_assembly_path, or the output changed since the last successful compile.
   *
   * A compile error leaves the last good assembly in place; see last_compile_succeeded().
   *
   * @param runtime The managed runtime that runs the compiler.
   * @param core_assembly_path Sbx.Core.dll, the compile reference.
   */
  auto compile_if_stale(managed::runtime& runtime, const std::filesystem::path& core_assembly_path) -> void;

  /**
   * @brief Whether the last compile_if_stale() call (or having no scripts) left a usable assembly.
   *
   * @return True if output_path() is usable.
   */
  [[nodiscard]] auto last_compile_succeeded() const noexcept -> bool {
    return _last_compile_succeeded;
  }

  /**
   * @brief Where the compiled game assembly lives.
   *
   * @return `.sbx/library/scripts/Game.dll`.
   */
  [[nodiscard]] auto output_path() const -> std::filesystem::path;

private:

  struct source_entry {
    std::uint64_t hash{0u};
    std::int64_t mtime{0};
  }; // struct source_entry

  /**
   * @brief Writes assets_directory/Game.csproj so IDEs resolve Sbx.Core for autocomplete; the engine never builds it.
   *
   * Skips the write when the content is unchanged so IDEs don't reload. Build output goes to @p ide_output_directory to keep bin/obj out of the Asset Browser.
   *
   * @param assets_directory The project's assets directory.
   * @param ide_output_directory Where the IDE's own build output goes.
   * @param core_assembly_path Sbx.Core.dll, referenced by the project.
   */
  auto _write_ide_project(const std::filesystem::path& assets_directory, const std::filesystem::path& ide_output_directory, const std::filesystem::path& core_assembly_path) -> void;

  [[nodiscard]] auto _manifest_path() const -> std::filesystem::path;

  [[nodiscard]] auto _is_stale(const std::vector<std::filesystem::path>& sources, std::uint64_t core_assembly_hash) -> bool;

  auto _record_manifest(const std::vector<std::filesystem::path>& sources, std::uint64_t core_assembly_hash) -> void;

  bool _manifest_loaded{false};
  std::uint32_t _manifest_compiler_version{0u};
  std::uint64_t _manifest_core_assembly_hash{0u};
  std::unordered_map<std::string, source_entry> _manifest_sources; // keyed by path relative to the assets directory

  bool _last_compile_succeeded{true}; // no sources counts as success, so Play isn't blocked

}; // class script_compiler

} // namespace sbx::scripting

#endif // LIBSBX_SCRIPTING_SCRIPT_COMPILER_HPP_
