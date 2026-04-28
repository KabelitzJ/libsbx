// SPDX-License-Identifier: MIT
#include <libsbx/graphics/pipeline/compiler.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/target.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <span>
#include <thread>

namespace sbx::graphics {

namespace {

inline constexpr auto cache_schema_version = std::uint32_t{1};
inline constexpr auto cache_magic = std::array<char, 4>{'S', 'B', 'X', 'S'};

auto hash_string(std::string_view value) -> std::uint64_t {
  return utility::fnv1a_hash<char>{}(value);
}

auto hash_file_content(const std::filesystem::path& path) -> std::optional<std::uint64_t> {
  auto file = std::ifstream{path, std::ios::binary | std::ios::ate};

  if (!file) {
    return std::nullopt;
  }

  const auto size = static_cast<std::streamsize>(file.tellg());

  file.seekg(0, std::ios::beg);

  auto hasher = utility::djb2_hash<std::uint64_t>{};

  if (size <= 0) {
    return hasher(std::span<const std::uint8_t>{});
  }

  auto buffer = std::vector<std::uint8_t>{};
  buffer.resize(static_cast<std::size_t>(size));

  file.read(reinterpret_cast<char*>(buffer.data()), size);

  if (!file) {
    return std::nullopt;
  }

  return hasher(std::span<const std::uint8_t>{buffer.data(), buffer.size()});
}

auto resolve_cache_directory() -> std::filesystem::path {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  return assets_module.asset_root() / ".shader_cache";
}

auto cache_path_for(const std::filesystem::path& cache_directory, std::uint64_t key) -> std::filesystem::path {
  return cache_directory / fmt::format("{:016x}.cache", key);
}

auto compute_stage_key(const compiler::compile_request& request, SlangStage stage, const char* stage_name, const char* stage_profile, std::string_view source, std::string_view slang_tag) -> std::uint64_t {
  auto meta = std::string{};
  meta.reserve(512u);

  meta.append("schema=").append(std::to_string(cache_schema_version)).push_back('\n');
  meta.append("debug=").append(utility::is_build_configuration_debug_v ? "1" : "0").push_back('\n');
  meta.append("slang=").append(slang_tag).push_back('\n');
  meta.append("stage=").append(stage_name).push_back('\n');
  meta.append("profile=").append(stage_profile).push_back('\n');
  meta.append("path=").append(request.path.generic_string()).push_back('\n');

  auto sorted_defines = request.defines;

  std::sort(sorted_defines.begin(), sorted_defines.end(), [](const auto& a, const auto& b) {
    return a.key < b.key;
  });

  for (const auto& define : sorted_defines) {
    meta.append("define=").append(define.key).push_back('=');
    meta.append(define.value).push_back('\n');
  }

  const auto& per_stage = request.per_stage.at(stage);

  meta.append("entry=").append(per_stage.entry_point).push_back('\n');

  for (const auto& specialization : per_stage.specializations) {
    meta.append("spec=").append(specialization).push_back('\n');
  }

  auto seed = std::size_t{0};
  utility::hash_combine(seed, hash_string(meta), hash_string(source));

  return static_cast<std::uint64_t>(seed);
}

auto try_load_cached(const std::filesystem::path& cache_path, std::vector<std::uint32_t>& out_code) -> bool {
  auto error = std::error_code{};

  if (!std::filesystem::exists(cache_path, error) || error) {
    return false;
  }

  try {
    auto file = std::ifstream{cache_path, std::ios::binary};

    if (!file) {
      return false;
    }

    auto magic = std::array<char, 4>{};

    file.read(magic.data(), magic.size());

    if (!file || magic != cache_magic) {
      return false;
    }

    auto format_version = std::uint32_t{};
    file.read(reinterpret_cast<char*>(&format_version), sizeof(format_version));

    if (!file || format_version != cache_schema_version) {
      return false;
    }

    auto word_count = std::uint32_t{};
    auto dependency_count = std::uint32_t{};

    file.read(reinterpret_cast<char*>(&word_count), sizeof(word_count));
    file.read(reinterpret_cast<char*>(&dependency_count), sizeof(dependency_count));

    if (!file) {
      return false;
    }

    auto code = std::vector<std::uint32_t>{};
    code.resize(word_count);

    file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(word_count * sizeof(std::uint32_t)));

    if (!file) {
      return false;
    }

    for (auto i = std::uint32_t{0}; i < dependency_count; ++i) {
      auto path_length = std::uint32_t{};
      file.read(reinterpret_cast<char*>(&path_length), sizeof(path_length));

      if (!file) {
        return false;
      }

      auto path_string = std::string{};
      path_string.resize(path_length);
      file.read(path_string.data(), static_cast<std::streamsize>(path_length));

      auto stored_hash = std::uint64_t{};
      file.read(reinterpret_cast<char*>(&stored_hash), sizeof(stored_hash));

      if (!file) {
        return false;
      }

      const auto current_hash = hash_file_content(std::filesystem::path{path_string});

      if (!current_hash || *current_hash != stored_hash) {
        return false;
      }
    }

    out_code = std::move(code);

    return true;
  } catch (const std::exception&) {
    return false;
  }
}

auto store_cached(const std::filesystem::path& cache_path, const std::vector<std::uint32_t>& code, const std::vector<std::filesystem::path>& dependencies) -> void {
  try {
    auto error = std::error_code{};

    std::filesystem::create_directories(cache_path.parent_path(), error);

    if (error) {
      utility::logger<"graphics">::warn("Failed to create shader cache directory '{}': {}", cache_path.parent_path().string(), error.message());
      return;
    }

    const auto thread_token = std::hash<std::thread::id>{}(std::this_thread::get_id());
    const auto temporary_path = std::filesystem::path{cache_path}.concat(fmt::format(".tmp.{:x}", thread_token));

    {
      auto file = std::ofstream{temporary_path, std::ios::binary | std::ios::trunc};

      if (!file) {
        utility::logger<"graphics">::warn("Failed to open shader cache file '{}' for writing.", temporary_path.string());
        return;
      }

      file.write(cache_magic.data(), static_cast<std::streamsize>(cache_magic.size()));

      const auto format_version = cache_schema_version;
      file.write(reinterpret_cast<const char*>(&format_version), sizeof(format_version));

      const auto word_count = static_cast<std::uint32_t>(code.size());
      file.write(reinterpret_cast<const char*>(&word_count), sizeof(word_count));

      const auto dependency_count = static_cast<std::uint32_t>(dependencies.size());
      file.write(reinterpret_cast<const char*>(&dependency_count), sizeof(dependency_count));

      file.write(reinterpret_cast<const char*>(code.data()), static_cast<std::streamsize>(code.size() * sizeof(std::uint32_t)));

      for (const auto& dependency : dependencies) {
        const auto dependency_string = dependency.generic_string();
        const auto path_length = static_cast<std::uint32_t>(dependency_string.size());

        file.write(reinterpret_cast<const char*>(&path_length), sizeof(path_length));
        file.write(dependency_string.data(), static_cast<std::streamsize>(dependency_string.size()));

        const auto content_hash = hash_file_content(dependency).value_or(std::uint64_t{0});
        file.write(reinterpret_cast<const char*>(&content_hash), sizeof(content_hash));
      }

      if (!file) {
        utility::logger<"graphics">::warn("Failed to write shader cache file '{}'.", temporary_path.string());
        return;
      }
    }

    std::filesystem::rename(temporary_path, cache_path, error);

    if (error) {
      utility::logger<"graphics">::warn("Failed to commit shader cache file '{}': {}", cache_path.string(), error.message());
      std::filesystem::remove(temporary_path, error);
    }
  } catch (const std::exception& e) {
    utility::logger<"graphics">::warn("Exception while writing shader cache '{}': {}", cache_path.string(), e.what());
  }
}

} // namespace

struct stage_info { 
  SlangStage stage; 
  const char* name; 
  const char* file;
  const char* profile;
}; // struct stage_info

static constexpr auto stage_infos = std::array<stage_info, SLANG_STAGE_COUNT>{
  stage_info{ SLANG_STAGE_NONE,           nullptr,         nullptr,               nullptr },
  stage_info{ SLANG_STAGE_VERTEX,         "vertex",        "vertex.slang",        "vs_6_0" },
  stage_info{ SLANG_STAGE_HULL,           "hull",          "hull.slang",          "hs_6_0" },
  stage_info{ SLANG_STAGE_DOMAIN,         "domain",        "domain.slang",        "ds_6_0" },
  stage_info{ SLANG_STAGE_GEOMETRY,       "geometry",      "geometry.slang",      "gs_6_0" },
  stage_info{ SLANG_STAGE_FRAGMENT,       "fragment",      "fragment.slang",      "ps_6_0" },
  stage_info{ SLANG_STAGE_COMPUTE,        "compute",       "compute.slang",       "cs_6_0" },
  stage_info{ SLANG_STAGE_RAY_GENERATION, "raygen",        "raygen.slang",        "rgs_6_0" },
  stage_info{ SLANG_STAGE_INTERSECTION,   "intersection",  "intersection.slang",  "is_6_0" },
  stage_info{ SLANG_STAGE_ANY_HIT,        "anyhit",        "anyhit.slang",        "ahs_6_0" },
  stage_info{ SLANG_STAGE_CLOSEST_HIT,    "closesthit",    "closesthit.slang",    "chs_6_0" },
  stage_info{ SLANG_STAGE_MISS,           "miss",          "miss.slang",          "ms_6_0" },
  stage_info{ SLANG_STAGE_CALLABLE,       "callable",      "callable.slang",      "cs_6_0" },
  stage_info{ SLANG_STAGE_MESH,           "mesh",          "mesh.slang",          "ms_6_0" },
  stage_info{ SLANG_STAGE_AMPLIFICATION,  "amplification", "amplification.slang", "as_6_0" },
  stage_info{ SLANG_STAGE_DISPATCH,       "dispatch",      "dispatch.slang",      "ds_6_0" }
};

compiler::compiler() {
  createGlobalSession(_global_session.writeRef());
}

compiler::~compiler() {

}

auto compiler::compile(const compile_request& compile_request) -> compile_result {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto result = compile_result{};

  const auto cache_directory = resolve_cache_directory();
  const auto slang_tag = std::string_view{_global_session->getBuildTagString()};

  for (const auto& [stage, name, file, profile] : stage_infos) {
    if (stage == SLANG_STAGE_NONE) {
      continue;
    }

    const auto file_path = assets_module.resolve_path(std::filesystem::path{compile_request.path}.append(file));

    if (!std::filesystem::exists(file_path)) {
      continue;
    }

    const auto source = _read_file(file_path);

    const auto cache_key = compute_stage_key(compile_request, stage, name, profile, source, slang_tag);
    const auto cache_path = cache_path_for(cache_directory, cache_key);

    if (try_load_cached(cache_path, result.code[stage])) {
      continue;
    }

    auto session = _create_session(compile_request, stage);

    auto& per_stage = compile_request.per_stage.at(stage);

    auto shader_module = Slang::ComPtr<slang::IModule>{};

    {
      auto diagnostics = Slang::ComPtr<ISlangBlob>{};

      shader_module = session->loadModuleFromSourceString(name, file_path.string().c_str(), source.data(), diagnostics.writeRef());

      if (diagnostics && diagnostics->getBufferSize() > 1) {
        utility::logger<"models">::warn("Slang diagnostics while loading '{}':\n{}", file_path.string(), static_cast<const char*>(diagnostics->getBufferPointer()));
      }

      if (!shader_module) {
        throw utility::runtime_error{"Failed to load shader_module '{}'.", file_path.string()};
      }
    }

    auto entry_point = Slang::ComPtr<slang::IEntryPoint>{};

    {
      shader_module->findEntryPointByName(per_stage.entry_point.c_str(), entry_point.writeRef());

      if (!entry_point) {
        auto diagnostics = Slang::ComPtr<ISlangBlob>{};

        shader_module->findAndCheckEntryPoint(per_stage.entry_point.c_str(), stage, entry_point.writeRef(), diagnostics.writeRef());

        if (diagnostics && diagnostics->getBufferSize() > 1) {
          utility::logger<"models">::warn("Slang entry-point check for '{}':\n{}", file_path.string(), static_cast<const char*>(diagnostics->getBufferPointer()));
        }

        if (!entry_point) {
          throw utility::runtime_error{"Entry point '{}' not found/valid in '{}'.", per_stage.entry_point, file_path.string()};
        }
      }
    }

    auto entry_for_link = Slang::ComPtr<slang::IComponentType>{entry_point};

    auto args = std::vector<slang::SpecializationArg>{};
    args.reserve(per_stage.specializations.size());

    for (const auto& type_name : per_stage.specializations) {
      auto* reflected_type = shader_module->getLayout()->findTypeByName(type_name.c_str());

      if (!reflected_type) {
        throw utility::runtime_error{"Specialization type '{}' not found in shader_module '{}' for stage '{}'.", type_name, file_path.string(), name};
      }

      args.push_back(slang::SpecializationArg{slang::SpecializationArg::Kind::Type, reflected_type});
    }

    auto specialized_entry_point = Slang::ComPtr<slang::IComponentType>{};

    {
      auto diagnostics = Slang::ComPtr<ISlangBlob>{};

      const auto result = entry_point->specialize(args.data(), (SlangInt)args.size(), specialized_entry_point.writeRef(), diagnostics.writeRef());

      if (diagnostics && diagnostics->getBufferSize() > 1) {
        utility::logger<"models">::warn("Slang specialization for '{}':\n{}", file_path.string(), static_cast<const char*>(diagnostics->getBufferPointer()));
      }

      if (SLANG_FAILED(result) || !specialized_entry_point) {
        throw utility::runtime_error{"Failed to specialize entry point in '{}'.", file_path.string()};
      }
    }

    entry_for_link = specialized_entry_point;

    auto program = Slang::ComPtr<slang::IComponentType>{};

    {
      auto parts = std::array<slang::IComponentType*, 2>{shader_module, entry_for_link};

      auto diagnostics = Slang::ComPtr<ISlangBlob>{};

      const auto result = session->createCompositeComponentType(parts.data(), (SlangInt)parts.size(), program.writeRef(), diagnostics.writeRef());

      if (diagnostics && diagnostics->getBufferSize() > 1) {
        utility::logger<"models">::warn("Slang link diagnostics for '{}':\n{}", file_path.string(), static_cast<const char*>(diagnostics->getBufferPointer()));
      }

      if (SLANG_FAILED(result) || !program) {
        throw utility::runtime_error{"Failed to link program for '{}'.", file_path.string()};
      }
    }

    // SPIR-V für Entry Point index 0 holen
    auto code_blob = Slang::ComPtr<ISlangBlob>{};
    auto container_blob = Slang::ComPtr<ISlangBlob>{};

    {
      const auto result = program->getEntryPointCode(0, 0, code_blob.writeRef(), container_blob.writeRef());

      if (SLANG_FAILED(result) || !code_blob) {
        throw utility::runtime_error{"Failed to get SPIR-V for stage '{}' in '{}'.", name, file_path.string()};
      }
    }

    const auto byte_size = code_blob->getBufferSize();

    if (byte_size % 4 != 0) {
      throw utility::runtime_error{"SPIR-V blob for stage '{}' not 4-byte aligned ({} bytes).", name, byte_size};
    }

    result.code[stage].resize(byte_size / 4);

    std::memcpy(result.code[stage].data(), code_blob->getBufferPointer(), byte_size);

    auto dependencies = std::vector<std::filesystem::path>{};
    dependencies.reserve(static_cast<std::size_t>(shader_module->getDependencyFileCount()) + 1u);
    dependencies.push_back(file_path);

    const auto reported_dependency_count = shader_module->getDependencyFileCount();

    for (auto index = SlangInt{0}; index < reported_dependency_count; ++index) {
      auto dependency_path = std::filesystem::path{shader_module->getDependencyFilePath(index)};

      if (dependency_path != file_path) {
        dependencies.push_back(std::move(dependency_path));
      }
    }

    store_cached(cache_path, result.code[stage], dependencies);
  }

  return result;
}

auto compiler::_read_file(const std::filesystem::path& path) -> std::string {
  auto file = std::ifstream{path, std::ios::binary | std::ios::ate};

  if (!file) {
    throw utility::runtime_error{"Could not open file {}", path.string()};
  }

  const auto size = file.tellg();
  
  file.seekg(0, std::ios::beg);

  auto buffer = std::vector<char>{};
  buffer.resize(size);

  file.read(buffer.data(), size);

  return std::string{buffer.data(), size};
}

auto compiler::_create_session(const compile_request& compile_request, SlangStage stage) -> Slang::ComPtr<slang::ISession> {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto session = Slang::ComPtr<slang::ISession>{};

  auto session_description = slang::SessionDesc{};

  auto target_description = slang::TargetDesc{};
  target_description.format = SLANG_SPIRV;
  target_description.profile = _global_session->findProfile(stage_infos[stage].profile);

  session_description.targets = &target_description;
  session_description.targetCount = 1u;

  auto compiler_options = std::array<slang::CompilerOptionEntry, 16u>{
    // slang::CompilerOptionEntry{slang::CompilerOptionName::Capability,         {slang::CompilerOptionValueKind::String, 0,                               0, "spirv_1_5",                           nullptr}},
    slang::CompilerOptionEntry{slang::CompilerOptionName::Capability,         {slang::CompilerOptionValueKind::String, 0,                               0, "SPV_EXT_physical_storage_buffer",     nullptr}},
    slang::CompilerOptionEntry{slang::CompilerOptionName::Capability,         {slang::CompilerOptionValueKind::String, 0,                               0, "SPV_EXT_demote_to_helper_invocation", nullptr}},
    
    // slang::CompilerOptionEntry{slang::CompilerOptionName::Capability,         {slang::CompilerOptionValueKind::String, 0,                               0, "SPV_KHR_non_semantic_info",           nullptr}},
    // slang::CompilerOptionEntry{slang::CompilerOptionName::Capability,         {slang::CompilerOptionValueKind::String, 0,                               0, "SPV_GOOGLE_user_type",                nullptr}},
    // slang::CompilerOptionEntry{slang::CompilerOptionName::Capability,         {slang::CompilerOptionValueKind::String, 0,                               0, "spvDerivativeControl",                nullptr}},
    // slang::CompilerOptionEntry{slang::CompilerOptionName::Capability,         {slang::CompilerOptionValueKind::String, 0,                               0, "spvImageQuery",                       nullptr}},
    // slang::CompilerOptionEntry{slang::CompilerOptionName::Capability,         {slang::CompilerOptionValueKind::String, 0,                               0, "spvImageGatherExtended",              nullptr}},
    // slang::CompilerOptionEntry{slang::CompilerOptionName::Capability,         {slang::CompilerOptionValueKind::String, 0,                               0, "spvSparseResidency",                  nullptr}},
    // slang::CompilerOptionEntry{slang::CompilerOptionName::Capability,         {slang::CompilerOptionValueKind::String, 0,                               0, "spvMinLod",                           nullptr}},
    // slang::CompilerOptionEntry{slang::CompilerOptionName::Capability,         {slang::CompilerOptionValueKind::String, 0,                               0, "spvFragmentFullyCoveredEXT",          nullptr}},
    // slang::CompilerOptionEntry{slang::CompilerOptionName::Capability,         {slang::CompilerOptionValueKind::String, 0,                               0, "spvDemoteToHelperInvocation",         nullptr}},

    slang::CompilerOptionEntry{slang::CompilerOptionName::MatrixLayoutColumn, {slang::CompilerOptionValueKind::Int,    1,                               0, "column_major",                        nullptr}},
    slang::CompilerOptionEntry{slang::CompilerOptionName::DebugInformation,   {slang::CompilerOptionValueKind::Int,    SLANG_DEBUG_INFO_LEVEL_STANDARD, 0, nullptr,                               nullptr}},
    slang::CompilerOptionEntry{slang::CompilerOptionName::Optimization,       {slang::CompilerOptionValueKind::Int,    SLANG_OPTIMIZATION_LEVEL_NONE,   0, nullptr,                               nullptr}},
    slang::CompilerOptionEntry{slang::CompilerOptionName::EmitSpirvDirectly,  {slang::CompilerOptionValueKind::Int,    1,                               0, nullptr,                               nullptr}}
  };

  session_description.compilerOptionEntries = compiler_options.data();
  session_description.compilerOptionEntryCount = compiler_options.size();

  auto preprocessor_macro_descriptions = std::vector<slang::PreprocessorMacroDesc>{};
  preprocessor_macro_descriptions.reserve(compile_request.defines.size() + 1u);

  preprocessor_macro_descriptions.push_back(slang::PreprocessorMacroDesc{"SBX_DEBUG", utility::is_build_configuration_debug_v ? "1" : "0"});

  for (const auto& [key, value] : compile_request.defines) {
    preprocessor_macro_descriptions.push_back(slang::PreprocessorMacroDesc{key.c_str(), value.c_str()});
  }

  session_description.preprocessorMacros = preprocessor_macro_descriptions.data();
  session_description.preprocessorMacroCount = preprocessor_macro_descriptions.size();

  auto& filesystem_module = core::engine::get_module<filesystem::filesystem_module>();

  const auto parent_path = assets_module.resolve_path(compile_request.path.parent_path()).generic_string();
  const auto path = assets_module.resolve_path(compile_request.path).generic_string();
  const auto asset_root = assets_module.asset_root().generic_string();
  const auto engine_shaders = filesystem_module.native_path_of(std::string{"engine://shaders"}).generic_string();

  auto search_paths = std::array<const char*, 4u>{
    parent_path.c_str(),
    path.c_str(),
    asset_root.c_str(),
    engine_shaders.c_str()
  };

  session_description.searchPaths = search_paths.data();
  session_description.searchPathCount = search_paths.size();

  _global_session->createSession(session_description, session.writeRef());

  return session;
}

} // namespace sbx::graphics
