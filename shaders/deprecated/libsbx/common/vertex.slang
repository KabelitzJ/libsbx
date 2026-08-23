#ifndef COMMON_VERTEX_SLANG_
#define COMMON_VERTEX_SLANG_

namespace sbx::common {

interface position<let N : uint> {
  vector<float, N> get_position();
}; // interface position

vector<float, N> get_position<let N : uint, Vertex: position<N>>(Vertex vertex) {
  return vertex.get_position();
}

interface normal<let N : uint> {
  vector<float, N> get_normal();
}; // interface position

vector<float, N> get_normal<let N : uint, Vertex: normal<N>>(Vertex vertex) {
  return vertex.get_normal();
}

interface tangent<let N : uint> {
  vector<float, N> get_tangent();
}; // interface position

vector<float, N> get_tangent<let N : uint, Vertex: tangent<N>>(Vertex vertex) {
  return vertex.get_tangent();
}

interface uv<let N : uint> {
  vector<float, N> get_uv();
}; // interface position

vector<float, N> get_uv<let N : uint, Vertex: uv<N>>(Vertex vertex) {
  return vertex.get_uv();
}

interface bone_indices<let N : uint> {
  vector<uint, N> get_bone_indices();
}; // interface position

vector<uint, N> get_bone_indices<let N : uint, Vertex: bone_indices<N>>(Vertex vertex) {
  return vertex.get_bone_indices();
}

interface bone_weights<let N : uint> {
  vector<float, N> get_bone_weights();
}; // interface position

vector<float, N> get_bone_weights<let N : uint, Vertex: bone_weights<N>>(Vertex vertex) {
  return vertex.get_bone_weights();
}

} // namespace sbx::common

#endif // COMMON_VERTEX_SLANG_