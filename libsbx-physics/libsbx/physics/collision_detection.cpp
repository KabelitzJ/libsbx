#include <libsbx/physics/collision_detection.hpp>

namespace sbx::physics {

static auto find_furthest_point(const math::vector3& direction, const box& box, const math::vector3& position, const math::matrix3x3& rotation_scale) -> math::vector3 {
  const auto inverse = math::matrix3x3::inverted(rotation_scale);

  const auto local_direction = math::vector3{inverse * direction};

  auto result = math::vector3{};

  result.x() = (local_direction.x() > 0.0f) ? box.half_extents.x() : -box.half_extents.x();
  result.y() = (local_direction.y() > 0.0f) ? box.half_extents.y() : -box.half_extents.y();
  result.z() = (local_direction.z() > 0.0f) ? box.half_extents.z() : -box.half_extents.z();

  return math::vector3{rotation_scale * result} + position;
}

static auto find_furthest_point(const math::vector3& direction, const sphere& sphere, const math::vector3& position, const math::matrix3x3& rotation_scale) -> math::vector3 {
  const auto inverse = math::matrix3x3::inverted(rotation_scale);

  const auto local_direction = math::vector3{inverse * direction};

  auto result = math::vector3::normalized(local_direction) * sphere.radius;

  return math::vector3{rotation_scale * result} + position;
}

static auto find_furthest_point(const math::vector3& direction, const cylinder& cylinder, const math::vector3& position, const math::matrix3x3& rotation_scale) -> math::vector3 {
  const auto inverse = math::matrix3x3::inverted(rotation_scale);

  const auto local_direction = math::vector3{inverse * direction};

  const auto local_direction_xz = math::vector3{local_direction.x(), 0.0f, local_direction.z()};

  auto result = math::vector3::normalized(local_direction_xz) * cylinder.radius;
  result.y() = (local_direction.y() > 0.0f) ? cylinder.cap : cylinder.base;

  return math::vector3{rotation_scale * result} + position;
}

static auto find_furthest_point(const math::vector3& direction, const capsule& capsule, const math::vector3& position, const math::matrix3x3& rotation_scale) -> math::vector3 {
  const auto inverse = math::matrix3x3::inverted(rotation_scale);

  const auto local_direction = math::vector3{inverse * direction};

  auto result = math::vector3::normalized(local_direction) * capsule.radius;
  result.y() = (local_direction.y() > 0.0f) ? capsule.cap : capsule.base;

  return math::vector3{rotation_scale * result} + position;
}

auto find_furthest_point(const collider_data& data, const math::vector3& direction) -> math::vector3 {
  return std::visit([&](const auto& shape) { return find_furthest_point(direction, shape, data.position, data.rotation_scale); }, data.collider.shape);
}

struct minkowski_vertex {
  math::vector3 minkowski_point;
  math::vector3 point_a;
}; // struct minkowski_vertex

auto support_point(const collider_data& first, const collider_data& second, const math::vector3& direction) -> minkowski_vertex {
  const auto p1 = find_furthest_point(first, direction);
  const auto p2 = find_furthest_point(second, -direction);

  return minkowski_vertex{p1 - p2, p1};
}

class simplex {

public:

  using value_type = minkowski_vertex;
  using reference = value_type;
  using const_iterator = const value_type*;

  simplex()
  : _size{0} { }

	auto operator=(std::initializer_list<value_type> list) -> simplex& {
		_size = 0u;

		for (const auto& point : list) {
			_points[_size++] = point;
    }

		return *this;
	}

	auto push_front(const value_type& point) -> void {
		_points = { point, _points[0], _points[1], _points[2] };
		_size = std::min(_size + 1u, std::size_t{4});
	}

	auto operator[](const std::size_t index) -> reference { 
    return _points[index]; 
  }

	auto size() const noexcept -> std::size_t { 
    return _size; 
  }

	auto begin() const -> const_iterator { 
    return _points.begin(); 
  }

	auto end() const -> const_iterator {
    return _points.end() - (4u - _size); 
  }

private:

  std::array<minkowski_vertex, 4u> _points;
  std::size_t _size;

}; // class simplex

static auto are_same_direction(const math::vector3& lhs, const math::vector3& rhs) -> bool {
  return math::vector3::dot(lhs, rhs) > 0.0f;
}

static auto line(simplex& simplex, math::vector3& direction) -> bool {
  auto a = simplex[0].minkowski_point;
  auto b = simplex[1].minkowski_point;

	auto ab = b - a;
	auto ao = -a;
 
	if (are_same_direction(ab, ao)) {
		direction = math::vector3::cross(math::vector3::cross(ab, ao), ab);
	} else {
		simplex = { simplex[0] };
		direction = ao;
	}

	return false;
}

static auto triangle(simplex& simplex, math::vector3& direction) -> bool {
  auto a = simplex[0].minkowski_point;
  auto b = simplex[1].minkowski_point;
  auto c = simplex[2].minkowski_point;

	auto ab = b - a;
	auto ac = c - a;
	auto ao =   - a;
 
	auto abc = math::vector3::cross(ab, ac);
 
	if (are_same_direction(math::vector3::cross(abc, ac), ao)) {
		if (are_same_direction(ac, ao)) {
			simplex = { simplex[0], simplex[2] };
			direction = math::vector3::cross(math::vector3::cross(ac, ao), ac);
		} else {
			return line(simplex = { simplex[0], simplex[1] }, direction);
		}
	} else {
		if (are_same_direction(math::vector3::cross(ab, abc), ao)) {
			return line(simplex = { simplex[0], simplex[1] }, direction);
		} else {
			if (are_same_direction(abc, ao)) {
				direction = abc;
			} else {
				simplex = { simplex[0], simplex[1], simplex[2] };
				direction = -abc;
			}
		}
	}

	return false;
}

static auto tetrahedron(simplex& simplex, math::vector3& direction) -> bool {
  auto a = simplex[0].minkowski_point;
  auto b = simplex[1].minkowski_point;
  auto c = simplex[2].minkowski_point;
  auto d = simplex[3].minkowski_point;

	auto ab = b - a;
	auto ac = c - a;
	auto ad = d - a;
	auto ao =   - a;
 
	auto abc = math::vector3::cross(ab, ac);
	auto acd = math::vector3::cross(ac, ad);
	auto adb = math::vector3::cross(ad, ab);
 
	if (are_same_direction(abc, ao)) {
		return triangle(simplex = { simplex[0], simplex[1], simplex[2] }, direction);
	}
		
	if (are_same_direction(acd, ao)) {
		return triangle(simplex = { simplex[0], simplex[2], simplex[3] }, direction);
	}
 
	if (are_same_direction(adb, ao)) {
		return triangle(simplex = { simplex[0], simplex[1], simplex[3] }, direction);
	}
 
	return true;
}

static auto barycentric_coordinates(const math::vector3& p, const math::vector3& a, const math::vector3& b, const math::vector3& c) -> math::vector3 {
  const auto v0 = b - a;
  const auto v1 = c - a;
  const auto v2 = p - a;

  const float d00 = math::vector3::dot(v0, v0);
  const float d01 = math::vector3::dot(v0, v1);
  const float d11 = math::vector3::dot(v1, v1);
  const float d20 = math::vector3::dot(v2, v0);
  const float d21 = math::vector3::dot(v2, v1);

  const float denom = d00 * d11 - d01 * d01;

  if (std::abs(denom) < 1e-6f) {
    return {1.0f, 0.0f, 0.0f};
  }

  const float v = (d11 * d20 - d01 * d21) / denom;
  const float w = (d00 * d21 - d01 * d20) / denom;
  const float u = 1.0f - v - w;

  return {u, v, w};
}

static auto next_simplex(simplex& simplex, math::vector3& direction) -> bool {
  switch (simplex.size()) {
    case 2u: return line(simplex, direction);
    case 3u: return triangle(simplex, direction);
    case 4u: return tetrahedron(simplex, direction);
  }

  return false;
}

template<math::unsigned_integral Start, math::unsigned_integral End, math::unsigned_integral Step>
static auto stepped_iota(const Start start, const End end, const Step step = Step{1u}) {
  return std::ranges::views::iota(0u, (end - start + step - 1) / step) | std::ranges::views::transform([=](auto x) { return x * step + start; });
}

auto check_collision(const collider_data& first, const collider_data& second) -> std::optional<collision_manifold> {
  auto simplex = physics::simplex{};

  auto support = support_point(first, second, math::vector3{1.0f, 0.0f, 0.0f});

  simplex.push_front(support);

  auto direction = -support.minkowski_point;

  for ([[maybe_unused]] auto iteration : std::views::iota(0u, 64u)) {
    support = support_point(first, second, direction);

    if (math::vector3::dot(support.minkowski_point, direction) <= 0.0f) {
      return std::nullopt;
    }

    simplex.push_front(support);

    if (next_simplex(simplex, direction)) {
      return collision_manifold{};
    }
  }

  return std::nullopt;
}

} // namespace sbx::physics