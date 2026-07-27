// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <span>
#include <vector>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/exit.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scripting/scripting_module.hpp>

#include <libsbx/render/render_module.hpp>

#include <libsbx/ecs/registry.hpp>

#include <demo/application.hpp>

constexpr auto tracy_depth = 10;

auto _aligned_malloc(std::size_t count, std::size_t alignment) -> void* {
  auto rounded = (count + alignment - 1) & ~(alignment - 1);

  return std::aligned_alloc(alignment, rounded);
}

auto operator new(std::size_t count) -> void* {
  auto ptr = std::malloc(count);
  if (!ptr) {
    throw std::bad_alloc{};
  }
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new[](std::size_t count) -> void* {
  auto ptr = std::malloc(count);
  if (!ptr) {
    throw std::bad_alloc{};
  }
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new(std::size_t count, std::align_val_t alignment) -> void* {
  auto ptr = _aligned_malloc(count, static_cast<std::size_t>(alignment));
  if (!ptr) {
    throw std::bad_alloc{};
  }
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new[](std::size_t count, std::align_val_t alignment) -> void* {
  auto ptr = _aligned_malloc(count, static_cast<std::size_t>(alignment));
  if (!ptr) {
    throw std::bad_alloc{};
  }
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

// nothrow new

auto operator new(std::size_t count, std::nothrow_t const&) noexcept -> void* {
  auto ptr = std::malloc(count);
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new[](std::size_t count, std::nothrow_t const&) noexcept -> void* {
  auto ptr = std::malloc(count);
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new(std::size_t count, std::align_val_t alignment, std::nothrow_t const&) noexcept -> void* {
  auto ptr = _aligned_malloc(count, static_cast<std::size_t>(alignment));
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new[](std::size_t count, std::align_val_t alignment, std::nothrow_t const&) noexcept -> void* {
  auto ptr = _aligned_malloc(count, static_cast<std::size_t>(alignment));
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

// delete

auto operator delete(void* ptr) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete[](void* ptr) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete(void* ptr, [[maybe_unused]] std::size_t size) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::size_t size) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

// aligned delete

auto operator delete(void* ptr, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete(void* ptr, [[maybe_unused]] std::size_t size, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::size_t size, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

// nothrow delete (called only if the matching nothrow new's ctor throws)

auto operator delete(void* ptr, std::nothrow_t const&) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete[](void* ptr, std::nothrow_t const&) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete(void* ptr, [[maybe_unused]] std::align_val_t alignment, std::nothrow_t const&) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::align_val_t alignment, std::nothrow_t const&) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

using module_list = sbx::core::module_list<
  sbx::platform::platform_module,
  sbx::filesystem::filesystem_module,
  sbx::graphics::graphics_module,
  sbx::assets::assets_module,
  sbx::scenes::scenes_module,
  sbx::scripting::scripting_module,
  sbx::render::render_module
>;

auto main(int argc, const char** argv) -> int {
  auto args = std::vector<std::string_view>{argv, argv + argc};

  try {
    auto engine = sbx::core::basic_engine<module_list>{args};

    engine.run<demo::application>();
  } catch (const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure;
  }

  return sbx::core::exit::success;
}
