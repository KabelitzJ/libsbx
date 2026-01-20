#ifndef DEMO_DUAL_GRID_HPP_
#define DEMO_DUAL_GRID_HPP_

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <libsbx/math/vector3.hpp>
#include <libsbx/utility/assert.hpp>

namespace demo {

class dual_grid_base {

public:

  inline static constexpr auto invalid_id = std::numeric_limits<std::uint32_t>::max();

  struct dual_vertex {
    sbx::math::vector3 position{};
    std::uint32_t is_fixed = 0u;
  }; // struct dual_vertex

  struct dual_quad {
    std::uint32_t a = 0u;
    std::uint32_t b = 0u;
    std::uint32_t c = 0u;
    std::uint32_t d = 0u;
  }; // struct dual_quad

  struct main_vertex {
    sbx::math::vector3 position{};
    std::uint32_t is_boundary = 0u;
  }; // struct main_vertex

  struct main_edge {
    std::uint32_t a = 0u;
    std::uint32_t b = 0u;

    std::uint32_t dual_u = invalid_id;
    std::uint32_t dual_v = invalid_id;

    bool is_boundary = false;
  }; // struct main_edge

  struct settings {
    std::uint32_t rings = 6u;
    std::float_t ring_distance = 6.0f;

    std::uint32_t seed = 0u;
    std::float_t merge_probability = 0.5f;

    std::uint32_t relax_iterations = 80u;
    std::float_t relax_lambda = 0.45f;
    std::float_t relax_mu = -0.50f;
  }; // struct settings

  dual_grid_base() = default;

  explicit dual_grid_base(const settings& settings);

  auto rebuild(const settings& settings) -> void;

  auto dual_vertices() const -> const std::vector<dual_vertex>&;

  auto dual_vertex_at(const std::size_t index) const -> const dual_vertex&;

  auto dual_quads() const -> const std::vector<dual_quad>&;

  auto dual_quad_at(const std::size_t index) const -> const dual_quad&;

  auto dual_quad_count() const -> std::uint32_t;

  auto pick_dual_quad_at(const sbx::math::vector3& point) const -> std::uint32_t;

  auto main_vertices() const -> std::span<const main_vertex>;

  auto main_vertex_at(const std::size_t index) const -> const main_vertex&;

  auto main_edges() const -> std::span<const main_edge>;

  auto main_cell_ccw(const std::uint32_t dual_vertex_id) const -> std::span<const std::uint32_t>;

  auto dual_quad_center(const std::uint32_t dual_quad_id) const -> sbx::math::vector3;

  auto pick_main_face_at(const sbx::math::vector3& point) const -> std::uint32_t;

  auto dual_quads_of_main_face(const std::uint32_t dual_vertex_id) const -> std::span<const std::uint32_t>;

protected:

  std::vector<dual_vertex> _dual_vertices{};
  std::vector<dual_quad> _dual_quads{};

  std::vector<main_vertex> _main_vertices{};
  std::vector<main_edge> _main_edges{};
  std::vector<std::vector<std::uint32_t>> _main_cells_ccw{};

  std::vector<std::vector<std::uint32_t>> _dual_quads_of_main_face{};

}; // class dual_grid_base

template<typename Type>
class dual_grid : public dual_grid_base {

public:

  using base_type = dual_grid_base;

  inline static constexpr auto invalid_data_index = std::numeric_limits<std::uint32_t>::max();

  using settings = base_type::settings;
  using dual_vertex = base_type::dual_vertex;
  using dual_quad = base_type::dual_quad;
  using main_vertex = base_type::main_vertex;
  using main_edge = base_type::main_edge;
  using value_type = Type;

  dual_grid() = default;

  explicit dual_grid(const settings& settings) {
    rebuild(settings);
  }

  auto rebuild(const settings& settings) -> void {
    base_type::rebuild(settings);

    const auto n = static_cast<std::uint32_t>(base_type::_dual_vertices.size());

    _data.clear();
    _data.reserve(n);

    _cell_to_data.clear();
    _cell_to_data.resize(n, invalid_data_index);
  }

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

  auto has_cell_data(const std::uint32_t cell_id) const -> bool {
    sbx::utility::assert_that(cell_id < _cell_to_data.size(), "has_cell_data(): out of range");

    return _cell_to_data[cell_id] != invalid_data_index;
  }

  template<typename... Args>
  requires(std::is_constructible_v<Type, Args...>)
  auto get_or_create_cell_data(const std::uint32_t cell_id, Args&&... args) -> Type& {
    sbx::utility::assert_that(cell_id < _cell_to_data.size(), "get_or_create_cell_data(): out of range");

    auto& data_index = _cell_to_data[cell_id];

    if (data_index == invalid_data_index) {
      data_index = static_cast<std::uint32_t>(_data.size());

      _data.emplace_back(std::forward<Args>(args)...);
    }

    sbx::utility::assert_that(data_index < _data.size(), "get_or_create_cell_data(): bad mapping");

    return _data[data_index];
  }

private:

  std::vector<Type> _data{};
  std::vector<std::uint32_t> _cell_to_data{};

}; // class dual_grid

} // namespace demo

#endif // DEMO_DUAL_GRID_HPP_
