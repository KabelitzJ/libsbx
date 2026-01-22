// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_COMPONENTS_TAG_HPP_
#define LIBSBX_SCENES_COMPONENTS_TAG_HPP_

#include <string>

#include <libsbx/utility/hashed_string.hpp>

namespace sbx::scenes {

template<typename Char>
class basic_tag final : public utility::basic_hashed_string<Char> {

  using base = utility::basic_hashed_string<Char>;

public:

  template<typename... Args>
  basic_tag(Args&&... args)
  : base{std::forward<Args>(args)...} { }

}; // class tag

using tag = basic_tag<char>;

} // namespace sbx::scenes

template<typename Char>
struct fmt::formatter<sbx::scenes::basic_tag<Char>> : fmt::formatter<sbx::utility::basic_hashed_string<Char>> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const sbx::scenes::basic_tag<Char>& tag, FormatContext& ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", tag.c_str());
  }

}; // struct fmt::formatter

#endif // LIBSBX_SCENES_COMPONENTS_TAG_HPP_
