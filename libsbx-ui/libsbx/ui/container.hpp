// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_CONTAINER_HPP_
#define LIBSBX_UI_CONTAINER_HPP_

#include <vector>
#include <memory>

#include <libsbx/ui/layout.hpp>
#include <libsbx/ui/widget.hpp>

namespace sbx::ui {

class container {

public:

  container(std::unique_ptr<layout> layout) noexcept
  : _layout{std::move(layout)} { }

  template<typename Widget, typename... Args>
  requires (std::is_base_of_v<widget, Widget>, std::is_constructible_v<Widget, Args...>)
  auto add_widget(Args&&... args) -> memory::observer_ptr<Widget> {
    auto widget = std::make_unique<Widget>(std::forward<Args>(args)...);

    auto reference = memory::make_observer<Widget>(widget.get());

    _widgets.push_back(std::move(widget));

    return reference;
  }

  auto widgets() const noexcept -> const std::vector<std::unique_ptr<widget>>& {
    return _widgets;
  }

  template<typename Layout, typename... Args>
  requires (std::is_base_of_v<layout, Layout>, std::is_constructible_v<Layout, Args...>)
  auto set_layout(Args&&... args) -> void {
    _layout = std::make_unique<Layout>(std::forward<Args>(args)...);
  }

private:

  std::vector<std::unique_ptr<widget>> _widgets;
  std::vector<container> _sub_containers;
  std::unique_ptr<layout> _layout;

}; // class container

} // namespace sbx::ui

#endif // LIBSBX_UI_CONTAINER_HPP_
