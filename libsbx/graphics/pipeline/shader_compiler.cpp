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

#include <libsbx/filesystem/filesystem_module.hpp>

namespace sbx::graphics {

/**
 * @brief The nearest ancestor of @p path named "shaders", or its parent directory if none is found.
 *
 * Every shader request is written as "engine://shaders/<category>/<file>.slang" and resolved
 * against the engine data directory by shader_cache::get() before reaching here, so @p path
 * always has a "shaders" ancestor -- this is always the shader tree's root.
 */
auto _shaders_root(const std::filesystem::path& path) -> std::filesystem::path {
  for (auto directory = path.parent_path(); !directory.empty(); directory = directory.parent_path()) {
    if (directory.filename() == "shaders") {
      return directory;
    }

    if (directory == directory.parent_path()) {
      break;
    }
  }

  return path.parent_path();
}

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

auto _default_options() -> std::array<slang::CompilerOptionEntry, 3u> {
  return std::array<slang::CompilerOptionEntry, 3u>{
    slang::CompilerOptionEntry{slang::CompilerOptionName::MatrixLayoutColumn, {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}},
    slang::CompilerOptionEntry{slang::CompilerOptionName::EmitSpirvDirectly, {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}},
    slang::CompilerOptionEntry{slang::CompilerOptionName::VulkanUseEntryPointName, {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}}
  };
}

shader_compiler::shader_compiler() {
  if (SLANG_FAILED(slang::createGlobalSession(_global_session.writeRef()))) {
    throw utility::runtime_error{"Failed to create slang global session"};
  }
}

shader_compiler::~shader_compiler() {

}

// Content hash over every dependency file, entry points, compiler options, target profile, and the
// Slang build tag (so a version bump invalidates the cache). Not mtime-based — unstable across a
// fresh checkout or a different machine — and hashes every dependency, not just the entry file, so
// editing a shared header changes every including shader's key too.
auto shader_compiler::_cache_key(std::span<const std::string> dependencies, std::span<const entry_point_request> entry_points, std::span<const slang::CompilerOptionEntry> options, const char* profile) const -> std::string {
  auto buffer = std::string{};
  buffer.reserve(256u);

  for (const auto& dependency : dependencies) {
    buffer.append(dependency);
    buffer.push_back('\0');
  }

  for (const auto& entry_point : entry_points) {
    buffer.append(entry_point.name);
    buffer.push_back('\0');

    const auto stage = static_cast<std::uint32_t>(entry_point.stage);
    buffer.append(reinterpret_cast<const char*>(&stage), sizeof(stage));

    if (entry_point.specialization) {
      buffer.append(*entry_point.specialization);
    }
    buffer.push_back('\0');
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

auto shader_compiler::_create_session(const std::filesystem::path& path, std::span<const slang::CompilerOptionEntry> options) -> Slang::ComPtr<slang::ISession> {
  const auto parent = path.parent_path().string();
  const auto root = _shaders_root(path).string();

  // _shaders_root only walks up from `path` itself, so a project-authored shader (e.g. under a
  // game's assets/shaders/) never sees the engine's own shaders/ directory this way -- its
  // nearest ancestor literally named "shaders" is its own tree, not the engine's. Since every
  // shader, engine or project, needs `#include <descriptors.slang>` and friends, the engine's
  // shaders root is always added too, regardless of where `path` lives.
  const auto engine_root = (filesystem::engine_data_directory() / "shaders").string();

  auto target = slang::TargetDesc{};
  target.format = SLANG_SPIRV;
  target.profile = _global_session->findProfile("spirv_1_5");

  auto search_paths = std::vector<const char*>{parent.c_str()};

  if (root != parent) {
    search_paths.push_back(root.c_str());
  }

  if (engine_root != parent && engine_root != root) {
    search_paths.push_back(engine_root.c_str());
  }

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

  return session;
}

auto shader_compiler::_load_module(slang::ISession& session, const std::filesystem::path& path) -> slang::IModule* {
  const auto source = _read_file(path);

  auto diagnostics = Slang::ComPtr<ISlangBlob>{};

  auto* module = session.loadModuleFromSourceString(path.stem().string().c_str(), path.string().c_str(), source.c_str(), diagnostics.writeRef());

  if (diagnostics && diagnostics->getBufferSize() > 1u) {
    utility::logger<"graphics">::warn("Slang diagnostics for '{}':\n{}", path.string(), static_cast<const char*>(diagnostics->getBufferPointer()));
  }

  if (module == nullptr) {
    throw utility::runtime_error{"Failed to load shader module '{}'", path.string()};
  }

  return module;
}

auto shader_compiler::reflect_push_constants(const std::filesystem::path& path, const std::string& entry_point_name) -> push_constant_layout {
  using kind = push_constant_field::kind;

  auto session = _create_session(path, _default_options());
  auto* module = _load_module(*session, path);

  auto entry_point = Slang::ComPtr<slang::IEntryPoint>{};

  if (SLANG_FAILED(module->findEntryPointByName(entry_point_name.c_str(), entry_point.writeRef())) || !entry_point) {
    throw utility::runtime_error{"Entry point '{}' not found in '{}'", entry_point_name, path.string()};
  }

  const auto components = std::array<slang::IComponentType*, 2u>{module, entry_point.get()};

  auto program = Slang::ComPtr<slang::IComponentType>{};

  if (SLANG_FAILED(session->createCompositeComponentType(components.data(), static_cast<SlangInt>(components.size()), program.writeRef(), nullptr)) || !program) {
    throw utility::runtime_error{"Failed to compose '{}' for reflection", path.string()};
  }

  auto* program_layout = program->getLayout();

  if (program_layout == nullptr) {
    throw utility::runtime_error{"No reflection layout for '{}'", path.string()};
  }

  auto result = push_constant_layout{};

  for (auto index = 0u; index < program_layout->getParameterCount(); ++index) {
    auto* parameter = program_layout->getParameterByIndex(index);

    if (parameter->getCategory() != slang::ParameterCategory::PushConstantBuffer) {
      continue;
    }

    auto* struct_layout = parameter->getTypeLayout()->getElementTypeLayout();

    result.size = static_cast<std::uint32_t>(struct_layout->getSize());

    for (auto field_index = 0u; field_index < struct_layout->getFieldCount(); ++field_index) {
      auto* field = struct_layout->getFieldByIndex(field_index);
      auto* field_layout = field->getTypeLayout();
      auto* field_type = field_layout->getType();

      const auto name = std::string{field->getName()};

      const auto unsupported = [&]() {
        return utility::runtime_error{"Push constant field '{}' in '{}' has unsupported type '{}'", name, path.string(), field_type->getName() ? field_type->getName() : "?"};
      };

      auto entry = push_constant_field{
        .name = name,
        .offset = static_cast<std::uint32_t>(field->getOffset()),
        .size = static_cast<std::uint32_t>(field_layout->getSize()),
        .type = kind::f32
      };

      switch (field_layout->getKind()) {
        case slang::TypeReflection::Kind::Scalar: {
          switch (field_type->getScalarType()) {
            case slang::TypeReflection::Float32: entry.type = kind::f32; break;
            case slang::TypeReflection::Int32: entry.type = kind::i32; break;
            case slang::TypeReflection::UInt32: entry.type = kind::u32; break;
            default: throw unsupported();
          }
          break;
        }
        case slang::TypeReflection::Kind::Vector: {
          if (field_type->getElementType()->getScalarType() != slang::TypeReflection::Float32) {
            throw unsupported();
          }

          switch (field_type->getElementCount()) {
            case 2u: entry.type = kind::f32x2; break;
            case 3u: entry.type = kind::f32x3; break;
            case 4u: entry.type = kind::f32x4; break;
            default: throw unsupported();
          }
          break;
        }
        case slang::TypeReflection::Kind::Pointer: {
          auto* pointee_layout = field_layout->getElementTypeLayout();

          if (pointee_layout == nullptr || pointee_layout->getStride() == 0u) {
            throw utility::runtime_error{"Push constant field '{}' in '{}': no layout for the pointee type", name, path.string()};
          }

          entry.type = kind::buffer;
          entry.element_stride = static_cast<std::uint32_t>(pointee_layout->getStride());
          break;
        }
        case slang::TypeReflection::Kind::Struct: {
          const auto type_name = std::string_view{field_type->getName()};

          if (type_name == "sampled_texture") {
            entry.type = kind::sampled_texture;
          } else if (type_name == "storage_texture") {
            entry.type = kind::storage_texture;
          } else if (type_name == "sampler_handle") {
            entry.type = kind::sampler;
          } else {
            throw unsupported();
          }
          break;
        }
        default: {
          throw unsupported();
        }
      }

      result.fields.push_back(std::move(entry));
    }

    break;
  }

  return result;
}

auto shader_compiler::compile(const std::filesystem::path& path, std::span<const entry_point_request> entry_points) -> std::vector<compiled_entry_point> {
  const auto options = _default_options();

  auto session = _create_session(path, options);
  auto* module = _load_module(*session, path);

  // Every file the module actually parsed — the entry file itself plus every #include it
  // transitively pulled in — so the cache key below tracks the full dependency set, not just the
  // one file named in `path`.
  auto dependencies = std::vector<std::string>{};
  dependencies.reserve(static_cast<std::size_t>(module->getDependencyFileCount()));

  for (auto index = SlangInt32{0}; index < module->getDependencyFileCount(); ++index) {
    const auto* dependency_path = module->getDependencyFilePath(index);

    if (dependency_path == nullptr) {
      continue;
    }

    auto blob = std::string{dependency_path};
    blob.push_back('\0');
    blob.append(_read_file(dependency_path));

    dependencies.push_back(std::move(blob));
  }

  const auto key = _cache_key(dependencies, entry_points, options, "spirv_1_5");

  if (auto cached = _disk_cache.try_load(key, path)) {
    auto results = std::vector<compiled_entry_point>{};
    results.reserve(cached->size());

    for (auto& entry : *cached) {
      results.push_back(compiled_entry_point{entry.stage, std::move(entry.name), std::move(entry.spirv)});
    }

    utility::logger<"graphics">::debug("Shader cache hit for '{}'", path.generic_string());

    return results;
  }

  utility::logger<"graphics">::debug("Shader cache miss for '{}', compiling", path.generic_string());

  auto results = std::vector<compiled_entry_point>{};
  results.reserve(entry_points.size());

  for (const auto& request : entry_points) {
    auto entry_point = Slang::ComPtr<slang::IEntryPoint>{};

    if (SLANG_FAILED(module->findEntryPointByName(request.name.c_str(), entry_point.writeRef())) || !entry_point) {
      throw utility::runtime_error{"Entry point '{}' not found in '{}'", request.name, path.string()};
    }

    auto component = Slang::ComPtr<slang::IComponentType>{entry_point};

    if (request.specialization) {
      auto* type = module->getLayout()->findTypeByName(request.specialization->c_str());

      if (type == nullptr) {
        throw utility::runtime_error{"Specialization type '{}' not found for entry point '{}' in '{}'", *request.specialization, request.name, path.string()};
      }

      const auto specialization_args = std::array{slang::SpecializationArg::fromType(type)};

      auto specialized_entry_point = Slang::ComPtr<slang::IComponentType>{};
      auto specialization_diagnostics = Slang::ComPtr<ISlangBlob>{};

      if (SLANG_FAILED(entry_point->specialize(specialization_args.data(), static_cast<SlangInt>(specialization_args.size()), specialized_entry_point.writeRef(), specialization_diagnostics.writeRef())) || !specialized_entry_point) {
        if (specialization_diagnostics && specialization_diagnostics->getBufferSize() > 1u) {
          utility::logger<"graphics">::error("Slang specialization error for '{}':\n{}", path.string(), static_cast<const char*>(specialization_diagnostics->getBufferPointer()));
        }

        throw utility::runtime_error{"Failed to specialize entry point '{}' with '{}' in '{}'", request.name, *request.specialization, path.string()};
      }

      component = specialized_entry_point;
    }

    const auto components = std::array<slang::IComponentType*, 2u>{module, component};

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

  _disk_cache.store(key, path, to_store);

  return results;
}

} // namespace sbx::graphics
