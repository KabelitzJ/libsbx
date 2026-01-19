#ifndef DEMO_DUAL_GRID_HPP_
#define DEMO_DUAL_GRID_HPP_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <libsbx/math/vector3.hpp>
#include <libsbx/utility/assert.hpp>

namespace demo {

class dual_grid_base {

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

  dual_grid_base() = default;

  explicit dual_grid_base(const settings& s) {
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

  auto quad_at(const std::size_t index) const -> const quad& {
    return _quads[index];
  }

  // primal cell id == quad id
  auto cell_count() const -> std::uint32_t {
    return static_cast<std::uint32_t>(_quads.size());
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
    sbx::utility::assert_that(quad_id < _quads.size(), "quad_center(): invalid quad id");
    sbx::utility::assert_that(quad_id < _dual_vertices.size(), "quad_center(): dual not built");
    return _dual_vertices[quad_id].position;
  }

protected:

  std::vector<vertex> _vertices{};
  std::vector<quad> _quads{};

  std::vector<dual_vertex> _dual_vertices{};
  std::vector<dual_edge> _dual_edges{};
  std::vector<std::vector<std::uint32_t>> _dual_cells_ccw{};

}; // class dual_grid_base

template<typename Type>
class dual_grid : public dual_grid_base {

  using base_type = dual_grid_base;

public:

  using settings = base_type::settings;
  using vertex = base_type::vertex;
  using quad = base_type::quad;
  using dual_vertex = base_type::dual_vertex;
  using dual_edge = base_type::dual_edge;

  using value_type = Type;

  dual_grid() = default;

  explicit dual_grid(const settings& s, const Type& default_value = Type{}) {
    rebuild(s, default_value);
  }

  auto rebuild(const settings& s, const Type& default_value = Type{}) -> void {
    base_type::rebuild(s);

    const auto n = static_cast<std::uint32_t>(base_type::_quads.size());

    _cell_to_data.resize(n);
    _data.assign(n, default_value);

    for (auto i = 0u; i < n; ++i) {
      _cell_to_data[i] = i;
    }
  }

  // ------------------- cell payload API -------------------------------------

  auto data_pool() -> std::span<Type> {
    return _data;
  }

  auto data_pool() const -> std::span<const Type> {
    return _data;
  }

  auto data_index_of_cell(const std::uint32_t cell_id) const -> std::uint32_t {
    sbx::utility::assert_that(cell_id < _cell_to_data.size(), "data_index_of_cell(): out of range");

    return _cell_to_data[cell_id];
  }

  auto cell_data(const std::uint32_t cell_id) -> Type& {
    sbx::utility::assert_that(cell_id < _cell_to_data.size(), "cell_data(): out of range");

    const auto idx = _cell_to_data[cell_id];

    sbx::utility::assert_that(idx < _data.size(), "cell_data(): bad mapping");

    return _data[idx];
  }

  auto cell_data(const std::uint32_t cell_id) const -> const Type& {
    sbx::utility::assert_that(cell_id < _cell_to_data.size(), "cell_data() const: out of range");

    const auto idx = _cell_to_data[cell_id];

    sbx::utility::assert_that(idx < _data.size(), "cell_data() const: bad mapping");

    return _data[idx];
  }

  auto set_cell_data_index(const std::uint32_t cell_id, const std::uint32_t pool_index) -> void {
    sbx::utility::assert_that(cell_id < _cell_to_data.size(), "set_cell_data_index(): out of range");
    sbx::utility::assert_that(pool_index < _data.size(), "set_cell_data_index(): pool out of range");

    _cell_to_data[cell_id] = pool_index;
  }

  auto allocate_data_slot(const Type& value = Type{}) -> std::uint32_t {
    _data.push_back(value);

    return static_cast<std::uint32_t>(_data.size() - 1u);
  }

  auto reset_cell_mapping_identity(const Type& default_value = Type{}) -> void {
    const auto n = static_cast<std::uint32_t>(base_type::_quads.size());

    _cell_to_data.resize(n);
    _data.assign(n, default_value);

    for (auto i = 0u; i < n; ++i) {
      _cell_to_data[i] = i;
    }
  }

private:

  std::vector<Type> _data{};
  std::vector<std::uint32_t> _cell_to_data{};

}; // class dual_grid

} // namespace demo

#endif // DEMO_DUAL_GRID_HPP_
