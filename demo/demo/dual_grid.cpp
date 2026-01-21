#include <demo/dual_grid.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <libsbx/math/algorithm.hpp>
#include <libsbx/math/random.hpp>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/iterator.hpp>

namespace demo {

struct triangle {
  std::uint32_t a{};
  std::uint32_t b{};
  std::uint32_t c{};

}; // struct triangle

struct hex_triangle_grid {
  std::vector<dual_grid_base::dual_vertex> vertices{};
  std::vector<triangle> triangles{};

}; // struct hex_triangle_grid

struct axial_key {
  int q{};
  int r{};

  auto operator==(const axial_key& other) const -> bool {
    return q == other.q && r == other.r;
  }

}; // struct axial_key

struct axial_key_hash {
  auto operator()(const axial_key& k) const noexcept -> std::size_t {
    auto h = std::size_t{0};
    sbx::utility::hash_combine(h, k.q, k.r);

    return h;
  }

}; // struct axial_key_hash

struct edge_key {
  std::uint32_t u{};
  std::uint32_t v{};

  auto operator==(const edge_key& other) const -> bool {
    return u == other.u && v == other.v;
  }

}; // struct edge_key

struct edge_key_hash {
  auto operator()(const edge_key& e) const noexcept -> std::size_t {
    auto h = std::size_t{0};
    sbx::utility::hash_combine(h, e.u, e.v);

    return h;
  }

}; // struct edge_key_hash

struct edge_tri_pair {
  std::uint32_t tri0 = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t tri1 = std::numeric_limits<std::uint32_t>::max();

}; // struct edge_tri_pair

struct edge_cell_pair {
  std::uint32_t c0 = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t c1 = std::numeric_limits<std::uint32_t>::max();

}; // struct edge_cell_pair

struct quadify_result {
  std::vector<dual_grid_base::dual_quad> cells{};
  std::vector<triangle> leftover_tris{};

}; // struct quadify_result

struct midpoint_cache {
  std::unordered_map<edge_key, std::uint32_t, edge_key_hash> index{};

}; // struct midpoint_cache

static auto make_edge_key(const std::uint32_t a, const std::uint32_t b) -> edge_key {
  if (a < b) {
    return edge_key{a, b};
  }

  return edge_key{b, a};
}

static auto signed_area_xz_dual_quad(const dual_grid_base::dual_quad& q, const std::vector<dual_grid_base::dual_vertex>& vertices) -> std::float_t {
  const auto p0 = vertices[q.a].position;
  const auto p1 = vertices[q.b].position;
  const auto p2 = vertices[q.c].position;
  const auto p3 = vertices[q.d].position;

  const auto x0 = p0.x();
  const auto z0 = p0.z();
  const auto x1 = p1.x();
  const auto z1 = p1.z();
  const auto x2 = p2.x();
  const auto z2 = p2.z();
  const auto x3 = p3.x();
  const auto z3 = p3.z();

  return 0.5f * (
    x0 * z1 - x1 * z0 +
    x1 * z2 - x2 * z1 +
    x2 * z3 - x3 * z2 +
    x3 * z0 - x0 * z3
  );
}

static auto canonicalize_dual_quad(dual_grid_base::dual_quad& q, const std::vector<dual_grid_base::dual_vertex>& dual_vertices) -> void {
  auto ids = std::array<std::uint32_t, 4u>{q.a, q.b, q.c, q.d};

  if (ids[0] == ids[1] || ids[0] == ids[2] || ids[0] == ids[3] ||
      ids[1] == ids[2] || ids[1] == ids[3] || ids[2] == ids[3]) {
    return;
  }

  auto center = sbx::math::vector3{0.0f, 0.0f, 0.0f};

  for (const auto id : ids) {
    center += dual_vertices[id].position;
  }

  center *= 0.25f;

  std::sort(ids.begin(), ids.end(), [&](const auto lhs, const auto rhs) {
    const auto pl = dual_vertices[lhs].position;
    const auto pr = dual_vertices[rhs].position;

    const auto al = std::atan2(pl.z() - center.z(), pl.x() - center.x());
    const auto ar = std::atan2(pr.z() - center.z(), pr.x() - center.x());

    return al < ar;
  });

  q.a = ids[0];
  q.b = ids[1];
  q.c = ids[2];
  q.d = ids[3];

  if (signed_area_xz_dual_quad(q, dual_vertices) < 0.0f) {
    std::swap(q.b, q.d);
  }
}

static auto edge_midpoint(
  const std::uint32_t u,
  const std::uint32_t v,
  const std::vector<dual_grid_base::dual_vertex>& vertices
) -> sbx::math::vector3 {
  const auto pu = vertices[u].position;
  const auto pv = vertices[v].position;

  return (pu + pv) * 0.5f;
}

static auto get_or_create_midpoint(
  std::vector<dual_grid_base::dual_vertex>& vertices,
  midpoint_cache& cache,
  const std::uint32_t u,
  const std::uint32_t v
) -> std::uint32_t {
  const auto key = make_edge_key(u, v);

  const auto it = cache.index.find(key);

  if (it != cache.index.end()) {
    return it->second;
  }

  const auto pm = edge_midpoint(u, v, vertices);

  const auto idx = static_cast<std::uint32_t>(vertices.size());
  const auto is_fixed = vertices[u].is_fixed && vertices[v].is_fixed;

  vertices.push_back(dual_grid_base::dual_vertex{pm, is_fixed});
  cache.index.emplace(key, idx);

  return idx;
}

static auto generate_grid(const std::uint32_t rings, const std::float_t ring_distance) -> hex_triangle_grid {
  auto result = hex_triangle_grid{};

  const auto radius = rings;

  const auto vertex_count = 1u + 3u * radius * (radius + 1u);
  const auto triangle_count = 6u * radius * radius;

  result.vertices = sbx::utility::make_reserved_vector<dual_grid_base::dual_vertex>(vertex_count);
  result.triangles = sbx::utility::make_reserved_vector<triangle>(triangle_count);

  constexpr auto sqrt3_over_2 = 0.8660254037844386f;

  const auto axial_to_world = [&](const auto q, const auto r) -> sbx::math::vector3 {
    const auto x = ring_distance * (static_cast<std::float_t>(q) + 0.5f * static_cast<std::float_t>(r));
    const auto z = ring_distance * (sqrt3_over_2 * static_cast<std::float_t>(r));

    return sbx::math::vector3{x, 0.0f, z};
  };

  const auto R = static_cast<int>(radius);

  auto index_of = std::unordered_map<axial_key, std::uint32_t, axial_key_hash>{};
  index_of.reserve(vertex_count);

  {
    auto idx = std::uint32_t{0u};

    for (auto q = -R; q <= R; ++q) {
      for (auto r = -R; r <= R; ++r) {
        const auto settings = -q - r;

        const auto abs_i = [](const auto v) -> decltype(v) {
          if (v < 0) {
            return -v;
          }

          return v;
        };

        const auto mq = abs_i(q);
        const auto mr = abs_i(r);
        const auto ms = abs_i(settings);

        const auto m = std::max({mq, mr, ms});

        if (m > R) {
          continue;
        }

        const auto is_fixed = (m == R);

        index_of.emplace(axial_key{q, r}, idx);
        result.vertices.push_back(dual_grid_base::dual_vertex{axial_to_world(q, r), is_fixed});
        ++idx;
      }
    }
  }

  const auto invalid = std::numeric_limits<std::uint32_t>::max();

  const auto find_index = [&](const auto q, const auto r) -> std::uint32_t {
    const auto it = index_of.find(axial_key{q, r});

    if (it != index_of.end()) {
      return it->second;
    }

    return invalid;
  };

  for (auto q = -R - 1; q <= R; ++q) {
    for (auto r = -R - 1; r <= R; ++r) {
      const auto A = find_index(q, r);
      const auto B = find_index(q + 1, r);
      const auto C = find_index(q, r + 1);
      const auto D = find_index(q + 1, r + 1);

      if (A != invalid && B != invalid && C != invalid) {
        result.triangles.push_back(triangle{A, B, C});
      }

      if (B != invalid && D != invalid && C != invalid) {
        result.triangles.push_back(triangle{B, D, C});
      }
    }
  }

  sbx::utility::assert_that(result.vertices.size() == vertex_count, "invalid vertices");
  sbx::utility::assert_that(result.triangles.size() == triangle_count, "invalid triangles");

  return result;
}

static auto third_vertex(const triangle& t, const std::uint32_t u, const std::uint32_t v) -> std::uint32_t {
  if (t.a != u && t.a != v) {
    return t.a;
  }

  if (t.b != u && t.b != v) {
    return t.b;
  }

  if (t.c != u && t.c != v) {
    return t.c;
  }

  return std::numeric_limits<std::uint32_t>::max();
}

static auto signed_area_xz(
  const dual_grid_base::dual_quad& c,
  const std::vector<dual_grid_base::dual_vertex>& vertices
) -> std::float_t {
  const auto p0 = vertices[c.a].position;
  const auto p1 = vertices[c.b].position;
  const auto p2 = vertices[c.c].position;
  const auto p3 = vertices[c.d].position;

  const auto x0 = p0.x();
  const auto z0 = p0.z();
  const auto x1 = p1.x();
  const auto z1 = p1.z();
  const auto x2 = p2.x();
  const auto z2 = p2.z();
  const auto x3 = p3.x();
  const auto z3 = p3.z();

  return 0.5f * (
    x0 * z1 - x1 * z0 +
    x1 * z2 - x2 * z1 +
    x2 * z3 - x3 * z2 +
    x3 * z0 - x0 * z3
  );
}

static auto make_merged_cell_ccw(
  const triangle& t0,
  const triangle& t1,
  const edge_key& e,
  const std::vector<dual_grid_base::dual_vertex>& vertices
) -> dual_grid_base::dual_quad {
  const auto u = e.u;
  const auto v = e.v;

  const auto w0 = third_vertex(t0, u, v);
  const auto w1 = third_vertex(t1, u, v);

  auto c = dual_grid_base::dual_quad{w0, u, w1, v};

  if (signed_area_xz(c, vertices) < 0.0f) {
    c = dual_grid_base::dual_quad{w0, v, w1, u};
  }

  return c;
}

static auto merge_tris_to_cells_random(
  const std::vector<dual_grid_base::dual_vertex>& vertices,
  const std::vector<triangle>& triangles,
  const std::float_t merge_probability
) -> quadify_result {
  auto result = quadify_result{};

  const auto invalid = std::numeric_limits<std::uint32_t>::max();

  auto edge_to_tris = std::unordered_map<edge_key, edge_tri_pair, edge_key_hash>{};
  edge_to_tris.reserve(triangles.size() * 2u);

  const auto add_edge = [&](const auto u, const auto v, const auto tri_index) {
    const auto key = make_edge_key(u, v);
    auto& slot = edge_to_tris[key];

    if (slot.tri0 == invalid) {
      slot.tri0 = tri_index;

      return;
    }

    if (slot.tri1 == invalid) {
      slot.tri1 = tri_index;

      return;
    }
  };

  for (auto ti = std::uint32_t{0u}; ti < static_cast<std::uint32_t>(triangles.size()); ++ti) {
    const auto& t = triangles[ti];

    add_edge(t.a, t.b, ti);
    add_edge(t.b, t.c, ti);
    add_edge(t.c, t.a, ti);
  }

  auto internal_edges = std::vector<edge_key>{};
  internal_edges.reserve(edge_to_tris.size());

  for (const auto& [e, pair] : edge_to_tris) {
    const auto has_two_tris = pair.tri0 != invalid && pair.tri1 != invalid;

    if (has_two_tris) {
      internal_edges.push_back(e);
    }
  }

  sbx::math::shuffle_range(internal_edges);

  auto tri_used = std::vector<bool>{};
  tri_used.resize(triangles.size());

  result.cells = sbx::utility::make_reserved_vector<dual_grid_base::dual_quad>(triangles.size() / 2u);

  for (const auto& e : internal_edges) {
    if (sbx::math::random::next<std::float_t>(0.0f, 1.0f) > merge_probability) {
      continue;
    }

    const auto it = edge_to_tris.find(e);

    if (it == edge_to_tris.end()) {
      continue;
    }

    const auto t0 = it->second.tri0;
    const auto t1 = it->second.tri1;

    if (t0 == invalid || t1 == invalid) {
      continue;
    }

    if (tri_used[t0] || tri_used[t1]) {
      continue;
    }

    const auto w0 = third_vertex(triangles[t0], e.u, e.v);
    const auto w1 = third_vertex(triangles[t1], e.u, e.v);

    if (w0 == invalid || w1 == invalid) {
      continue;
    }

    if (w0 == w1) {
      continue;
    }

    const auto merged_cell = make_merged_cell_ccw(triangles[t0], triangles[t1], e, vertices);
    result.cells.push_back(merged_cell);

    tri_used[t0] = true;
    tri_used[t1] = true;
  }

  result.leftover_tris = sbx::utility::make_reserved_vector<triangle>(triangles.size());

  for (auto i = std::size_t{0u}; i < triangles.size(); ++i) {
    if (!tri_used[i]) {
      result.leftover_tris.push_back(triangles[i]);
    }
  }

  return result;
}

static auto signed_area_xz_triangle(
  const triangle& t,
  const std::vector<dual_grid_base::dual_vertex>& vertices
) -> std::float_t {
  const auto p0 = vertices[t.a].position;
  const auto p1 = vertices[t.b].position;
  const auto p2 = vertices[t.c].position;

  const auto x0 = p0.x();
  const auto z0 = p0.z();
  const auto x1 = p1.x();
  const auto z1 = p1.z();
  const auto x2 = p2.x();
  const auto z2 = p2.z();

  return 0.5f * (
    x0 * (z1 - z2) +
    x1 * (z2 - z0) +
    x2 * (z0 - z1)
  );
}

static auto split_leftover_tris_to_cells(
  std::vector<dual_grid_base::dual_vertex>& vertices,
  const std::vector<triangle>& leftover_tris,
  midpoint_cache& cache
) -> std::vector<dual_grid_base::dual_quad> {
  auto out = std::vector<dual_grid_base::dual_quad>{};
  out.reserve(leftover_tris.size() * 3u);

  vertices.reserve(vertices.size() + leftover_tris.size() * 4u);

  for (const auto& tri_in : leftover_tris) {
    auto tri = tri_in;

    if (signed_area_xz_triangle(tri, vertices) < 0.0f) {
      std::swap(tri.b, tri.c);
    }

    const auto a = tri.a;
    const auto b = tri.b;
    const auto c = tri.c;

    const auto pa = vertices[a].position;
    const auto pb = vertices[b].position;
    const auto pc = vertices[c].position;

    const auto center_pos = (pa + pb + pc) * (1.0f / 3.0f);
    const auto center = static_cast<std::uint32_t>(vertices.size());

    vertices.push_back(dual_grid_base::dual_vertex{center_pos, 0u});

    const auto mab = get_or_create_midpoint(vertices, cache, a, b);
    const auto mbc = get_or_create_midpoint(vertices, cache, b, c);
    const auto mca = get_or_create_midpoint(vertices, cache, c, a);

    out.push_back(dual_grid_base::dual_quad{a, mab, center, mca});
    out.push_back(dual_grid_base::dual_quad{b, mbc, center, mab});
    out.push_back(dual_grid_base::dual_quad{c, mca, center, mbc});
  }

  return out;
}

static auto split_cells_to_4(
  std::vector<dual_grid_base::dual_vertex>& vertices,
  const std::vector<dual_grid_base::dual_quad>& in_cells,
  midpoint_cache& cache
) -> std::vector<dual_grid_base::dual_quad> {
  auto out = std::vector<dual_grid_base::dual_quad>{};
  out.reserve(in_cells.size() * 4u);

  vertices.reserve(vertices.size() + in_cells.size() * 5u);

  for (const auto& c : in_cells) {
    const auto a = c.a;
    const auto b = c.b;
    const auto cc = c.c;
    const auto d = c.d;

    const auto mab = get_or_create_midpoint(vertices, cache, a, b);
    const auto mbc = get_or_create_midpoint(vertices, cache, b, cc);
    const auto mcd = get_or_create_midpoint(vertices, cache, cc, d);
    const auto mda = get_or_create_midpoint(vertices, cache, d, a);

    const auto pa = vertices[a].position;
    const auto pb = vertices[b].position;
    const auto pc = vertices[cc].position;
    const auto pd = vertices[d].position;

    const auto center_pos = (pa + pb + pc + pd) * 0.25f;
    const auto center = static_cast<std::uint32_t>(vertices.size());

    const auto center_fixed = vertices[a].is_fixed && vertices[b].is_fixed && vertices[cc].is_fixed && vertices[d].is_fixed;

    vertices.push_back(dual_grid_base::dual_vertex{center_pos, center_fixed});

    out.push_back(dual_grid_base::dual_quad{a, mab, center, mda});
    out.push_back(dual_grid_base::dual_quad{mab, b, mbc, center});
    out.push_back(dual_grid_base::dual_quad{center, mbc, cc, mcd});
    out.push_back(dual_grid_base::dual_quad{mda, center, mcd, d});
  }

  return out;
}

static auto relax_cells_taubin_keep_fixed(
  std::vector<dual_grid_base::dual_vertex>& vertices,
  const std::vector<dual_grid_base::dual_quad>& cells,
  const std::uint32_t iterations,
  const std::float_t lambda,
  const std::float_t mu
) -> void {
  if (vertices.empty() || cells.empty() || iterations == 0u) {
    return;
  }

  auto adjacency = std::vector<std::vector<std::uint32_t>>{};
  adjacency.resize(vertices.size());

  const auto add_edge = [&](const auto u, const auto v) {
    adjacency[u].push_back(v);
    adjacency[v].push_back(u);
  };

  for (const auto& c : cells) {
    add_edge(c.a, c.b);
    add_edge(c.b, c.c);
    add_edge(c.c, c.d);
    add_edge(c.d, c.a);

    add_edge(c.a, c.c);
    add_edge(c.b, c.d);
  }

  for (auto& nbrs : adjacency) {
    std::sort(nbrs.begin(), nbrs.end());
    nbrs.erase(std::unique(nbrs.begin(), nbrs.end()), nbrs.end());
  }

  auto tmp = std::vector<sbx::math::vector3>{};
  auto tmp2 = std::vector<sbx::math::vector3>{};

  tmp.resize(vertices.size());
  tmp2.resize(vertices.size());

  const auto laplacian_step = [&](const std::vector<sbx::math::vector3>& src, std::vector<sbx::math::vector3>& dst, const std::float_t k) {
    for (auto i = std::size_t{0u}; i < vertices.size(); ++i) {
      auto p = src[i];

      if (vertices[i].is_fixed) {
        dst[i] = p;

        continue;
      }

      const auto& nbrs = adjacency[i];

      if (nbrs.empty()) {
        dst[i] = p;

        continue;
      }

      auto avg = sbx::math::vector3{0.0f, 0.0f, 0.0f};

      for (const auto nb : nbrs) {
        avg += src[nb];
      }

      avg *= (1.0f / static_cast<std::float_t>(nbrs.size()));

      auto delta = (avg - p);
      delta.y() = 0.0f;

      p += delta * k;
      p.y() = src[i].y();

      dst[i] = p;
    }
  };

  for (auto iter = std::uint32_t{0u}; iter < iterations; ++iter) {
    for (auto i = std::size_t{0u}; i < vertices.size(); ++i) {
      tmp[i] = vertices[i].position;
    }

    laplacian_step(tmp, tmp2, lambda);
    laplacian_step(tmp2, tmp, mu);

    for (auto i = std::size_t{0u}; i < vertices.size(); ++i) {
      vertices[i].position = tmp[i];
    }
  }
}

static auto compute_dual_quad_center(
  const dual_grid_base::dual_quad& c,
  const std::vector<dual_grid_base::dual_vertex>& vertices
) -> sbx::math::vector3 {
  const auto pa = vertices[c.a].position;
  const auto pb = vertices[c.b].position;
  const auto pc = vertices[c.c].position;
  const auto pd = vertices[c.d].position;

  return (pa + pb + pc + pd) * 0.25f;
}

static auto build_main_mesh(
  const std::vector<dual_grid_base::dual_vertex>& dual_vertices,
  const std::vector<dual_grid_base::dual_quad>& dual_quads,
  std::vector<dual_grid_base::main_vertex>& out_main_vertices,
  std::vector<dual_grid_base::main_edge>& out_main_edges,
  std::vector<std::vector<std::uint32_t>>& out_main_cells_ccw
) -> void {
  out_main_vertices.clear();
  out_main_edges.clear();
  out_main_cells_ccw.clear();

  if (dual_vertices.empty() || dual_quads.empty()) {
    return;
  }

  const auto invalid = std::numeric_limits<std::uint32_t>::max();

  // ----- 1) dual edge -> adjacent dual cells --------------------------------

  auto edge_to_cells = std::unordered_map<edge_key, edge_cell_pair, edge_key_hash>{};
  edge_to_cells.reserve(dual_quads.size() * 4u);

  const auto add_dual_edge = [&](const auto u, const auto v, const auto ci) {
    const auto key = make_edge_key(u, v);
    auto& slot = edge_to_cells[key];

    if (slot.c0 == invalid) {
      slot.c0 = ci;

      return;
    }

    if (slot.c1 == invalid) {
      slot.c1 = ci;

      return;
    }
  };

  for (auto ci = std::uint32_t{0u}; ci < static_cast<std::uint32_t>(dual_quads.size()); ++ci) {
    const auto& c = dual_quads[ci];

    add_dual_edge(c.a, c.b, ci);
    add_dual_edge(c.b, c.c, ci);
    add_dual_edge(c.c, c.d, ci);
    add_dual_edge(c.d, c.a, ci);
  }

  auto cell_is_boundary = std::vector<bool>{};
  cell_is_boundary.resize(dual_quads.size());

  for (const auto& [e, pair] : edge_to_cells) {
    if (pair.c0 != invalid && pair.c1 == invalid) {
      cell_is_boundary[pair.c0] = true;
    }
  }

  // ----- 2) main vertices = dual cell centers -------------------------------

  out_main_vertices.reserve(dual_quads.size());

  for (auto ci = std::uint32_t{0u}; ci < static_cast<std::uint32_t>(dual_quads.size()); ++ci) {
    const auto center = compute_dual_quad_center(dual_quads[ci], dual_vertices);
    out_main_vertices.push_back(dual_grid_base::main_vertex{center, cell_is_boundary[ci]});
  }

  // ----- 3) boundary main vertices (midpoints of boundary dual edges) --------

  auto boundary_main_of_edge = std::unordered_map<edge_key, std::uint32_t, edge_key_hash>{};
  boundary_main_of_edge.reserve(edge_to_cells.size());

  auto boundary_mains_of_dual_vertex = std::vector<std::vector<std::uint32_t>>{};
  boundary_mains_of_dual_vertex.resize(dual_vertices.size());

  for (const auto& [e, pair] : edge_to_cells) {
    const auto is_boundary_edge = (pair.c0 != invalid) && (pair.c1 == invalid);

    if (!is_boundary_edge) {
      continue;
    }

    const auto mp = edge_midpoint(e.u, e.v, dual_vertices);
    const auto mid_id = static_cast<std::uint32_t>(out_main_vertices.size());

    out_main_vertices.push_back(dual_grid_base::main_vertex{mp, 1u});
    boundary_main_of_edge.emplace(e, mid_id);

    boundary_mains_of_dual_vertex[e.u].push_back(mid_id);
    boundary_mains_of_dual_vertex[e.v].push_back(mid_id);
  }

  // ----- 4) main edges from dual edges --------------------------------------

  auto main_edge_set = std::unordered_set<edge_key, edge_key_hash>{};
  main_edge_set.reserve(edge_to_cells.size() * 2u);

  const auto add_main_edge_unique = [&](const auto ma, const auto mb, const auto du, const auto dv, const auto boundary) {
    if (ma == mb) {
      return;
    }

    const auto key = make_edge_key(ma, mb);

    if (main_edge_set.find(key) != main_edge_set.end()) {
      return;
    }

    main_edge_set.emplace(key);

    auto e = dual_grid_base::main_edge{};
    e.a = key.u;
    e.b = key.v;
    e.dual_u = du;
    e.dual_v = dv;
    e.is_boundary = boundary;

    out_main_edges.push_back(e);
  };

  for (const auto& [e, pair] : edge_to_cells) {
    const auto c0 = pair.c0;
    const auto c1 = pair.c1;

    if (c0 == invalid) {
      continue;
    }

    if (c1 != invalid) {
      add_main_edge_unique(c0, c1, e.u, e.v, false);

      continue;
    }

    const auto it = boundary_main_of_edge.find(e);

    if (it != boundary_main_of_edge.end()) {
      const auto mid_main = it->second;
      add_main_edge_unique(c0, mid_main, e.u, e.v, true);
    }
  }

  // ----- 5) main cells around each dual vertex -------------------------------

  out_main_cells_ccw.resize(dual_vertices.size());

  auto incident_cells = std::vector<std::vector<std::uint32_t>>{};
  incident_cells.resize(dual_vertices.size());

  for (auto ci = std::uint32_t{0u}; ci < static_cast<std::uint32_t>(dual_quads.size()); ++ci) {
    const auto& c = dual_quads[ci];

    incident_cells[c.a].push_back(ci);
    incident_cells[c.b].push_back(ci);
    incident_cells[c.c].push_back(ci);
    incident_cells[c.d].push_back(ci);
  }

  for (auto vid = std::uint32_t{0u}; vid < static_cast<std::uint32_t>(dual_vertices.size()); ++vid) {
    auto poly = std::vector<std::uint32_t>{};
    poly.reserve(incident_cells[vid].size() + boundary_mains_of_dual_vertex[vid].size());

    for (const auto ci : incident_cells[vid]) {
      poly.push_back(ci);
    }

    for (const auto mid : boundary_mains_of_dual_vertex[vid]) {
      poly.push_back(mid);
    }

    if (poly.size() < 3u) {
      continue;
    }

    const auto p = dual_vertices[vid].position;

    std::sort(poly.begin(), poly.end(), [&](const auto lhs, const auto rhs) {
      const auto pl = out_main_vertices[lhs].position;
      const auto pr = out_main_vertices[rhs].position;

      const auto al = std::atan2(pl.z() - p.z(), pl.x() - p.x());
      const auto ar = std::atan2(pr.z() - p.z(), pr.x() - p.x());

      return al < ar;
    });

    poly.erase(std::unique(poly.begin(), poly.end()), poly.end());

    out_main_cells_ccw[vid] = std::move(poly);
  }

  // ----- 6) closure edges from main cell polygons ----------------------------

  for (auto vid = std::uint32_t{0u}; vid < static_cast<std::uint32_t>(out_main_cells_ccw.size()); ++vid) {
    const auto& poly = out_main_cells_ccw[vid];

    if (poly.size() < 3u) {
      continue;
    }

    for (auto i = std::uint32_t{0u}; i < static_cast<std::uint32_t>(poly.size()); ++i) {
      const auto a = poly[i];
      const auto b = poly[(i + 1u) % static_cast<std::uint32_t>(poly.size())];

      add_main_edge_unique(a, b, invalid, invalid, true);
    }
  }
}

static auto point_in_triangle_xz(
  const sbx::math::vector3& p,
  const sbx::math::vector3& a,
  const sbx::math::vector3& b,
  const sbx::math::vector3& c
) -> bool {
  const auto sign = [](const auto& p0, const auto& p1, const auto& p2) -> std::float_t {
    return (p2.x() - p1.x()) * (p0.z() - p1.z()) - (p2.z() - p1.z()) * (p0.x() - p1.x());
  };

  const auto d1 = sign(p, a, b);
  const auto d2 = sign(p, b, c);
  const auto d3 = sign(p, c, a);

  const auto has_neg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
  const auto has_pos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);

  return !(has_neg && has_pos);
}

static auto point_in_cell_xz(
  const sbx::math::vector3& p,
  const sbx::math::vector3& a,
  const sbx::math::vector3& b,
  const sbx::math::vector3& c,
  const sbx::math::vector3& d
) -> bool {
  if (point_in_triangle_xz(p, a, b, c)) {
    return true;
  }

  if (point_in_triangle_xz(p, a, c, d)) {
    return true;
  }

  return false;
}

dual_grid_base::dual_grid_base(const settings& settings) {
  rebuild(settings);
}

auto dual_grid_base::rebuild(const settings& settings) -> void {
  sbx::math::random::seed(settings.seed);

  auto grid = generate_grid(settings.rings, settings.ring_distance);
  auto merged = merge_tris_to_cells_random(grid.vertices, grid.triangles, settings.merge_probability);

  auto cache = midpoint_cache{};
  cache.index.reserve(merged.cells.size() * 4u + merged.leftover_tris.size() * 3u);

  auto final_cells = std::vector<dual_quad>{};
  final_cells.reserve(merged.cells.size() * 4u + merged.leftover_tris.size() * 3u);

  auto c4 = split_cells_to_4(grid.vertices, merged.cells, cache);
  final_cells.insert(final_cells.end(), c4.begin(), c4.end());

  auto ct = split_leftover_tris_to_cells(grid.vertices, merged.leftover_tris, cache);
  final_cells.insert(final_cells.end(), ct.begin(), ct.end());

  relax_cells_taubin_keep_fixed( grid.vertices, final_cells, settings.relax_iterations, settings.relax_lambda, settings.relax_mu);

  _dual_vertices = std::move(grid.vertices);
  _dual_quads = std::move(final_cells);

  for (auto& q : _dual_quads) {
    canonicalize_dual_quad(q, _dual_vertices);
  }

  for (const auto& q : _dual_quads) {
    sbx::utility::assert_that(signed_area_xz_dual_quad(q, _dual_vertices) > 0.0f, "dual quad winding is not CCW");
  }

  build_main_mesh(_dual_vertices, _dual_quads, _main_vertices, _main_edges, _main_cells_ccw);

  _dual_quads_of_main_face.clear();
  _dual_quads_of_main_face.resize(_dual_vertices.size());

  for (auto qi = std::uint32_t{0u}; qi < dual_quad_count(); ++qi) {
    const auto& q = _dual_quads[qi];

    _dual_quads_of_main_face[q.a].push_back(qi);
    _dual_quads_of_main_face[q.b].push_back(qi);
    _dual_quads_of_main_face[q.c].push_back(qi);
    _dual_quads_of_main_face[q.d].push_back(qi);
  }
}

auto dual_grid_base::dual_vertices() const -> const std::vector<dual_vertex>& {
  return _dual_vertices;
}

auto dual_grid_base::dual_vertex_at(const std::size_t index) const -> const dual_vertex& {
  return _dual_vertices[index];
}

auto dual_grid_base::dual_quads() const -> const std::vector<dual_quad>& {
  return _dual_quads;
}

auto dual_grid_base::dual_quad_at(const std::size_t index) const -> const dual_quad& {
  return _dual_quads[index];
}

auto dual_grid_base::dual_quad_count() const -> std::uint32_t {
  return static_cast<std::uint32_t>(_dual_quads.size());
}

auto dual_grid_base::pick_dual_quad_at(const sbx::math::vector3& point) const -> std::uint32_t {
  if (_dual_quads.empty() || _dual_vertices.empty()) {
    return invalid_id;
  }

  for (auto ci = std::size_t{0u}; ci < _dual_quads.size(); ++ci) {
    const auto& c = _dual_quads[ci];

    const auto a = _dual_vertices[c.a].position;
    const auto b = _dual_vertices[c.b].position;
    const auto cc = _dual_vertices[c.c].position;
    const auto d = _dual_vertices[c.d].position;

    const auto min_x = std::min({a.x(), b.x(), cc.x(), d.x()});
    const auto max_x = std::max({a.x(), b.x(), cc.x(), d.x()});
    const auto min_z = std::min({a.z(), b.z(), cc.z(), d.z()});
    const auto max_z = std::max({a.z(), b.z(), cc.z(), d.z()});

    if (point.x() < min_x || point.x() > max_x || point.z() < min_z || point.z() > max_z) {
      continue;
    }

    if (point_in_cell_xz(point, a, b, cc, d)) {
      return static_cast<std::uint32_t>(ci);
    }
  }

  return invalid_id;
}

auto dual_grid_base::main_vertices() const -> std::span<const main_vertex> {
  return _main_vertices;
}

auto dual_grid_base::main_vertex_at(const std::size_t index) const -> const main_vertex& {
  return _main_vertices[index];
}

auto dual_grid_base::main_edges() const -> std::span<const main_edge> {
  return _main_edges;
}

auto dual_grid_base::main_cell_ccw(const std::uint32_t dual_vertex_id) const -> std::span<const std::uint32_t> {
  return _main_cells_ccw[dual_vertex_id];
}

auto dual_grid_base::dual_quad_center(const std::uint32_t dual_quad_id) const -> sbx::math::vector3 {
  sbx::utility::assert_that(dual_quad_id < _dual_quads.size(), "dual_quad_center(): invalid dual cell id");
  sbx::utility::assert_that(dual_quad_id < _main_vertices.size(), "dual_quad_center(): main not built");

  return _main_vertices[dual_quad_id].position;
}

static auto point_in_polygon_xz(const sbx::math::vector3& p, const std::span<const std::uint32_t> poly, const std::vector<dual_grid_base::main_vertex>& verts) -> bool {
  auto is_inside = false;

  const auto px = p.x();
  const auto pz = p.z();

  for (auto i = std::size_t{0u}, j = poly.size() - 1u; i < poly.size(); j = i, ++i) {
    const auto& vi = verts[poly[i]].position;
    const auto& vj = verts[poly[j]].position;

    const auto xi = vi.x();
    const auto zi = vi.z();
    const auto xj = vj.x();
    const auto zj = vj.z();

    const auto zi_above = (zi > pz);
    const auto zj_above = (zj > pz);

    if (zi_above == zj_above) {
      continue;
    }

    const auto denom = (zj - zi);

    if (std::abs(denom) < 1e-7f) {
      continue;
    }

    const auto x_intersect = xi + (xj - xi) * (pz - zi) / denom;

    if (px < x_intersect) {
      is_inside = !is_inside;
    }
  }

  return is_inside;
}

static auto polygon_aabb_contains_xz(const sbx::math::vector3& p, const std::span<const std::uint32_t> poly, const std::vector<dual_grid_base::main_vertex>& verts) -> bool {
  auto min_x = std::numeric_limits<std::float_t>::max();
  auto max_x = std::numeric_limits<std::float_t>::lowest();
  auto min_z = std::numeric_limits<std::float_t>::max();
  auto max_z = std::numeric_limits<std::float_t>::lowest();

  for (const auto id : poly) {
    const auto& v = verts[id].position;

    min_x = std::min(min_x, v.x());
    max_x = std::max(max_x, v.x());
    min_z = std::min(min_z, v.z());
    max_z = std::max(max_z, v.z());
  }

  if (p.x() < min_x || p.x() > max_x || p.z() < min_z || p.z() > max_z) {
    return false;
  }

  return true;
}

auto dual_grid_base::pick_main_face_at(const sbx::math::vector3& point) const -> std::uint32_t {
  if (_main_cells_ccw.empty() || _main_vertices.empty() || _dual_vertices.empty()) {
    return invalid_id;
  }

  for (auto dual_vid = std::uint32_t{0u}; dual_vid < static_cast<std::uint32_t>(_dual_vertices.size()); ++dual_vid) {
    const auto poly = main_cell_ccw(dual_vid);

    if (poly.size() < 3u) {
      continue;
    }

    if (!polygon_aabb_contains_xz(point, poly, _main_vertices)) {
      continue;
    }

    if (point_in_polygon_xz(point, poly, _main_vertices)) {
      return dual_vid;
    }
  }

  return invalid_id;
}

auto dual_grid_base::dual_quads_of_main_face(const std::uint32_t dual_vertex_id) const -> std::span<const std::uint32_t> {
  sbx::utility::assert_that(dual_vertex_id < _dual_quads_of_main_face.size(), "dual_quads_of_main_face(): out of range");

  return _dual_quads_of_main_face[dual_vertex_id];
}

} // namespace demo
