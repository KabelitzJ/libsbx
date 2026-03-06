// SPDX-License-Identifier: MIT
#ifndef DEMO_WORLD_HPP_
#define DEMO_WORLD_HPP_

#include <demo/grid.hpp>
#include <demo/terrain.hpp>

namespace demo {

class world {

  inline static constexpr auto slope_penalty = 2.0f;

public:

  world(std::uint32_t w, std::uint32_t h)
  : _grid{w, h},
    _terrain{w, h} { }

  auto flatten_for_building(std::uint32_t cx, std::uint32_t cy, std::uint32_t size_w, std::uint32_t size_h) -> void {
    auto avg   = 0.0f;
    auto count = 0u;

    for (auto vy = cy; vy <= cy + size_h; ++vy) {
      for (auto vx = cx; vx <= cx + size_w; ++vx) {
        avg += _terrain.get_height(vx, vy);
        ++count;
      }
    }

    avg /= static_cast<float>(count);

    for (auto vy = cy; vy <= cy + size_h; ++vy) {
      for (auto vx = cx; vx <= cx + size_w; ++vx) {
        _terrain.set_height(vx, vy, avg);
      }
    }
  }

  auto get_slope_cost(std::uint32_t cx, std::uint32_t cy) const -> float {
    auto h00 = _terrain.get_height(cx, cy);
    auto h10 = _terrain.get_height(cx + 1, cy);
    auto h01 = _terrain.get_height(cx, cy + 1);
    auto h11 = _terrain.get_height(cx + 1, cy + 1);

    auto max_diff = std::max({
      std::abs(h00 - h10),
      std::abs(h00 - h01),
      std::abs(h10 - h11),
      std::abs(h01 - h11),
      std::abs(h00 - h11),
      std::abs(h10 - h01)
    });

    return max_diff * slope_penalty;
  }

  auto grid() -> demo::grid& {
    return _grid;
  }

  auto grid() const -> const demo::grid& {
    return _grid;
  }

  auto terrain() -> demo::terrain& {
    return _terrain;
  }

  auto terrain() const -> const demo::terrain& {
    return _terrain;
  }

private:

  demo::grid _grid;
  demo::terrain _terrain;

}; // class world

} // namespace demo

#endif // DEMO_WORLD_HPP_