// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_METADATA_HPP_
#define LIBSBX_ASSETS_METADATA_HPP_

#include <filesystem>

#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/math/uuid.hpp>

namespace sbx::assets {

struct metadata {
  math::uuid id;
  std::filesystem::path path;
}; // struct metadata

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_METADATA_HPP_
