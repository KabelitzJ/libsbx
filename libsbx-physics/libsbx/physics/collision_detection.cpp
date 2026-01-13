#include <libsbx/physics/collision_detection.hpp>

namespace sbx::physics {

auto get_local_support(const sphere& sphere, const math::vector3& direction) -> math::vector3 {
  if (direction.length_squared() < std::numeric_limits<std::float_t>::epsilon()) {
    return math::vector3::zero;
  }

  const auto normalized = math::vector3::normalized(direction);

  return normalized * sphere.radius;
}

auto get_local_support(const box& box, const math::vector3& direction) -> math::vector3 {
  return math::vector3{
    (direction.x() > 0.0f) ? box.half_extents.x() : -box.half_extents.x(),
    (direction.y() > 0.0f) ? box.half_extents.y() : -box.half_extents.y(),
    (direction.z() > 0.0f) ? box.half_extents.z() : -box.half_extents.z()
  };
}

auto get_local_support(const cylinder& cylinder, const math::vector3& direction) -> math::vector3 {
  const auto y = (direction.y() > 0.0f) ? cylinder.cap : cylinder.base;
  const auto dir_xz = math::vector3{direction.x(), 0.0f, direction.z()};
  const auto length_squared_xz = dir_xz.length_squared();

  if (length_squared_xz < std::numeric_limits<std::float_t>::epsilon()) {
    return math::vector3{0.0f, y, 0.0f};
  }

  const auto scalar = cylinder.radius / std::sqrt(length_squared_xz);

  return math::vector3{direction.x() * scalar, y, direction.z() * scalar};
}

auto get_local_support(const capsule& capsule, const math::vector3& direction) -> math::vector3 {
  const auto y = (direction.y() > 0.0f) ? capsule.cap : capsule.base;
  const auto line_endpoint = math::vector3{0.0f, y, 0.0f};
  
  if (direction.length_squared() < std::numeric_limits<std::float_t>::epsilon()) {
    return line_endpoint;
  }

  return line_endpoint + (math::vector3::normalized(direction) * capsule.radius);
}

auto get_support(const collider& collider, const math::matrix3x3& rotation_scale, const math::vector3& direction) -> math::vector3 {
  const auto local_direction = math::matrix3x3::transposed(rotation_scale) * direction;

  auto local_point = std::visit([&local_direction](const auto& shape) {
    return get_local_support(shape, local_direction);
  }, collider.shape);

  local_point += collider.offset;

  return rotation_scale * local_point;
}

auto get_minkowski_support(const collider& c_a, const math::vector3& p_a, const math::matrix3x3& rs_a, const collider& c_b, const math::vector3& p_b, const math::matrix3x3& rs_b, const math::vector3& direction) -> math::vector3 {
  const auto support_a = get_support(c_a, rs_a, direction) + p_a;
  const auto support_b = get_support(c_b, rs_b, -direction) + p_b;

  return support_a - support_b;
}

struct simplex {

  std::array<math::vector3, 4> points;
  std::size_t size{0};

  auto push_front(const math::vector3& point) -> void {
    points = {point, points[0], points[1], points[2]};
    size = std::min(size + 1, std::size_t{4});
  }

  auto operator[](std::size_t i) const -> const math::vector3& { 
    return points[i]; 
  }

  auto operator[](std::size_t i) -> math::vector3& { 
    return points[i]; 
  }

}; // struct simplex

auto handle_simplex(simplex& simplex, math::vector3& direction) -> bool {
  const auto& a = simplex[0];
  const auto& b = simplex[1];
  const auto& c = simplex[2];
  const auto& d = simplex[3];

  const auto ao = -a;

  // Line Case
  if (simplex.size == 2u) {
    const auto ab = b - a;
    const auto ab_perp = math::vector3::cross(math::vector3::cross(ab, ao), ab);

    direction = ab_perp;

    return false;
  }

  // Triangle Case
  if (simplex.size == 3u) {
    const auto ab = b - a;
    const auto ac = c - a;
    const auto abc = math::vector3::cross(ab, ac);

    if (math::vector3::dot(math::vector3::cross(abc, ac), ao) > 0.0f) {
      if (math::vector3::dot(ac, ao) > 0) {
        simplex = {a, c, {}, {}};
        simplex.size = 2;
        direction = math::vector3::cross(math::vector3::cross(ac, ao), ac);
      } else {
        simplex = {a, b, {}, {}};
        simplex.size = 2;
        direction = math::vector3::cross(math::vector3::cross(ab, ao), ab);
      }
    } else {
      if (math::vector3::dot(math::vector3::cross(ab, abc), ao) > 0) {
        simplex = {a, b, {}, {}};
        simplex.size = 2;
        direction = math::vector3::cross(math::vector3::cross(ab, ao), ab);
      } else {
        if (math::vector3::dot(abc, ao) > 0.0f) {
          direction = abc;
        } else {
          simplex = {a, c, b, {}};
          direction = -abc;
        }
      }
    }

    return false;
  }

  // Tetrahedron Case
  if (simplex.size == 4) {
    const auto ab = b - a;
    const auto ac = c - a;
    const auto ad = d - a;
    
    const auto abc = math::vector3::cross(ab, ac);
    const auto acd = math::vector3::cross(ac, ad);
    const auto adb = math::vector3::cross(ad, ab);

    if (math::vector3::dot(abc, ao) > 0.0f) {
      simplex = {a, b, c, {}};
      simplex.size = 3u;
      direction = abc;

      return false;
    }
    
    if (math::vector3::dot(acd, ao) > 0.0f) {
      simplex = {a, c, d, {}};
      simplex.size = 3u;
      direction = acd;

      return false;
    }

    if (math::vector3::dot(adb, ao) > 0.0f) {
      simplex = {a, d, b, {}};
      simplex.size = 3u;
      direction = adb;

      return false;
    }

    return true;
  }

  return false;
}

auto run_gjk(const collider& c_a, const math::vector3& p_a, const math::matrix3x3& rs_a, const collider& c_b, const math::vector3& p_b, const math::matrix3x3& rs_b) -> std::optional<simplex> {
  auto simplex = physics::simplex{};
  auto direction = p_b - p_a;
  
  if (direction.length_squared() < 1e-6f) {
    direction = math::vector3::right;
  }

  auto support = get_minkowski_support(c_a, p_a, rs_a, c_b, p_b, rs_b, direction);

  simplex.push_front(support);
  direction = -support;

  for (auto i = 0; i < 64; ++i) {
    support = get_minkowski_support(c_a, p_a, rs_a, c_b, p_b, rs_b, direction);

    if (math::vector3::dot(support, direction) < 0) {
      return std::nullopt;
    }

    simplex.push_front(support);

    if (handle_simplex(simplex, direction)) {
      return simplex;
    }
  }

  return std::nullopt;
}

struct epa_face {
  math::vector3 normal;
  std::float_t distance;
  std::size_t a;
  std::size_t b;
  std::size_t c; 
}; // struct epa_face

struct epa_edge {

  std::size_t a;
  std::size_t b;

  bool operator==(const epa_edge& other) const {
    return (a == other.a && b == other.b) || (a == other.b && b == other.a);
  }

}; // struct epa_edge

auto run_epa(const simplex& simplex, const collider& c_a, const math::vector3& p_a, const math::matrix3x3& rs_a,const collider& c_b, const math::vector3& p_b, const math::matrix3x3& rs_b) -> collision_manifold {
    
  auto polytope = std::vector<math::vector3>{simplex.points.begin(), simplex.points.end()};
  auto faces = std::vector<epa_face>{};

  auto add_face = [&](std::size_t a, std::size_t b, std::size_t c) -> void {
    const auto& va = polytope[a];
    const auto& vb = polytope[b];
    const auto& vc = polytope[c];
    
    const auto ab = vb - va;
    const auto ac = vc - va;
    auto normal = math::vector3::normalized(math::vector3::cross(ab, ac));
    auto distance = math::vector3::dot(normal, va);

    if (distance < 0.0f) {
      normal = -normal;
      distance = -distance;
      faces.push_back({normal, distance, a, c, b}); 
    } else {
      faces.push_back({normal, distance, a, b, c});
    }
  };

  add_face(0, 1, 2);
  add_face(0, 3, 1);
  add_face(0, 2, 3);
  add_face(1, 3, 2);

  const auto tolerance = 1e-3f;

  for (auto i = 0u; i < 64u; ++i) {
    auto min_face_idx = 0u;
    auto min_dist = std::numeric_limits<std::float_t>::max();

    for (auto f = 0u; f < faces.size(); ++f) {
      if (faces[f].distance < min_dist) {
        min_dist = faces[f].distance;
        min_face_idx = f;
      }
    }
    
    const auto closest_face = faces[min_face_idx];

    auto support = get_minkowski_support(c_a, p_a, rs_a, c_b, p_b, rs_b, closest_face.normal);
    const auto distance = math::vector3::dot(support, closest_face.normal);

    if (distance - closest_face.distance < tolerance) {
      return collision_manifold{closest_face.normal, distance};
    }

    auto unique_edges = std::vector<epa_edge>{};
    
    for (auto entry = faces.begin(); entry != faces.end();) {
      if (math::vector3::dot(entry->normal, support) - entry->distance > 0.0f) {
        const auto current_edges = std::array<epa_edge, 3u>{
          epa_edge{entry->a, entry->b},
          epa_edge{entry->b, entry->c},
          epa_edge{entry->c, entry->a}
        };

        for (const auto& edge : current_edges) {
          auto edge_it = std::find(unique_edges.begin(), unique_edges.end(), edge);
          
          if (edge_it != unique_edges.end()) {
            *edge_it = unique_edges.back();
            unique_edges.pop_back();
          } else {
            unique_edges.push_back(edge);
          }
        }

        *entry = faces.back();
        faces.pop_back();
      } else {
        ++entry;
      }
    }

    const auto new_idx = polytope.size();
    polytope.push_back(support);

    for (const auto& edge : unique_edges) {
      add_face(edge.a, edge.b, new_idx);
    }
  }

  auto min_face_idx = 0u;
  auto min_dist = std::numeric_limits<std::float_t>::max();

  for (auto f = 0u; f < faces.size(); ++f) {
    if (faces[f].distance < min_dist) {
      min_dist = faces[f].distance;
      min_face_idx = f;
    }
  }
  return collision_manifold{faces[min_face_idx].normal, faces[min_face_idx].distance};
}

auto check_collision(const collider& c_a, const math::vector3& p_a, const math::matrix3x3& rs_a, const collider& c_b, const math::vector3& p_b, const math::matrix3x3& rs_b) -> std::optional<collision_manifold> {
  auto simplex = run_gjk(c_a, p_a, rs_a, c_b, p_b, rs_b);
  
  if (!simplex) {
    return std::nullopt;
  }

  return run_epa(*simplex, c_a, p_a, rs_a, c_b, p_b, rs_b);
}

} // namespace sbx::physics