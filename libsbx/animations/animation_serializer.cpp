// SPDX-License-Identifier: MIT
#include <libsbx/animations/animation_serializer.hpp>

#include <libsbx/utility/exception.hpp>

#include <libsbx/animations/animation.hpp>

namespace sbx::animations {

auto animation_serializer::type() const -> std::string_view {
  return std::string_view{"animation"};
}

auto animation_serializer::enumerate(const assets::serializer_context& context) -> std::vector<assets::sub_asset_info> {
  auto result = std::vector<assets::sub_asset_info>{};

  for (auto& name : animation::clip_names(context.source)) {
    result.push_back(assets::sub_asset_info{.sub_id = std::string{sub_id_prefix} + name, .type = std::string{"animation"}});
  }

  return result;
}

auto animation_serializer::owns(const assets::serializer_context& context, std::string_view sub_id) -> bool {
  static_cast<void>(context);

  return sub_id.starts_with(sub_id_prefix);
}

auto animation_serializer::read(const assets::serializer_context& context) -> std::unique_ptr<assets::asset> {
  if (!context.sub_id.starts_with(sub_id_prefix)) {
    throw utility::runtime_error{"animation_serializer cannot read sub-asset '{}'", context.sub_id};
  }

  const auto name = context.sub_id.substr(sub_id_prefix.size());

  return std::make_unique<animation>(context.source, name);
}

auto animation_serializer::write(const assets::serializer_context& context, const std::unique_ptr<assets::asset>& asset) -> bool {
  static_cast<void>(context);
  static_cast<void>(asset);

  return false;
}

} // namespace sbx::animations
