#include <demo/dual_grid.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/math/algorithm.hpp>
#include <libsbx/math/random.hpp>

namespace demo {

struct triangle {
  std::uint32_t a{};
  std::uint32_t b{};
  std::uint32_t c{};
};

struct hex_triangle_grid {
  std::vector<dual_grid_base::vertex> vertices{};
  std::vector<triangle> triangles{};
};

struct axial_key {
  int q{};
  int r{};

  auto operator==(const axial_key& other) const -> bool {
    return q == other.q && r == other.r;
  }
};

struct axial_key_hash {
  auto operator()(const axial_key& k) const noexcept -> std::size_t {
    auto h = std::size_t{0};
    sbx::utility::hash_combine(h, k.q, k.r);
    return h;
  }
};

struct edge_key {
  std::uint32_t u{};
  std::uint32_t v{};

  auto operator==(const edge_key& other) const -> bool {
    return u == other.u && v == other.v;
  }
};

struct edge_key_hash {
  auto operator()(const edge_key& e) const noexcept -> std::size_t {
    auto h = std::size_t{0};
    sbx::utility::hash_combine(h, e.u, e.v);
    return h;
  }
};

struct edge_tri_pair {
  std::uint32_t tri0 = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t tri1 = std::numeric_limits<std::uint32_t>::max();
};

struct edge_quad_pair {
  std::uint32_t q0 = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t q1 = std::numeric_limits<std::uint32_t>::max();
};

struct quadify_result {
  std::vector<dual_grid_base::quad> quads{};
  std::vector<triangle> leftover_tris{};
};

struct midpoint_cache {
  std::unordered_map<edge_key, std::uint32_t, edge_key_hash> index{};
};

static auto make_edge_key(const std::uint32_t a, const std::uint32_t b) -> edge_key {
  return (a < b) ? edge_key{a, b} : edge_key{b, a};
}

static auto edge_midpoint(
  const std::uint32_t u,
  const std::uint32_t v,
  const std::vector<dual_grid_base::vertex>& vertices
) -> sbx::math::vector3 {
  const auto pu = vertices[u].position;
  const auto pv = vertices[v].position;
  return (pu + pv) * 0.5f;
}

static auto get_or_create_midpoint(
  std::vector<dual_grid_base::vertex>& vertices,
  midpoint_cache& cache,
  const std::uint32_t u,
  const std::uint32_t v
) -> std::uint32_t {
  const auto key = make_edge_key(u, v);

  if (const auto it = cache.index.find(key); it != cache.index.end()) {
    return it->second;
  }

  const auto pm = edge_midpoint(u, v, vertices);

  const auto idx = static_cast<std::uint32_t>(vertices.size());
  const auto is_fixed = vertices[u].is_fixed && vertices[v].is_fixed;

  vertices.push_back(dual_grid_base::vertex{pm, is_fixed});
  cache.index.emplace(key, idx);

  return idx;
}

static auto generate_grid(const std::uint32_t rings, const std::float_t ring_distance) -> hex_triangle_grid {
  auto result = hex_triangle_grid{};

  const auto radius = rings;

  const auto vertex_count = 1u + 3u * radius * (radius + 1u);
  const auto triangle_count = 6u * radius * radius;

  result.vertices = sbx::utility::make_reserved_vector<dual_grid_base::vertex>(vertex_count);
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
          return (v < 0) ? -v : v;
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
        result.vertices.push_back(dual_grid_base::vertex{axial_to_world(q, r), is_fixed});
        ++idx;
      }
    }
  }

  const auto invalid = std::numeric_limits<std::uint32_t>::max();

  const auto find_index = [&](const auto q, const auto r) -> std::uint32_t {
    if (const auto it = index_of.find(axial_key{q, r}); it != index_of.end()) {
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
  if (t.a != u && t.a != v) return t.a;
  if (t.b != u && t.b != v) return t.b;
  if (t.c != u && t.c != v) return t.c;
  return std::numeric_limits<std::uint32_t>::max();
}

static auto signed_area_xz(const dual_grid_base::quad& q, const std::vector<dual_grid_base::vertex>& vertices) -> std::float_t {
  const auto p0 = vertices[q.a].position;
  const auto p1 = vertices[q.b].position;
  const auto p2 = vertices[q.c].position;
  const auto p3 = vertices[q.d].position;

  const auto x0 = p0.x(); const auto z0 = p0.z();
  const auto x1 = p1.x(); const auto z1 = p1.z();
  const auto x2 = p2.x(); const auto z2 = p2.z();
  const auto x3 = p3.x(); const auto z3 = p3.z();

  return 0.5f * (
    x0 * z1 - x1 * z0 +
    x1 * z2 - x2 * z1 +
    x2 * z3 - x3 * z2 +
    x3 * z0 - x0 * z3
  );
}

static auto make_merged_quad_ccw(
  const triangle& t0,
  const triangle& t1,
  const edge_key& e,
  const std::vector<dual_grid_base::vertex>& vertices
) -> dual_grid_base::quad {
  const auto u = e.u;
  const auto v = e.v;

  const auto w0 = third_vertex(t0, u, v);
  const auto w1 = third_vertex(t1, u, v);

  auto q = dual_grid_base::quad{w0, u, w1, v};

  if (signed_area_xz(q, vertices) < 0.0f) {
    q = dual_grid_base::quad{w0, v, w1, u};
  }

  return q;
}

static auto merge_tris_to_quads_random(
  const std::vector<dual_grid_base::vertex>& vertices,
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

  auto tri_used = std::vector<bool>(triangles.size(), false);

  result.quads = sbx::utility::make_reserved_vector<dual_grid_base::quad>(triangles.size() / 2u);

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

    const auto merged_quad = make_merged_quad_ccw(triangles[t0], triangles[t1], e, vertices);
    result.quads.push_back(merged_quad);

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

static auto signed_area_xz_triangle(const triangle& t, const std::vector<dual_grid_base::vertex>& vertices) -> std::float_t {
  const auto p0 = vertices[t.a].position;
  const auto p1 = vertices[t.b].position;
  const auto p2 = vertices[t.c].position;

  const auto x0 = p0.x(); const auto z0 = p0.z();
  const auto x1 = p1.x(); const auto z1 = p1.z();
  const auto x2 = p2.x(); const auto z2 = p2.z();

  return 0.5f * (
    x0 * (z1 - z2) +
    x1 * (z2 - z0) +
    x2 * (z0 - z1)
  );
}

static auto split_leftover_tris_to_quads(
  std::vector<dual_grid_base::vertex>& vertices,
  const std::vector<triangle>& leftover_tris,
  midpoint_cache& cache
) -> std::vector<dual_grid_base::quad> {
  auto out = std::vector<dual_grid_base::quad>{};
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
    vertices.push_back(dual_grid_base::vertex{center_pos, false});

    const auto mab = get_or_create_midpoint(vertices, cache, a, b);
    const auto mbc = get_or_create_midpoint(vertices, cache, b, c);
    const auto mca = get_or_create_midpoint(vertices, cache, c, a);

    out.push_back(dual_grid_base::quad{a, mab, center, mca});
    out.push_back(dual_grid_base::quad{b, mbc, center, mab});
    out.push_back(dual_grid_base::quad{c, mca, center, mbc});
  }

  return out;
}

static auto split_quads_to_4(
  std::vector<dual_grid_base::vertex>& vertices,
  const std::vector<dual_grid_base::quad>& in_quads,
  midpoint_cache& cache
) -> std::vector<dual_grid_base::quad> {
  auto out = std::vector<dual_grid_base::quad>{};
  out.reserve(in_quads.size() * 4u);

  vertices.reserve(vertices.size() + in_quads.size() * 5u);

  for (const auto& q : in_quads) {
    const auto a = q.a;
    const auto b = q.b;
    const auto c = q.c;
    const auto d = q.d;

    const auto mab = get_or_create_midpoint(vertices, cache, a, b);
    const auto mbc = get_or_create_midpoint(vertices, cache, b, c);
    const auto mcd = get_or_create_midpoint(vertices, cache, c, d);
    const auto mda = get_or_create_midpoint(vertices, cache, d, a);

    const auto pa = vertices[a].position;
    const auto pb = vertices[b].position;
    const auto pc = vertices[c].position;
    const auto pd = vertices[d].position;

    const auto center_pos = (pa + pb + pc + pd) * 0.25f;
    const auto center = static_cast<std::uint32_t>(vertices.size());

    const auto center_fixed =
      vertices[a].is_fixed &&
      vertices[b].is_fixed &&
      vertices[c].is_fixed &&
      vertices[d].is_fixed;

    vertices.push_back(dual_grid_base::vertex{center_pos, center_fixed});

    out.push_back(dual_grid_base::quad{a, mab, center, mda});
    out.push_back(dual_grid_base::quad{mab, b, mbc, center});
    out.push_back(dual_grid_base::quad{center, mbc, c, mcd});
    out.push_back(dual_grid_base::quad{mda, center, mcd, d});
  }

  return out;
}

static auto relax_quads_taubin_keep_fixed(
  std::vector<dual_grid_base::vertex>& vertices,
  const std::vector<dual_grid_base::quad>& quads,
  const std::uint32_t iterations,
  const std::float_t lambda,
  const std::float_t mu,
  const bool add_diagonals
) -> void {
  if (vertices.empty() || quads.empty() || iterations == 0u) {
    return;
  }

  auto adjacency = std::vector<std::vector<std::uint32_t>>(vertices.size());

  const auto add_edge = [&](const auto u, const auto v) {
    adjacency[u].push_back(v);
    adjacency[v].push_back(u);
  };

  for (const auto& q : quads) {
    add_edge(q.a, q.b);
    add_edge(q.b, q.c);
    add_edge(q.c, q.d);
    add_edge(q.d, q.a);

    if (add_diagonals) {
      add_edge(q.a, q.c);
      add_edge(q.b, q.d);
    }
  }

  for (auto& nbrs : adjacency) {
    std::sort(nbrs.begin(), nbrs.end());
    nbrs.erase(std::unique(nbrs.begin(), nbrs.end()), nbrs.end());
  }

  auto tmp = std::vector<sbx::math::vector3>(vertices.size());
  auto tmp2 = std::vector<sbx::math::vector3>(vertices.size());

  const auto laplacian_step = [&](const std::vector<sbx::math::vector3>& src,
                                  std::vector<sbx::math::vector3>& dst,
                                  const std::float_t k) {
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

static auto compute_quad_center(const dual_grid_base::quad& q, const std::vector<dual_grid_base::vertex>& vertices) -> sbx::math::vector3 {
  const auto pa = vertices[q.a].position;
  const auto pb = vertices[q.b].position;
  const auto pc = vertices[q.c].position;
  const auto pd = vertices[q.d].position;

  return (pa + pb + pc + pd) * 0.25f;
}

static auto build_dual_mesh(
  const std::vector<dual_grid_base::vertex>& primal_vertices,
  const std::vector<dual_grid_base::quad>& primal_quads,
  std::vector<dual_grid_base::dual_vertex>& out_dual_vertices,
  std::vector<dual_grid_base::dual_edge>& out_dual_edges,
  std::vector<std::vector<std::uint32_t>>& out_dual_cells_ccw
) -> void {
  out_dual_vertices.clear();
  out_dual_edges.clear();
  out_dual_cells_ccw.clear();

  if (primal_vertices.empty() || primal_quads.empty()) {
    return;
  }

  const auto invalid = std::numeric_limits<std::uint32_t>::max();

  // ----- 1) primal edge -> adjacent quads -----------------------------------

  auto edge_to_quads = std::unordered_map<edge_key, edge_quad_pair, edge_key_hash>{};
  edge_to_quads.reserve(primal_quads.size() * 4u);

  const auto add_primal_edge = [&](const auto u, const auto v, const auto qi) {
    const auto key = make_edge_key(u, v);
    auto& slot = edge_to_quads[key];

    if (slot.q0 == invalid) {
      slot.q0 = qi;
      return;
    }

    if (slot.q1 == invalid) {
      slot.q1 = qi;
      return;
    }
  };

  for (auto qi = std::uint32_t{0u}; qi < static_cast<std::uint32_t>(primal_quads.size()); ++qi) {
    const auto& q = primal_quads[qi];
    add_primal_edge(q.a, q.b, qi);
    add_primal_edge(q.b, q.c, qi);
    add_primal_edge(q.c, q.d, qi);
    add_primal_edge(q.d, q.a, qi);
  }

  auto quad_is_boundary = std::vector<bool>(primal_quads.size(), false);

  for (const auto& [e, pair] : edge_to_quads) {
    if (pair.q0 != invalid && pair.q1 == invalid) {
      quad_is_boundary[pair.q0] = true;
    }
  }

  // ----- 2) dual vertices = quad centers ------------------------------------

  out_dual_vertices.reserve(primal_quads.size());

  for (auto qi = std::uint32_t{0u}; qi < static_cast<std::uint32_t>(primal_quads.size()); ++qi) {
    const auto c = compute_quad_center(primal_quads[qi], primal_vertices);
    out_dual_vertices.push_back(dual_grid_base::dual_vertex{c, quad_is_boundary[qi]});
  }

  // ----- 3) boundary dual vertices (midpoints of boundary primal edges) ------

  auto boundary_dual_of_edge = std::unordered_map<edge_key, std::uint32_t, edge_key_hash>{};
  boundary_dual_of_edge.reserve(edge_to_quads.size());

  auto boundary_duals_of_vertex = std::vector<std::vector<std::uint32_t>>(primal_vertices.size());

  for (const auto& [e, pair] : edge_to_quads) {
    const auto is_boundary_edge = (pair.q0 != invalid) && (pair.q1 == invalid);
    if (!is_boundary_edge) {
      continue;
    }

    const auto mp = edge_midpoint(e.u, e.v, primal_vertices);
    const auto did = static_cast<std::uint32_t>(out_dual_vertices.size());

    out_dual_vertices.push_back(dual_grid_base::dual_vertex{mp, true});
    boundary_dual_of_edge.emplace(e, did);

    boundary_duals_of_vertex[e.u].push_back(did);
    boundary_duals_of_vertex[e.v].push_back(did);
  }

  // ----- 4) dual edges from primal edges ------------------------------------

  auto dual_edge_set = std::unordered_set<edge_key, edge_key_hash>{};
  dual_edge_set.reserve(edge_to_quads.size() * 2u);

  const auto add_dual_edge_unique = [&](const auto da, const auto db,
                                       const auto pu, const auto pv,
                                       const auto boundary) {
    if (da == db) {
      return;
    }

    const auto key = make_edge_key(da, db);
    if (dual_edge_set.find(key) != dual_edge_set.end()) {
      return;
    }

    dual_edge_set.emplace(key);

    auto e = dual_grid_base::dual_edge{};
    e.a = key.u;
    e.b = key.v;
    e.primal_u = pu;
    e.primal_v = pv;
    e.is_boundary = boundary;

    out_dual_edges.push_back(e);
  };

  for (const auto& [e, pair] : edge_to_quads) {
    const auto q0 = pair.q0;
    const auto q1 = pair.q1;

    if (q0 == invalid) {
      continue;
    }

    if (q1 != invalid) {
      // internal primal edge => connect quad centers
      add_dual_edge_unique(q0, q1, e.u, e.v, false);
      continue;
    }

    // boundary primal edge => connect quad center to boundary midpoint dual vertex
    if (const auto it = boundary_dual_of_edge.find(e); it != boundary_dual_of_edge.end()) {
      const auto mid_dual = it->second;
      add_dual_edge_unique(q0, mid_dual, e.u, e.v, true);
    }
  }

  // ----- 5) dual cells around each primal vertex -----------------------------

  out_dual_cells_ccw = std::vector<std::vector<std::uint32_t>>(primal_vertices.size());

  auto incident_quads = std::vector<std::vector<std::uint32_t>>(primal_vertices.size());

  for (auto qi = std::uint32_t{0u}; qi < static_cast<std::uint32_t>(primal_quads.size()); ++qi) {
    const auto& q = primal_quads[qi];
    incident_quads[q.a].push_back(qi);
    incident_quads[q.b].push_back(qi);
    incident_quads[q.c].push_back(qi);
    incident_quads[q.d].push_back(qi);
  }

  for (auto vid = std::uint32_t{0u}; vid < static_cast<std::uint32_t>(primal_vertices.size()); ++vid) {
    auto poly = std::vector<std::uint32_t>{};
    poly.reserve(incident_quads[vid].size() + boundary_duals_of_vertex[vid].size());

    // quad centers
    for (const auto qi : incident_quads[vid]) {
      poly.push_back(qi);
    }

    // boundary midpoints
    for (const auto mid : boundary_duals_of_vertex[vid]) {
      poly.push_back(mid);
    }

    if (poly.size() < 3u) {
      continue;
    }

    const auto p = primal_vertices[vid].position;

    std::sort(poly.begin(), poly.end(), [&](const auto lhs, const auto rhs) {
      const auto pl = out_dual_vertices[lhs].position;
      const auto pr = out_dual_vertices[rhs].position;

      const auto al = std::atan2(pl.z() - p.z(), pl.x() - p.x());
      const auto ar = std::atan2(pr.z() - p.z(), pr.x() - p.x());

      return al < ar;
    });

    // Optional: remove duplicates (shouldn't happen, but safe)
    poly.erase(std::unique(poly.begin(), poly.end()), poly.end());

    out_dual_cells_ccw[vid] = std::move(poly);
  }

  // ----- 6) closure edges from dual cell polygons ----------------------------
  // This ensures boundary cells close (adds midpoint-to-midpoint edges).
  // For interior vertices it mostly duplicates existing edges (skipped by set).

  for (auto vid = std::uint32_t{0u}; vid < static_cast<std::uint32_t>(out_dual_cells_ccw.size()); ++vid) {
    const auto& poly = out_dual_cells_ccw[vid];
    if (poly.size() < 3u) {
      continue;
    }

    for (auto i = std::uint32_t{0u}; i < static_cast<std::uint32_t>(poly.size()); ++i) {
      const auto a = poly[i];
      const auto b = poly[(i + 1u) % static_cast<std::uint32_t>(poly.size())];

      // closure edges do not necessarily correspond to a single primal edge
      add_dual_edge_unique(a, b, invalid, invalid, true);
    }
  }
}

static auto _point_in_triangle_xz(const sbx::math::vector3& p, const sbx::math::vector3& a, const sbx::math::vector3& b, const sbx::math::vector3& c) -> bool {
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

static auto _point_in_quad_xz(const sbx::math::vector3& p, const sbx::math::vector3& a, const sbx::math::vector3& b, const sbx::math::vector3& c, const sbx::math::vector3& d) -> bool {
  if (_point_in_triangle_xz(p, a, b, c)) {
    return true;
  }

  if (_point_in_triangle_xz(p, a, c, d)) {
    return true;
  }

  return false;
}

auto dual_grid_base::rebuild(const settings& settings) -> void {
  sbx::math::random::seed(settings.seed);

  auto grid = generate_grid(settings.rings, settings.ring_distance);
  auto merged = merge_tris_to_quads_random(grid.vertices, grid.triangles, settings.merge_probability);

  auto cache = midpoint_cache{};
  cache.index.reserve(merged.quads.size() * 4u + merged.leftover_tris.size() * 3u);

  auto final_quads = std::vector<quad>{};
  final_quads.reserve(merged.quads.size() * 4u + merged.leftover_tris.size() * 3u);

  if (settings.split_quads_to_4) {
    auto q4 = split_quads_to_4(grid.vertices, merged.quads, cache);
    final_quads.insert(final_quads.end(), q4.begin(), q4.end());
  } else {
    final_quads.insert(final_quads.end(), merged.quads.begin(), merged.quads.end());
  }

  if (settings.split_leftover_tris_to_3) {
    auto qt = split_leftover_tris_to_quads(grid.vertices, merged.leftover_tris, cache);
    final_quads.insert(final_quads.end(), qt.begin(), qt.end());
  }

  if (settings.relax) {
    relax_quads_taubin_keep_fixed(
      grid.vertices,
      final_quads,
      settings.relax_iterations,
      settings.relax_lambda,
      settings.relax_mu,
      settings.relax_add_diagonals
    );
  }

  _vertices = std::move(grid.vertices);
  _quads = std::move(final_quads);

  // ----- build dual ----------------------------------------------------------

  if (settings.build_dual) {
    build_dual_mesh(_vertices, _quads, _dual_vertices, _dual_edges, _dual_cells_ccw);
  } else {
    _dual_vertices.clear();
    _dual_edges.clear();
    _dual_cells_ccw.clear();
  }
}

auto dual_grid_base::pick_primal_quad_at(const sbx::math::vector3& point) const -> std::uint32_t {
  if (_quads.empty() || _vertices.empty()) {
    return invalid_id;
  }

  for (auto qi = 0u; qi < _quads.size(); ++qi) {
    const auto& q = _quads[qi];

    const auto a = _vertices[q.a].position;
    const auto b = _vertices[q.b].position;
    const auto c = _vertices[q.c].position;
    const auto d = _vertices[q.d].position;

    const auto min_x = std::min({a.x(), b.x(), c.x(), d.x()});
    const auto max_x = std::max({a.x(), b.x(), c.x(), d.x()});
    const auto min_z = std::min({a.z(), b.z(), c.z(), d.z()});
    const auto max_z = std::max({a.z(), b.z(), c.z(), d.z()});

    if (point.x() < min_x || point.x() > max_x || point.z() < min_z || point.z() > max_z) {
      continue;
    }

    if (_point_in_quad_xz(point, a, b, c, d)) {
      return qi;
    }
  }

  return invalid_id;
}

} // namespace demo
