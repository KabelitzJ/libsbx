// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_MATERIAL_IMPORTER_HPP_
#define LIBSBX_MODELS_MATERIAL_IMPORTER_HPP_

#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/reflection/description.hpp>

#include <libsbx/assets/asset.hpp>
#include <libsbx/assets/serializer_registry.hpp>
#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/texture.hpp>

#include <libsbx/models/material.hpp>
#include <libsbx/models/vertex_stream.hpp>

namespace sbx::models {

/**
 * @brief Imports a `.material.yaml` file into a material.
 *
 * Each texture slot's `image:` URI is loaded through the assets_module, so the texture becomes a dependency of this material. The legacy per-slot `format:` seeds the texture's color space only when that texture has no .meta yet; thereafter the texture's own .meta governs it.
 */
class material_importer final : public assets::serializer<material_importer> {

  inline static const auto is_registered = register_serializer({".material.yaml"});

public:

  auto type() const -> std::string_view override;

  auto read(const assets::serializer_context& context) -> std::unique_ptr<assets::asset_base> override;

  auto write(const assets::serializer_context& context, const std::unique_ptr<assets::asset_base>& asset) -> bool override;

private:

  static auto _resolve_texture(assets::assets_module& assets_module, std::vector<math::uuid>& dependencies, const YAML::Node& node) -> graphics::image2d_handle;

  static auto _parse_alpha_mode(const std::string& value) -> models::alpha_mode;

}; // class material_importer

} // namespace sbx::models

#endif // LIBSBX_MODELS_MATERIAL_IMPORTER_HPP_
