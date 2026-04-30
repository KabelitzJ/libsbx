// SPDX-License-Identifier: MIT
#include <libsbx/animations/skeleton.hpp>

#include <libsbx/utility/profiler.hpp>

namespace sbx::animations {

auto skeleton::reserve(const std::size_t size) -> void {
  _bones.reserve(size);
  _bone_names_by_id.reserve(size);
  _bone_id_by_name.reserve(size);
}

auto skeleton::shrink_to_fit() -> void {
  _bones.shrink_to_fit();
  _bone_names_by_id.shrink_to_fit();
}

auto skeleton::add_bone(const std::string& name, const bone& bone) -> void {
  const auto id = _bone_names_by_id.size();

  _bone_names_by_id.push_back(name);

  _bone_id_by_name.emplace(name, id);

  _bones.push_back(bone);
}

auto skeleton::inverse_root_transform() const -> const math::matrix4x4& {
  return _inverse_root_transform;
}

auto skeleton::root_transform() const -> const math::matrix4x4& {
  return _root_transform;
}

auto skeleton::set_inverse_root_transform(const math::matrix4x4& inverse_root_transform) -> void {
  _inverse_root_transform = inverse_root_transform;
  _root_transform = math::matrix4x4::inverted(_inverse_root_transform);
}

auto skeleton::bones() const -> const std::vector<bone>& {
  return _bones;
}

auto skeleton::evaluate_pose(const animation& animation, std::float_t time) const -> std::vector<math::matrix4x4> {
  SBX_PROFILE_SCOPE("skeleton::evaluate_pose");

  auto final_bones = std::vector<math::matrix4x4>{};
  final_bones.resize(_bones.size(), math::matrix4x4::identity);

  auto global_transforms = std::vector<math::matrix4x4>{};
  global_transforms.resize(_bones.size(), math::matrix4x4::identity);

  for (std::uint32_t bone_id = 0; bone_id < _bones.size(); ++bone_id) {
    const auto& bone = _bones[bone_id];
    const auto& bone_name = _bone_names_by_id[bone_id];

    auto local_transform = bone.local_bind_matrix;

    const auto& track_map = animation.track_map();

    SBX_PROFILE_SCOPE_START(s0, "skeleton::find_track");

    auto it = track_map.find(bone_name);

    SBX_PROFILE_SCOPE_END(s0);

    // Sample animation track if present
    if (it != track_map.cend()) {
      SBX_PROFILE_SCOPE("skeleton::sample_track");
      const auto& track = it->second;

      SBX_PROFILE_SCOPE_START(s1, "skeleton::sample_position_rotation_scale");

      const auto position = track.position_spline.sample(time);
      const auto rotation = math::quaternion::normalized(track.rotation_spline.sample(time));
      const auto scale = track.scale_spline.sample(time);

      SBX_PROFILE_SCOPE_END(s1);

      SBX_PROFILE_SCOPE_START(s2, "skeleton::calculate_local_transform");

      const auto translation_matrix = math::matrix4x4::translated(math::matrix4x4::identity, position);
      const auto rotation_matrix = math::matrix_cast<math::matrix4x4>(rotation);
      const auto scale_matrix = math::matrix4x4::scaled(math::matrix4x4::identity, scale);

      SBX_PROFILE_SCOPE_END(s2);

      local_transform = translation_matrix * rotation_matrix * scale_matrix;
    }

    SBX_PROFILE_SCOPE("skeleton::local_transform");

    const auto global_transform = (bone.parent_id != skeleton::bone::null) ? global_transforms[bone.parent_id] * local_transform : local_transform;

    final_bones[bone_id] = _inverse_root_transform * global_transform * bone.inverse_bind_matrix;

    global_transforms[bone_id] = std::move(global_transform);
  }

  return final_bones;
}

auto skeleton::bone_count() const -> std::uint32_t {
  return _bones.size();
}

auto skeleton::name_for_bone(const std::size_t index) const -> const utility::hashed_string& {
  return _bone_names_by_id[index];
}

auto skeleton::bone_index(const utility::hashed_string& name) const -> std::optional<std::uint32_t> {
  if (auto entry = _bone_id_by_name.find(name); entry != _bone_id_by_name.cend()) {
    return entry->second;
  }

  return std::nullopt;
}

} // namespace sbx::animations
