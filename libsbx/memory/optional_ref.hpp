#ifndef LIBSBX_MEMORY_OPTIONAL_REF_HPP_
#define LIBSBX_MEMORY_OPTIONAL_REF_HPP_

namespace sbx::memory {

template<typename Type>
class optional_ref {

public:

  using value_type = Type;

  constexpr optional_ref() noexcept
  : _pointer{nullptr} {}

  constexpr optional_ref(value_type& value) noexcept
  : _pointer{std::addressof(value)} {}

  constexpr optional_ref(std::nullopt_t) noexcept
  : _pointer{nullptr} {}

  optional_ref(value_type&&) = delete;

  optional_ref(value_type*) = delete;

  auto operator=(value_type& value) noexcept -> optional_ref& {
    _pointer = std::addressof(value);
    return *this;
  }

  auto operator=(std::nullopt_t) noexcept -> optional_ref& {
    _pointer = nullptr;
    return *this;
  }

  auto operator=(value_type*) -> optional_ref& = delete;

  [[nodiscard]] auto has_value() const noexcept -> bool {
    return _pointer != nullptr;
  }

  explicit operator bool() const noexcept {
    return has_value();
  }

  [[nodiscard]] auto value() const -> Type& {
    if (!_pointer) {
      throw std::bad_optional_access{};
    }

    return *_pointer;
  }

  [[nodiscard]] auto operator*() const noexcept -> Type& {
    return *_pointer;
  }

  [[nodiscard]] auto operator->() const noexcept -> Type* {
    return _pointer;
  }

  template<typename U>
  [[nodiscard]] auto value_or(U&& fallback) const noexcept -> Type& {
    return _pointer ? *_pointer : static_cast<Type&>(fallback);
  }

  auto reset() noexcept -> void {
    _pointer = nullptr;
  }

private:

  Type* _pointer;

}; // class optional_ref

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_OPTIONAL_REF_HPP_
