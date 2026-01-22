// SPDX-License-Identifier: MIT
#include <libsbx/animations/animation.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/timer.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace sbx::animations {

static constexpr auto import_flags =
  aiProcess_CalcTangentSpace |        // Create binormals/tangents just in case
  aiProcess_Triangulate |             // Make sure we're triangles
  aiProcess_SortByPType |             // Split meshes by primitive type
  aiProcess_GenNormals |              // Make sure we have legit normals
  aiProcess_GenUVCoords |             // Convert UVs if required
  aiProcess_OptimizeMeshes |          // Batch draws where possible
  aiProcess_JoinIdenticalVertices |
  aiProcess_LimitBoneWeights |        // If more than N (=4) bone weights, discard least influencing bones and renormalise sum to 1
  aiProcess_GlobalScale |             // e.g. convert cm to m for fbx import (and other formats where cm is native)
  aiProcess_ValidateDataStructure;    // Validation 

static auto _convert_vec2(const aiVector2D& vector) -> math::vector2 {
  return math::vector2{vector.x, vector.y};
}

static auto _convert_vec3(const aiVector3D& vector) -> math::vector3 {
  return math::vector3{vector.x, vector.y, vector.z};
}

static auto _convert_quat(const aiQuaternion& quaternion) -> math::quaternion {
  return math::quaternion{quaternion.x, quaternion.y, quaternion.z, quaternion.w};
}

static auto _convert_mat4(const aiMatrix4x4& matrix) -> math::matrix4x4 {
  auto result = math::matrix4x4{};

  //the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
  result[0][0] = matrix.a1;
  result[0][1] = matrix.b1;
  result[0][2] = matrix.c1;
  result[0][3] = matrix.d1;

  result[1][0] = matrix.a2;
  result[1][1] = matrix.b2;
  result[1][2] = matrix.c2;
  result[1][3] = matrix.d2;

  result[2][0] = matrix.a3;
  result[2][1] = matrix.b3;
  result[2][2] = matrix.c3;
  result[2][3] = matrix.d3;

  result[3][0] = matrix.a4;
  result[3][1] = matrix.b4;
  result[3][2] = matrix.c4;
  result[3][3] = matrix.d4;

  return result;
}

animation::animation(const std::filesystem::path& path, const std::string& name) {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  const auto resolved_path = assets_module.resolve_path(path);

  if (!std::filesystem::exists(resolved_path)) {
    throw std::runtime_error{"Animation file not found: " + resolved_path.string()};
  }

  auto importer = Assimp::Importer{};

  auto timer = utility::timer{};

  const auto* scene = importer.ReadFile(resolved_path.string(), import_flags);

  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
    throw std::runtime_error{fmt::format("Error loading animation '{}': {}", resolved_path.string(), importer.GetErrorString())};
  }

  auto index = scene->mNumAnimations;

  for (auto i = 0; i < scene->mNumAnimations; ++i) {
    const auto animation_name = std::string{scene->mAnimations[i]->mName.C_Str()};

    if (animation_name == name) {
      index = i;
      break;
    }
  }

  if (index == scene->mNumAnimations) {
    throw utility::runtime_error{"No animatiton '{}' in file {}", name, resolved_path.string()};
  }

  const auto* animation = scene->mAnimations[index]; // or all animations
  _name = animation->mName.C_Str();

  _ticks_per_second = animation->mTicksPerSecond > 0.0 ? static_cast<std::float_t>(animation->mTicksPerSecond) : 25.0f;
  _duration = (animation->mDuration > 0.0 ? static_cast<std::float_t>(animation->mDuration) : 150.0f) / _ticks_per_second;

  for (auto i = 0u; i < animation->mNumChannels; ++i) {
    const auto* channel = animation->mChannels[i];
    
    auto track = bone_track{};
    
    const auto bone_name = std::string{channel->mNodeName.C_Str()};

    for (auto k = 0u; k < channel->mNumPositionKeys; ++k) {
      const auto time = static_cast<std::float_t>(channel->mPositionKeys[k].mTime / _ticks_per_second);
      const auto position = _convert_vec3(channel->mPositionKeys[k].mValue);

      track.position_spline.add(time, position);
    }

    for (auto k = 0u; k < channel->mNumRotationKeys; ++k) {
      const auto time = static_cast<std::float_t>(channel->mRotationKeys[k].mTime / _ticks_per_second);
      const auto rotation = _convert_quat(channel->mRotationKeys[k].mValue);

      track.rotation_spline.add(time, rotation);
    }

    for (auto k = 0u; k < channel->mNumScalingKeys; ++k) {
      const auto time = static_cast<std::float_t>(channel->mScalingKeys[k].mTime / _ticks_per_second);
      const auto scale = _convert_vec3(channel->mScalingKeys[k].mValue);

      track.scale_spline.add(time, scale);
    }

    _track_map.emplace(bone_name, std::move(track));
  }

  utility::logger<"animations">::debug("Loaded animation: {} '{}' in {:.2f}ms", resolved_path.string(), _name, units::quantity_cast<units::millisecond>(timer.elapsed()));
}

auto animation::track_map() const noexcept -> const std::unordered_map<utility::hashed_string, bone_track>& {
  return _track_map;
}

auto animation::duration() const noexcept -> std::float_t {
  return _duration;
}

} // namespace sbx::animations