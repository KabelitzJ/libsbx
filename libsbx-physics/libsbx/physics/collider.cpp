#include <libsbx/physics/collider.hpp>

#include <array>

namespace sbx::physics {

static auto bounding_volume(const sphere& sphere, const math::vector3& position) -> math::volume {
  return math::volume{position - math::vector3{sphere.radius, sphere.radius, sphere.radius}, position + math::vector3{sphere.radius, sphere.radius, sphere.radius}};
}

static auto bounding_volume(const cylinder& cylinder, const math::vector3& position) -> math::volume {
  return math::volume{position - math::vector3{cylinder.radius, cylinder.base, cylinder.radius}, position + math::vector3{cylinder.radius, cylinder.cap, cylinder.radius}};
}

static auto bounding_volume(const capsule& capsule, const math::vector3& position) -> math::volume {
  return math::volume{position - math::vector3{capsule.radius, capsule.base - capsule.radius, capsule.radius}, position + math::vector3{capsule.radius, capsule.cap + capsule.radius, capsule.radius}};
}

static auto bounding_volume(const box& box, const math::vector3& position) -> math::volume {
  return math::volume{position - box.half_extents, position + box.half_extents};
}

auto bounding_volume(const collider& collider, const math::vector3& position) -> math::volume {
  return std::visit([&](const auto& shape) { return bounding_volume(shape, position); }, collider);
}

static auto find_furthest_point(const math::vector3& direction, const box& box, const math::vector3& position, const math::matrix4x4& rotation_scale) -> math::vector3 {
  const auto inverse = math::matrix4x4::inverted(rotation_scale);

  const auto local_direction = math::vector3{inverse * math::vector4{direction, 0.0f}};

  auto result = math::vector3{};

  result.x() = (local_direction.x() > 0.0f) ? box.half_extents.x() : -box.half_extents.x();
  result.y() = (local_direction.y() > 0.0f) ? box.half_extents.y() : -box.half_extents.y();
  result.z() = (local_direction.z() > 0.0f) ? box.half_extents.z() : -box.half_extents.z();

  return math::vector3{rotation_scale * math::vector4{result, 1.0f}} + position;
}

static auto find_furthest_point(const math::vector3& direction, const sphere& sphere, const math::vector3& position, const math::matrix4x4& rotation_scale) -> math::vector3 {
  return math::vector3::normalized(direction) * sphere.radius + position;
}

static auto find_furthest_point(const math::vector3& direction, const cylinder& cylinder, const math::vector3& position, const math::matrix4x4& rotation_scale) -> math::vector3 {
  const auto inverse = math::matrix4x4::inverted(rotation_scale);

  const auto local_direction = math::vector3{inverse * math::vector4{direction, 0.0f}};

  const auto local_direction_xz = math::vector3{local_direction.x(), 0.0f, local_direction.z()};

  auto result = math::vector3::normalized(local_direction_xz) * cylinder.radius;
  result.y() = (local_direction.y() > 0.0f) ? cylinder.cap : cylinder.base;

  return math::vector3{rotation_scale * math::vector4{result, 1.0f}} + position;
}

static auto find_furthest_point(const math::vector3& direction, const capsule& capsule, const math::vector3& position, const math::matrix4x4& rotation_scale) -> math::vector3 {
  const auto inverse = math::matrix4x4::inverted(rotation_scale);

  const auto local_direction = math::vector3{inverse * math::vector4{direction, 0.0f}};

  auto result = math::vector3::normalized(local_direction) * capsule.radius;
  result.y() = (local_direction.y() > 0.0f) ? capsule.cap : capsule.base;

  return math::vector3{rotation_scale * math::vector4{result, 1.0f}} + position;
}

auto find_furthest_point(const collider_data& data, const math::vector3& direction) -> math::vector3 {
  return std::visit([&](const auto& shape) { return find_furthest_point(direction, shape, data.position, data.rotation_scale); }, data.collider);
}

auto support(const collider_data& first, const collider_data& second, const math::vector3& direction) -> minkowski_vertex {
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

static auto same_direction(const math::vector3& lhs, const math::vector3& rhs) -> bool {
  return math::vector3::dot(lhs, rhs) > 0.0f;
}

static auto line(simplex& simplex, math::vector3& direction) -> bool {
  auto a = simplex[0].minkowski_point;
  auto b = simplex[1].minkowski_point;

	auto ab = b - a;
	auto ao = -a;
 
	if (same_direction(ab, ao)) {
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
 
	if (same_direction(math::vector3::cross(abc, ac), ao)) {
		if (same_direction(ac, ao)) {
			simplex = { simplex[0], simplex[2] };
			direction = math::vector3::cross(math::vector3::cross(ac, ao), ac);
		} else {
			return line(simplex = { simplex[0], simplex[1] }, direction);
		}
	} else {
		if (same_direction(math::vector3::cross(ab, abc), ao)) {
			return line(simplex = { simplex[0], simplex[1] }, direction);
		} else {
			if (same_direction(abc, ao)) {
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
 
	if (same_direction(abc, ao)) {
		return triangle(simplex = { simplex[0], simplex[1], simplex[2] }, direction);
	}
		
	if (same_direction(acd, ao)) {
		return triangle(simplex = { simplex[0], simplex[2], simplex[3] }, direction);
	}
 
	if (same_direction(adb, ao)) {
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
    // Fallback for degenerate triangle
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

auto get_face_normals(const std::vector<minkowski_vertex>& polytope, const std::vector<std::size_t>& faces) -> std::pair<std::vector<math::vector4>, std::size_t> {
	auto normals = std::vector<math::vector4>{};

	auto min_triangle = std::size_t{0};
	auto min_distance = std::numeric_limits<std::float_t>::max();

	for (auto i : stepped_iota(0u, faces.size(), 3u)) {
		auto a = polytope[faces[i + 0u]].minkowski_point;
		auto b = polytope[faces[i + 1u]].minkowski_point;
		auto c = polytope[faces[i + 2u]].minkowski_point;

		auto normal = math::vector3::normalized(math::vector3::cross(b - a, c - a));
		auto distance = math::vector3::dot(normal, a);

		if (distance < 0) {
			normal *= -1.0f;
			distance *= -1.0f;
		}

		normals.emplace_back(normal, distance);

		if (distance < min_distance) {
			min_triangle = i / 3u;
			min_distance = distance;
		}
	}

	return {normals, min_triangle};
}

auto add_if_unique_edge(std::vector<std::pair<std::size_t, std::size_t>>& edges, const std::vector<std::size_t>& faces, std::size_t a, std::size_t b) -> void {
	auto reverse = std::find(                     //      0--<--3
		edges.begin(),                              //     / \ B /   A: 2-0
		edges.end(),                                //    / A \ /    B: 0-2
		std::make_pair(faces[b], faces[a])          //   1-->--2
	);
 
	if (reverse != edges.end()) {
		edges.erase(reverse);
	} else {
		edges.emplace_back(faces[a], faces[b]);
	}
}

static auto epa(const simplex& simplex, const collider_data& first, const collider_data& second) -> std::optional<collision_manifold> {
  auto polytope = std::vector<minkowski_vertex>{simplex.begin(), simplex.end()};

  auto faces = std::vector<std::size_t>{
    0u, 1u, 2u,
		0u, 3u, 1u,
		0u, 2u, 3u,
		1u, 3u, 2u
  };

  auto [normals, min_face] = get_face_normals(polytope, faces);

  auto min_normal = math::vector3{};
	auto min_distance = std::numeric_limits<std::float_t>::max();
	
	auto iterations = 0u;

  while (min_distance == std::numeric_limits<std::float_t>::max()) {
    min_normal   = math::vector3{normals[min_face]};
    min_distance = normals[min_face].w();

    if (iterations++ > 32u) {
      break;
    }

    auto support = physics::support(first, second, min_normal);
    auto distance = math::vector3::dot(min_normal, support.minkowski_point);

    if (std::abs(distance - min_distance) > 0.001f) {
      min_distance = std::numeric_limits<std::float_t>::max();

      auto unique_edges = std::vector<std::pair<std::size_t, std::size_t>>{};

      for (auto i = 0u; i < normals.size(); ++i) {
        if (same_direction(math::vector3{normals[i]}, support.minkowski_point)) {
          auto f = i * 3u;

          add_if_unique_edge(unique_edges, faces, f + 0u, f + 1u);
          add_if_unique_edge(unique_edges, faces, f + 1u, f + 2u);
          add_if_unique_edge(unique_edges, faces, f + 2u, f + 0u);

          faces[f + 2u] = faces.back(); faces.pop_back();
          faces[f + 1u] = faces.back(); faces.pop_back();
          faces[f + 0u] = faces.back(); faces.pop_back();

          normals[i] = normals.back(); normals.pop_back();

          i--;
        }
      }

      if (unique_edges.size() == 0u) {
        break;
      }

      auto new_faces = std::vector<std::size_t>{};

      for (auto [edge1, edge2] : unique_edges) {
        new_faces.push_back(edge1);
        new_faces.push_back(edge2);
        new_faces.push_back(polytope.size());
      }

      polytope.push_back(support);

      auto [new_normals, new_min_face] = get_face_normals(polytope, new_faces);

      float new_min_distance = std::numeric_limits<std::float_t>::max();

      for (auto i : std::views::iota(0u, normals.size())) {
        if (normals[i].w() < new_min_distance) {
          new_min_distance = normals[i].w();
          min_face = i;
        }
      }

      if (new_normals[new_min_face].w() < new_min_distance) {
        min_face = new_min_face + normals.size();
      }

      faces.insert(faces.end(), new_faces.begin(), new_faces.end());
      normals.insert(normals.end(), new_normals.begin(), new_normals.end());
    }
  }

  if (min_distance == std::numeric_limits<std::float_t>::max()) {
    return std::nullopt;
  }

  // --- Contact Point Generation ---
  // The result of EPA is the normal and depth of the penetration.
  auto result = collision_manifold{};
  result.normal = min_normal;
  result.depth = min_distance + 0.001f; // Add a small epsilon

  auto relevant_faces = std::vector<std::tuple<std::size_t, std::size_t, std::size_t>>{};

  for (auto i : stepped_iota(0u, faces.size(), 3u)) {
    const auto& v0 = polytope[faces[i + 0u]];
    const auto& v1 = polytope[faces[i + 1u]];
    const auto& v2 = polytope[faces[i + 2u]];

    const auto face_normal = math::vector3::normalized(math::vector3::cross(v1.minkowski_point - v0.minkowski_point, v2.minkowski_point - v0.minkowski_point));

    const float face_distance = math::vector3::dot(face_normal, v0.minkowski_point);

    const bool coplanar = math::vector3::dot(face_normal, result.normal) > 0.98f; // ~11.5° angle
    const bool close_enough = std::abs(face_distance - result.depth) < 0.01f;

    if (coplanar && close_enough) {
      relevant_faces.emplace_back(faces[i + 0u], faces[i + 1u], faces[i + 2u]);
    }
  }

  for (const auto& [i0, i1, i2] : relevant_faces) {
    const auto& a = polytope[i0];
    const auto& b = polytope[i1];
    const auto& c = polytope[i2];

    // Project the origin onto the triangle plane
    const auto p_on_minkowski = result.normal * result.depth;

    // Barycentric weights w.r.t. the triangle
    const auto weights = barycentric_coordinates(p_on_minkowski, a.minkowski_point, b.minkowski_point, c.minkowski_point);

    // Interpolate original points on shape A and B
    const auto contact_on_a = a.point_a * weights.x() + b.point_a * weights.y() + c.point_a * weights.z();
    const auto contact_on_b = contact_on_a - result.normal * result.depth;

    // Midpoint is the final contact
    const auto contact_point = (contact_on_a + contact_on_b) * 0.5f;

    result.contact_points.push_back(contact_point);
  }

  if (result.contact_points.empty()) {
    result.contact_points.push_back(polytope[faces[min_face * 3]].point_a);
  } else if (result.contact_points.size() > 4) {
    // Step 1: Find the deepest point (closest to origin along normal)
    auto compare_depth = [&](const auto& a, const auto& b) {
      return math::vector3::dot(a, result.normal) < math::vector3::dot(b, result.normal);
    };

    const auto deepest_it = std::ranges::min_element(result.contact_points, compare_depth);
    const auto deepest = math::vector3{*deepest_it};

    // Remove it temporarily from the list
    result.contact_points.erase(deepest_it);

    // Step 2: Sort remaining points by distance to the deepest point
    std::ranges::sort(result.contact_points, [&](const auto& a, const auto& b) {
      return math::vector3::distance_squared(a, deepest) > math::vector3::distance_squared(b, deepest);
    });

    // Step 3: Keep only the 3 farthest
    if (result.contact_points.size() > 3) {
      result.contact_points.resize(3);
    }

    // Add the deepest point back in
    result.contact_points.push_back(deepest);
  }

  return result;
}

auto gjk(const collider_data& first, const collider_data& second) -> std::optional<collision_manifold> {
  auto simplex = physics::simplex{};

  auto support = physics::support(first, second, math::vector3{1.0f, 0.0f, 0.0f});

  simplex.push_front(support);

  auto direction = -support.minkowski_point;

  for ([[maybe_unused]] auto iteration : std::views::iota(0u, 32u)) {
    support = physics::support(first, second, direction);

    if (math::vector3::dot(support.minkowski_point, direction) <= 0.0f) {
      return std::nullopt;
    }

    simplex.push_front(support);

    if (next_simplex(simplex, direction)) {
      return epa(simplex, first, second);
    }
  }

  return std::nullopt;
}

} // namespace sbx::physics