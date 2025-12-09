#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <range/v3/all.hpp>

#include <libsbx/utility/timer.hpp>
#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/buffers/uniform_buffer.hpp>
#include <libsbx/graphics/buffers/storage_buffer.hpp>
#include <libsbx/graphics/images/image.hpp>

namespace sbx::graphics {

compute_pipeline::compute_pipeline(const std::filesystem::path& path)
: base{},
  _bind_point{VK_PIPELINE_BIND_POINT_COMPUTE} {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  auto timer = utility::timer{};

  const auto resolved_path = assets_module.resolve_path(path);

  _name = resolved_path.filename().string();

  const auto binary_path = resolved_path / "bin";

  if (!std::filesystem::exists(binary_path) && !std::filesystem::is_directory(binary_path)) {
    throw std::runtime_error{fmt::format("Path '{}' does not exist", binary_path.string())};
  }

  for (const auto& entry : std::filesystem::directory_iterator(binary_path)) {
    if (entry.is_regular_file()) {
      const auto& file = entry.path();
      
      const auto& stem = file.stem().string();

      const auto stage = _get_stage_from_name(stem);

      if (stage == VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM) {
        utility::logger<"graphics">::warn("Unsupported shader stage '{}' in compute pipeline '{}'", stem, _name);
        continue;
      }

      _shader = std::make_unique<shader>(file, stage);
      
      break;
    }
  }

  if (!_shader) {
    throw std::runtime_error{"Required compute shader not found"};
  }

  auto shader_stage = VkPipelineShaderStageCreateInfo{};

  shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stage.stage = _shader->stage();
  shader_stage.module = *_shader;
  shader_stage.pName = "main";

  // [NOTE] 2025-05-14: Create or append uniforms
  for (auto&& [i, set] : ranges::views::enumerate(_shader->set_uniforms())) {
    _set_data.resize(std::max(_set_data.size(), i + 1u));

    auto& set_uniforms = _set_data[i].uniforms;

    for (const auto& [name, uniform] : set) {
      if (auto entry = set_uniforms.find(name); entry != set_uniforms.end()) {
        entry->second.add_stage_flag(uniform.stage_flags());
      } else {
        set_uniforms.insert({name, uniform});
      }
    }
  }
    
  for (auto&& [i, set] : ranges::views::enumerate(_shader->set_uniform_blocks())) {
    _set_data.resize(std::max(_set_data.size(), i + 1u));

    auto& set_uniform_blocks = _set_data[i].uniform_blocks;

    for (const auto& [name, uniform_block] : set) {
      if (auto entry = set_uniform_blocks.find(name); entry != set_uniform_blocks.end()) {
        entry->second.add_stage_flag(uniform_block.stage_flags());
      } else {
        set_uniform_blocks.insert({name, uniform_block});
      }
    }
  }

  auto descriptor_set_layout_bindings = std::vector<std::unordered_map<std::uint32_t, VkDescriptorSetLayoutBinding>>{};
  descriptor_set_layout_bindings.resize(_set_data.size());

  for (auto& set_data : _set_data) {
    for (const auto& [name, uniform_block] : set_data.uniform_blocks) {
      auto& descriptor_set_layout_binding = descriptor_set_layout_bindings[uniform_block.set()];

      switch (uniform_block.buffer_type()) {
        case shader::uniform_block::type::uniform: {
          descriptor_set_layout_binding[uniform_block.binding()] = uniform_buffer::create_descriptor_set_layout_binding(uniform_block.binding(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniform_block.stage_flags());
          break;
        }
        case shader::uniform_block::type::storage: {
          descriptor_set_layout_binding[uniform_block.binding()] = storage_buffer::create_descriptor_set_layout_binding(uniform_block.binding(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, uniform_block.stage_flags());
          break;
        }
        case shader::uniform_block::type::push: {
          // [NOTE] KAJ 2024-01-19 : We dont need descriptor sets for push constants but we still want to add them the the bindings and sizes
          if (_push_constant) {
            utility::logger<"graphics">::warn("Multiple push constant blocks found in shader '{}'", _name);
          }

          _push_constant = uniform_block;
          break;
        }
        default: {
          utility::logger<"graphics">::warn("Unsupported uniform block type (sbx::graphics::shader::uniform_block::type): {}", uniform_block.buffer_type());
          continue;
        }
      }

      set_data.descriptor_bindings.insert({name, uniform_block.binding()});
      set_data.descriptor_sizes.insert({name, uniform_block.size()});
    }
  }

  for (auto& set_data : _set_data) {
    for (const auto& [name, uniform] : set_data.uniforms) {
      auto& descriptor_set_layout_binding = descriptor_set_layout_bindings[uniform.set()];

      switch (uniform.type()) {
        case shader::data_type::sampler2d: {
          descriptor_set_layout_binding[uniform.binding()] = image2d::create_descriptor_set_layout_binding(uniform.binding(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, uniform.stage_flags());
          break;
        }
        case shader::data_type::sampler2d_array: {
          descriptor_set_layout_binding[uniform.binding()] = image2d::create_descriptor_set_layout_binding(uniform.binding(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, uniform.stage_flags());
          break;
        }
        case shader::data_type::sampler_cube: {
          descriptor_set_layout_binding[uniform.binding()] = cube_image::create_descriptor_set_layout_binding(uniform.binding(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, uniform.stage_flags());
          break;
        }
        case shader::data_type::sampler_state: {
          descriptor_set_layout_binding[uniform.binding()] = sampler_state::create_descriptor_set_layout_binding(uniform.binding(), VK_DESCRIPTOR_TYPE_SAMPLER, uniform.stage_flags());
          break;
        }
        case shader::data_type::storage_image: {
          descriptor_set_layout_binding[uniform.binding()] = image::create_descriptor_set_layout_binding(uniform.binding(), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, uniform.stage_flags());
          break;
        }
        default: {
          utility::logger<"graphics">::warn("Unsupported uniform type (sbx::graphics::shader::data_type): {} for set: {} binding: {}", uniform.type(), uniform.set(), uniform.binding());
          continue;
        }
      }

      set_data.descriptor_bindings.insert({name, uniform.binding()});
      set_data.descriptor_sizes.insert({name, uniform.size()});
    }
  }

  // std::ranges::sort(descriptor_set_layout_bindings, [](const auto& lhs, const auto& rhs) {
  //   return lhs.binding < rhs.binding;
  // });

  for (auto&& [set, descriptors] : ranges::views::enumerate(descriptor_set_layout_bindings)) {
    _set_data[set].binding_data.resize(descriptors.size());

    for (auto&& [binding, descriptor] : descriptors) {
      _set_data[set].binding_data[binding].descriptor_type = descriptor.descriptorType;
      _set_data[set].binding_data[binding].descriptor_count = descriptor.descriptorCount;
    }
  }

  for (auto&& [set, descriptor_set_layout_binding] : ranges::views::enumerate(descriptor_set_layout_bindings)) {
    const auto bindings = utility::map_to<std::vector>(descriptor_set_layout_binding, [](const auto& entry) -> VkDescriptorSetLayoutBinding { return entry.second; });

    auto descriptor_set_layout_create_info = VkDescriptorSetLayoutCreateInfo{};
    descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.bindingCount = static_cast<std::uint32_t>(bindings.size());
    descriptor_set_layout_create_info.pBindings = bindings.data();
  
    validate(vkCreateDescriptorSetLayout(logical_device, &descriptor_set_layout_create_info, nullptr, &_set_data[set].layout));
  }


  auto descriptor_pool_sizes = std::vector<VkDescriptorPoolSize>{
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2048},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2048},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2048}
  };

  auto descriptor_pool_create_info = VkDescriptorPoolCreateInfo{};
  descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptor_pool_create_info.poolSizeCount = static_cast<std::uint32_t>(descriptor_pool_sizes.size());
  descriptor_pool_create_info.pPoolSizes = descriptor_pool_sizes.data();
  descriptor_pool_create_info.maxSets = 8192;

  validate(vkCreateDescriptorPool(logical_device, &descriptor_pool_create_info, nullptr, &_descriptor_pool));

  auto push_constant_ranges = std::vector<VkPushConstantRange>{};

  auto current_offset = std::uint32_t{0u};

  for (const auto& set_data : _set_data) {
    for (const auto& [name, uniform] : set_data.uniform_blocks) {
      if (uniform.buffer_type() != shader::uniform_block::type::push) {
        continue;
      }

      auto push_constant_range = VkPushConstantRange{};
      push_constant_range.stageFlags = uniform.stage_flags();
      push_constant_range.offset = current_offset;
      push_constant_range.size = uniform.size();

      push_constant_ranges.push_back(push_constant_range);

      current_offset += push_constant_range.size;
    }
  }

  const auto layouts = utility::map_to<std::vector>(_set_data, [](const auto& set_data) -> VkDescriptorSetLayout { return set_data.layout; });

  auto pipeline_layout_create_info = VkPipelineLayoutCreateInfo{};
  pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_create_info.setLayoutCount = static_cast<std::uint32_t>(layouts.size());
  pipeline_layout_create_info.pSetLayouts = layouts.data();
  pipeline_layout_create_info.pushConstantRangeCount = static_cast<std::uint32_t>(push_constant_ranges.size());
  pipeline_layout_create_info.pPushConstantRanges = push_constant_ranges.data();

  validate(vkCreatePipelineLayout(logical_device, &pipeline_layout_create_info, nullptr, &_layout));

  auto pipeline_create_info = VkComputePipelineCreateInfo{};
  pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_create_info.stage = shader_stage;
  pipeline_create_info.layout = _layout;
  pipeline_create_info.basePipelineIndex = -1;
  pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

  validate(vkCreateComputePipelines(logical_device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &_handle));

  utility::logger<"graphics">::debug("Pipeline '{}' created in {:.2f}ms", _name, units::quantity_cast<units::millisecond>(timer.elapsed()).value());
}

compute_pipeline::~compute_pipeline() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  _shader.reset();

  logical_device.wait_idle();

  vkDestroyDescriptorPool(logical_device, _descriptor_pool, nullptr);

  for (const auto& set_data : _set_data) {
    vkDestroyDescriptorSetLayout(logical_device, set_data.layout, nullptr);
  }

  vkDestroyPipelineLayout(logical_device, _layout, nullptr);

  vkDestroyPipeline(logical_device, _handle, nullptr);
}

auto compute_pipeline::_get_stage_from_name(const std::string& name) const noexcept -> VkShaderStageFlagBits {
  if (name == "compute") {
    return VK_SHADER_STAGE_COMPUTE_BIT;
  }
  
  return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}

} // namespace sbx::graphics {
