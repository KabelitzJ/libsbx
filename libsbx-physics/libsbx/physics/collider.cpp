#include <libsbx/physics/collider.hpp>

#include <vector>
#include <array>
#include <optional>
#include <limits>
#include <algorithm>
#include <cmath>
#include <map>

namespace sbx::physics {

// --- Constants ---

static constexpr auto GJK_EPSILON = 1e-4f;
static constexpr auto EPA_TOLERANCE = 1e-4f;
static constexpr auto EPA_MAX_ITERATIONS = 32u;
static constexpr auto GJK_MAX_ITERATIONS = 64u;

// --- Helper Functions ---

static auto are_same_direction(const math::vector3& lhs, const math::vector3& rhs) -> bool {
  return math::vector3::dot(lhs, rhs) > 0.0f;
}

// --- Support Functions (The Shapes) ---

static auto find_furthest_point(const math::vector3& direction, const box& box, const math::vector3& position, const math::matrix4x4& rotation_scale) -> math::vector3 {
  const auto inverse = math::matrix4x4::inverted(rotation_scale);
  const auto local_dir = math::vector3{inverse * math::vector4{direction, 0.0f}};

  auto result = math::vector3{};
  // Select the corner of the box in the direction of the vector
  result.x() = (local_dir.x() > 0.0f) ? box.half_extents.x() : -box.half_extents.x();
  result.y() = (local_dir.y() > 0.0f) ? box.half_extents.y() : -box.half_extents.y();
  result.z() = (local_dir.z() > 0.0f) ? box.half_extents.z() : -box.half_extents.z();

  return math::vector3{rotation_scale * math::vector4{result, 1.0f}} + position;
}

static auto find_furthest_point(const math::vector3& direction, const sphere& sphere, const math::vector3& position, const math::matrix4x4& rotation_scale) -> math::vector3 {
  // Sphere support is Direction * Radius + Position
  return position + math::vector3::normalized(direction) * sphere.radius;
}

static auto find_furthest_point(const math::vector3& direction, const cylinder& cylinder, const math::vector3& position, const math::matrix4x4& rotation_scale) -> math::vector3 {
  const auto inverse = math::matrix4x4::inverted(rotation_scale);
  const auto local_dir = math::vector3{inverse * math::vector4{direction, 0.0f}};

  // 1. XZ Plane (The Disk)
  const auto dir_xz = math::vector3{local_dir.x(), 0.0f, local_dir.z()};

  auto result = math::vector3::zero;
  if (dir_xz.length_squared() > 1e-6f) {
    result = math::vector3::normalized(dir_xz) * cylinder.radius;
  }

  // 2. Y Axis (The Height)
  result.y() = (local_dir.y() > 0.0f) ? cylinder.cap : cylinder.base;

  return math::vector3{rotation_scale * math::vector4{result, 1.0f}} + position;
}

static auto find_furthest_point(const math::vector3& direction, const capsule& capsule, const math::vector3& position, const math::matrix4x4& rotation_scale) -> math::vector3 {
  const auto inverse = math::matrix4x4::inverted(rotation_scale);
  const auto local_dir = math::vector3{inverse * math::vector4{direction, 0.0f}};

  // Capsule Support = Support(Segment) + Support(Sphere)

  // 1. Segment support (Line along Y axis)
  auto result = math::vector3{0.0f, (local_dir.y() > 0.0f) ? capsule.cap : capsule.base, 0.0f};

  // 2. Add radius expansion in the direction of the vector
  result += math::vector3::normalized(local_dir) * capsule.radius;

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

static auto bounding_volume(const sphere& sphere, const math::vector3& position) -> math::volume {
  const auto offset = math::vector3{sphere.radius, sphere.radius, sphere.radius};
  return math::volume{position - offset, position + offset};
}

static auto bounding_volume(const box& box, const math::vector3& position) -> math::volume {
  return math::volume{position - box.half_extents, position + box.half_extents};
}

static auto bounding_volume(const cylinder& cylinder, const math::vector3& position) -> math::volume {
  // Cylinder is aligned along the Y-axis.
  // X and Z extents are the radius.
  // Y extents are defined by base and cap.
  
  const auto min_x_z = cylinder.radius;
  const auto max_x_z = cylinder.radius;

  // We construct the min and max vectors based on the shape's local bounds
  const auto min = math::vector3{
    position.x() - min_x_z, 
    position.y() + cylinder.base, 
    position.z() - min_x_z
  };

  const auto max = math::vector3{
    position.x() + max_x_z, 
    position.y() + cylinder.cap, 
    position.z() + max_x_z
  };

  return math::volume{min, max};
}

static auto bounding_volume(const capsule& capsule, const math::vector3& position) -> math::volume {
  // Capsule is a segment along Y, expanded by radius.
  // base/cap define the centers of the hemispheres.
  // We must subtract radius from base and add radius to cap for total Y bounds.
  
  const auto xz_extent = capsule.radius;

  const auto min = math::vector3{
    position.x() - xz_extent,
    position.y() + capsule.base - capsule.radius,
    position.z() - xz_extent
  };

  const auto max = math::vector3{
    position.x() + xz_extent,
    position.y() + capsule.cap + capsule.radius,
    position.z() + xz_extent
  };

  return math::volume{min, max};
}

auto bounding_volume(const collider& collider, const math::vector3& position) -> math::volume {
  return std::visit([&](const auto& shape) { return bounding_volume(shape, position); }, collider);
}

// --- Simplex Logic ---

class simplex {

public:

  using value_type = minkowski_vertex;

  simplex() : _size{0} {}

  auto push_front(const value_type& point) -> void {
    _points = { point, _points[0], _points[1], _points[2] };
    _size = std::min(_size + 1u, std::size_t{4});
  }

  auto operator[](std::size_t i) const -> const value_type& { return _points[i]; }
  auto size() const -> std::size_t { return _size; }

  auto begin() const { return _points.begin(); }
  auto end() const { return _points.begin() + _size; }

  // Resets the simplex to a specific set of points (used during reduction)
  auto set(std::initializer_list<value_type> list) -> void {
    _size = 0;
    for (const auto& p : list) _points[_size++] = p;
  }

private:

  std::array<minkowski_vertex, 4> _points;
  std::size_t _size;

};

// --- GJK Solver Cases ---

static auto line(simplex& simplex, math::vector3& direction) -> bool {
  const auto& a = simplex[0];
  const auto& b = simplex[1];

  const auto ab = b.minkowski_point - a.minkowski_point;
  const auto ao = -a.minkowski_point;

  if (are_same_direction(ab, ao)) {
    direction = math::vector3::cross(math::vector3::cross(ab, ao), ab);
  } else {
    simplex.set({a});
    direction = ao;
  }
  return false;
}

static auto triangle(simplex& simplex, math::vector3& direction) -> bool {
  const auto& a = simplex[0];
  const auto& b = simplex[1];
  const auto& c = simplex[2];

  const auto ab = b.minkowski_point - a.minkowski_point;
  const auto ac = c.minkowski_point - a.minkowski_point;
  const auto ao = -a.minkowski_point;

  const auto abc = math::vector3::cross(ab, ac);

  // Check edge AC
  if (are_same_direction(math::vector3::cross(abc, ac), ao)) {
    if (are_same_direction(ac, ao)) {
      simplex.set({a, c});
      direction = math::vector3::cross(math::vector3::cross(ac, ao), ac);
    } else {
      return line(simplex, direction); // Check AB logic
    }
  } else {
    // Check edge AB
    if (are_same_direction(math::vector3::cross(ab, abc), ao)) {
      return line(simplex, direction);
    } else {
      // Origin is strictly above/below the triangle (or inside infinite prism)
      if (are_same_direction(abc, ao)) {
        direction = abc;
      } else {
        simplex.set({a, c, b}); // Rewind for correct normal direction
        direction = -abc;
      }
    }
  }
  return false;
}

static auto tetrahedron(simplex& simplex, math::vector3& direction) -> bool {
  const auto& a = simplex[0];
  const auto& b = simplex[1];
  const auto& c = simplex[2];
  const auto& d = simplex[3];

  const auto ab = b.minkowski_point - a.minkowski_point;
  const auto ac = c.minkowski_point - a.minkowski_point;
  const auto ad = d.minkowski_point - a.minkowski_point;
  const auto ao = -a.minkowski_point;

  const auto abc = math::vector3::cross(ab, ac);
  const auto acd = math::vector3::cross(ac, ad);
  const auto adb = math::vector3::cross(ad, ab);

  // We only check the new faces (ABC, ACD, ADB). The base (BCD) is known to not face origin.
  if (are_same_direction(abc, ao)) {
    simplex.set({a, b, c});
    return triangle(simplex, direction);
  }
  if (are_same_direction(acd, ao)) {
    simplex.set({a, c, d});
    return triangle(simplex, direction);
  }
  if (are_same_direction(adb, ao)) {
    simplex.set({a, d, b});
    return triangle(simplex, direction);
  }

  return true; // Origin is inside all faces! Collision.
}

static auto next_simplex(simplex& simplex, math::vector3& direction) -> bool {
  switch (simplex.size()) {
    case 2: return line(simplex, direction);
    case 3: return triangle(simplex, direction);
    case 4: return tetrahedron(simplex, direction);
  }
  return false;
}

// --- EPA Implementation ---

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

  // Robust check for degenerate triangle
  if (std::abs(denom) < 1e-8f) {
    return {1.0f, 0.0f, 0.0f};
  }

  const float v = (d11 * d20 - d01 * d21) / denom;
  const float w = (d00 * d21 - d01 * d20) / denom;
  const float u = 1.0f - v - w;
  return {u, v, w};
}

struct epa_face {
  minkowski_vertex a, b, c;
  math::vector3 normal;
  float distance;
};

static auto make_face(const minkowski_vertex& a, const minkowski_vertex& b, const minkowski_vertex& c) -> epa_face {
  auto n = math::vector3::cross(b.minkowski_point - a.minkowski_point, c.minkowski_point - a.minkowski_point);
  n = math::vector3::normalized(n);
  const auto d = math::vector3::dot(n, a.minkowski_point);
  return {a, b, c, n, d};
}

static auto epa(const simplex& simplex, const collider_data& first, const collider_data& second) -> std::optional<collision_manifold> {
  auto faces = std::vector<epa_face>{};

  // Initial Tetrahedron faces
  faces.push_back(make_face(simplex[0], simplex[1], simplex[2]));
  faces.push_back(make_face(simplex[0], simplex[2], simplex[3])); // Winding
  faces.push_back(make_face(simplex[0], simplex[3], simplex[1]));
  faces.push_back(make_face(simplex[1], simplex[3], simplex[2]));

  // Find closest face to origin
  auto closest_face_idx = 0u;

  for (size_t iteration = 0; iteration < EPA_MAX_ITERATIONS; ++iteration) {
    // 1. Find the closest face
    float min_dist = std::numeric_limits<float>::max();
    closest_face_idx = 0;

    for (size_t i = 0; i < faces.size(); ++i) {
      if (faces[i].distance < min_dist) {
        min_dist = faces[i].distance;
        closest_face_idx = i;
      }
    }

    // 2. Search outwards from closest face
    const auto& close_f = faces[closest_face_idx];
    auto support_point = support(first, second, close_f.normal);

    const float s_dist = math::vector3::dot(support_point.minkowski_point, close_f.normal);

    if (std::abs(s_dist - close_f.distance) < EPA_TOLERANCE) {
      break; // Convergence reached
    }

    // 3. Horizon Expansion
    // Find all faces that are "visible" from the new support point
    using edge = std::pair<minkowski_vertex, minkowski_vertex>;
    auto unique_edges = std::vector<edge>{};

    auto is_visible = [&](const epa_face& f) {
      return math::vector3::dot(f.normal, support_point.minkowski_point - f.a.minkowski_point) > 0;
    };

    // We iterate backwards to remove efficiently
    for (auto i = static_cast<int>(faces.size()) - 1; i >= 0; --i) {
      if (is_visible(faces[i])) {
        const auto& f = faces[i];

        // Add edges. If an edge already exists in reverse, it's shared -> internal -> remove both.
        auto add_edge = [&](const minkowski_vertex& a, const minkowski_vertex& b) {
          auto it = std::find_if(unique_edges.begin(), unique_edges.end(), [&](const edge& e) {
            // Check equality of minkowski points
            return e.second.minkowski_point == a.minkowski_point && e.first.minkowski_point == b.minkowski_point;
          });

          if (it != unique_edges.end()) {
            unique_edges.erase(it); // Internal edge
          } else {
            unique_edges.emplace_back(a, b); // Boundary edge
          }
        };

        add_edge(f.a, f.b);
        add_edge(f.b, f.c);
        add_edge(f.c, f.a);

        faces.erase(faces.begin() + i);
      }
    }

    // 4. Build new faces from the horizon edges to the support point
    for (const auto& [a, b] : unique_edges) {
      faces.push_back(make_face(a, b, support_point));
    }
  }

  // --- Result Generation ---
  const auto& f = faces[closest_face_idx];
  auto result = collision_manifold{};

  result.normal = f.normal;
  result.depth = f.distance + 0.001f; // Bias

  // Calculate contact point using Barycentric coordinates
  const auto p_on_face = result.normal * f.distance;
  const auto bar = barycentric_coordinates(p_on_face, f.a.minkowski_point, f.b.minkowski_point, f.c.minkowski_point);

  // Map barycentric weights to World Space points
  const auto contact_on_a = f.a.point_a * bar.x() + f.b.point_a * bar.y() + f.c.point_a * bar.z();

  result.contact_points.push_back(contact_on_a);

  return result;
}

// --- Main GJK ---

auto gjk(const collider_data& first, const collider_data& second) -> std::optional<collision_manifold> {
  auto simplex = physics::simplex{};

  // Initial Search Direction
  auto direction = math::vector3{1.0f, 0.0f, 0.0f};
  if (math::vector3::distance_squared(first.position, second.position) > 1e-6f) {
    direction = second.position - first.position;
  }

  // 1. Get first point
  auto start_point = support(first, second, direction);
  simplex.push_front(start_point);

  direction = -start_point.minkowski_point;

  for (auto i = 0u; i < GJK_MAX_ITERATIONS; ++i) {
    auto new_point = support(first, second, direction);

    // If we didn't pass the origin in this direction, we can't enclose it -> No Collision
    if (math::vector3::dot(new_point.minkowski_point, direction) < 0) {
      return std::nullopt;
    }

    simplex.push_front(new_point);

    if (next_simplex(simplex, direction)) {
      // Simplex contains origin -> Intersection -> Run EPA for depth/normal
      
      return epa(simplex, first, second);
    }

    // Safety for degenerate cases
    if (direction.length_squared() < 1e-10f) {
      return epa(simplex, first, second);
    }
  }

  return std::nullopt;
}

} // namespace sbx::physics