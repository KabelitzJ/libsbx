// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_cooker.hpp>

#include <fstream>
#include <system_error>

#include <libsbx/utility/fourcc.hpp>
#include <libsbx/utility/logger.hpp>

namespace sbx::assets {

inline constexpr auto skeleton_magic = utility::fourcc_v<"SBSK">; // 'SBSK'
inline constexpr auto animation_magic = utility::fourcc_v<"SBAN">; // 'SBAN'

struct skeleton_file_header {
  std::uint32_t magic;
  std::uint32_t version;
  std::uint32_t joint_count;
}; // struct skeleton_file_header

// Immediately followed by name_length bytes of the joint's name.
struct skeleton_joint_record {
  std::int32_t parent_index; // -1 = root; always < this joint's own index (topologically sorted)
  std::float_t inverse_bind_matrix[16]; // column-major, matches math::matrix4x4's layout
  std::float_t bind_translation[3];
  std::float_t bind_rotation[4]; // x, y, z, w
  std::float_t bind_scale[3];
  std::uint32_t name_length;
}; // struct skeleton_joint_record

struct animation_clip_file_header {
  std::uint32_t magic;
  std::uint32_t version;
  std::float_t duration;
  std::uint32_t channel_count;
  std::uint32_t name_length; // name bytes immediately follow this header
}; // struct animation_clip_file_header

// Immediately followed by translation_key_count vector3_key_records, then rotation_key_count
// quaternion_key_records, then scale_key_count vector3_key_records.
struct animation_channel_record {
  std::uint32_t joint_index;
  std::uint32_t translation_key_count;
  std::uint32_t rotation_key_count;
  std::uint32_t scale_key_count;
  std::uint32_t translation_interpolation;
  std::uint32_t rotation_interpolation;
  std::uint32_t scale_interpolation;
}; // struct animation_channel_record

struct vector3_key_record {
  std::float_t time;
  std::float_t value[3];
}; // struct vector3_key_record

struct quaternion_key_record {
  std::float_t time;
  std::float_t value[4]; // x, y, z, w
}; // struct quaternion_key_record

auto asset_cooker::derive_skeleton_uuid(const math::uuid& mesh) -> math::uuid {
  // Same splitmix64 shape as derive_material_uuid, salted differently so a mesh's skeleton uuid
  // never collides with one of its material uuids.
  auto x = mesh.value() ^ 0xff51afd7ed558ccdull;
  x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ull;
  x ^= x >> 27; x *= 0x94d049bb133111ebull;
  x ^= x >> 31;

  return math::uuid::from_value(x == 0ull ? 1ull : x); // never nil
}

auto asset_cooker::derive_animation_clip_uuid(const math::uuid& mesh, std::size_t index) -> math::uuid {
  auto x = mesh.value() ^ (0xc2b2ae3d27d4eb4full * (static_cast<std::uint64_t>(index) + 1ull));
  x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ull;
  x ^= x >> 27; x *= 0x94d049bb133111ebull;
  x ^= x >> 31;

  return math::uuid::from_value(x == 0ull ? 1ull : x); // never nil
}

auto asset_cooker::resolve_skeleton(const math::uuid& id) -> std::optional<std::vector<skeleton::joint>> {
  auto joints = std::vector<skeleton::joint>{};

  if (!_load_cooked_skeleton(id, joints)) {
    return std::nullopt;
  }

  return joints;
}

auto asset_cooker::resolve_animation_clip(const math::uuid& id) -> std::optional<animation_clip_data> {
  auto data = animation_clip_data{};

  if (!_load_cooked_animation_clip(id, data)) {
    return std::nullopt;
  }

  return data;
}

auto asset_cooker::_cook_skeleton(const math::uuid& id, const std::vector<skeleton::joint>& joints) -> bool {
  const auto cooked = cooked_path(id, ".sbxskl");

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write skeleton '{}'", cooked.generic_string());
    return false;
  }

  auto header = skeleton_file_header{};
  header.magic = skeleton_magic;
  header.version = skeleton_cook_version;
  header.joint_count = static_cast<std::uint32_t>(joints.size());

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));

  for (const auto& joint : joints) {
    auto record = skeleton_joint_record{};
    record.parent_index = joint.parent_index;

    for (auto column = std::size_t{0u}; column < 4u; ++column) {
      for (auto row = std::size_t{0u}; row < 4u; ++row) {
        record.inverse_bind_matrix[column * 4u + row] = joint.inverse_bind_matrix[column][row];
      }
    }

    record.bind_translation[0] = joint.bind_local_translation.x();
    record.bind_translation[1] = joint.bind_local_translation.y();
    record.bind_translation[2] = joint.bind_local_translation.z();
    record.bind_rotation[0] = joint.bind_local_rotation.x();
    record.bind_rotation[1] = joint.bind_local_rotation.y();
    record.bind_rotation[2] = joint.bind_local_rotation.z();
    record.bind_rotation[3] = joint.bind_local_rotation.w();
    record.bind_scale[0] = joint.bind_local_scale.x();
    record.bind_scale[1] = joint.bind_local_scale.y();
    record.bind_scale[2] = joint.bind_local_scale.z();
    record.name_length = static_cast<std::uint32_t>(joint.name.size());

    out.write(reinterpret_cast<const char*>(&record), sizeof(record));
    out.write(joint.name.data(), static_cast<std::streamsize>(joint.name.size()));
  }

  return true;
}

auto asset_cooker::_load_cooked_skeleton(const math::uuid& id, std::vector<skeleton::joint>& joints) -> bool {
  const auto cooked = cooked_path(id, ".sbxskl");

  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = skeleton_file_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != skeleton_magic || header.version != skeleton_cook_version) {
    utility::logger<"assets">::warn("Invalid cooked skeleton '{}'", cooked.generic_string());
    return false;
  }

  joints.clear();
  joints.reserve(header.joint_count);

  for (auto i = std::uint32_t{0u}; i < header.joint_count; ++i) {
    auto record = skeleton_joint_record{};
    in.read(reinterpret_cast<char*>(&record), sizeof(record));

    if (!in) {
      return false;
    }

    auto name = std::string(record.name_length, '\0');

    if (record.name_length > 0u) {
      in.read(name.data(), static_cast<std::streamsize>(record.name_length));

      if (!in) {
        return false;
      }
    }

    auto joint = skeleton::joint{};
    joint.name = std::move(name);
    joint.parent_index = record.parent_index;

    for (auto column = std::size_t{0u}; column < 4u; ++column) {
      for (auto row = std::size_t{0u}; row < 4u; ++row) {
        joint.inverse_bind_matrix[column][row] = record.inverse_bind_matrix[column * 4u + row];
      }
    }

    joint.bind_local_translation = math::vector3{record.bind_translation[0], record.bind_translation[1], record.bind_translation[2]};
    joint.bind_local_rotation = math::quaternion::wxyz(record.bind_rotation[3], record.bind_rotation[0], record.bind_rotation[1], record.bind_rotation[2]);
    joint.bind_local_scale = math::vector3{record.bind_scale[0], record.bind_scale[1], record.bind_scale[2]};

    joints.push_back(std::move(joint));
  }

  return true;
}

auto asset_cooker::_cook_animation_clip(const math::uuid& id, const animation_clip_data& data) -> bool {
  const auto cooked = cooked_path(id, ".sbxanm");

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write animation clip '{}'", cooked.generic_string());
    return false;
  }

  auto header = animation_clip_file_header{};
  header.magic = animation_magic;
  header.version = animation_cook_version;
  header.duration = data.duration;
  header.channel_count = static_cast<std::uint32_t>(data.channels.size());
  header.name_length = static_cast<std::uint32_t>(data.name.size());

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(data.name.data(), static_cast<std::streamsize>(data.name.size()));

  for (const auto& channel : data.channels) {
    auto record = animation_channel_record{};
    record.joint_index = channel.joint_index;
    record.translation_key_count = static_cast<std::uint32_t>(channel.translation_keys.size());
    record.rotation_key_count = static_cast<std::uint32_t>(channel.rotation_keys.size());
    record.scale_key_count = static_cast<std::uint32_t>(channel.scale_keys.size());
    record.translation_interpolation = static_cast<std::uint32_t>(channel.translation_interpolation);
    record.rotation_interpolation = static_cast<std::uint32_t>(channel.rotation_interpolation);
    record.scale_interpolation = static_cast<std::uint32_t>(channel.scale_interpolation);

    out.write(reinterpret_cast<const char*>(&record), sizeof(record));

    for (const auto& key : channel.translation_keys) {
      const auto key_record = vector3_key_record{key.time, {key.value.x(), key.value.y(), key.value.z()}};
      out.write(reinterpret_cast<const char*>(&key_record), sizeof(key_record));
    }

    for (const auto& key : channel.rotation_keys) {
      const auto key_record = quaternion_key_record{key.time, {key.value.x(), key.value.y(), key.value.z(), key.value.w()}};
      out.write(reinterpret_cast<const char*>(&key_record), sizeof(key_record));
    }

    for (const auto& key : channel.scale_keys) {
      const auto key_record = vector3_key_record{key.time, {key.value.x(), key.value.y(), key.value.z()}};
      out.write(reinterpret_cast<const char*>(&key_record), sizeof(key_record));
    }
  }

  return true;
}

auto asset_cooker::_load_cooked_animation_clip(const math::uuid& id, animation_clip_data& data) -> bool {
  const auto cooked = cooked_path(id, ".sbxanm");

  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = animation_clip_file_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != animation_magic || header.version != animation_cook_version) {
    utility::logger<"assets">::warn("Invalid cooked animation clip '{}'", cooked.generic_string());
    return false;
  }

  auto name = std::string(header.name_length, '\0');

  if (header.name_length > 0u) {
    in.read(name.data(), static_cast<std::streamsize>(header.name_length));

    if (!in) {
      return false;
    }
  }

  data.name = std::move(name);
  data.duration = header.duration;
  data.channels.clear();
  data.channels.reserve(header.channel_count);

  for (auto i = std::uint32_t{0u}; i < header.channel_count; ++i) {
    auto record = animation_channel_record{};
    in.read(reinterpret_cast<char*>(&record), sizeof(record));

    if (!in) {
      return false;
    }

    auto channel = animation_joint_channel{};
    channel.joint_index = record.joint_index;
    channel.translation_interpolation = static_cast<animation_interpolation>(record.translation_interpolation);
    channel.rotation_interpolation = static_cast<animation_interpolation>(record.rotation_interpolation);
    channel.scale_interpolation = static_cast<animation_interpolation>(record.scale_interpolation);

    channel.translation_keys.reserve(record.translation_key_count);

    for (auto k = std::uint32_t{0u}; k < record.translation_key_count; ++k) {
      auto key_record = vector3_key_record{};
      in.read(reinterpret_cast<char*>(&key_record), sizeof(key_record));

      if (!in) {
        return false;
      }

      channel.translation_keys.push_back({key_record.time, math::vector3{key_record.value[0], key_record.value[1], key_record.value[2]}});
    }

    channel.rotation_keys.reserve(record.rotation_key_count);

    for (auto k = std::uint32_t{0u}; k < record.rotation_key_count; ++k) {
      auto key_record = quaternion_key_record{};
      in.read(reinterpret_cast<char*>(&key_record), sizeof(key_record));

      if (!in) {
        return false;
      }

      channel.rotation_keys.push_back({key_record.time, math::quaternion::wxyz(key_record.value[3], key_record.value[0], key_record.value[1], key_record.value[2])});
    }

    channel.scale_keys.reserve(record.scale_key_count);

    for (auto k = std::uint32_t{0u}; k < record.scale_key_count; ++k) {
      auto key_record = vector3_key_record{};
      in.read(reinterpret_cast<char*>(&key_record), sizeof(key_record));

      if (!in) {
        return false;
      }

      channel.scale_keys.push_back({key_record.time, math::vector3{key_record.value[0], key_record.value[1], key_record.value[2]}});
    }

    data.channels.push_back(std::move(channel));
  }

  return true;
}


} // namespace sbx::assets
