// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_IMPORTER_HPP_
#define LIBSBX_ASSETS_IMPORTER_HPP_

#include <filesystem>
#include <memory>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/asset.hpp>

namespace sbx::assets {

/**
 * @brief Per-import state passed to an importer.
 *
 * `source` is the engine-relative URI (e.g. `res://textures/base.png`).
 * `resolved` is the absolute on-disk path used for actual file IO.
 * `settings` is the `import` subtree from the asset's .meta file (may be Null).
 * `id` is the UUID assigned by the database for this asset; importers may use it for diagnostics or to register dependency edges against the module.
 * `module` is the owning assets_module, used by composite importers (e.g. a material importer importing its texture dependencies).
 */
struct import_context {
  std::filesystem::path source;
  std::filesystem::path resolved;
  YAML::Node settings;
  math::uuid id;
}; // struct import_context

/**
 * @brief Stateless transformer from a source file to a typed asset payload.
 *
 * One importer instance can be registered for multiple file extensions. The importer must not retain references to the import_context after returning.
 */
class importer {

public:

  virtual ~importer() = default;

  /**
   * @brief Stable type tag for the produced asset. Must match the payload's asset_base::asset_type() and the record's `type` field.
   */
  virtual auto type() const -> std::string_view = 0;

  /**
   * @brief Reads `context.resolved` from disk and returns the typed payload.
   *
   * @throws sbx::utility::runtime_error on IO or parse failure.
   */
  virtual auto import(const import_context& context) -> std::unique_ptr<asset_base> = 0;

}; // class importer

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_IMPORTER_HPP_
