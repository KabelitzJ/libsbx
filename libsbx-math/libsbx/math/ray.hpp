#ifndef LIBSBX_MATH_RAY_HPP_
#define LIBSBX_MATH_RAY_HPP_

#include <libsbx/math/vector3.hpp>

namespace sbx::math {

class ray {

public:

  ray() : 
  _origin{vector3::zero}, 
  _direction{vector3::forward} { }

  ray(const vector3& origin, const vector3& direction)
  : _origin{origin}, 
    _direction{vector3::normalized(direction)} { }

  auto origin() const -> const vector3& { 
    return _origin; 
  }

  auto direction() const -> const vector3& { 
    return _direction; 
  }

  vector3 point_at(const std::float_t t) const {
    return _origin + _direction * t;
  }

private:

  vector3 _origin;
  vector3 _direction;

}; // class ray

} // namespace sbx::math

#endif // LIBSBX_MATH_RAY_HPP_