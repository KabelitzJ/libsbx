// SPDX-License-Identifier: MIT
#include <libsbx/graphics/pipeline/shader.hpp>

#include <fstream>
#include <ranges>

#include <libsbx/utility/fast_mod.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/io/read_file.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/buffers/storage_buffer.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

shader::shader(const std::filesystem::path& path, VkShaderStageFlagBits stage)
: _stage{stage} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  auto code = io::load_spirv_words(path);

  try {
    auto compiler = spirv_cross::Compiler{code};
    _create_reflection(compiler);
  } catch (const spirv_cross::CompilerError& e) {
    throw std::runtime_error(std::string{"SPIRV-Cross failed: "} + e.what());
  }

  auto create_info = VkShaderModuleCreateInfo{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = code.size() * sizeof(std::uint32_t);
  create_info.pCode = code.data();

  validate(vkCreateShaderModule(logical_device, &create_info, nullptr, &_handle));
}

shader::shader(const std::vector<std::uint32_t>& code, VkShaderStageFlagBits stage)
: _stage{stage} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  try {
    auto compiler = spirv_cross::Compiler{code};
    _create_reflection(compiler);
  } catch (const spirv_cross::CompilerError& e) {
    throw std::runtime_error(std::string{"SPIRV-Cross failed: "} + e.what());
  }

  auto create_info = VkShaderModuleCreateInfo{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = code.size() * sizeof(std::uint32_t);
  create_info.pCode = code.data();

  validate(vkCreateShaderModule(logical_device, &create_info, nullptr, &_handle));
}

shader::~shader() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  vkDestroyShaderModule(logical_device, _handle, nullptr);
}

auto shader::handle() const noexcept -> handle_type {
  return _handle;
}

shader::operator handle_type() const noexcept {
  return _handle;
}

auto shader::stage() const noexcept -> VkShaderStageFlagBits {
  return _stage;
}

auto shader::_create_reflection(const spirv_cross::Compiler& compiler) -> void {
  auto resources = compiler.get_shader_resources();

  // Reflection for uniform blocks
  for (const auto& uniform_buffer : resources.uniform_buffers) {
    const auto& type = compiler.get_type(uniform_buffer.type_id);

    const auto& uniform_blocks_name = compiler.get_name(uniform_buffer.id);
    const auto uniform_blocks_set = compiler.get_decoration(uniform_buffer.id, spv::DecorationDescriptorSet);
    const auto uniform_blocks_binding = compiler.get_decoration(uniform_buffer.id, spv::DecorationBinding);
    const auto uniform_blocks_size = compiler.get_declared_struct_size(type);

    utility::logger<"graphics">::debug("uniform block: '{}' set: {} binding: {} size: {}", uniform_blocks_name, uniform_blocks_set, uniform_blocks_binding, uniform_blocks_size);
 
    const auto member_count = type.member_types.size();

    auto uniforms = std::map<std::string, uniform>{};

    for (auto i : std::views::iota(0u, member_count)) {
      const auto& member_type = compiler.get_type(type.member_types[i]);
      const auto& member_name = compiler.get_member_name(type.self, i);

      const auto member_set = compiler.get_member_decoration(type.self, i, spv::DecorationDescriptorSet);
      const auto member_binding = compiler.get_member_decoration(type.self, i, spv::DecorationBinding);
      const auto member_offset = compiler.type_struct_member_offset(type, i);
      const auto member_size = compiler.get_declared_struct_member_size(type, i);
      const auto member_data_type = _get_data_type(member_type);

      utility::logger<"graphics">::debug("  binding: {}\toffset: {}\tsize: {}{}\tdata_type: {}", member_binding, member_offset, member_size, member_size < 10 ? "\t" : "", _data_type_to_string(member_data_type));

      uniforms.emplace(member_name, uniform{member_set, member_binding, member_offset, member_size, member_data_type, false, false, _stage});
    }

    _set_uniform_blocks.resize(std::max(_set_uniform_blocks.size(), static_cast<std::size_t>(uniform_blocks_set + 1u)));

    _set_uniform_blocks[uniform_blocks_set].emplace(uniform_blocks_name, uniform_block{uniform_blocks_set, uniform_blocks_binding, uniform_blocks_size, _stage, uniform_block::type::uniform, std::move(uniforms)});
  }

  // Reflection for push constants
  for (const auto& push_constant : resources.push_constant_buffers) {
    const auto& type = compiler.get_type(push_constant.type_id);

    const auto& uniform_blocks_name = compiler.get_name(push_constant.id);
    const auto uniform_blocks_set = compiler.get_decoration(push_constant.id, spv::DecorationDescriptorSet);
    const auto uniform_blocks_binding = compiler.get_decoration(push_constant.id, spv::DecorationBinding);
    const auto uniform_blocks_size = compiler.get_declared_struct_size(type);

    utility::logger<"graphics">::debug("push constant: '{}' set: {} binding: {} size: {}", uniform_blocks_name, uniform_blocks_set, uniform_blocks_binding, uniform_blocks_size);
 
    const auto member_count = type.member_types.size();

    auto uniforms = std::map<std::string, uniform>{};

    for (auto i : std::views::iota(0u, member_count)) {
      const auto& member_type = compiler.get_type(type.member_types[i]);
      const auto& member_name = compiler.get_member_name(type.self, i);

      const auto member_set = compiler.get_member_decoration(type.self, i, spv::DecorationDescriptorSet);
      const auto member_binding = compiler.get_member_decoration(type.self, i, spv::DecorationBinding);
      const auto member_offset = compiler.type_struct_member_offset(type, i);
      const auto member_size = compiler.get_declared_struct_member_size(type, i);
      const auto member_data_type = _get_data_type(member_type);

      utility::logger<"graphics">::debug("  binding: {}\toffset: {}\tsize: {}{}\tdata_type: {}", member_binding, member_offset, member_size, member_size < 10 ? "\t" : "", _data_type_to_string(member_data_type));

      uniforms.insert({member_name, uniform{member_set, member_binding, member_offset, member_size, member_data_type, false, false, _stage}});
    }

    _set_uniform_blocks.resize(std::max(_set_uniform_blocks.size(), static_cast<std::size_t>(uniform_blocks_set + 1u)));

    _set_uniform_blocks[uniform_blocks_set].emplace(uniform_blocks_name, uniform_block{uniform_blocks_set, uniform_blocks_binding, uniform_blocks_size, _stage, uniform_block::type::push, std::move(uniforms)});
  }

  // Reflection for storage buffers
  for (const auto& storage_buffer : resources.storage_buffers) {
    const auto& type = compiler.get_type(storage_buffer.type_id);

    const auto& storage_buffer_name = compiler.get_name(storage_buffer.id);
    const auto storage_buffer_set = compiler.get_decoration(storage_buffer.id, spv::DecorationDescriptorSet);
    const auto storage_buffer_binding = compiler.get_decoration(storage_buffer.id, spv::DecorationBinding);

    // Get the size of one element in the storage buffer
    const auto storage_buffer_element_size = compiler.get_declared_struct_size_runtime_array(type, 1);  

    utility::logger<"graphics">::debug("storage buffer: '{}' set: {} binding: {} element_size: {}", storage_buffer_name, storage_buffer_set, storage_buffer_binding, storage_buffer_element_size);

    _set_uniform_blocks.resize(std::max(_set_uniform_blocks.size(), static_cast<std::size_t>(storage_buffer_set + 1u)));

    _set_uniform_blocks[storage_buffer_set].emplace(storage_buffer_name, uniform_block{storage_buffer_set, storage_buffer_binding, 0u, _stage, uniform_block::type::storage});
  }

  // Reflection for image samplers
  for (const auto& image_sampler : resources.sampled_images) {
    const auto& type = compiler.get_type(image_sampler.type_id);

    const auto& name = image_sampler.name;
    const auto set = compiler.get_decoration(image_sampler.id, spv::DecorationDescriptorSet);
    const auto binding = compiler.get_decoration(image_sampler.id, spv::DecorationBinding);

    _set_uniforms.resize(std::max(_set_uniforms.size(), static_cast<std::size_t>(set + 1u)));

    if (type.array.size() == 0u) {
      if (type.image.dim == spv::Dim::Dim2D) {
        utility::logger<"graphics">::debug("image2d sampler: '{}' binding: {}", name, binding);
        _set_uniforms[set].emplace(name, uniform{set, binding, 0, 0, data_type::sampler2d, false, false, _stage});
      } else if (type.image.dim == spv::Dim::DimCube) {
        utility::logger<"graphics">::debug("image cube sampler: '{}' binding: {}", name, binding);
        _set_uniforms[set].emplace(name, uniform{set, binding, 0, 0, data_type::sampler_cube, false, false, _stage});
      }
    } else if (type.array.size() == 1u) {
      utility::logger<"graphics">::debug("image sampler[{}]: '{}' binding: {}", type.array[0], name, binding);
      _set_uniforms[set].emplace(name, uniform{set, binding, 0, 32u, data_type::sampler2d_array, false, false, _stage});
    }
  }

  for (const auto& separate_image : resources.separate_images) {
    const auto& type = compiler.get_type(separate_image.type_id);

    const auto& name = separate_image.name;
    const auto set = compiler.get_decoration(separate_image.id, spv::DecorationDescriptorSet);
    const auto binding = compiler.get_decoration(separate_image.id, spv::DecorationBinding);

    _set_uniforms.resize(std::max(_set_uniforms.size(), static_cast<std::size_t>(set + 1u)));

    if (type.array.size() == 0u) {
      utility::logger<"graphics">::debug("separate image: '{}' binding: {}", name, binding);
      _set_uniforms[set].emplace(name, uniform{set, binding, 0, 0, data_type::separate_image2d, false, false, _stage});
    } else if (type.array.size() == 1u) {
      utility::logger<"graphics">::debug("separate image[{}]: '{}' binding: {}", type.array[0], name, binding);
      _set_uniforms[set].emplace(name, uniform{set, binding, 0, 32u, data_type::separate_image2d_array, false, false, _stage});
    }
  }

  for (const auto& sampler_state : resources.separate_samplers) {
    const auto& type = compiler.get_type(sampler_state.type_id);

    const auto& name = sampler_state.name;
    const auto set = compiler.get_decoration(sampler_state.id, spv::DecorationDescriptorSet);
    const auto binding = compiler.get_decoration(sampler_state.id, spv::DecorationBinding);

    _set_uniforms.resize(std::max(_set_uniforms.size(), static_cast<std::size_t>(set + 1u)));

    if (type.array.size() == 0u) {
      utility::logger<"graphics">::debug("separate sampler_state: '{}' binding: {}", name, binding);
      _set_uniforms[set].emplace(name, uniform{set, binding, 0, 0, data_type::sampler_state, false, false, _stage});
    } else if (type.array.size() == 1u) {
      utility::logger<"graphics">::debug("separate sampler_state[{}]: '{}' binding: {}", type.array[0], name, binding);
      _set_uniforms[set].emplace(name, uniform{set, binding, 0, 32u, data_type::sampler_state_array, false, false, _stage});
    }
  }

  // Reflection for storage images
  for (const auto& storage_image : resources.storage_images) {
    const auto& name = storage_image.name;
    const auto set = compiler.get_decoration(storage_image.id, spv::DecorationDescriptorSet);
    const auto binding = compiler.get_decoration(storage_image.id, spv::DecorationBinding);
    const auto is_readonly = compiler.get_decoration(storage_image.id, spv::DecorationNonWritable) == 1;
    const auto is_writeonly = compiler.get_decoration(storage_image.id, spv::DecorationNonReadable) == 1;

    _set_uniforms.resize(std::max(_set_uniforms.size(), static_cast<std::size_t>(set + 1u)));

    utility::logger<"graphics">::debug("storage image: '{}' binding: {} readonly: {} writeonly: {}", name, binding, is_readonly, is_writeonly);

    _set_uniforms[set].emplace(name, uniform{set, binding, 0, 0, data_type::storage_image, is_readonly, is_writeonly, _stage});
  }

  // Reflection for subpass inputs
  for (const auto& storage_image : resources.subpass_inputs) {
    const auto& name = storage_image.name;
    const auto set = compiler.get_decoration(storage_image.id, spv::DecorationDescriptorSet);
    const auto binding = compiler.get_decoration(storage_image.id, spv::DecorationBinding);

    _set_uniforms.resize(std::max(_set_uniforms.size(), static_cast<std::size_t>(set + 1u)));

    utility::logger<"graphics">::debug("subpass input: '{}' binding: {}", name, binding);

    _set_uniforms[set].emplace(name, uniform{set, binding, 0, 0, data_type::subpass_input, true, false, _stage});
  }
}

auto shader::_get_data_type(const spirv_cross::SPIRType& type) -> data_type {
  if (type.basetype == spirv_cross::SPIRType::Boolean) {
    return data_type::boolean;
  } else if (type.basetype == spirv_cross::SPIRType::Int) {
    if (type.vecsize == 1) {
      return data_type::int32;
    } else if (type.vecsize == 2) {
      if (type.columns == 1) {
        return data_type::ivec2;
      } else if (type.columns == 2) {
        return data_type::imat2;
      } else {
        return data_type::unknown;
      }
    } else if (type.vecsize == 3) {
      if (type.columns == 1) {
        return data_type::ivec3;
      } else if (type.columns == 3) {
        return data_type::imat3;
      } else {
        return data_type::unknown;
      }
    } else if (type.vecsize == 4) {
      if (type.columns == 1) {
        return data_type::ivec4;
      } else if (type.columns == 4) {
        return data_type::imat4;
      } else {
        return data_type::unknown;
      }
    } else {
      return data_type::unknown;
    }
  } else if (type.basetype == spirv_cross::SPIRType::UInt) {
    if (type.vecsize == 1) {
      return data_type::uint32;
    } else if (type.vecsize == 2) {
      if (type.columns == 1) {
        return data_type::uvec2;
      } else if (type.columns == 2) {
        return data_type::umat2;
      } else {
        return data_type::unknown;
      }
    } else if (type.vecsize == 3) {
      if (type.columns == 1) {
        return data_type::uvec3;
      } else if (type.columns == 3) {
        return data_type::umat3;
      } else {
        return data_type::unknown;
      }
    } else if (type.vecsize == 4) {
      if (type.columns == 1) {
        return data_type::uvec4;
      } else if (type.columns == 4) {
        return data_type::umat4;
      } else {
        return data_type::unknown;
      }
    } else {
      return data_type::unknown;
    }
  } else if (type.basetype == spirv_cross::SPIRType::Float) {
    if (type.vecsize == 1) {
      return data_type::float32;
    } else if (type.vecsize == 2) {
      if (type.columns == 1) {
        return data_type::vec2;
      } else if (type.columns == 2) {
        return data_type::mat2;
      } else {
        return data_type::unknown;
      }
    } else if (type.vecsize == 3) {
      if (type.columns == 1) {
        return data_type::vec3;
      } else if (type.columns == 3) {
        return data_type::mat3;
      } else {
        return data_type::unknown;
      }
    } else if (type.vecsize == 4) {
      if (type.columns == 1) {
        return data_type::vec4;
      } else if (type.columns == 4) {
        return data_type::mat4;
      } else {
        return data_type::unknown;
      }
    } else {
      return data_type::unknown;
    }
  } else if (type.basetype == spirv_cross::SPIRType::SampledImage) {
    if (type.array.size() == 0u) {
      if (type.image.dim == spv::Dim::Dim2D) {
        return data_type::sampler2d;
      } else if (type.image.dim == spv::Dim::DimCube) {
        return data_type::sampler_cube;
      } else {
        return data_type::unknown;
      }
    } else if (type.array.size() == 1u) {
      return data_type::sampler2d_array;
    } else {
      return data_type::unknown;
    }
  } else if (type.basetype == spirv_cross::SPIRType::Image) {
    if (type.array.size() == 0u) {
      return data_type::separate_image2d;
    } else if (type.array.size() == 1u) {
      return data_type::separate_image2d_array;
    } else {
      return data_type::unknown;
    }
  } else if (type.basetype == spirv_cross::SPIRType::Sampler) {
    if (type.array.size() == 0u) {
      return data_type::sampler_state;
    } else if (type.array.size() == 1u) {
      return data_type::sampler_state_array;
    } else {
      return data_type::unknown;
    }
  } else if (type.basetype == spirv_cross::SPIRType::Struct) {
    return data_type::structure;
  }

  return data_type::unknown;
}

auto shader::_data_type_to_string(data_type type) -> std::string {
  switch (type) {
    case data_type::boolean: return "bool";
    case data_type::int32: return "int";
    case data_type::uint32: return "uint";
    case data_type::float32: return "float";
    case data_type::vec2: return "vec2";
    case data_type::ivec2: return "ivec2";
    case data_type::uvec2: return "uvec2";
    case data_type::vec3: return "vec3";
    case data_type::ivec3: return "ivec3";
    case data_type::uvec3: return "uvec3";
    case data_type::vec4: return "vec4";
    case data_type::ivec4: return "ivec4";
    case data_type::uvec4: return "uvec4";
    case data_type::mat2: return "mat2";
    case data_type::imat2: return "imat2";
    case data_type::umat2: return "umat2";
    case data_type::mat3: return "mat3";
    case data_type::imat3: return "imat3";
    case data_type::umat3: return "umat3";
    case data_type::mat4: return "mat4";
    case data_type::imat4: return "imat4";
    case data_type::umat4: return "umat4";
    case data_type::sampler2d: return "sampler2d";
    case data_type::sampler2d_array: return "sampler2d_array";
    case data_type::sampler_cube: return "sampler_cube";
    case data_type::sampler_state: return "sampler_state";
    case data_type::sampler_state_array: return "sampler_state_array";
    case data_type::separate_image2d: return "separate_image2d";
    case data_type::separate_image2d_array: return "separate_image2d_array";
    case data_type::structure: return "structure";
    case data_type::unknown:
    default: return "unknown";
  }
}

} // namespace sbx::graphics
