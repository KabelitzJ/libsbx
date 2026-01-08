#ifndef LIBSBX_SCENES_SCENE_HPP_
#define LIBSBX_SCENES_SCENE_HPP_

#include <algorithm>
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

class scene {

  // friend class node;

public:

  using node_type = node;
  using registry_type = ecs::basic_registry<node_type>;

  // template<typename... Get, typename... Exclude>
  // using query_result = ecs::basic_view

  // template<typename Type, typename... Other>
  // using const_query_result = registry_type::const_view_type<Type, Other...>;

  template<typename... Exclude>
  inline static constexpr auto query_filter = ecs::exclude<Exclude...>;

  scene(const std::filesystem::path& path);

  virtual ~scene() = default;

  auto create_child_node(const node_type parent, const std::string& tag = "Node", const scenes::transform& transform = scenes::transform{}) -> node_type;

  auto create_node(const std::string& tag = "Node", const scenes::transform& transform = scenes::transform{}) -> node_type;  

  auto destroy_node(const node_type node) -> void;

  auto camera() -> node_type {
    return _camera;
  }

  auto set_active_camera(const node_type camera) -> void {
    _camera = camera;
  }

  auto world_transform(const node_type node) -> math::matrix4x4;

  auto world_normal(const node_type node) -> math::matrix4x4;

  auto parent_transform(const node_type node) -> math::matrix4x4;

  auto world_position(const node_type node) -> math::vector3;

  auto world_rotation(const node_type node) -> math::quaternion;

  auto world_scale(const node_type node) -> math::vector3;

  auto make_child_of(const node_type node, const node_type parent) -> void;

  auto is_valid(const node_type node) const -> bool {
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
  auto has_component(const node_type node) const -> bool {
    return _registry.all_of<Component>(node);
  }

  template<typename Component, typename... Args>
  auto add_component(const node_type node, Args&&... args) -> decltype(auto) {
    return _registry.emplace<Component>(node, std::forward<Args>(args)...);
  }

  template<typename Component>
  auto get_component(const node_type node) const -> const Component& {
    return _registry.get<Component>(node);
  }

  template<typename Component>
  auto get_component(const node_type node) -> Component& {
    return _registry.get<Component>(node);
  }

  template<typename Component, typename... Args>
  auto get_or_add_component(const node_type node, Args&&... args) -> Component& {
    return _registry.get_or_emplace<Component>(node, std::forward<Args>(args)...);
  }

  auto light() -> directional_light& {
    return _light;
  }

  auto root() -> node_type {
    return _root;
  }

  auto light_space() -> math::matrix4x4 {
    const auto& camera = get_component<scenes::camera>(_camera);
    const auto& camera_transform = get_component<scenes::transform>(_camera);

    const auto camera_view = math::matrix4x4::inverted(camera_transform.local_transform());
    const auto camera_projection = camera.projection(0.1f, 100.0f);

    const auto inverse_view_projection = math::matrix4x4::inverted(camera_projection * camera_view);

    // auto frustum_corners_world = std::array<math::vector3, 8>{};

    static constexpr auto frustum_corners_clip = std::array<math::vector4, 8u>{
      math::vector4{-1, -1, -1, 1},
      math::vector4{ 1, -1, -1, 1},
      math::vector4{ 1,  1, -1, 1},
      math::vector4{-1,  1, -1, 1},
      math::vector4{-1, -1,  1, 1},
      math::vector4{ 1, -1,  1, 1},
      math::vector4{ 1,  1,  1, 1},
      math::vector4{-1,  1,  1, 1}
    };

    const auto to_world_transform = [&inverse_view_projection](const auto& corner) {
      const auto frustum_corner = inverse_view_projection * corner;

      return math::vector3{frustum_corner} / frustum_corner.w();
    };

    const auto frustum_corners_world = frustum_corners_clip | ranges::views::transform(to_world_transform) | ranges::to<std::vector>();

    const auto center = std::accumulate(frustum_corners_world.begin(), frustum_corners_world.end(), math::vector3::zero) / 8.0f;

    const auto light_position = center - _light.direction() * 20.0f; // Move back along light dir
    const auto light_view = math::matrix4x4::look_at(light_position, center, math::vector3::up);

    const auto min_max_transform = [&light_view](const auto& bounds, const auto& corner) {
      const auto corner_light_space = light_view * math::vector4(corner, 1.0f);

      return std::pair{
        math::vector3::min(bounds.first, math::vector3(corner_light_space)),
        math::vector3::max(bounds.second, math::vector3(corner_light_space))
      };
    };

    const auto [min, max] = std::accumulate(frustum_corners_world.begin(), frustum_corners_world.end(), std::pair{math::vector3{std::numeric_limits<std::float_t>::max()}, math::vector3{std::numeric_limits<std::float_t>::lowest()}}, min_max_transform);

    static constexpr float z_mult = 10.0f;

    const auto min_z = std::min(min.z() * z_mult, min.z() / z_mult);
    const auto max_z = std::max(max.z() * z_mult, max.z() / z_mult);

    auto light_projection = math::matrix4x4::orthographic(min.x() - 10.0f, max.x() + 10.0f, min.y() - 10.0f, max.y() + 10.0f, -max_z, -min_z);

    return light_projection * light_view;
  }

  auto find_node(const scenes::id& id) -> node_type {
    if (auto entry = _nodes.find(id); entry != _nodes.end()) {
      return entry->second;
    } 
      
    return node_type::null;
  }

  auto save(const std::filesystem::path& path)-> void;

  template<typename... Args>
  auto add_image(const utility::hashed_string& name, const std::filesystem::path& path, Args&&... args) -> void {
    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

    const auto id = graphics_module.add_resource<graphics::image2d>(path, std::forward<Args>(args)...);

    _image_ids.emplace(name, id);
    _image_metadata.emplace(id, assets::asset_metadata{path, name.str(), "image", "disk"});
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

    const auto& camera_transform = get_component<scenes::transform>(_camera);
    const auto& camera_global_transform = get_component<scenes::global_transform>(_camera);

    const auto view = math::matrix4x4::inverted(world_transform(_camera));

    _uniform_handler.push("view", view);

    _uniform_handler.push("viewport", graphics_module.viewport());

    // utility::logger<"scenes">::debug("viewport: {}", graphics_module.viewport());
    // utility::logger<"scenes">::debug("camera_near: {}", camera.near_plane());
    // utility::logger<"scenes">::debug("camera_far: {}", camera.far_plane());
    // utility::logger<"scenes">::debug("camera_fov_radians: {}", camera.field_of_view().to_radians().value());

    _uniform_handler.push("camera_position", world_position(_camera));
    _uniform_handler.push("camera_near", camera.near_plane());
    _uniform_handler.push("camera_far", camera.far_plane());
    _uniform_handler.push("camera_fov_radians", camera.field_of_view().to_radians().value());

    _uniform_handler.push("light_space", light_space());

    _uniform_handler.push("light_direction", sbx::math::vector3::normalized(_light.direction()));
    _uniform_handler.push("light_color", _light.color());

    _uniform_handler.push("time", core::engine::time().value());
  }

  auto screen_point_to_ray(const math::vector2& position) -> math::ray {
    const auto& camera_transform = get_component<scenes::transform>(_camera); 
    const auto& camera = get_component<scenes::camera>(_camera);

    const auto ndc = math::vector3{
      position.x() * 2.0f - 1.0f,
      1.0f - position.y() * 2.0f,
      1.0f
    };

    const auto inv_projection = math::matrix4x4::inverted(camera.projection());
    const auto view_dir4 = inv_projection * math::vector4{ndc.x(), ndc.y(), -1.0f, 1.0f};
    const auto view_dir = math::vector3::normalized(math::vector3{view_dir4.x(), view_dir4.y(), view_dir4.z()});

    const auto camera_matrix = world_transform(_camera);
    const auto ray_direction = math::vector3::normalized(camera_matrix * math::vector4{view_dir.x(), view_dir.y(), view_dir.z(), 0.0f});

    return math::ray{camera_transform.position(), ray_direction};
  }

private:

  auto _save_assets(YAML::Emitter& emitter) -> void;

  auto _save_meshes(YAML::Emitter& emitter) -> void;

  auto _save_textures(YAML::Emitter& emitter) -> void;

  auto _save_node(YAML::Emitter& emitter, const node_type node) -> void;
  
  auto _save_components(YAML::Emitter& emitter, const node_type node) -> void;

  auto _load_assets(const YAML::Node& assets) -> void;

  auto _load_nodes(const YAML::Node& nodes) -> void;

  auto _ensure_world(const node_type node) -> const scenes::global_transform&;

  std::unordered_map<math::uuid, node_type> _nodes;

  registry_type _registry;
  node_type _root;
  node_type _camera;

  std::string _name;

  containers::octree<math::uuid> _octtree;

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
