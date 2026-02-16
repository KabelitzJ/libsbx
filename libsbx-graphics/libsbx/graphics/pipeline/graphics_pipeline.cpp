// SPDX-License-Identifier: MIT
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <unordered_set>

#include <range/v3/all.hpp>

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <libsbx/units/time.hpp>

#include <libsbx/utility/timer.hpp>
#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/buffers/uniform_buffer.hpp>
#include <libsbx/graphics/buffers/storage_buffer.hpp>

#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/images/depth_image.hpp>
#include <libsbx/graphics/images/cube_image.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>
#include <libsbx/graphics/images/separate_image2d_array.hpp>

#include <libsbx/graphics/render_pass/swapchain.hpp>

namespace sbx::graphics {

static auto _get_stage_from_slang(SlangStage stage) -> VkShaderStageFlagBits {
  switch (stage) {
    case SLANG_STAGE_VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
    case SLANG_STAGE_HULL: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case SLANG_STAGE_DOMAIN: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    case SLANG_STAGE_GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
    case SLANG_STAGE_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
    case SLANG_STAGE_COMPUTE: return VK_SHADER_STAGE_COMPUTE_BIT;
    default: return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
  }
}

graphics_pipeline::graphics_pipeline(const std::filesystem::path& path, const std::vector<attachment_description>& attachments, const pipeline_definition& default_definition, const VkSpecializationInfo* specialization_info)
: base{},
  _bind_point{VK_PIPELINE_BIND_POINT_GRAPHICS} {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto resolved_path = assets_module.resolve_path(path);

  const auto definition = _update_definition(resolved_path, default_definition);

  _name = resolved_path.filename().string();

  utility::logger<"graphics">::debug("Creating pipeline '{}'", _name);

  const auto binary_path = resolved_path / "bin";

  if (!std::filesystem::exists(binary_path) && !std::filesystem::is_directory(binary_path)) {
    throw std::runtime_error{fmt::format("Path '{}' does not exist", binary_path.string())};
  }

  for (const auto& entry : std::filesystem::directory_iterator(binary_path)) {
    if (entry.is_regular_file()) {
      const auto& file = entry.path();
      
      const auto& stem = file.stem().string();

      const auto stage = _get_stage_from_name(stem);

      if (stage == VK_SHADER_STAGE_COMPUTE_BIT) {
        // [NOTE] : Allow compute and graphics pipelines in same folder
        continue;
      } else if (stage == VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM) {
        utility::logger<"graphics">::warn("Unsupported shader stage '{}' in graphics pipeline '{}'", stem, _name);
        continue;
      }

      _shaders.insert({stage, std::make_unique<shader>(file, stage)});
    }
  }

  if (!_shaders.contains(VK_SHADER_STAGE_VERTEX_BIT)) {
    throw std::runtime_error{"Required vertex shader not found"};
  }

  if (!_shaders.contains(VK_SHADER_STAGE_FRAGMENT_BIT)) {
    throw std::runtime_error{"Required fragment shader not found"};
  }

  _initialize(attachments, definition, specialization_info);
}

graphics_pipeline::graphics_pipeline(const compiled_shaders& shaders, const std::vector<attachment_description>& attachments, const pipeline_definition& default_definition, const VkSpecializationInfo* specialization_info)
: base{},
  _bind_point{VK_PIPELINE_BIND_POINT_GRAPHICS} {

  _name = shaders.name;

  for (const auto& [stage, code] : shaders.shader_codes) {
    const auto stage_flag = _get_stage_from_slang(static_cast<SlangStage>(stage));

    if (stage_flag == VK_SHADER_STAGE_COMPUTE_BIT) {
      // [NOTE] : Allow compute and graphics pipelines in same folder
      continue;
    } else if (stage_flag == VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM) {
      utility::logger<"graphics">::warn("Unsupported shader stage '{}' in graphics pipeline '{}'", stage, _name);
      continue;
    }

    _shaders.insert({stage_flag, std::make_unique<shader>(code, stage_flag)});
  }

  if (!_shaders.contains(VK_SHADER_STAGE_VERTEX_BIT)) {
    throw std::runtime_error{"Required vertex shader not found"};
  }

  if (!_shaders.contains(VK_SHADER_STAGE_FRAGMENT_BIT)) {
    throw std::runtime_error{"Required fragment shader not found"};
  }

  _initialize(attachments, default_definition, specialization_info);
}

auto graphics_pipeline::_initialize(const std::vector<attachment_description>& attachments, const pipeline_definition& definition, const VkSpecializationInfo* specialization_info) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  auto timer = utility::timer{};

  auto shader_stages = std::vector<VkPipelineShaderStageCreateInfo>{};

  for (const auto& [stage, shader] : _shaders) {
    auto shader_stage = VkPipelineShaderStageCreateInfo{};

    shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage.stage = stage;
    shader_stage.module = *shader;
    shader_stage.pName = "main";
    shader_stage.pSpecializationInfo = specialization_info;

    shader_stages.push_back(shader_stage);

    // [NOTE] 2025-05-14: Create or append uniforms
    for (auto&& [i, set] : ranges::views::enumerate(shader->set_uniforms())) {
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
    
    for (auto&& [i, set] : ranges::views::enumerate(shader->set_uniform_blocks())) {
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
          if (_push_constant.has_value()) {
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
        case shader::data_type::separate_image2d_array: {
          descriptor_set_layout_binding[uniform.binding()] = separate_image2d_array::create_descriptor_set_layout_binding(uniform.binding(), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, uniform.stage_flags());
          _has_variable_descriptors = true;
          break;
        }
        case shader::data_type::sampler_state: {
          descriptor_set_layout_binding[uniform.binding()] = sampler_state::create_descriptor_set_layout_binding(uniform.binding(), VK_DESCRIPTOR_TYPE_SAMPLER, uniform.stage_flags());
          break;
        }
        case shader::data_type::sampler_state_array: {
          descriptor_set_layout_binding[uniform.binding()] = separate_sampler_array::create_descriptor_set_layout_binding(uniform.binding(), VK_DESCRIPTOR_TYPE_SAMPLER, uniform.stage_flags());
          break;
        }
        case shader::data_type::storage_image: {
          descriptor_set_layout_binding[uniform.binding()] = image::create_descriptor_set_layout_binding(uniform.binding(), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, uniform.stage_flags());
          break;
        }
        case shader::data_type::subpass_input: {
          descriptor_set_layout_binding[uniform.binding()] = image::create_descriptor_set_layout_binding(uniform.binding(), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, uniform.stage_flags());
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

  for (auto&& [set, descriptors] : ranges::views::enumerate(descriptor_set_layout_bindings)) {
    _set_data[set].binding_data.resize(descriptors.size());
    
    for (auto&& [binding, descriptor] : descriptors) {
      _set_data[set].binding_data[binding].descriptor_type = descriptor.descriptorType;
      _set_data[set].binding_data[binding].descriptor_count = descriptor.descriptorCount;
    }
  }

  auto viewport_state = VkPipelineViewportStateCreateInfo{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  auto rasterization_state = VkPipelineRasterizationStateCreateInfo{};
  rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization_state.depthClampEnable = false;
  rasterization_state.rasterizerDiscardEnable = false;
  rasterization_state.depthBiasEnable = false;
  rasterization_state.polygonMode = to_vk_enum<VkPolygonMode>(definition.rasterization_state.polygon_mode);
  rasterization_state.lineWidth = definition.rasterization_state.line_width;
  rasterization_state.cullMode = to_vk_enum<VkCullModeFlags>(definition.rasterization_state.cull_mode);
  rasterization_state.frontFace = to_vk_enum<VkFrontFace>(definition.rasterization_state.front_face);

  if (definition.rasterization_state.depth_bias.has_value()) {
    const auto& depth_bias = definition.rasterization_state.depth_bias.value();

    rasterization_state.depthBiasEnable = true;
    rasterization_state.depthBiasConstantFactor = depth_bias.constant_factor;
    rasterization_state.depthBiasClamp = depth_bias.clamp;
    rasterization_state.depthBiasSlopeFactor = depth_bias.slope_factor;
  } else {
    rasterization_state.depthBiasEnable = false;
    rasterization_state.depthBiasConstantFactor = 0.0f;
    rasterization_state.depthBiasClamp = 0.0f;
    rasterization_state.depthBiasSlopeFactor = 0.0f;
  }

  auto multisample_state = VkPipelineMultisampleStateCreateInfo{};
  multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample_state.sampleShadingEnable = false;
  multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  auto color_blend_attachment_disabled = VkPipelineColorBlendAttachmentState{};
  color_blend_attachment_disabled.blendEnable = false;
  color_blend_attachment_disabled.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment_disabled.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment_disabled.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment_disabled.colorBlendOp = VK_BLEND_OP_ADD;
  color_blend_attachment_disabled.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment_disabled.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment_disabled.alphaBlendOp = VK_BLEND_OP_ADD;

  auto color_blend_attachments = std::vector<VkPipelineColorBlendAttachmentState>{};
  color_blend_attachments.reserve(attachments.size());

  if (!definition.uses_transparency) {
    const auto color_attachments = std::ranges::count_if(attachments, [](const auto& attachment) {
      return attachment.image_type != attachment::type::depth;
    });

    std::fill_n(std::back_inserter(color_blend_attachments), color_attachments, color_blend_attachment_disabled);
  } else {
    const auto filer = std::views::filter([](const auto& attachment) { return attachment.image_type != attachment::type::depth; });

    for (const auto& attachment : attachments | filer) {
      if (attachment.format == graphics::format::r32_uint || attachment.format == graphics::format::r64_uint || attachment.format == graphics::format::r32g32_uint) {
        color_blend_attachments.push_back(color_blend_attachment_disabled);
      } else {
        const auto& blend_state = attachment.blend_state;

        auto color_blend_attachment = VkPipelineColorBlendAttachmentState{};
        color_blend_attachment.blendEnable = true;
        color_blend_attachment.colorWriteMask = to_vk_enum<VkColorComponentFlags>(blend_state.color_write_mask);
        color_blend_attachment.srcColorBlendFactor = to_vk_enum<VkBlendFactor>(blend_state.color_source);
        color_blend_attachment.dstColorBlendFactor = to_vk_enum<VkBlendFactor>(blend_state.color_destination);
        color_blend_attachment.colorBlendOp = to_vk_enum<VkBlendOp>(blend_state.color_operation);
        color_blend_attachment.srcAlphaBlendFactor = to_vk_enum<VkBlendFactor>(blend_state.alpha_source);
        color_blend_attachment.dstAlphaBlendFactor = to_vk_enum<VkBlendFactor>(blend_state.alpha_destination);
        color_blend_attachment.alphaBlendOp = to_vk_enum<VkBlendOp>(blend_state.alpha_operation);

        color_blend_attachments.push_back(color_blend_attachment);
      }
    }
  }


  auto color_blend_state = VkPipelineColorBlendStateCreateInfo{};
  color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend_state.logicOpEnable = false;
  color_blend_state.logicOp = VK_LOGIC_OP_COPY;
  color_blend_state.attachmentCount = static_cast<std::uint32_t>(color_blend_attachments.size());
  color_blend_state.pAttachments = color_blend_attachments.data();
  color_blend_state.blendConstants[0] = 0.0f;
  color_blend_state.blendConstants[1] = 0.0f;
  color_blend_state.blendConstants[2] = 0.0f;
  color_blend_state.blendConstants[3] = 0.0f;

  _rendering_info.color_formats.reserve(attachments.size());

  _rendering_info.depth_format = VK_FORMAT_UNDEFINED;
  _rendering_info.stencil_format = VK_FORMAT_UNDEFINED;

  for (const auto& attachment : attachments) {
    if (attachment.image_type == attachment::type::depth) {
      _rendering_info.depth_format = depth_image::format();
      _rendering_info.stencil_format = depth_image::format();
    } else {
      _rendering_info.color_formats.push_back(to_vk_enum<VkFormat>(attachment.format));
    }
  }

  auto dynamic_states = std::array<VkDynamicState, 2u>{
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  auto dynamic_state = VkPipelineDynamicStateCreateInfo{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
  dynamic_state.pDynamicStates = dynamic_states.data();

  auto depth_stencil_state = VkPipelineDepthStencilStateCreateInfo{};
  depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_state.depthBoundsTestEnable = false;
  depth_stencil_state.stencilTestEnable = false;

  switch (definition.depth) {
    case depth::disabled: {
      depth_stencil_state.depthTestEnable = false;
      depth_stencil_state.depthWriteEnable = false;
      depth_stencil_state.depthCompareOp = VK_COMPARE_OP_ALWAYS;
      break;
    }
    case depth::read_write: {
      depth_stencil_state.depthTestEnable = true;
      depth_stencil_state.depthWriteEnable = true;
      depth_stencil_state.depthCompareOp = to_vk_enum<VkCompareOp>(definition.compare_operation);
      break;
    }
    case depth::read_only: {
      depth_stencil_state.depthTestEnable = true;
      depth_stencil_state.depthWriteEnable = false;
      depth_stencil_state.depthCompareOp = to_vk_enum<VkCompareOp>(definition.compare_operation);
      break;
    }
    default: {
      throw std::runtime_error{"Unsupported depth test type"};
    }
  }

  const auto [binding_descriptions, attribute_descriptions] = definition.vertex_input;

  auto vertex_input_state = VkPipelineVertexInputStateCreateInfo{};
  vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_state.vertexBindingDescriptionCount = static_cast<std::uint32_t>(binding_descriptions.size());
  vertex_input_state.pVertexBindingDescriptions = (vertex_input_state.vertexBindingDescriptionCount > 0u) ? binding_descriptions.data() : nullptr;
  vertex_input_state.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attribute_descriptions.size());
  vertex_input_state.pVertexAttributeDescriptions = (vertex_input_state.vertexAttributeDescriptionCount > 0u) ? attribute_descriptions.data() : nullptr;

  auto input_assembly_state = VkPipelineInputAssemblyStateCreateInfo{};
  input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_state.topology = to_vk_enum<VkPrimitiveTopology>(definition.primitive_topology);
  input_assembly_state.primitiveRestartEnable = false;

  for (auto&& [set, descriptor_set_layout_binding] : ranges::views::enumerate(descriptor_set_layout_bindings)) {
    auto binding_flags = std::vector<VkDescriptorBindingFlags>{};

    for (const auto& [id, binding] : descriptor_set_layout_binding) {
      binding_flags.push_back((binding.descriptorCount > 1u) ? (VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT) : 0u);
    }

    const auto bindings = utility::map_to<std::vector>(descriptor_set_layout_binding, [](const auto& entry) -> VkDescriptorSetLayoutBinding { return entry.second; });

    auto descriptor_set_layout_binding_flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{};
    descriptor_set_layout_binding_flags_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    descriptor_set_layout_binding_flags_create_info.bindingCount = static_cast<std::uint32_t>(binding_flags.size());
    descriptor_set_layout_binding_flags_create_info.pBindingFlags = binding_flags.data();

    auto descriptor_set_layout_create_info = VkDescriptorSetLayoutCreateInfo{};
    descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.pNext = &descriptor_set_layout_binding_flags_create_info;
    descriptor_set_layout_create_info.bindingCount = static_cast<std::uint32_t>(bindings.size());
    descriptor_set_layout_create_info.pBindings = bindings.data();

    validate(vkCreateDescriptorSetLayout(logical_device, &descriptor_set_layout_create_info, nullptr, &_set_data[set].layout));
  }

  // [NOTE] KAJ 2023-09-13 : Workaround
  auto descriptor_pool_sizes = std::vector<VkDescriptorPoolSize>{
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 2048},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2048},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2048},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2048},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 2048},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 2048},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2048},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2048}
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

  _rendering_info.info = VkPipelineRenderingCreateInfo{};
  _rendering_info.info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  _rendering_info.info.colorAttachmentCount = static_cast<std::uint32_t>(_rendering_info.color_formats.size());
  _rendering_info.info.pColorAttachmentFormats = _rendering_info.color_formats.data();
  _rendering_info.info.depthAttachmentFormat = _rendering_info.depth_format;
  _rendering_info.info.stencilAttachmentFormat = _rendering_info.stencil_format;

  auto pipeline_create_info = VkGraphicsPipelineCreateInfo{};
  pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_create_info.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
  pipeline_create_info.pNext = &_rendering_info.info;
  pipeline_create_info.stageCount = static_cast<std::uint32_t>(shader_stages.size());
  pipeline_create_info.pStages = shader_stages.data();
  pipeline_create_info.pVertexInputState = &vertex_input_state;
  pipeline_create_info.pInputAssemblyState = &input_assembly_state;
  pipeline_create_info.pViewportState = &viewport_state;
  pipeline_create_info.pRasterizationState = &rasterization_state;
  pipeline_create_info.pMultisampleState = &multisample_state;
  pipeline_create_info.pDepthStencilState = &depth_stencil_state;
  pipeline_create_info.pColorBlendState = &color_blend_state;
  pipeline_create_info.pDynamicState = &dynamic_state;
  pipeline_create_info.layout = _layout;
  // pipeline_create_info.renderPass = nullptr;
  // pipeline_create_info.subpass = stage.subpass;
  pipeline_create_info.basePipelineIndex = -1;
  pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

  validate(vkCreateGraphicsPipelines(logical_device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &_handle));

  utility::logger<"graphics">::debug("Pipeline '{}' created in {:.2f}ms", _name, units::quantity_cast<units::millisecond>(timer.elapsed()));
}

graphics_pipeline::~graphics_pipeline() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  _shaders.clear();

  logical_device.wait_idle();

  vkDestroyDescriptorPool(logical_device, _descriptor_pool, nullptr);
  
  for (const auto& set_data : _set_data) {
    vkDestroyDescriptorSetLayout(logical_device, set_data.layout, nullptr);
  }

  vkDestroyPipelineLayout(logical_device, _layout, nullptr);

  vkDestroyPipeline(logical_device, _handle, nullptr);
}

auto graphics_pipeline::handle() const noexcept -> VkPipeline {
  return _handle;
}

auto graphics_pipeline::descriptor_set_layout(std::uint32_t set) const noexcept -> VkDescriptorSetLayout {
  return _set_data[set].layout;
}

auto graphics_pipeline::descriptor_pool() const noexcept -> VkDescriptorPool {
  return _descriptor_pool;
}

auto graphics_pipeline::layout() const noexcept -> VkPipelineLayout {
  return _layout;
}

auto graphics_pipeline::bind_point() const noexcept -> VkPipelineBindPoint {
  return _bind_point;
}

auto graphics_pipeline::_update_definition(const std::filesystem::path& path, const pipeline_definition default_definition) -> pipeline_definition {
  if (!std::filesystem::exists(path / "definition.json")) {
    return default_definition;
  }

  auto file = std::ifstream{path / "definition.json"};

  if (!file.is_open()) {
    return default_definition;
  }

  auto definition = nlohmann::json::parse(file);

  auto result = default_definition;

  if (definition.contains("depth")) {
    auto depth = utility::from_string<graphics::depth>(definition["depth"].get<std::string>());

    if (depth) {
      result.depth = *depth;
    } else {
      utility::logger<"graphics">::warn("Could not parse 'sbx::graphics::depth' value '{}'", definition["depth"].get<std::string>());
    }
  }

  if (definition.contains("uses_transparency")) {
    result.uses_transparency = definition["uses_transparency"].get<bool>();
  }

  if (definition.contains("rasterization_state")) {
    auto rasterization_state = definition["rasterization_state"];

    if (rasterization_state.contains("polygon_mode")) {
      auto polygon_mode = utility::from_string<graphics::polygon_mode>(rasterization_state["polygon_mode"].get<std::string>());

      if (polygon_mode) {
        result.rasterization_state.polygon_mode = *polygon_mode;
      } else {
        utility::logger<"graphics">::warn("Could not parse 'sbx::graphics::polygon_mode' value '{}'", rasterization_state["polygon_mode"].get<std::string>());
      }
    }

    if (rasterization_state.contains("cull_mode")) {
      auto cull_mode = utility::from_string<graphics::cull_mode>(rasterization_state["cull_mode"].get<std::string>());

      if (cull_mode) {
        result.rasterization_state.cull_mode = *cull_mode;
      } else {
        utility::logger<"graphics">::warn("Could not parse 'sbx::graphics::cull_mode' value '{}'", rasterization_state["cull_mode"].get<std::string>());
      }
    }

    if (rasterization_state.contains("front_face")) {
      auto front_face = utility::from_string<graphics::front_face>(rasterization_state["front_face"].get<std::string>());

      if (front_face) {
        result.rasterization_state.front_face = *front_face;
      } else {
        utility::logger<"graphics">::warn("Could not parse 'sbx::graphics::front_face' value '{}'", rasterization_state["front_face"].get<std::string>());
      }
    }

    // if (rasterization_state.contains("depth_bias")) {
    //   auto depth_bias = rasterization_state["depth_bias"];

    //   if (depth_bias.contains("constant_factor")) {
    //     result.rasterization_state.depth_bias->constant_factor = depth_bias["constant_factor"].get<std::float_t>();
    //   }

    //   if (depth_bias.contains("clamp")) {
    //     result.rasterization_state.depth_bias->clamp = depth_bias["clamp"].get<std::float_t>();
    //   }

    //   if (depth_bias.contains("slope_factor")) {
    //     result.rasterization_state.depth_bias->slope_factor = depth_bias["slope_factor"].get<std::float_t>();
    //   }
    // }

    if (rasterization_state.contains("line_width")) {
      result.rasterization_state.line_width = rasterization_state["line_width"].get<std::float_t>();
    }
  }

  return result;
}

auto graphics_pipeline::_get_stage_from_name(const std::string& name) const noexcept -> VkShaderStageFlagBits {
  if (name == "vertex") {
    return VK_SHADER_STAGE_VERTEX_BIT;
  } else if (name == "fragment") {
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  } else if (name == "tesscontrol") {
    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
  } else if (name == "tesseval") {
    return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
  } else if (name == "compute") {
    return VK_SHADER_STAGE_COMPUTE_BIT;
  }
  
  return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}

} // namespace sbx::graphics
