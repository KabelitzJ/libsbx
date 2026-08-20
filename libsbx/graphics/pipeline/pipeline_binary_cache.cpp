// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/pipeline/pipeline_binary_cache.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vector>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

auto cache_file() -> std::filesystem::path {
  return core::engine::project().library_directory() / "pipeline_cache.bin";
}

pipeline_binary_cache::pipeline_binary_cache(const graphics::logical_device& logical_device) {
  auto initial_data = std::vector<std::byte>{};

  if (auto in = std::ifstream{cache_file(), std::ios::binary}; in) {
    in.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);

    initial_data.resize(size);
    in.read(reinterpret_cast<char*>(initial_data.data()), static_cast<std::streamsize>(size));

    if (!in) {
      initial_data.clear();
    }
  }

  auto create_info = VkPipelineCacheCreateInfo{};
  create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  create_info.initialDataSize = initial_data.size();
  create_info.pInitialData = initial_data.empty() ? nullptr : initial_data.data();

  validate(vkCreatePipelineCache(logical_device, &create_info, nullptr, &_handle), "vkCreatePipelineCache");

  logical_device.set_debug_name(_handle, "Global Pipeline Cache");
}

pipeline_binary_cache::~pipeline_binary_cache() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  auto size = std::size_t{0u};
  vkGetPipelineCacheData(logical_device, _handle, &size, nullptr);

  if (size > 0u) {
    auto data = std::vector<std::byte>(size);
    vkGetPipelineCacheData(logical_device, _handle, &size, data.data());

    auto error = std::error_code{};
    std::filesystem::create_directories(cache_file().parent_path(), error);

    if (auto out = std::ofstream{cache_file(), std::ios::binary}; out) {
      out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(size));
    } else {
      utility::logger<"graphics">::warn("Pipeline binary cache: could not write '{}'", cache_file().generic_string());
    }
  }

  vkDestroyPipelineCache(logical_device, _handle, nullptr);
}

} // namespace sbx::graphics
