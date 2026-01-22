// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_HPP_
#define LIBSBX_GRAPHICS_HPP_

#include <libsbx/graphics/version.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/renderer.hpp>
#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/task.hpp>

#include <libsbx/graphics/devices/extensions.hpp>
#include <libsbx/graphics/devices/validation_layers.hpp>
#include <libsbx/graphics/devices/instance.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>

#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/pipeline.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/pipeline/vertex_input_description.hpp>
#include <libsbx/graphics/pipeline/mesh.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/graphics/commands/command_pool.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/descriptor/descriptor.hpp>
#include <libsbx/graphics/descriptor/descriptor_set.hpp>
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/graphics/render_pass/swapchain.hpp>

#include <libsbx/graphics/buffers/buffer.hpp>
#include <libsbx/graphics/buffers/uniform_buffer.hpp>
#include <libsbx/graphics/buffers/uniform_handler.hpp>
#include <libsbx/graphics/buffers/storage_buffer.hpp>
#include <libsbx/graphics/buffers/storage_handler.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>

#include <libsbx/graphics/images/image.hpp>
#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/images/depth_image.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>
#include <libsbx/graphics/images/separate_image2d_array.hpp>

#endif // LIBSBX_GRAPHICS_HPP_
