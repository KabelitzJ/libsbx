// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

#include <array>
#include <cstring>
#include <fstream>
#include <string_view>

#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/hash.hpp>

namespace sbx::graphics {

auto _read_file(const std::filesystem::path& path) -> std::string {
  auto file = std::ifstream{path, std::ios::binary | std::ios::ate};

  if (!file) {
    throw utility::runtime_error{"Could not open shader file '{}'", path.string()};
  }

  const auto size = static_cast<std::streamsize>(file.tellg());

  file.seekg(0, std::ios::beg);

  auto buffer = std::string{};
  buffer.resize(static_cast<std::size_t>(size));

  file.read(buffer.data(), size);

  return buffer;
}

shader_compiler::shader_compiler() {
  if (SLANG_FAILED(slang::createGlobalSession(_global_session.writeRef()))) {
    throw utility::runtime_error{"Failed to create slang global session"};
  }
}

shader_compiler::~shader_compiler() {

}

// Content hash over everything that affects the emitted SPIR-V: the source itself, the requested
// entry points, the fixed compiler options below and the target profile, plus the Slang toolchain's
// own build tag so a Slang version bump invalidates every cache entry automatically. Deliberately
// not mtime-based — that isn't stable across a fresh checkout or a different machine.
auto shader_compiler::_cache_key(const std::string& source, std::span<const entry_point_request> entry_points, std::span<const slang::CompilerOptionEntry> options, const char* profile) const -> std::string {
  auto buffer = std::string{};
  buffer.reserve(source.size() + 256u);

  buffer.append(source);
  buffer.push_back('\0');

  for (const auto& entry_point : entry_points) {
    buffer.append(entry_point.name);
    buffer.push_back('\0');

    const auto stage = static_cast<std::uint32_t>(entry_point.stage);
    buffer.append(reinterpret_cast<const char*>(&stage), sizeof(stage));
  }

  for (const auto& option : options) {
    const auto name = static_cast<std::uint32_t>(option.name);
    const auto kind = static_cast<std::uint32_t>(option.value.kind);
    const auto value0 = option.value.intValue0;
    const auto value1 = option.value.intValue1;

    buffer.append(reinterpret_cast<const char*>(&name), sizeof(name));
    buffer.append(reinterpret_cast<const char*>(&kind), sizeof(kind));
    buffer.append(reinterpret_cast<const char*>(&value0), sizeof(value0));
    buffer.append(reinterpret_cast<const char*>(&value1), sizeof(value1));
  }

  buffer.append(profile);
  buffer.push_back('\0');
  buffer.append(_global_session->getBuildTagString());

  const auto hash = utility::fnv1a_hash<char>{}(std::string_view{buffer});

  return fmt::format("{:016x}", hash);
}

auto shader_compiler::compile(const std::filesystem::path& path, std::span<const entry_point_request> entry_points) -> std::vector<compiled_entry_point> {
  const auto source = _read_file(path);
  const auto parent = path.parent_path().string();

  auto target = slang::TargetDesc{};
  target.format = SLANG_SPIRV;
  target.profile = _global_session->findProfile("spirv_1_5");

  const auto options = std::array<slang::CompilerOptionEntry, 3u>{
    slang::CompilerOptionEntry{slang::CompilerOptionName::MatrixLayoutColumn, {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}},
    slang::CompilerOptionEntry{slang::CompilerOptionName::EmitSpirvDirectly, {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}},
    slang::CompilerOptionEntry{slang::CompilerOptionName::VulkanUseEntryPointName, {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}}
  };

  const auto key = _cache_key(source, entry_points, options, "spirv_1_5");

  if (auto cached = _disk_cache.try_load(key)) {
    auto results = std::vector<compiled_entry_point>{};
    results.reserve(cached->size());

    for (auto& entry : *cached) {
      results.push_back(compiled_entry_point{entry.stage, std::move(entry.name), std::move(entry.spirv)});
    }

    utility::logger<"graphics">::debug("Shader cache hit for '{}'", path.generic_string());

    return results;
  }

  utility::logger<"graphics">::debug("Shader cache miss for '{}', compiling", path.generic_string());

  const auto search_paths = std::array<const char*, 1u>{parent.c_str()};

  auto session_description = slang::SessionDesc{};
  session_description.targets = &target;
  session_description.targetCount = 1u;
  session_description.searchPaths = search_paths.data();
  session_description.searchPathCount = static_cast<SlangInt>(search_paths.size());
  session_description.compilerOptionEntries = const_cast<slang::CompilerOptionEntry*>(options.data());
  session_description.compilerOptionEntryCount = static_cast<std::uint32_t>(options.size());

  auto session = Slang::ComPtr<slang::ISession>{};

  if (SLANG_FAILED(_global_session->createSession(session_description, session.writeRef()))) {
    throw utility::runtime_error{"Failed to create slang session for '{}'", path.string()};
  }

  auto diagnostics = Slang::ComPtr<ISlangBlob>{};

  auto* module = session->loadModuleFromSourceString(path.stem().string().c_str(), path.string().c_str(), source.c_str(), diagnostics.writeRef());

  if (diagnostics && diagnostics->getBufferSize() > 1u) {
    utility::logger<"graphics">::warn("Slang diagnostics for '{}':\n{}", path.string(), static_cast<const char*>(diagnostics->getBufferPointer()));
  }

  if (module == nullptr) {
    throw utility::runtime_error{"Failed to load shader module '{}'", path.string()};
  }

  auto results = std::vector<compiled_entry_point>{};
  results.reserve(entry_points.size());

  for (const auto& request : entry_points) {
    auto entry_point = Slang::ComPtr<slang::IEntryPoint>{};

    if (SLANG_FAILED(module->findEntryPointByName(request.name.c_str(), entry_point.writeRef())) || !entry_point) {
      throw utility::runtime_error{"Entry point '{}' not found in '{}'", request.name, path.string()};
    }

    const auto components = std::array<slang::IComponentType*, 2u>{module, entry_point};

    auto program = Slang::ComPtr<slang::IComponentType>{};
    auto link_diagnostics = Slang::ComPtr<ISlangBlob>{};

    if (SLANG_FAILED(session->createCompositeComponentType(components.data(), static_cast<SlangInt>(components.size()), program.writeRef(), link_diagnostics.writeRef())) || !program) {
      if (link_diagnostics && link_diagnostics->getBufferSize() > 1u) {
        utility::logger<"graphics">::error("Slang link error for '{}':\n{}", path.string(), static_cast<const char*>(link_diagnostics->getBufferPointer()));
      }

      throw utility::runtime_error{"Failed to link entry point '{}' in '{}'", request.name, path.string()};
    }

    auto code = Slang::ComPtr<ISlangBlob>{};
    auto code_diagnostics = Slang::ComPtr<ISlangBlob>{};

    if (SLANG_FAILED(program->getEntryPointCode(0, 0, code.writeRef(), code_diagnostics.writeRef())) || !code) {
      if (code_diagnostics && code_diagnostics->getBufferSize() > 1u) {
        utility::logger<"graphics">::error("Slang codegen error for '{}':\n{}", path.string(), static_cast<const char*>(code_diagnostics->getBufferPointer()));
      }

      throw utility::runtime_error{"Failed to generate SPIR-V for entry point '{}' in '{}'", request.name, path.string()};
    }

    const auto byte_size = code->getBufferSize();

    if (byte_size % sizeof(std::uint32_t) != 0u) {
      throw utility::runtime_error{"SPIR-V for '{}' is not 4-byte aligned", request.name};
    }

    auto spirv = std::vector<std::uint32_t>(byte_size / sizeof(std::uint32_t));
    std::memcpy(spirv.data(), code->getBufferPointer(), byte_size);

    results.push_back(compiled_entry_point{request.stage, request.name, std::move(spirv)});
  }

  auto to_store = std::vector<shader_binary_entry>{};
  to_store.reserve(results.size());

  for (const auto& entry : results) {
    to_store.push_back(shader_binary_entry{entry.stage, entry.name, entry.spirv});
  }

  _disk_cache.store(key, to_store);

  return results;
}

} // namespace sbx::graphics
