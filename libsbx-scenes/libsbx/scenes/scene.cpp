#include <libsbx/scenes/scene.hpp>

#include <unordered_map>

// #include <portable-file-dialogs.h>

#include <yaml-cpp/yaml.h>

#include <easy/profiler.h>

#include <libsbx/units/time.hpp>

#include <libsbx/utility/timer.hpp>
#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/target.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/devices/devices_module.hpp>
#include <libsbx/devices/window.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/relationship.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>
#include <libsbx/scenes/components/point_light.hpp>
#include <libsbx/scenes/components/global_transform.hpp>

namespace sbx::scenes {

scene::scene(const std::filesystem::path& path)
: _registry{}, 
  _root{_registry.create()},
  _camera{_registry.create()},
  _light{math::vector3{-1.0, -1.0, -1.0}, math::color{1.0f, 1.0f, 1.0f, 1.0f}},
  _octtree{math::volume{math::vector3{-1000.0f, -1000.0f, -1000.0f}, math::vector3{1000.0f, 1000.0f, 1000.0f}}} {
  // [NOTE] KAJ 2023-10-17 : Initialize root node
  const auto& root_id = add_component<scenes::id>(_root);
  _nodes.insert({root_id, _root});

  add_component<scenes::relationship>(_root, node_type::null);
  add_component<scenes::transform>(_root);
  add_component<scenes::tag>(_root, "ROOT");
  add_component<scenes::global_transform>(_root);

  // [NOTE] KAJ 2023-10-17 : Initialize camera node
  const auto& camera_id = add_component<scenes::id>(_camera);

  _nodes.insert({camera_id, _camera});

  add_component<scenes::relationship>(_camera, _root);
  get_component<scenes::relationship>(_root).add_child(_camera);

  add_component<scenes::global_transform>(_camera);

  add_component<scenes::transform>(_camera);
  add_component<scenes::tag>(_camera, "CAMERA");

  auto& devices_module = core::engine::get_module<devices::devices_module>();
  auto& window = devices_module.window();

  add_component<scenes::camera>(_camera, math::angle{math::degree{60.0f}}, window.aspect_ratio(), 0.1f, 1000.0f);

  // window.on_framebuffer_resized() += [this](const devices::framebuffer_resized_event& event) {
  //   auto& camera = get_component<scenes::camera>(_camera);
  //   camera.set_aspect_ratio(static_cast<std::float_t>(event.width) / static_cast<std::float_t>(event.height));
  // };

  const auto scene = YAML::LoadFile(path.string());

  if (!scene || scene.IsNull() || scene.size() == 0) {
    utility::logger<"scenes">::warn("Scene '{}' is empty", path.string());
    return;
  }

  const auto& name = scene["name"].as<std::string>();

  const auto& metadata = scene["metadata"];

  _load_assets(scene["assets"]);
  _load_nodes(scene["nodes"]);
}

auto scene::create_child_node(const node_type parent, const std::string& tag, const scenes::transform& transform, const selection_tag& selection_tag) -> node_type {
  auto node = _registry.create();

  const auto& id = add_component<scenes::id>(node);

  _nodes.insert({id, node});

  add_component<scenes::relationship>(node, parent);
  get_component<scenes::relationship>(parent).add_child(node);

  add_component<scenes::global_transform>(node);

  add_component<scenes::transform>(node, transform);

  add_component<scenes::tag>(node, !tag.empty() ? tag : scenes::tag{"Node"});

  add_component<scenes::selection_tag>(node, selection_tag);

  return node;
}

auto scene::create_node(const std::string& tag, const scenes::transform& transform, const selection_tag& selection_tag) -> node_type {
  return create_child_node(_root, tag, transform, selection_tag);
}

auto scene::destroy_node(const node_type node) -> void {
  // [TODO] KAJ 2025-05-10 : Fix this using heirarchy component and a stack
  const auto& id = get_component<scenes::id>(node);
  const auto& relationship = get_component<scenes::relationship>(node);

  for (auto& child : relationship.children()) {
    if (child != node::null) {
      destroy_node(child);
    }
  }

  _nodes.erase(id);

  _registry.destroy(node);
}

auto scene::_ensure_world(const node_type node) -> const scenes::global_transform& {
  EASY_FUNCTION();

  auto chain = utility::make_array<node_type, 32u>(node_type::null);
  auto chain_size = std::uint32_t{0};

  for (auto current = node; current != node_type::null;) {
    chain[chain_size++] = current;

    const auto& relationship = get_component<scenes::relationship>(current);

    current = relationship.parent();
  }

  auto parent_world = math::matrix4x4::identity;
  auto parent_world_version = std::uint64_t{0};

  for (auto i = chain_size - 1u; (i >= 0u && i < chain_size); --i) {
    const auto current = chain[i];

    auto& local = get_component<scenes::transform>(current);
    auto& world = get_component<scenes::global_transform>(current);

    if (world.local_seen != local.version() || world.parent_seen != parent_world_version) {
      EASY_BLOCK("Recalculate world space")

      world.model = parent_world * local.local_transform();
      world.normal = math::matrix4x4::transposed(math::matrix4x4::inverted(world.model));
      world.local_seen = local.version();
      world.parent_seen = parent_world_version;

      ++world.version;

      EASY_END_BLOCK;
    }

    parent_world = world.model;
    parent_world_version  = world.version;
  }

  return get_component<scenes::global_transform>(node);
}

auto scene::world_transform(const node_type node) -> math::matrix4x4 {
  EASY_FUNCTION();

  return _ensure_world(node).model;
}

auto scene::world_normal(const node_type node) -> math::matrix4x4 {
  EASY_FUNCTION();

  return _ensure_world(node).normal;
}

auto scene::parent_transform(const node_type node) -> math::matrix4x4 {
  EASY_FUNCTION();

  const auto& relationship = get_component<scenes::relationship>(node);

  return (relationship.parent() != node_type::null) ? world_transform(relationship.parent()) : math::matrix4x4::identity;
}

auto scene::world_position(const node_type node) -> math::vector3 {
  return math::vector3{world_transform(node)[3]};
}

auto scene::world_rotation(const node_type node) -> math::quaternion {
  const auto world = world_transform(node);

  auto x = math::vector3{world[0]};
  auto y = math::vector3{world[1]};
  auto z = math::vector3{world[2]};

  const auto x_length = x.length();
  const auto y_length = y.length();
  const auto z_length = z.length();

  if (x_length < math::epsilonf || y_length < math::epsilonf || z_length < math::epsilonf) {
    return math::quaternion::identity;
  }

  x /= x_length;
  y /= y_length;
  z /= z_length;

  if (math::vector3::dot(math::vector3::cross(x, y), z) < 0.0f) {
    if (x_length >= y_length && x_length >= z_length) {
      x = -x;
    } else if (y_length >= z_length) {
      y = -y;
    } else {
      z = -z;
    }
  }

  auto matrix = math::matrix3x3{x, y, z};

  return math::quaternion{matrix};
}

auto scene::world_scale(const node_type node) -> math::vector3 {

}


auto scene::save(const std::filesystem::path& path)-> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto resolved_path = assets_module.resolve_path(path);

  // _registry.invoke("save", [this](const auto node) {
  //   return (node != _root);
  // });

  auto emitter = YAML::Emitter{};

  utility::logger<"scenes">::debug("Serializing scene '{}' to {}", _name, resolved_path.string());

  emitter << YAML::BeginMap;

  emitter << YAML::Key << "name";
  emitter << YAML::Value << (!_name.empty() ? _name : "Scene");

  emitter << YAML::Key << "assets";
  emitter << YAML::Value << YAML::BeginMap;

  _save_assets(emitter);

  emitter << YAML::EndMap;

  emitter << YAML::Key << "nodes";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto node : _registry.view<const node_type>()) {
    _save_node(emitter, node);
  }

  emitter << YAML::EndSeq;

  emitter << YAML::EndMap;

  auto stream = std::ofstream{resolved_path};

  stream << emitter.c_str();

  stream.flush();
  stream.close();
}

auto scene::_load_assets(const YAML::Node& assets) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  // for (const auto& mesh : assets["meshes"]) {
  //   const auto& name = mesh["name"].as<std::string>();
  //   const auto& path = mesh["path"].as<std::string>();
  //   const auto& id = mesh["id"].as<std::string>();
  // }

  // for (const auto& mesh : assets["materials"]) {
  //   const auto& name = mesh["name"].as<std::string>();
  //   const auto& path = mesh["path"].as<std::string>();
  //   const auto& id = mesh["id"].as<std::string>();
  // }
}

auto scene::_load_nodes(const YAML::Node& nodes) -> void {

}

auto scene::_save_assets(YAML::Emitter& emitter) -> void {
  emitter << YAML::Key << "images";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto& [id, metadata] : _image_metadata) {
    emitter << YAML::Anchor(metadata.name);

    emitter << YAML::BeginMap;

    emitter << YAML::Key << "name" << YAML::Value << metadata.name;
    emitter << YAML::Key << "path" << YAML::Value << metadata.path.string();

    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;

  emitter << YAML::Key << "cube_images";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto& [id, metadata] : _cube_image_metadata) {
    emitter << YAML::Anchor(metadata.name);

    emitter << YAML::BeginMap;

    emitter << YAML::Key << "name" << YAML::Value << metadata.name;
    emitter << YAML::Key << "path" << YAML::Value << metadata.path.string();

    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;

  emitter << YAML::Key << "static_meshes";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto& [id, metadata] : _mesh_metadata) {
    emitter << YAML::Anchor(metadata.name);

    emitter << YAML::BeginMap;

    emitter << YAML::Key << "name" << YAML::Value << metadata.name;
    emitter << YAML::Key << "path" << YAML::Value << metadata.path.string();
    emitter << YAML::Key << "source" << YAML::Value << metadata.source;

    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;

  emitter << YAML::Key << "materials";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto& [id, metadata] : _material_metadata) {
    emitter << YAML::Anchor(metadata.name);

    emitter << YAML::BeginMap;

    emitter << YAML::Key << "name" << YAML::Value << metadata.name;
    emitter << YAML::Key << "path" << YAML::Value << metadata.path.string();

    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;
}

auto scene::_save_meshes(YAML::Emitter& emitter) -> void {
  emitter << YAML::Anchor("bmp");
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "name";
  emitter << YAML::Value << "bmp";
  emitter << YAML::Key << "path";
  emitter << YAML::Value << "demo/assets/meshes/tank/bmp.gltf";
  emitter << YAML::EndMap;
}

auto scene::scene::_save_textures(YAML::Emitter& emitter) -> void {
  emitter << YAML::Anchor("bmp_body1_albedo");
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "name";
  emitter << YAML::Value << "bmp_body1_albedo";
  emitter << YAML::Key << "path";
  emitter << YAML::Value << "demo/assets/textures/bmp/body1_albedo.png";
  emitter << YAML::EndMap;

  emitter << YAML::Anchor("bmp_body1_normal");
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "name";
  emitter << YAML::Value << "bmp_body1_normal";
  emitter << YAML::Key << "path";
  emitter << YAML::Value << "demo/assets/textures/bmp/body1_normal.png";
  emitter << YAML::EndMap;
}

auto scene::_save_node(YAML::Emitter& emitter, const node_type node) -> void {
  if (node == _root) {
    return;
  }

  emitter << YAML::BeginMap;

  const auto& tag = get_component<scenes::tag>(node);

  emitter << YAML::Key << "tag";
  emitter << YAML::Value << tag.str();

  const auto& id = get_component<scenes::id>(node);

  emitter << YAML::Key << "id";
  emitter << YAML::Value << id.value();

  const auto& relationship = get_component<scenes::relationship>(node);

  if (relationship.parent() != _root) {
    emitter << YAML::Key << "parent";
    emitter << YAML::Value << get_component<scenes::id>(relationship.parent()).value();
  }

  emitter << YAML::Key << "components";
  emitter << YAML::Value << YAML::BeginSeq;

  _save_components(emitter, node);

  emitter << YAML::EndSeq;

  emitter << YAML::EndMap;
}

auto scene::_save_components(YAML::Emitter& emitter, const node_type node) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  
  for (auto&& [type, container] : _registry.storage()) {
    if (!container.contains(node) || !scenes_module.has_component_io(type)) {
      continue;
    }
    
    auto& component_io = scenes_module.component_io(type);
    
    // auto yaml = YAML::Node{};

    emitter << YAML::BeginMap;

    // yaml["type"] = component_io.name;

    emitter << YAML::Key << "type";
    emitter << YAML::Value << component_io.name;

    component_io.save(emitter, *this, node);

    emitter << YAML::EndMap;
  }

  // // Trasform
  // const auto& transform = get_component<scenes::transform>(node);
  
  // emitter << YAML::BeginMap;

  // emitter << YAML::Key << "type" << YAML::Value << "transform";
  // emitter << YAML::Key << "position" << YAML::Value << transform.position();
  // emitter << YAML::Key << "rotation" << YAML::Value << transform.rotation();
  // emitter << YAML::Key << "scale" << YAML::Value << transform.scale();

  // emitter << YAML::EndMap;

  // // Hierarchy
  // const auto& relationship = get_component<scenes::relationship>(node);

  // emitter << YAML::BeginMap;

  // // We maybe dont need to store children as we can resolve dependencies by the parent alone
  // emitter << YAML::Key << "type" << YAML::Value << "hierarchy";
  // emitter << YAML::Key << "parent" << YAML::Value << ((node != _root && relationship.parent() != node_type::null) ? get_component<scenes::id>(relationship.parent()).value() : math::uuid::null().value());

  // emitter << YAML::EndMap;

  // Other
}

} // namespace sbx::scenes
