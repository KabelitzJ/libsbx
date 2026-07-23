// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSET_HANDLE_HPP_
#define LIBSBX_ASSETS_ASSET_HANDLE_HPP_

#include <memory>

namespace sbx::assets {

/**
 * @brief A ref-counted handle to a loaded asset. Holding one keeps the asset alive.
 */
template<typename Type>
class asset_handle {

public:

  using value_type = const std::remove_cvref_t<Type>;
  using pointer = value_type*;
  using reference = value_type&;

  asset_handle() = default;

  explicit asset_handle(std::shared_ptr<value_type> record)
  : _record{std::move(record)} { }

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return _record != nullptr;
  }
  
  [[nodiscard]] operator bool() const noexcept {
    return is_valid();
  }

  [[nodiscard]] auto operator->() const noexcept -> pointer {
    return _record.get();
  }

  [[nodiscard]] auto operator*() const noexcept -> reference {
    return *_record;
  }

private:

  std::shared_ptr<const Type> _record{};

}; // class asset_handle

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_HANDLE_HPP_
