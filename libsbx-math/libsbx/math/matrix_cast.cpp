// SPDX-License-Identifier: MIT
#include <libsbx/math/matrix_cast.hpp>

namespace sbx::math {

auto decompose(const matrix4x4& matrix) noexcept -> decompose_result {
  auto result = decompose_result{};

  result.position = vector3{matrix[3][0], matrix[3][1], matrix[3][2]};

  result.scale.x() = vector3{matrix[0][0], matrix[0][1], matrix[0][2]}.length();
  result.scale.y() = vector3{matrix[1][0], matrix[1][1], matrix[1][2]}.length();
  result.scale.z() = vector3{matrix[2][0], matrix[2][1], matrix[2][2]}.length();

  auto rotation_matrix = matrix4x4{matrix};

  rotation_matrix[0][0] /= result.scale.x();
  rotation_matrix[0][1] /= result.scale.x();
  rotation_matrix[0][2] /= result.scale.x();

  rotation_matrix[1][0] /= result.scale.y();
  rotation_matrix[1][1] /= result.scale.y();
  rotation_matrix[1][2] /= result.scale.y();

  rotation_matrix[2][0] /= result.scale.z();
  rotation_matrix[2][1] /= result.scale.z();
  rotation_matrix[2][2] /= result.scale.z();

  result.rotation = quaternion{rotation_matrix};

  return result;
}

} // namespace sbx::math
