// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_MANAGED_GARBAGE_COLLECTION_HPP_
#define LIBSBX_SCRIPTING_MANAGED_GARBAGE_COLLECTION_HPP_

#include <cstdint>

namespace sbx::scripting::managed {

struct garbage_collection {

  enum class mode : std::uint8_t {
    standart,    // Default is the same as using Forced directly
    forced,     // Forces the garbage collection to occur immediately
    optimized,  // Allows the garbage collector to determine whether it should reclaim objects right now
    aggressive  // Requests that the garbage collector decommit as much memory as possible
  }; // enum class mode

  static auto collect() -> void;

  static auto collect(std::int32_t generation, mode collection_mode = mode::standart, bool is_blocking = true, bool compacting = false) -> void;

  static auto wait_for_pending_finalizers() -> void;

}; // struct garbage_collection

} // namespace sbx::scripting::managed

#endif // LIBSBX_SCRIPTING_MANAGED_GARBAGE_COLLECTION_HPP_