// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_COMPONENTS_ID_HPP_
#define LIBSBX_SCENES_COMPONENTS_ID_HPP_

#include <libsbx/math/uuid.hpp>

namespace sbx::scenes {

class id final : public math::uuid {

  using base = math::uuid;

public:

  id()
  : base{} { }

  id(const base& other)
  : base{other} { }

}; // class id

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_COMPONENTS_ID_HPP_
