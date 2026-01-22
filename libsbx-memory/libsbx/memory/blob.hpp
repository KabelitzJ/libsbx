// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MEMORY_BLOB_HPP_
#define LIBSBX_MEMORY_BLOB_HPP_

#include <memory>

namespace sbx::memory {

using blob = std::shared_ptr<std::uint8_t[]>;

inline auto make_blob(const std::uint8_t* data, const std::size_t size) -> blob {
  auto blob = std::make_shared<std::uint8_t[]>(size);

  if (data != nullptr && size > 0u) {
    std::memcpy(blob.get(), data, size);
  }

  return blob;
}


} // namespace sbx::memory

#endif // LIBSBX_MEMORY_BLOB_HPP_
