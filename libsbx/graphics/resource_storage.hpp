// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_RESOURSE_STORAGE_HPP_
#define LIBSBX_GRAPHICS_RESOURSE_STORAGE_HPP_

#include <vector>
#include <memory>
#include <unordered_set>
#include <filesystem>

#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/assert.hpp>

#include <libsbx/memory/aligned_storage.hpp>

namespace sbx::graphics {

template<typename Type>
class resource_handle {

public:

  using type = Type;

  inline static constexpr auto invalid = std::uint32_t{0x00FFFFFF};

  constexpr resource_handle()
  : _handle{invalid}, 
    _generation{0} { }

  constexpr resource_handle(const std::uint32_t handle, const std::uint8_t generation)
  : _handle{handle}, 
    _generation{generation} { }

  constexpr auto handle() const noexcept -> std::uint32_t {
    return _handle;
  }

  constexpr auto generation() const noexcept -> std::uint8_t {
    return _generation;
  }

  constexpr auto operator==(const resource_handle& other) const noexcept -> bool {
    return std::memcmp(this, &other, sizeof(std::uint32_t)) == 0; 
  }

  constexpr auto is_valid() const noexcept -> bool {
    return _handle != invalid;
  }

  constexpr operator bool() const noexcept {
    return is_valid();
  }

private:

  std::uint32_t _handle     : 24;
  std::uint32_t _generation :  8;

}; // class resource_handle

struct resource_metadata {
  std::filesystem::path path;
}; // struct resource_metadata

template<typename Type>
class resource_storage {

public:

  using value_type = Type;
  using size_type = std::size_t;
  using handle_type = resource_handle<value_type>;

  resource_storage() {
    _storage.reserve(32u);
    _generations.reserve(32u);
    _free_handles.reserve(32u);
  }

  resource_storage(const resource_storage& other) = delete;

  ~resource_storage() {
    clear();
  }

  auto operator=(const resource_storage& other) -> resource_storage& = delete;

  template<typename... Args>
  requires (std::is_constructible_v<value_type, Args...>)
  auto emplace(Args&&... args) -> handle_type {
    if (!_free_handles.empty()) {
      const auto handle = _free_handles.back();
      _free_handles.pop_back();

      std::construct_at(_ptr(handle), std::forward<Args>(args)...);

      const auto generation = ++_generations[handle];

      return handle_type{handle, generation};
    }

    const auto handle = static_cast<std::uint32_t>(_storage.size());

    _storage.emplace_back();
    std::construct_at(_ptr(handle), std::forward<Args>(args)...);

    _generations.push_back(0u);

    return handle_type{handle, _generations[handle]};
  }

  auto get(const handle_type& handle) -> value_type& {
    utility::assert_that(handle.handle() < _storage.size(), "Handle is out of bounds");
    utility::assert_that(handle.generation() == _generations[handle.handle()], "Handle generation does not match");

    return *_ptr(handle.handle());
  }

  auto get(const handle_type& handle) const -> const value_type& {
    utility::assert_that(handle.handle() < _storage.size(), "Handle is out of bounds");
    utility::assert_that(handle.generation() == _generations[handle.handle()], "Handle generation does not match");

    return *_ptr(handle.handle());
  }

  auto remove(const handle_type& handle) -> void {
    utility::assert_that(handle.handle() < _storage.size(), "Handle is out of bounds");
    utility::assert_that(handle.generation() == _generations[handle.handle()], "Handle generation does not match");

    std::destroy_at(_ptr(handle.handle()));
    _free_handles.push_back(handle.handle());
  }

  auto clear() -> void {
    auto free_list = std::unordered_set<std::uint32_t>{_free_handles.begin(), _free_handles.end()};

    for (std::uint32_t i = 0; i < _storage.size(); ++i) {
      if (free_list.contains(i)) {
        continue;
      }

      std::destroy_at(_ptr(i));
    }

    _storage.clear();
    _generations.clear();
    _free_handles.clear();
  }

private:

  auto _ptr(const size_type index) -> value_type* {
    return std::launder(reinterpret_cast<value_type*>(_storage.data() + index));
  }

  auto _ptr(const size_type index) const -> const value_type* {
    return std::launder(reinterpret_cast<const value_type*>(_storage.data() + index));
  }

  std::vector<memory::storage_for_t<value_type>> _storage;
  std::vector<std::uint8_t> _generations;
  std::vector<std::uint32_t> _free_handles;

}; // class resource_storage

} // namespace sbx::graphics

template<typename Type>
struct std::hash<sbx::graphics::resource_handle<Type>> {

  auto operator()(const sbx::graphics::resource_handle<Type>& handle) const noexcept -> std::size_t {
    auto hash = std::size_t{};

    sbx::utility::hash_combine(hash, handle.handle(), handle.generation());

    return hash;
  }

}; // struct std::hash

#endif // LIBSBX_GRAPHICS_RESOURSE_STORAGE_HPP_
