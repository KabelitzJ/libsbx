#ifndef LIBSBX_GRAPHICS_PIPELINE_SHADER_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_SHADER_HPP_

#include <filesystem>
#include <map>
#include <unordered_map>
#include <optional>

#include <spirv_cross/spirv_cross.hpp>

#include <vulkan/vulkan.hpp>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/containers/static_vector.hpp>

namespace sbx::graphics {

class shader : public utility::noncopyable {

public:

  enum class data_type : std::uint8_t {
    unknown,
    boolean,
    int32,
    uint32,
    float32,
    vec2,
    ivec2,
    uvec2,
    vec3,
    ivec3,
    uvec3,
    vec4,
    ivec4,
    uvec4,
    mat2,
    imat2,
    umat2,
    mat3,
    imat3,
    umat3,
    mat4,
    imat4,
    umat4,
    sampler2d,
    sampler2d_array,
    sampler_cube,
    sampler_state,
    sampler_state_array,
    separate_image2d,
    separate_image2d_array,
    storage_image,
    subpass_input,
    structure
  }; // enum class data_type

  class uniform {

  public:

    explicit uniform(std::uint32_t set, std::uint32_t binding, std::uint32_t offset, std::uint32_t size, data_type type, bool is_readonly, bool is_writeonly, VkShaderStageFlags stage_flags)
    : _set{set},
      _binding{binding},
      _offset{offset},
      _size{size},
      _type{type},
      _is_readonly{is_readonly},
      _is_writeonly{is_writeonly},
      _stage_flags{stage_flags} { }

    auto set() const noexcept -> std::uint32_t {
      return _set;
    }

    auto binding() const noexcept -> std::uint32_t {
      return _binding;
    }

    auto offset() const noexcept -> std::uint32_t {
      return _offset;
    }

    auto size() const noexcept -> std::uint32_t {
      return _size;
    }

    auto type() const noexcept -> data_type {
      return _type;
    }

    auto is_readonly() const noexcept -> bool {
      return _is_readonly;
    }

    auto is_writeonly() const noexcept -> bool {
      return _is_writeonly;
    }

    auto stage_flags() const noexcept -> VkShaderStageFlags {
      return _stage_flags;
    }

    auto add_stage_flag(VkShaderStageFlags stage) noexcept -> void {
      _stage_flags |= stage;
    }

    auto operator==(const uniform& other) const noexcept -> bool {
      return _set == other._set && _binding == other._binding && _offset == other._offset && _size == other._size && _type == other._type && _is_readonly == other._is_readonly && _is_writeonly == other._is_writeonly;
    }

  private:

    std::uint32_t _set{};
    std::uint32_t _binding{};
    std::uint32_t _offset{};
    std::uint32_t _size{};
    data_type _type{};
    bool _is_readonly{};
    bool _is_writeonly{};
    VkShaderStageFlags _stage_flags{};

  }; // class uniform

class uniform_block {

  public:

    enum class type : std::uint8_t {
      uniform,
      storage,
      push
    }; // enum class type

    explicit uniform_block(std::uint32_t set, std::uint32_t binding, std::uint32_t size, VkShaderStageFlags stage_flags, type type, std::map<std::string, uniform> uniforms = {})
    : _set{set},
      _binding{binding},
      _size{size},
      _stage_flags{stage_flags},
      _type{type},
      _uniforms{std::move(uniforms)} { }

    auto set() const noexcept -> std::uint32_t {
      return _set;
    }

    auto binding() const noexcept -> std::uint32_t {
      return _binding;
    }

    auto size() const noexcept -> std::uint32_t {
      return _size;
    }

    auto stage_flags() const noexcept -> VkShaderStageFlags {
      return _stage_flags;
    }

    auto add_stage_flag(VkShaderStageFlags stage) noexcept -> void {
      _stage_flags |= stage;
    }

    auto buffer_type() const noexcept -> type {
      return _type;
    }

    auto uniforms() const noexcept -> const std::map<std::string, uniform>& {
      return _uniforms;
    }

    auto find_uniform(const std::string& name) const noexcept -> std::optional<uniform> {
      if (auto entry = _uniforms.find(name); entry != _uniforms.end()) {
        return entry->second;
      }

      return std::nullopt;
    }

    auto operator==(const uniform_block& other) const noexcept -> bool {
      return _set == other._set && _binding == other._binding && _size == other._size && _type == other._type && _uniforms == other._uniforms;
    }

  private:

    std::uint32_t _set{};
    std::uint32_t _binding{};
    std::uint32_t _size{};
    VkShaderStageFlags _stage_flags{};
    type _type{};
    std::map<std::string, uniform> _uniforms{};

  }; // class uniform_block

  class attribute {

  public:

    explicit attribute(std::uint32_t binding, std::uint32_t size, VkShaderStageFlags stage_flags, data_type type)
    : _binding{binding},
      _size{size},
      _stage_flags{stage_flags},
      _type{type} { }

    auto binding() const noexcept -> std::uint32_t {
      return _binding;
    }

    auto size() const noexcept -> std::uint32_t {
      return _size;
    }

    auto stage_flags() const noexcept -> VkShaderStageFlags {
      return _stage_flags;
    }

    auto type() const noexcept -> data_type {
      return _type;
    }

    auto operator==(const attribute& other) const noexcept -> bool {
      return _binding == other._binding && _size == other._size && _stage_flags == other._stage_flags && _type == other._type;
    }

  private:

    std::uint32_t _binding{};
    std::uint32_t _size{};
    VkShaderStageFlags _stage_flags{};
    data_type _type{};

  }; // class attribute

  struct define {
    std::string name{};
    std::string value{};
  }; // struct define

  using handle_type = VkShaderModule;

  shader(const std::filesystem::path& path, VkShaderStageFlagBits stage);

  shader(const std::vector<std::uint32_t>& code, VkShaderStageFlagBits stage);

  ~shader();

  auto handle() const noexcept -> handle_type;

  operator handle_type() const noexcept;

  auto stage() const noexcept -> VkShaderStageFlagBits;

  auto set_uniforms() const noexcept -> const std::vector<std::unordered_map<std::string, uniform>>& {
    return _set_uniforms;
  }

  auto set_uniform_blocks() const noexcept -> const std::vector<std::unordered_map<std::string, uniform_block>>& {
    return _set_uniform_blocks;
  }

  auto uniforms(std::uint32_t set) const noexcept -> const std::unordered_map<std::string, uniform>& {
    return _set_uniforms[set];
  }

  auto uniform_blocks(std::uint32_t set) const noexcept -> const std::unordered_map<std::string, uniform_block>& {
    return _set_uniform_blocks[set];
  }

private:

  auto _create_reflection(const spirv_cross::Compiler& compiler) -> void;

  auto _get_data_type(const spirv_cross::SPIRType& type) -> data_type;

  auto _data_type_to_string(data_type type) -> std::string;

  std::vector<std::unordered_map<std::string, uniform>> _set_uniforms{};
  std::vector<std::unordered_map<std::string, uniform_block>> _set_uniform_blocks{};

  VkShaderStageFlagBits _stage{};
  handle_type _handle{};

}; // class shader

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_SHADER_HPP_
