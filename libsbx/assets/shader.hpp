// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_SHADER_HPP_
#define LIBSBX_ASSETS_SHADER_HPP_

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <libsbx/math/uuid.hpp>

#include <libsbx/reflection/enum.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>
#include <libsbx/assets/shader_property.hpp>

namespace sbx::assets {

/** @brief What a `.slang` asset is for, from the entry points it defines. */
enum class [[=reflection::named]] shader_kind : std::uint8_t {
  library, // no known entry points; only #included by other shaders
  surface, // vertex_main + fragment_main; a material can render with it
  compute  // compute_main; a script can dispatch it
}; // enum class shader_kind

/**
 * @brief A hand-written `.slang` file from the project's assets.
 *
 * Holds only what reflection found; the render passes compile it through shader_cache on first use.
 * generation() bumps every time the file is re-reflected after an edit, and is part of the shader_cache key so the next draw compiles the new source.
 */
class shader final : public loadable {

  friend class asset_residency;

public:

  shader() = default;

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  /** @brief The absolute path of the source file. */
  [[nodiscard]] auto path() const noexcept -> const std::filesystem::path& {
    return _path;
  }

  [[nodiscard]] auto kind() const noexcept -> shader_kind {
    return _kind;
  }

  /** @brief Whether it defines depth_vertex_main and depth_fragment_main, which the depth pre-pass and shadow pass need. */
  [[nodiscard]] auto has_depth_entry_points() const noexcept -> bool {
    return _has_depth_entry_points;
  }

  /** @brief The material inputs declared by its `properties` struct, in declaration order. Main thread only: rewritten on reload. */
  [[nodiscard]] auto properties() const noexcept -> const std::vector<shader_property>& {
    return _properties;
  }

  /** @brief The generated load_properties appended to the source when it compiles, or null without a `properties` struct. Safe from the render thread. */
  [[nodiscard]] auto material_loader() const -> std::shared_ptr<const std::string> {
    return _material_loader.load();
  }

  /** @brief Slang's diagnostics if the file didn't parse, or why its `properties` struct is unusable, else empty. A shader with an error is a library and never renders. */
  [[nodiscard]] auto error() const noexcept -> const std::string& {
    return _error;
  }

private:

  [[nodiscard]] auto _depends_on(const std::filesystem::path& file) const -> bool {
    return std::ranges::find(_dependencies, file) != _dependencies.end();
  }

  auto _finalize_content(shader_kind kind, bool has_depth_entry_points, std::vector<shader_property> properties, std::string material_loader, std::string error) -> void {
    _kind = kind;
    _has_depth_entry_points = has_depth_entry_points;
    _properties = std::move(properties);
    _material_loader.store(material_loader.empty() ? nullptr : std::make_shared<const std::string>(std::move(material_loader)));
    _error = std::move(error);
    _bump_generation();
  }

  math::uuid _id{math::uuid::nil()};
  std::filesystem::path _path{};
  shader_kind _kind{shader_kind::library};
  bool _has_depth_entry_points{false};
  std::vector<shader_property> _properties{};
  std::atomic<std::shared_ptr<const std::string>> _material_loader{};
  std::string _error{};
  std::vector<std::filesystem::path> _dependencies{}; // normalized; set with each reflection, read by asset_residency on the main thread

}; // class shader

using shader_handle = asset_handle<shader>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SHADER_HPP_
