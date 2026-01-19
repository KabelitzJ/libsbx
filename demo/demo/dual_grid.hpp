#ifndef DEMO_DUAL_GRID_HPP_
#define DEMO_DUAL_GRID_HPP_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <libsbx/math/vector3.hpp>

namespace demo {

class dual_grid {

public:

  inline static constexpr auto invalid_id = std::numeric_limits<std::uint32_t>::max();

  struct vertex {
    sbx::math::vector3 position{};
    bool is_fixed = false;
  };

  struct quad {
    std::uint32_t a = 0u;
    std::uint32_t b = 0u;
    std::uint32_t c = 0u;
    std::uint32_t d = 0u;
  };

  struct dual_vertex {
    sbx::math::vector3 position{};
    bool is_boundary = false;
  };

  struct dual_edge {
    std::uint32_t a = 0u;
    std::uint32_t b = 0u;

    std::uint32_t primal_u = invalid_id;
    std::uint32_t primal_v = invalid_id;

    bool is_boundary = false;
  };

  struct settings {
    std::uint32_t rings = 6u;
    std::float_t ring_distance = 6.0f;

    std::uint32_t seed = 0u;
    std::float_t merge_probability = 0.5f;

    bool split_quads_to_4 = true;
    bool split_leftover_tris_to_3 = true;

    bool relax = true;
    std::uint32_t relax_iterations = 80u;
    std::float_t relax_lambda = 0.45f;
    std::float_t relax_mu = -0.50f;
    bool relax_add_diagonals = true;

    bool build_dual = true;
  };

  dual_grid() = default;

  explicit dual_grid(const settings& s) {
    rebuild(s);
  }

  auto rebuild(const settings& s) -> void;

  // ----- primal -------------------------------------------------------------

  auto vertices() const -> std::span<const vertex> {
    return _vertices;
  }

  auto vertex_at(const std::size_t index) const -> const vertex& {
    return _vertices[index];
  }

  auto quads() const -> std::span<const quad> {
    return _quads;
  }

  // ----- dual ---------------------------------------------------------------

  auto dual_vertices() const -> std::span<const dual_vertex> {
    return _dual_vertices;
  }

  auto dual_vertex_at(const std::size_t index) const -> const dual_vertex& {
    return _dual_vertices[index];
  }

  auto dual_edges() const -> std::span<const dual_edge> {
    return _dual_edges;
  }

  auto dual_cell_ccw(const std::uint32_t primal_vertex_id) const -> std::span<const std::uint32_t> {
    return _dual_cells_ccw[primal_vertex_id];
  }

  // Convention:
  // dual vertex id == quad id for 0..quads.size()-1
  auto quad_center(const std::uint32_t quad_id) const -> sbx::math::vector3 {
    return _dual_vertices[quad_id].position;
  }

private:

  std::vector<vertex> _vertices{};
  std::vector<quad> _quads{};

  std::vector<dual_vertex> _dual_vertices{};
  std::vector<dual_edge> _dual_edges{};
  std::vector<std::vector<std::uint32_t>> _dual_cells_ccw{};
};

} // namespace demo

#endif // DEMO_DUAL_GRID_HPP_
