import os
from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, CMake, cmake_layout
from conan.tools.system.package_manager import Apt

class libsbx_recipe(ConanFile):
  name = "libsbx"
  version = "0.1.0"
  
  # Metadata
  license = "MIT"
  author = "Jonas Kabelitz <jonas-kabelitz@gmx.de>"
  description = "libsbx library"
  topics = (
    "libsbx",
    "lib"
  )

  # Binary configuration
  settings = (
    "os", 
    "compiler", 
    "build_type", 
    "arch"
  )
  options = {
    "shared": [True, False],
    "fPIC": [True, False],
    "build_demo": [True, False],
    "build_tests": [True, False]
  }
  default_options = {
    "shared": False,
    "fPIC": True,
    "build_demo": True,
    "build_tests": True
  }

  # Source directories
  exports_sources = (
    "CMakeLists.txt",
    "cmake/modules/**",
    "scripts/**",
    "libsbx-animations/**",
    "libsbx-bitmaps/**",
    "libsbx-core/**",
    "libsbx-ecs/**",
    "libsbx-gizmos/**",
    "libsbx-io/**",
    "libsbx-memory/**",
    "libsbx-physics/**",
    "libsbx-scenes/**",
    "libsbx-shadows/**",
    "libsbx-ui/**",
    "libsbx-utility/**",
    "libsbx-assets/**",
    "libsbx-containers/**",
    "libsbx-devices/**",
    "libsbx-editor/**",
    "libsbx-graphics/**",
    "libsbx-math/**",
    "libsbx-models/**",
    "libsbx-post/**",
    "libsbx-scripting/**",
    "libsbx-signals/**",
    "libsbx-units/**",
    "demo/**",
    # "!demo/assets/**"
  )

  def config_options(self):
    if self.settings.os == "Windows":
      self.options.fPIC = False

  def layout(self):
    cmake_layout(self)

    # Define custom layout for build artifacts

    is_compiler_multi_config = self.settings.compiler == "msvc"

    self.folders.build = os.path.join("build", str(self.settings.arch), str(self.settings.compiler))

    if not is_compiler_multi_config:
      self.folders.build = os.path.join(self.folders.build, str(self.settings.build_type).lower())

    self.folders.generators = os.path.join(self.folders.build, "dependencies")

    self.cpp.source.components["utility"].includedirs = ["libsbx-utility"]
    self.cpp.source.components["core"].includedirs = ["libsbx-core"]
    self.cpp.source.components["editor"].includedirs = ["libsbx-editor"]

    self.cpp.build.components["core"].libdirs = [self.folders.build]
    self.cpp.build.components["utility"].libdirs = [self.folders.build]
    self.cpp.build.components["editor"].libdirs = [self.folders.build]

  def system_requirements(self):
    apt = Apt(self)
    apt.install(["dotnet-sdk-9.0"], update=True, check=True)

  def build_requirements(self):
    self.tool_requires("cmake/[>=3.20]")

  def requirements(self):
    self.requires("fmt/11.2.0", transitive_headers=True)
    self.requires("spdlog/1.15.3", transitive_headers=True)
    # self.requires("glm/1.0.1")
    self.requires("yaml-cpp/0.7.0")
    self.requires("nlohmann_json/3.11.3")
    self.requires("base64/0.5.2")
    self.requires("glfw/3.3.8")
    # self.requires("sol2/3.3.1")
    self.requires("tinyobjloader/2.0.0-rc10")
    self.requires("spirv-cross/1.4.321.0")
    # self.requires("spirv-headers/1.2.198.0")
    # self.requires("spirv-tools/1.4.309.0")
    # self.requires("slang/0.9")
    self.requires("vulkan-memory-allocator/3.3.0")
    self.requires("vulkan-headers/1.4.321.0")
    self.requires("stb/cci.20230920")
    self.requires("range-v3/0.12.0", transitive_headers=True)
    self.requires("freetype/2.14.1")
    self.requires("gtest/1.17.0")
    # self.requires("openal-soft/1.22.2")
    # self.requires("drwav/0.13.12")
    # self.requires("drmp3/0.6.34")
    self.requires("imgui/1.91.8-docking")
    self.requires("implot/0.16-docking")
    self.requires("imnodes/0.5.0-docking")
    # self.requires("imguizmo/1.83-docking")
    # self.requires("portable-file-dialogs/0.1.0")
    self.requires("easy_profiler/2.1.0")
    self.requires("tsl-robin-map/1.3.0")
    self.requires("lz4/1.10.0")
    self.requires("assimp/5.4.3")
    # self.requires("bullet3/3.25")
    self.requires("meshoptimizer/0.25")
    self.requires("sol2/3.5.0")
    self.requires("magic_enum/0.9.7")

  def generate(self):
    deps = CMakeDeps(self)
    toolchain = CMakeToolchain(self)

    deps.generate()
    toolchain.generate()

  def build(self):
    cmake = CMake(self)

    variables = {
      "SBX_BUILD_DEMO": "On" if self.options.build_demo else "Off",
      "SBX_BUILD_SHARED": "On" if self.options.shared else "Off",
      "SBX_BUILD_TESTS": "On" if self.options.build_tests else "Off"
    }

    cmake.configure(variables)
    cmake.build()

  def package(self):
    cmake = CMake(self)
    cmake.install()

  def package_info(self):
    self.cpp_info.set_property("cmake_file_name", "libsbx")
    self.cpp_info.set_property("cmake_target_name", "libsbx::libsbx")
    
    utility = self.cpp_info.components["utility"]
    utility.libs = ["utility"]
    utility.requires = [
      "fmt::fmt", 
      "spdlog::spdlog",
      "magic_enum::magic_enum",
      "lz4::lz4"
    ]

    math = self.cpp_info.components["math"]
    math.libs = ["math"]
    math.requires = [
      "fmt::fmt",
      "yaml-cpp::yaml-cpp",
      "range-v3::range-v3",
      "gtest::gtest"
    ]

    core = self.cpp_info.components["core"]
    core.libs = ["core"]
    core.requires = [
      "range-v3::range-v3",
      "easy_profiler::easy_profiler"
    ]

    devices = self.cpp_info.components["devices"]
    devices.libs = ["devices"]
    devices.requires = [
      "glfw::glfw"
    ]

    graphics = self.cpp_info.components["graphics"]
    graphics.libs = ["graphics"]
    graphics.requires = [
      "vulkan-memory-allocator::vulkan-memory-allocator",
      "vulkan-headers::vulkan-headers",
      "stb::stb",
      "spirv-cross::spirv-cross"
    ]

    models = self.cpp_info.components["models"]
    models.libs = ["models"]
    models.requires = [
      "tinyobjloader::tinyobjloader",
      "nlohmann_json::nlohmann_json",
      "base64::base64",
      "tsl-robin-map::tsl-robin-map",
      "assimp::assimp",
      "meshoptimizer::meshoptimizer"
    ]

    scripting = self.cpp_info.components["scripting"]
    scripting.libs = ["scripting"]
    scripting.requires = [
      "sol2::sol2"
    ]

    ui = self.cpp_info.components["ui"]
    ui.libs = ["ui"]
    ui.requires = [
      "freetype::freetype"
    ]

    editor = self.cpp_info.components["editor"]
    editor.libs = ["editor"]
    editor.requires = [
      "imgui::imgui",
      "implot::implot",
      "imnodes::imnodes"
    ]