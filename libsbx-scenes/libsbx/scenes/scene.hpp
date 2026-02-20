// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_SCENE_HPP_
#define LIBSBX_SCENES_SCENE_HPP_

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>
#include <numbers>
#include <ranges>
#include <ranges>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <range/v3/all.hpp>

#include <yaml-cpp/yaml.h>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/utility/hashed_string.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/memory/tracking_allocator.hpp>

#include <libsbx/containers/octree.hpp>

#include <libsbx/ecs/registry.hpp>
#include <libsbx/ecs/entity.hpp>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/ray.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/images/cube_image.hpp>

#include <libsbx/signals/signal.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/components/directional_light.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/transform.hpp>
#include <libsbx/scenes/components/global_transform.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>
#include <libsbx/scenes/components/skinned_mesh.hpp>

namespace sbx::scenes {

struct node_destroyed_event {
  scenes::node node;
}; // struct node_destroyed_event

template<typename Component>
struct component_removed_event {
  scenes::node node;
}; // struct component_removed_event

class scene {

  // friend class node;

  static constexpr auto csm_cascade_count = std::uint32_t{4u};

  struct csm_data {
    std::array<math::matrix4x4, csm_cascade_count> light_spaces{};
    math::vector4 cascade_splits{};
  }; // struct csm_data

public:

  using registry_type = ecs::basic_registry<scenes::node>;

  // template<typename... Get, typename... Exclude>
  // using query_result = ecs::basic_view

  // template<typename Type, typename... Other>
  // using const_query_result = registry_type::const_view_type<Type, Other...>;

  template<typename... Exclude>
  inline static constexpr auto query_filter = ecs::exclude<Exclude...>;

  scene(const std::filesystem::path& path);

  virtual ~scene() = default;

  auto create_child_node(const scenes::node parent, const std::string& tag = "Node", const scenes::transform& transform = scenes::transform{}) -> scenes::node;

  auto create_node(const std::string& tag = "Node", const scenes::transform& transform = scenes::transform{}) -> scenes::node;  

  auto destroy_node(const scenes::node node) -> void;

  auto camera() -> scenes::node {
    return _camera;
  }

  auto set_active_camera(const scenes::node camera) -> void {
    _camera = camera;
  }

  auto world_transform(const scenes::node node) -> math::matrix4x4;

  auto world_normal(const scenes::node node) -> math::matrix4x4;

  auto parent_transform(const scenes::node node) -> math::matrix4x4;

  auto world_position(const scenes::node node) -> math::vector3;

  auto world_rotation(const scenes::node node) -> math::quaternion;

  auto world_scale(const scenes::node node) -> math::vector3;

  auto make_child_of(const scenes::node node, const scenes::node parent) -> void;

  auto is_valid(const scenes::node node) const -> bool {
    return _registry.is_valid(node);
  }

  template<typename Type, typename... Other, typename... Exclude>
  auto query(ecs::exclude_t<Exclude...> = ecs::exclude_t{}) -> decltype(auto) {
    return _registry.view<Type, Other...>(ecs::exclude<Exclude...>);
  }

  template<typename Type, typename... Other, typename... Exclude>
  auto query(ecs::exclude_t<Exclude...> = ecs::exclude_t{}) const -> decltype(auto) {
    return _registry.view<Type, Other...>(ecs::exclude<Exclude...>);
  }

  template<typename Type, typename Compare, typename Sort = utility::std_sort, typename... Args>
  auto sort(Compare compare, Sort sort = Sort{}, Args&&... args) -> void {
    _registry.sort<Type>(std::move(compare), std::move(sort), std::forward<Args>(args)...);
  }

  template<typename Component>
  auto has_component(const scenes::node node) const -> bool {
    return _registry.all_of<Component>(node);
  }

  template<typename Component, typename... Args>
  auto add_component(const scenes::node node, Args&&... args) -> decltype(auto) {
    return _registry.emplace<Component>(node, std::forward<Args>(args)...);
  }

  template<typename Component>
  auto get_component(const scenes::node node) const -> const Component& {
    return _registry.get<Component>(node);
  }

  template<typename Component>
  auto get_component(const scenes::node node) -> Component& {
    return _registry.get<Component>(node);
  }

  template<typename Component, typename... Args>
  auto get_or_add_component(const scenes::node node, Args&&... args) -> Component& {
    return _registry.get_or_emplace<Component>(node, std::forward<Args>(args)...);
  }

  auto light() -> directional_light& {
    return _light;
  }

  auto root() -> scenes::node {
    return _root;
  }

  auto light_space() -> math::matrix4x4 {
    const auto& camera = get_component<scenes::camera>(_camera);

    const auto camera_view = math::matrix4x4::inverted(world_transform(_camera));
    const auto camera_projection = camera.projection(0.1f, 100.0f);

    const auto inverse_view_projection = math::matrix4x4::inverted(camera_projection * camera_view);

    static constexpr auto frustum_corners_clip = std::array<math::vector4, 8u>{
      math::vector4{-1.0f, -1.0f, 0.0f, 1.0f},
      math::vector4{ 1.0f, -1.0f, 0.0f, 1.0f},
      math::vector4{ 1.0f,  1.0f, 0.0f, 1.0f},
      math::vector4{-1.0f,  1.0f, 0.0f, 1.0f},
      math::vector4{-1.0f, -1.0f, 1.0f, 1.0f},
      math::vector4{ 1.0f, -1.0f, 1.0f, 1.0f},
      math::vector4{ 1.0f,  1.0f, 1.0f, 1.0f},
      math::vector4{-1.0f,  1.0f, 1.0f, 1.0f}
    };

    auto frustum_corners_world = std::array<math::vector3, 8u>{};

    for (auto i = 0u; i < 8u; i++) {
      auto corner_world = inverse_view_projection * frustum_corners_clip[i];

      corner_world /= corner_world.w();

      frustum_corners_world[i] = math::vector3{corner_world};
    }

    auto center = math::vector3::zero;

    for (const auto& corner : frustum_corners_world) {
      center += corner;
    }

    center /= 8.0f;

    const auto light_position = center - _light.direction() * 10.0f;
    const auto light_view = math::matrix4x4::look_at(light_position, center, math::vector3::up);

    auto min_bounds = math::vector3{std::numeric_limits<std::float_t>::max()};
    auto max_bounds = math::vector3{std::numeric_limits<std::float_t>::lowest()};

    for (const auto& corner : frustum_corners_world) {
      const auto corner_light_space = light_view * math::vector4{corner, 1.0f};
      const auto p = math::vector3{corner_light_space};

      min_bounds = math::vector3::min(min_bounds, p);
      max_bounds = math::vector3::max(max_bounds, p);
    }

    static constexpr auto z_mult = 10.0f;

    const auto min_z = std::min(min_bounds.z() * z_mult, min_bounds.z() / z_mult);
    const auto max_z = std::max(max_bounds.z() * z_mult, max_bounds.z() / z_mult);

    auto near_plane = -max_z;
    auto far_plane = -min_z;

    if (near_plane > far_plane) {
      std::swap(near_plane, far_plane);
    }

    const auto light_projection = math::matrix4x4::orthographic(min_bounds.x() - 10.0f, max_bounds.x() + 10.0f, min_bounds.y() - 10.0f, max_bounds.y() + 10.0f, near_plane, far_plane);

    return light_projection * light_view;
  }


  auto find_node(const scenes::id& id) -> scenes::node {
    if (auto entry = _nodes.find(id); entry != _nodes.end()) {
      return entry->second;
    } 
      
    return scenes::node::null;
  }

  auto save(const std::filesystem::path& path)-> void;

  template<typename... Args>
  auto add_image(const utility::hashed_string& name, const std::filesystem::path& path, Args&&... args) -> void {
    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

    const auto id = graphics_module.add_resource<graphics::image2d>(path, std::forward<Args>(args)...);

    _image_ids.emplace(name, id);
    _image_metadata.emplace(id, assets::asset_metadata{path, name.str(), "image", "disk"});
  }

  auto has_image(const utility::hashed_string& name) const -> bool {
    return _image_ids.find(name) != _image_ids.end();
  }

  auto get_image(const utility::hashed_string& name) -> graphics::image2d_handle {
    if (auto entry = _image_ids.find(name); entry != _image_ids.end()) {
      return entry->second;
    }

    throw utility::runtime_error{"Could not find image '{}", name.str()};
  }

  auto image_metadata(const graphics::image2d_handle& handle) const -> const assets::asset_metadata& {
    return _image_metadata.at(handle);
  }

  template<typename... Args>
  auto add_cube_image(const utility::hashed_string& name, const std::filesystem::path& path, Args&&... args) -> void {
    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

    const auto id = graphics_module.add_resource<graphics::cube_image>(path, std::forward<Args>(args)...);

    _cube_image_ids.emplace(name, id);
    _cube_image_metadata.emplace(id, assets::asset_metadata{path, name.str(), "cube_image", "disk"});
  }

  auto get_cube_image(const utility::hashed_string& name) -> graphics::cube_image2d_handle {
    return _cube_image_ids.at(name);
  }

  auto cube_image_metadata(const graphics::cube_image2d_handle& handle) const -> const assets::asset_metadata& {
    return _cube_image_metadata.at(handle);
  }

  template<typename Mesh, typename... Args>
  auto add_mesh(const utility::hashed_string& name, Args&&... args) -> void {
    auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

    const auto id = assets_module.add_asset<Mesh>(std::forward<Args>(args)...);

    _mesh_ids.emplace(name, id);
    _mesh_metadata.emplace(id, assets::asset_metadata{"", name.str(), "mesh", "generated"});
  }

  template<typename Mesh, typename Path, typename... Args>
  requires (std::is_constructible_v<std::filesystem::path, Path>)
  auto add_mesh(const utility::hashed_string& name, const Path& path, Args&&... args) -> void {
    auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();


    const auto id = assets_module.add_asset<Mesh>(path, std::forward<Args>(args)...);

    _mesh_ids.emplace(name, id);
    _mesh_metadata.emplace(id, assets::asset_metadata{path, name.str(), "mesh", "disk"});
  }

  auto get_mesh(const utility::hashed_string& name) -> math::uuid {
    return _mesh_ids.at(name);
  }

  auto mesh_metadata(const math::uuid& handle) const -> const assets::asset_metadata& {
    return _mesh_metadata.at(handle);
  }

  template<typename Mesh, typename... Args>
  auto add_animation(const utility::hashed_string& name, Args&&... args) -> void {
    auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

    _animation_ids.emplace(name, assets_module.add_asset<Mesh>(std::forward<Args>(args)...));
  }

  auto get_animation(const utility::hashed_string& name) -> math::uuid {
    return _animation_ids.at(name);
  }

  template<typename Material, typename... Args>
  auto add_material(const utility::hashed_string& name, Args&&... args) -> Material& {
    auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

    const auto id = assets_module.add_asset<Material>(std::forward<Args>(args)...);

    _materials_ids.emplace(name, id);
    _material_metadata.emplace(id, assets::asset_metadata{"", name.str(), "material", "dynamic"});

    return assets_module.get_asset<Material>(id);
  }
  
  auto has_material(const utility::hashed_string& name) const -> bool {
    return _materials_ids.find(name) != _materials_ids.end(); 
  }

  auto get_material(const utility::hashed_string& name) -> math::uuid {
    return _materials_ids.at(name);
  }

  auto material_metadata(const math::uuid& handle) const -> const assets::asset_metadata& {
    return _material_metadata.at(handle);
  }

  auto uniform_handler() -> graphics::uniform_handler& {
    return _uniform_handler;
  }

  auto update_uniform_handler() -> void {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    auto& camera = get_component<scenes::camera>(_camera);

    const auto& projection = camera.projection();

    _uniform_handler.push("projection", projection);
    _uniform_handler.push("inverse_projection", math::matrix4x4::inverted(projection));

    auto inverse_view = world_transform(_camera);

    const auto view = math::matrix4x4::inverted(inverse_view);

    inverse_view[3] = math::vector4{0.0f, 0.0f, 0.0f, 1.0f};

    _uniform_handler.push("view", view);
    _uniform_handler.push("inverse_view", inverse_view);

    _uniform_handler.push("viewport", graphics_module.viewport());

    _uniform_handler.push("camera_position", world_position(_camera));
    _uniform_handler.push("camera_near", camera.near_plane());
    _uniform_handler.push("camera_far", camera.far_plane());
    _uniform_handler.push("camera_fov_radians", camera.field_of_view().to_radians().value());

    const auto& camera_transform = get_component<scenes::transform>(_camera);

    const auto camera_rotation = math::matrix_cast<math::matrix4x4>(world_rotation(_camera));
    const auto camera_right = math::vector3{camera_rotation[0]};
    const auto camera_up = math::vector3{camera_rotation[1]};

    _uniform_handler.push("camera_right", camera_right);
    _uniform_handler.push("camera_up", camera_up);

    const auto csm = _build_csm();

    _uniform_handler.push("light_spaces", csm.light_spaces);
    _uniform_handler.push("cascade_splits", csm.cascade_splits);

    _uniform_handler.push("light_direction", sbx::math::vector3::normalized(_light.direction()));
    _uniform_handler.push("light_color", _light.color());

    _uniform_handler.push("time", core::engine::time().value());
  }

  auto screen_point_to_ray(const math::vector2& position) -> math::ray {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    const auto& viewport = graphics_module.viewport();

    const auto& camera = get_component<scenes::camera>(_camera);

    const auto world = world_transform(_camera);
    const auto view = math::matrix4x4::inverted(world);
    const auto projection = camera.projection();

    const auto inverse_view_projection = math::matrix4x4::inverted(projection * view);

    const auto px = position.x() + 0.5f;
    const auto py = position.y() + 0.5f;

    const auto x = (2.0f * px) / viewport.x() - 1.0f;
    const auto y = (2.0f * py) / viewport.y() - 1.0f;

    const auto near_clip = math::vector4{x, y, 0.0f, 1.0f};
    const auto far_clip  = math::vector4{x, y, 1.0f, 1.0f};

    auto near_world = inverse_view_projection * near_clip;
    near_world /= near_world.w();

    auto far_world = inverse_view_projection * far_clip;
    far_world /= far_world.w();

    const auto origin = math::vector3{near_world.x(), near_world.y(), near_world.z()};
    const auto direction = math::vector3::normalized(math::vector3{far_world.x(), far_world.y(), far_world.z()} - origin);

    return math::ray{origin, direction};
  }

  auto node_count() const -> std::size_t {
    return _nodes.size();
  }

private:

  static auto _lerp_float(const std::float_t a, const std::float_t b, const std::float_t t) -> std::float_t {
    return a + (b - a) * t;
  }

  static auto _compute_csm_splits(const std::float_t near_plane, const std::float_t far_plane, const std::float_t lambda) -> std::array<std::float_t, csm_cascade_count> {
    auto splits = std::array<std::float_t, csm_cascade_count>{};

    for (auto i = 0u; i < csm_cascade_count; ++i) {
      const auto p = static_cast<std::float_t>(i + 1u) / static_cast<std::float_t>(csm_cascade_count);

      const auto log_split = near_plane * std::pow(far_plane / near_plane, p);
      const auto uni_split = near_plane + (far_plane - near_plane) * p;

      splits[i] = _lerp_float(uni_split, log_split, lambda);
    }

    return splits;
  }

  static auto _build_light_space_for_slice(const scenes::camera& camera, const math::matrix4x4& camera_world, const math::vector3& light_direction, const std::float_t slice_near, const std::float_t slice_far, const std::uint32_t shadow_resolution) -> math::matrix4x4 {
    const auto camera_view = math::matrix4x4::inverted(camera_world);
    const auto camera_projection = camera.projection(slice_near, slice_far);

    const auto inv_view_projection = math::matrix4x4::inverted(camera_projection * camera_view);

    static constexpr auto frustum_corners_clip = std::array<math::vector4, 8u>{
      math::vector4{-1.0f, -1.0f, 0.0f, 1.0f},
      math::vector4{ 1.0f, -1.0f, 0.0f, 1.0f},
      math::vector4{ 1.0f,  1.0f, 0.0f, 1.0f},
      math::vector4{-1.0f,  1.0f, 0.0f, 1.0f},
      math::vector4{-1.0f, -1.0f, 1.0f, 1.0f},
      math::vector4{ 1.0f, -1.0f, 1.0f, 1.0f},
      math::vector4{ 1.0f,  1.0f, 1.0f, 1.0f},
      math::vector4{-1.0f,  1.0f, 1.0f, 1.0f}
    };

    auto corners_world = std::array<math::vector3, 8u>{};
    auto center_world = math::vector3::zero;

    for (auto i = 0u; i < 8u; ++i) {
      auto corner_world = inv_view_projection * frustum_corners_clip[i];
      corner_world /= corner_world.w();

      corners_world[i] = math::vector3{corner_world};
      center_world += corners_world[i];
    }

    center_world /= 8.0f;

    const auto light_dir = math::vector3::normalized(light_direction);

    const auto light_position = center_world - light_dir * 50.0f;

    auto up = math::vector3::up;

    if (std::abs(math::vector3::dot(light_dir, up)) > 0.99f) {
      up = math::vector3{1.0f, 0.0f, 0.0f};
    }

    const auto light_view = math::matrix4x4::look_at(light_position, center_world, up);

    auto min_bounds = math::vector3{std::numeric_limits<std::float_t>::max()};
    auto max_bounds = math::vector3{std::numeric_limits<std::float_t>::lowest()};

    for (const auto& corner : corners_world) {
      const auto p4 = light_view * math::vector4{corner, 1.0f};
      const auto p = math::vector3{p4};

      min_bounds = math::vector3::min(min_bounds, p);
      max_bounds = math::vector3::max(max_bounds, p);
    }

    const auto extent_x = max_bounds.x() - min_bounds.x();
    const auto extent_y = max_bounds.y() - min_bounds.y();

    static constexpr auto xy_padding = 10.0f;
    const auto extent = std::max(extent_x, extent_y) + 2.0f * xy_padding;

    const auto texel_size = extent / static_cast<std::float_t>(shadow_resolution);

    const auto center_ls = (min_bounds + max_bounds) * 0.5f;

    auto min_x = center_ls.x() - extent * 0.5f;
    auto min_y = center_ls.y() - extent * 0.5f;

    min_x = std::floor(min_x / texel_size) * texel_size;
    min_y = std::floor(min_y / texel_size) * texel_size;

    const auto max_x = min_x + texel_size * static_cast<std::float_t>(shadow_resolution);
    const auto max_y = min_y + texel_size * static_cast<std::float_t>(shadow_resolution);

    min_bounds = math::vector3{min_x, min_y, min_bounds.z()};
    max_bounds = math::vector3{max_x, max_y, max_bounds.z()};

    static constexpr auto z_mult = 10.0f;

    const auto min_z = std::min(min_bounds.z() * z_mult, min_bounds.z() / z_mult);
    const auto max_z = std::max(max_bounds.z() * z_mult, max_bounds.z() / z_mult);

    auto near_plane = -max_z;
    auto far_plane = -min_z;

    if (near_plane > far_plane) {
      std::swap(near_plane, far_plane);
    }

    const auto light_projection = math::matrix4x4::orthographic(min_bounds.x(), max_bounds.x(), min_bounds.y(), max_bounds.y(), near_plane, far_plane);

    return light_projection * light_view;
  }

  auto _build_csm() -> csm_data {
    const auto& camera = get_component<scenes::camera>(_camera);
    const auto camera_world = world_transform(_camera);

    const auto camera_near = camera.near_plane();
    const auto camera_far = camera.far_plane();

    static constexpr auto lambda = 0.65f;
    static constexpr auto shadow_resolution = std::uint32_t{2048u};

    const auto splits = _compute_csm_splits(camera_near, camera_far, lambda);

    auto result = csm_data{};

    auto slice_near = camera_near;

    for (auto i = 0u; i < csm_cascade_count; ++i) {
      const auto slice_far = splits[i];

      result.light_spaces[i] = _build_light_space_for_slice(camera, camera_world, _light.direction(), slice_near, slice_far, shadow_resolution);

      slice_near = slice_far;
    }

    result.cascade_splits = math::vector4{splits[0], splits[1], splits[2], camera_far};

    return result;
  }

  auto _save_assets(YAML::Emitter& emitter) -> void;

  auto _save_meshes(YAML::Emitter& emitter) -> void;

  auto _save_textures(YAML::Emitter& emitter) -> void;

  auto _save_node(YAML::Emitter& emitter, const scenes::node node) -> void;
  
  auto _save_components(YAML::Emitter& emitter, const scenes::node node) -> void;

  auto _load_assets(const YAML::Node& assets) -> void;

  auto _load_nodes(const YAML::Node& nodes) -> void;

  auto _ensure_world(const scenes::node node) -> const scenes::global_transform&;

  std::unordered_map<math::uuid, scenes::node> _nodes;

  registry_type _registry;
  scenes::node _root;
  scenes::node _camera;

  std::string _name;

  directional_light _light;

  graphics::uniform_handler _uniform_handler;



  std::unordered_map<utility::hashed_string, graphics::image2d_handle> _image_ids;
  std::unordered_map<utility::hashed_string, graphics::cube_image2d_handle> _cube_image_ids;
  std::unordered_map<utility::hashed_string, math::uuid> _mesh_ids;
  std::unordered_map<utility::hashed_string, math::uuid> _animation_ids;
  std::unordered_map<utility::hashed_string, math::uuid> _materials_ids;

  std::unordered_map<graphics::image2d_handle, assets::asset_metadata> _image_metadata;
  std::unordered_map<graphics::cube_image2d_handle, assets::asset_metadata> _cube_image_metadata;
  std::unordered_map<math::uuid, assets::asset_metadata> _mesh_metadata;
  std::unordered_map<math::uuid, assets::asset_metadata> _material_metadata;

}; // class scene

struct component_io {
  std::string name;
  std::function<void(YAML::Emitter&, scene& scene, const node)> save; 
  std::function<void(const YAML::Node&, scene& scene, const node)> load; 
}; // struct component_io

class component_io_registry {

public:

  template<typename Type, std::invocable<YAML::Emitter&, scene&, const Type&> Save, std::invocable<YAML::Node&> Load>
  auto register_component(const std::string& name, Save&& save, Load&& load) -> void {
    const auto id = ecs::type_id<Type>::value();

    _by_name[name] = id;

    _by_id[id] = component_io{
      .name = name,
      .save = [name, s = std::forward<Save>(save)](YAML::Emitter& yaml, scene& scene, const node node) -> void {
        const auto& component = scene.get_component<Type>(node);

        std::invoke(s, yaml, scene, component);
      },
      .load = [name, l = std::forward<Load>(load)](const YAML::Node& yaml, scene& scene, const node node) -> void {
        scene.add_component<Type>(node, std::invoke(l, yaml));
      }
    };
  }

  auto get(const std::uint32_t id) -> component_io& {
    return _by_id.at(id);
  }

  auto has(const std::uint32_t id) -> bool {
    return _by_id.contains(id);
  }

  auto get(const std::string& name) -> component_io& {
    return _by_id.at(_by_name.at(name));
  }

private:

  std::unordered_map<std::uint32_t, component_io> _by_id;
  std::unordered_map<std::string, std::uint32_t> _by_name;

}; // class component_io_registry

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_HPP_
