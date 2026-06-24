// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_MATERIAL_SERIALIZER_HPP_
#define LIBSBX_MODELS_MATERIAL_SERIALIZER_HPP_

#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

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
 * @brief Reads and writes `.material.yaml` files.
 *
 * On read, each texture slot's `image:` URI is loaded through the assets_module; the slot keeps the texture's uuid as its serializable identity and the resolved image2d_handle as a render-time cache. Write is the exact inverse: each slot's texture uuid is resolved back to a source and emitted as `image: <source>`.
 */
class material_serializer final : public assets::serializer<material_serializer> {

  inline static const auto is_registered = register_serializer({".material.yaml"});

public:

  auto type() const -> std::string_view override;

  auto read(const assets::serializer_context& context) -> std::unique_ptr<assets::asset_base> override;

  auto write(const assets::serializer_context& context, const std::unique_ptr<assets::asset_base>& asset) -> bool override;

private:

  static auto _resolve_texture(assets::assets_module& assets_module, const YAML::Node& node) -> texture_slot;

  static auto _encode_texture(assets::assets_module& assets_module, YAML::Node& parent, const std::string& key, const texture_slot& slot) -> void;

  static auto _parse_alpha_mode(const std::string& value) -> models::alpha_mode;

  static auto _alpha_mode_name(models::alpha_mode value) -> std::string;

}; // class material_serializer

} // namespace sbx::models

#endif // LIBSBX_MODELS_MATERIAL_SERIALIZER_HPP_