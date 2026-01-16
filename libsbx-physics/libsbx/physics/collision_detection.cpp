#include <libsbx/physics/collision_detection.hpp>

#include <libsbx/utility/fast_mod.hpp>

#include <libsbx/scenes/node.hpp>

namespace sbx::physics {

static constexpr auto collision_margin = 0.02f;

static auto find_furthest_point(const box& box, const math::vector3& direction, const math::vector3& position, const math::quaternion& rotation) -> math::vector3 {
  const auto inverse_rotation = math::quaternion::inverted(rotation);
  const auto local_direction = math::vector3{ inverse_rotation * direction };

  const auto inflated_extents =
    box.half_extents + math::vector3{ collision_margin };

  math::vector3 local_support;

  local_support.x() = (local_direction.x() > 0.0f)
    ?  inflated_extents.x()
    : -inflated_extents.x();

  local_support.y() = (local_direction.y() > 0.0f)
    ?  inflated_extents.y()
    : -inflated_extents.y();

  local_support.z() = (local_direction.z() > 0.0f)
    ?  inflated_extents.z()
    : -inflated_extents.z();

  return rotation * local_support + position;
}

static auto find_furthest_point(const sphere& sphere, const math::vector3& direction, const math::vector3& position, const math::quaternion& rotation) -> math::vector3 {
  const auto inverse_rotation = math::quaternion::inverted(rotation);

  const auto local_direction = math::vector3{inverse_rotation * direction};

  auto result = math::vector3::normalized(local_direction) * sphere.radius;

  return math::vector3{rotation * result} + position;
}

static auto find_furthest_point(const cylinder& cylinder, const math::vector3& direction, const math::vector3& position, const math::quaternion& rotation) -> math::vector3 {
  const auto inverse_rotation = math::quaternion::inverted(rotation);

  const auto local_direction = math::vector3{inverse_rotation * direction};

  auto result = math::vector3{0.0f, 0.0f, 0.0f};

  result.y() = (local_direction.y() > 0.0f) ?  cylinder.half_height : -cylinder.half_height;

  auto radial = math::vector3{local_direction.x(), 0.0f, local_direction.z()};

  const auto length_squared = radial.length_squared();

  if (length_squared > 1e-6f) {
    radial *= (cylinder.radius / std::sqrt(length_squared));

    result.x() = radial.x();
    result.z() = radial.z();
  }

  return math::vector3{rotation * result} + position;
}

static auto find_furthest_point(const capsule& capsule, const math::vector3& direction, const math::vector3& position, const math::quaternion& rotation) -> math::vector3 {
  const auto inverse_rotation = math::quaternion::inverted(rotation);

  const auto local_direction = math::vector3{inverse_rotation * direction};

  auto result = math::vector3{0.0f, 0.0f, 0.0f};

  result.y() = (local_direction.y() > 0.0f) ?  capsule.half_height : -capsule.half_height;

  if (local_direction.length_squared() > 1e-6f) {
    result += math::vector3::normalized(local_direction) * capsule.radius;
  }

  return math::vector3{rotation * result} + position;
}

auto find_furthest_point(const collider_data& data, const math::vector3& direction) -> math::vector3 {
  return std::visit([&](const auto& shape) { return find_furthest_point(shape, direction, data.position, data.rotation); }, data.collider.shape);
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
  using size_type = std::size_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using const_iterator = const_pointer;

  simplex()
  : _points{},
    _size{0} { }

	auto operator=(std::initializer_list<value_type> list) -> simplex& {
		_size = 0u;

		for (const auto& point : list) {
			_points[_size++] = point;
    }

		return *this;
	}

	auto push_front(const value_type& point) -> void {
		_points = { point, _points[0], _points[1], _points[2] };
		_size = std::min(_size + 1u, size_type{4});
	}

	auto operator[](const size_type index) -> reference { 
    return _points[index]; 
  }

  auto operator[](const size_type index) const -> const_reference { 
    return _points[index]; 
  }

	auto size() const noexcept -> size_type { 
    return _size; 
  }

	auto begin() const -> const_iterator { 
    return _points.begin(); 
  }

	auto end() const -> const_iterator {
    return _points.end() - (_points.size() - _size); 
  }

private:

  std::array<value_type, 4u> _points;
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

    if (direction.length_squared() < 1e-12f) {
      direction = math::vector3::orthogonal(ab);
    }
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

      if (direction.length_squared() < 1e-12f) {
        direction = math::vector3::orthogonal(ac);
      }
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

static auto next_simplex(simplex& simplex, math::vector3& direction) -> bool {
  switch (simplex.size()) {
    case 2u: return line(simplex, direction);
    case 3u: return triangle(simplex, direction);
    case 4u: return tetrahedron(simplex, direction);
  }

  return false;
}

static auto barycentric_coordinates(const math::vector3& p, const math::vector3& a, const math::vector3& b, const math::vector3& c) -> math::vector3 {
  const auto v0 = b - a;
  const auto v1 = c - a;
  const auto v2 = p - a;

  const auto d00 = math::vector3::dot(v0, v0);
  const auto d01 = math::vector3::dot(v0, v1);
  const auto d11 = math::vector3::dot(v1, v1);
  const auto d20 = math::vector3::dot(v2, v0);
  const auto d21 = math::vector3::dot(v2, v1);

  const auto denom = d00 * d11 - d01 * d01;

  if (std::abs(denom) < 1e-6f) {
    return {1.0f, 0.0f, 0.0f};
  }

  const auto v = (d11 * d20 - d01 * d21) / denom;
  const auto w = (d00 * d21 - d01 * d20) / denom;
  const auto u = 1.0f - v - w;

  return math::vector3{u, v, w};
}

struct epa_face {
  std::size_t a;
  std::size_t b;
  std::size_t c;

  math::vector3 normal;
  std::float_t distance;
}; // struct epa_face

static auto make_face(const std::vector<minkowski_vertex>& vertices, std::size_t a, std::size_t b, std::size_t c) -> epa_face {
  const auto& pa = vertices[a].minkowski_point;
  const auto& pb = vertices[b].minkowski_point;
  const auto& pc = vertices[c].minkowski_point;

  const auto ab = pb - pa;
  const auto ac = pc - pa;

  auto normal = math::vector3::normalized(math::vector3::cross(ab, ac));
  auto distance = math::vector3::dot(normal, pa);

  if (distance < 0.0f) {
    normal = -normal;
    distance = -distance;
  }

  return epa_face{a, b, c, normal, distance};
}

static auto add_if_unique_edge(std::vector<std::pair<std::size_t, std::size_t>>& edges, std::size_t a, std::size_t b) -> void {
  const auto reverse = std::find(edges.begin(), edges.end(), std::make_pair(b, a));

  if (reverse != edges.end()) {
    edges.erase(reverse);
  } else {
    edges.emplace_back(a, b);
  }
}

static auto build_initial_polytope(const std::vector<minkowski_vertex>& vertices) -> std::vector<epa_face> {
  return std::vector<epa_face>{
    make_face(vertices, 0u, 1u, 2u),
    make_face(vertices, 0u, 2u, 3u),
    make_face(vertices, 0u, 3u, 1u),
    make_face(vertices, 1u, 3u, 2u)
  };
}

static auto find_closest_face(const std::vector<epa_face>& faces) -> std::size_t {
  auto min_distance = std::numeric_limits<float>::max();
  auto index = std::size_t{0};

  for (auto i = std::size_t{0}; i < faces.size(); ++i) {
    if (faces[i].distance < min_distance) {
      min_distance = faces[i].distance;
      index = i;
    }
  }

  return index;
}

static auto epa(const collider_data& first, const collider_data& second, const simplex& simplex) -> std::optional<collision_manifold> {
  constexpr auto max_iterations = std::size_t{32};
  constexpr auto epsilon = 1e-4f;

  auto vertices = std::vector<minkowski_vertex>{};
  vertices.reserve(64u);

  for (auto i = 0u; i < simplex.size(); ++i) {
    vertices.push_back(simplex[i]);
  }

  auto faces = build_initial_polytope(vertices);
  auto closest_face_index = find_closest_face(faces);

  for (auto iteration = 0u; iteration < max_iterations; ++iteration) {
    const auto& closest_face = faces[closest_face_index];
    const auto support = support_point(first, second, closest_face.normal);
    const auto support_distance = math::vector3::dot(closest_face.normal,support.minkowski_point);

    if (support_distance - closest_face.distance < epsilon) {
      auto manifold = collision_manifold{};

      manifold.normal = closest_face.normal;
      manifold.depth = closest_face.distance;

      return manifold;
    }

    const auto new_vertex_index = vertices.size();
    vertices.push_back(support);

    auto horizon_edges = std::vector<std::pair<std::size_t, std::size_t>>{};

    for (auto i = 0u; i < faces.size();) {
      const auto& face = faces[i];

      if (math::vector3::dot(face.normal, support.minkowski_point) > 0.0f) {
        add_if_unique_edge(horizon_edges, face.a, face.b);
        add_if_unique_edge(horizon_edges, face.b, face.c);
        add_if_unique_edge(horizon_edges, face.c, face.a);

        faces.erase(faces.begin() + i);
      } else {
        ++i;
      }
    }

    for (const auto& [a, b] : horizon_edges) {
      faces.push_back(make_face(vertices, a, b, new_vertex_index));
    }

    closest_face_index = find_closest_face(faces);
  }

  return std::nullopt;
}

static auto gjk(const collider_data& first, const collider_data& second) -> std::optional<simplex> {
  auto simplex = physics::simplex{};

  auto direction = second.position - first.position;

  if (direction.length_squared() < 1e-6f) {
    direction = math::vector3{0.0f, 1.0f, 0.0f};
  }

  auto support = support_point(first, second, direction);
  simplex.push_front(support);

  direction = -support.minkowski_point;

  for ([[maybe_unused]] auto i = 0u; i < 64u; ++i) {
    support = support_point(first, second, direction);

    if (math::vector3::dot(support.minkowski_point, direction) <= 1e-6f) {
      return std::nullopt;
    }

    simplex.push_front(support);

    if (next_simplex(simplex, direction)) {
      return simplex;
    }
  }

  return std::nullopt;
}

struct clipping_plane {
  math::vector3 normal;
  math::vector3 point;
}; // struct clipping_plane

static auto clip_polygon_against_plane(std::array<math::vector3, 8u>& input_buffer, std::size_t vertex_count, const clipping_plane& plane) -> std::size_t {
  if (vertex_count == 0u) {
    return 0u;
  }

  auto output_buffer = std::array<math::vector3, 8u>{};
  auto output_count = std::size_t{0};

  auto previous_vertex = input_buffer[vertex_count - 1u];
  auto previous_distance = math::vector3::dot(previous_vertex - plane.point, plane.normal);

  for (auto i = 0u; i < vertex_count; ++i) {
    const auto current_vertex = input_buffer[i];
    const auto current_distance = math::vector3::dot(current_vertex - plane.point, plane.normal);

    const bool previous_inside = (previous_distance <= 0.0f);
    const bool current_inside = (current_distance <= 0.0f);

    if (previous_inside && current_inside) {
      output_buffer[output_count++] = current_vertex;
    } else if (previous_inside && !current_inside) {
      const auto t = previous_distance / (previous_distance - current_distance);

      output_buffer[output_count++] = previous_vertex + (current_vertex - previous_vertex) * t;
    } else if (!previous_inside && current_inside) {
      const auto t = previous_distance / (previous_distance - current_distance);

      output_buffer[output_count++] = previous_vertex + (current_vertex - previous_vertex) * t;
      output_buffer[output_count++] = current_vertex;
    }

    previous_vertex = current_vertex;
    previous_distance = current_distance;
  }

  for (auto i = 0u; i < output_count; ++i) {
    input_buffer[i] = output_buffer[i];
  }

  return output_count;
}

static auto get_incident_face_vertices(const box& incident_box, const collider_data& incident_data, const math::vector3& reference_face_normal) -> std::array<math::vector3, 4u> {

  const auto axes = std::array<math::vector3, 3u>{
    incident_data.rotation * math::vector3{1,0,0},
    incident_data.rotation * math::vector3{0,1,0},
    incident_data.rotation * math::vector3{0,0,1}
  };

  // Pick axis MOST aligned with the reference normal (faces are +/- that axis)
  auto best_abs = -1.0f;
  auto axis_index = std::size_t{0};

  for (auto i = 0u; i < 3u; ++i) {
    const auto d = math::vector3::dot(axes[i], reference_face_normal);
    const auto a = std::abs(d);

    if (a > best_abs) {
      best_abs = a;
      axis_index = i;
    }
  }

  // Choose the incident face whose normal points opposite the reference normal
  const auto d = math::vector3::dot(axes[axis_index], reference_face_normal);
  const auto face_sign = (d > 0.0f) ? -1.0f : 1.0f;

  const auto i1 = utility::fast_mod(axis_index + 1u, 3u);
  const auto i2 = utility::fast_mod(axis_index + 2u, 3u);

  // Perimeter winding in the face plane:
  // (-,-) -> (+,-) -> (+,+) -> (-,+)
  constexpr std::array<std::pair<float, float>, 4u> winding = {{
    { -1.0f, -1.0f },
    {  1.0f, -1.0f },
    {  1.0f,  1.0f },
    { -1.0f,  1.0f }
  }};

  auto world = std::array<math::vector3, 4u>{};

  for (auto i = 0u; i < 4u; ++i) {
    const auto [u, v] = winding[i];

    auto local = incident_box.half_extents;

    local[axis_index] *= face_sign;
    local[i1] *= u;
    local[i2] *= v;

    world[i] = incident_data.rotation * local + incident_data.position;
  }

  return world;
}

static auto generate_contacts(const box& box_a, const collider_data& box_a_data, const box& box_b, const collider_data& box_b_data, collision_manifold& manifold) -> void {
  manifold.contact_points.clear();

  // Ensure the normal points A -> B
  const auto penetration_normal = (math::vector3::dot(manifold.normal, box_b_data.position - box_a_data.position) >= 0.0f) ? manifold.normal : -manifold.normal;

  const auto axes_a = std::array<math::vector3, 3u>{
    box_a_data.rotation * math::vector3::right,
    box_a_data.rotation * math::vector3::up,
    box_a_data.rotation * math::vector3::backward
  };

  const auto axes_b = std::array<math::vector3, 3u>{
    box_b_data.rotation * math::vector3::right,
    box_b_data.rotation * math::vector3::up,
    box_b_data.rotation * math::vector3::backward
  };

  // Choose reference box by best axis alignment to collision normal
  auto best_alignment_a = 0.0f;
  auto best_alignment_b = 0.0f;
  auto reference_axis_a = std::size_t{0};
  auto reference_axis_b = std::size_t{0};

  for (auto i = 0u; i < 3u; ++i) {
    const auto alignment_a = std::abs(math::vector3::dot(penetration_normal, axes_a[i]));
    const auto alignment_b = std::abs(math::vector3::dot(penetration_normal, axes_b[i]));

    if (alignment_a > best_alignment_a) {
      best_alignment_a = alignment_a;
      reference_axis_a = i;
    }

    if (alignment_b > best_alignment_b) {
      best_alignment_b = alignment_b;
      reference_axis_b = i;
    }
  }

  const auto box_a_is_reference = (best_alignment_a >= best_alignment_b);

  const auto& reference_box  = box_a_is_reference ? box_a : box_b;
  const auto& reference_data = box_a_is_reference ? box_a_data : box_b_data;
  const auto& reference_axes = box_a_is_reference ? axes_a : axes_b;

  const auto& incident_box  = box_a_is_reference ? box_b : box_a;
  const auto& incident_data = box_a_is_reference ? box_b_data : box_a_data;

  const auto reference_axis_index = box_a_is_reference ? reference_axis_a : reference_axis_b;

  // Reference face normal (choose the face that points toward the other box)
  auto reference_face_normal = reference_axes[reference_axis_index];

  if (math::vector3::dot(reference_face_normal, penetration_normal) < 0.0f) {
    reference_face_normal = -reference_face_normal;
  }

  const auto reference_face_extent = reference_box.half_extents[reference_axis_index];

  // Point on the reference face plane
  const auto reference_face_point = reference_data.position + reference_face_normal * reference_face_extent;

  // Incident face polygon (must be in perimeter winding order)
  const auto incident_face_vertices = get_incident_face_vertices(incident_box, incident_data, reference_face_normal);

  // Clip incident polygon against the side planes of the reference box
  auto polygon_buffer = std::array<math::vector3, 8u>{};
  auto polygon_vertex_count = std::size_t{4};

  for (auto i = 0u; i < 4u; ++i) {
    polygon_buffer[i] = incident_face_vertices[i];
  }

  for (auto i = 0u; i < 3u; ++i) {
    // don't clip against the reference face normal axis
    if (i == reference_axis_index) {
      continue;
    }

    for (const auto sign : { -1.0f, 1.0f }) {
      const auto plane_normal = reference_axes[i] * sign;
      const auto plane_point  = reference_data.position + plane_normal * reference_box.half_extents[i];

      polygon_vertex_count = clip_polygon_against_plane(polygon_buffer, polygon_vertex_count, { plane_normal, plane_point });

      if (polygon_vertex_count == 0u) {
        return;
      }
    }
  }

  // Keep points behind/on the reference plane, project onto plane, and compute depth from them
  constexpr auto separation_epsilon = 1e-6f;

  auto max_penetration = 0.0f;

  for (auto i = 0u; i < polygon_vertex_count; ++i) {
    const auto separation = math::vector3::dot(polygon_buffer[i] - reference_face_point, reference_face_normal);

    if (separation <= separation_epsilon) {
      // Project contact point onto the reference face plane
      const auto contact = polygon_buffer[i] - reference_face_normal * separation;

      manifold.contact_points.push_back(contact);
      max_penetration = std::max(max_penetration, -separation);
    }
  }

  // If numerical issues removed all points, fall back to at least one reasonable contact
  if (manifold.contact_points.empty()) {
    manifold.contact_points.push_back(reference_face_point);
    max_penetration = std::max(max_penetration, manifold.depth);
  }

  manifold.depth = max_penetration;
}

template<typename B>
static auto generate_contacts(const sphere& a, const collider_data& a_data, [[maybe_unused]] const B& b, [[maybe_unused]] const collider_data& b_data, collision_manifold& manifold) -> void {
  const auto contact = a_data.position - manifold.normal * a.radius;

  manifold.contact_points.push_back(contact);
}

template<typename B>
static auto generate_contacts(const capsule& a, const collider_data& a_data, [[maybe_unused]] const B& b, [[maybe_unused]] const collider_data& b_data, collision_manifold& manifold) -> void {
  const auto contact = a_data.position - manifold.normal * a.radius;

  manifold.contact_points.push_back(contact);
}

template<typename A, typename B>
static auto generate_contacts([[maybe_unused]] const A& a, const collider_data& a_data, [[maybe_unused]] const B& b, [[maybe_unused]] const collider_data& b_data, collision_manifold& manifold) -> void {
  utility::logger<"physics">::debug("using fallback generate_contacts");

  const auto contact = a_data.position + manifold.normal * (-manifold.depth * 0.5f);

  manifold.contact_points.push_back(contact);
}

static auto generate_contacts(const collider_data& a, const collider_data& b, collision_manifold& manifold) -> void {
  manifold.contact_points.clear();

  std::visit([&](const auto& shape_a, const auto& shape_b) {
    generate_contacts(shape_a, a, shape_b, b, manifold);
  }, a.collider.shape, b.collider.shape);

  if (manifold.contact_points.empty()) {
    const auto contact = a.position + manifold.normal * (-manifold.depth * 0.5f);

    manifold.contact_points.push_back(contact);
  }
}

auto check_collision(const collider_data& first, const collider_data& second) -> std::optional<collision_manifold> {
  return gjk(first, second)
    .and_then([&](const auto& simplex){
      return epa(first, second, simplex);
    })
    .transform([&](auto manifold){
      generate_contacts(first, second, manifold);

      return manifold;
    });
}

} // namespace sbx::physics