// SPDX-License-Identifier: MIT
#include <demo/application.hpp>

namespace demo {

application::application()
: sbx::core::application{},
  _is_paused{false} {

 
}

auto application::update() -> void {
  
}

auto application::fixed_update() -> void {

}

auto application::is_paused() const -> bool {
  return _is_paused;
}

} // namespace demo
