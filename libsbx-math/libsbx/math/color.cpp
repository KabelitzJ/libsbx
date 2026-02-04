// SPDX-License-Identifier: MIT
#include <libsbx/math/color.hpp>

#include <libsbx/utility/enum.hpp>

namespace sbx::math {

static constexpr auto extract(std::uint32_t rgba, std::uint8_t shift) noexcept -> std::uint8_t {
  return static_cast<std::uint8_t>((rgba >> shift) & 0xFFu);
}

static constexpr  auto scale(std::uint8_t component) noexcept -> std::float_t {
  return static_cast<std::float_t>(component) / 255.0f;
}

enum class component : std::uint8_t {
  red = 24u,
  green = 16u,
  blue = 8u,
  alpha = 0u
}; // enum class component

static constexpr auto extract_component(std::uint32_t rgba, component component) noexcept -> std::float_t {
  return scale(extract(rgba, utility::to_underlying(component)));
}

color::color() noexcept
: _red{1.0f},
  _green{1.0f},
  _blue{1.0f},
  _alpha{1.0f} { }

color::color(std::uint32_t rgba) noexcept
: _red{extract_component(rgba, component::red)},
  _green{extract_component(rgba, component::green)},
  _blue{extract_component(rgba, component::blue)},
  _alpha{extract_component(rgba, component::alpha)} { }

color::color(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) noexcept
: _red{static_cast<std::float_t>(red) / 255.0f},
  _green{static_cast<std::float_t>(green) / 255.0f},
  _blue{static_cast<std::float_t>(blue) / 255.0f},
  _alpha{static_cast<std::float_t>(alpha) / 255.0f} { }

template<std::floating_point Type>
color::color(Type red, Type green, Type blue, Type alpha) noexcept
: _red{static_cast<std::float_t>(red)},
  _green{static_cast<std::float_t>(green)},
  _blue{static_cast<std::float_t>(blue)},
  _alpha{static_cast<std::float_t>(alpha)} { }

auto color::black() noexcept -> color {
  return color{0.0f, 0.0f, 0.0f, 1.0f};
}

auto color::white() noexcept -> color {
  return color{1.0f, 1.0f, 1.0f, 1.0f};
}

auto color::red() noexcept -> color {
  return color{1.0f, 0.0f, 0.0f, 1.0f};
}

auto color::green() noexcept -> color {
  return color{0.0f, 1.0f, 0.0f, 1.0f};
}

auto color::blue() noexcept -> color {
  return color{0.0f, 0.0f, 1.0f, 1.0f};
}

auto color::magenta() noexcept -> color {
  return color{1.0f, 0.0f, 1.0f, 1.0f};
}

auto color::yellow() noexcept -> color {
  return color{1.0f, 1.0f, 0.0f, 1.0f};
}

auto color::cyan() noexcept -> color {
  return color{0.0f, 1.0f, 1.0f, 1.0f};
}

auto color::orange() noexcept -> color {
  return color{1.0f, 0.5f, 0.0f, 1.0f};
}

auto color::r() const noexcept -> const std::float_t& {
  return _red;
}

auto color::r() noexcept -> std::float_t& {
  return _red;
}

auto color::g() const noexcept -> const std::float_t& {
  return _green;
}

auto color::g() noexcept -> std::float_t& {
  return _green;
}

auto color::b() const noexcept -> const std::float_t& {
  return _blue;
}

auto color::b() noexcept -> std::float_t& {
  return _blue;
}

auto color::a() const noexcept -> const std::float_t& {
  return _alpha;
}

auto color::a() noexcept -> std::float_t& {
  return _alpha;
}

auto operator==(const color& lhs, const color& rhs) noexcept -> bool {
  return lhs.r() == rhs.r() && lhs.g() == rhs.g() && lhs.b() == rhs.b() && lhs.a() == rhs.a();
}

auto operator*(color lhs, const std::float_t value) -> color {
  return color{lhs.r() * value, lhs.g() * value, lhs.b() * value, lhs.a()};
}

} // namespace sbx::math

auto YAML::convert<sbx::math::color>::encode(const sbx::math::color& color) -> Node {
  auto node = Node{};

  node.SetStyle(YAML::EmitterStyle::Flow);

  node["r"] = color.r();
  node["g"] = color.g();
  node["b"] = color.b();
  node["a"] = color.a();

  return node;
}

auto YAML::convert<sbx::math::color>::decode(const Node& node, sbx::math::color& color) -> bool {
  if (!node.IsMap() || node.size() != 4) {
    return false;
  }

  color.r() = node["r"].as<std::float_t>();
  color.g() = node["g"].as<std::float_t>();
  color.b() = node["b"].as<std::float_t>();
  color.a() = node["a"].as<std::float_t>();

  return true;
}

auto operator<<(YAML::Emitter& out, const sbx::math::color& color) -> YAML::Emitter& {
  return out << YAML::convert<sbx::math::color>::encode(color);
}

auto std::hash<sbx::math::color>::operator()(const sbx::math::color& color) const noexcept -> std::size_t {
  auto hash = std::size_t{0};

  sbx::utility::hash_combine(hash, color.r(), color.g(), color.b(), color.a());

  return hash;
}
