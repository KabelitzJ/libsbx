import os
import shutil
import subprocess
from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMakeDeps, CMakeToolchain, CMake, cmake_layout
from conan.tools.files import copy, mkdir
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
    "build_tests": [True, False],
    "build_benchmarks": [True, False]
  }

  default_options = {
    "shared": False,
    "fPIC": True,
    "build_demo": True,
    "build_tests": True,
    "build_benchmarks": True
  }

  # Source directories
  exports_sources = (
    "CMakeLists.txt",
    "cmake/**",
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

  REQUIRED_VULKAN_VERSION = (1, 4, 335, 0)
  REQUIRED_VULKAN_VERSION_STR = ".".join(map(str, REQUIRED_VULKAN_VERSION))

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

    self.cpp.build.components["core"].libdirs = [self.folders.build]
    self.cpp.build.components["utility"].libdirs = [self.folders.build]

  def _ensure_dotnet(self):
    if self.settings.os == "Linux":
      apt = Apt(self)
      apt.install(["dotnet-sdk-9.0"], update=True, check=True)
    elif self.settings.os == "Windows": 
      if not shutil.which("dotnet"):
        raise ConanInvalidConfiguration(
          "dotnet SDK 9.0 is required\n."
          "Please install it from https://dotnet.microsoft.com/"
        )
      
  def _normalize_version(self, version, length=4):
    return version + (0,) * (length - len(version))
      
  def _get_vulkan_instance_version(self):
    exe = shutil.which("vulkaninfo")
    if not exe:
      return None

    try:
      out = subprocess.check_output(
        [exe, "--summary"],
        stderr=subprocess.STDOUT,
        text=True
      )
    except Exception:
      return None

    for line in out.splitlines():
      if line.startswith("Vulkan Instance Version"):
        version = line.split(":")[-1].strip()
        return tuple(map(int, version.split(".")))

    return None
  
  def _endure_vulkan_sdk(self):
    """
    Ensures that a Vulkan SDK is installed and the GPU supports
    at least the required Vulkan version. On Linux, it uses Mesa
    Vulkan API detection to avoid false negatives.
    """
    if self.settings.os == "Linux":
      apt = Apt(self)
      # Ensure vulkaninfo is installed
      apt.install(["vulkan-tools"], update=True, check=True)

      version = self._get_vulkan_instance_version()
      if not version:
        raise ConanInvalidConfiguration(
          "Vulkan not found.\n"
          f"Vulkan SDK {self.REQUIRED_VULKAN_VERSION_STR} required.\n"
          "Please install Vulkan SDK:\n"
          "  https://vulkan.lunarg.com/sdk/home#linux"
        )

      version = self._normalize_version(version)
      required = self._normalize_version(self.REQUIRED_VULKAN_VERSION)

      if version < required:
        self.output.warn(
          f"Detected Vulkan version: {'.'.join(map(str, version))} "
          f"< required {self.REQUIRED_VULKAN_VERSION_STR}. "
          "Check that your Mesa driver is up-to-date. "
          "Intel GPUs with Mesa >= 26 should provide Vulkan 1.4 features."
        )
      else:
        self.output.info(
          f"Vulkan SDK detected: {'.'.join(map(str, version))} >= {self.REQUIRED_VULKAN_VERSION_STR}"
        )

    elif self.settings.os == "Windows":
      # Windows still uses strict check
      version = self._get_vulkan_instance_version()
      if not version:
        raise ConanInvalidConfiguration(
          "Vulkan not found.\n"
          f"Vulkan SDK {self.REQUIRED_VULKAN_VERSION_STR} required.\n"
          "Please install Vulkan SDK:\n"
          "  https://vulkan.lunarg.com/sdk/home#windows"
        )

      version = self._normalize_version(version)
      required = self._normalize_version(self.REQUIRED_VULKAN_VERSION)

      if version < required:
        raise ConanInvalidConfiguration(
          f"Vulkan Instance >= {self.REQUIRED_VULKAN_VERSION_STR} required.\n"
          f"Detected: {'.'.join(map(str, version))}"
        )
      else:
        self.output.info(
          f"Vulkan SDK detected: {'.'.join(map(str, version))} >= {self.REQUIRED_VULKAN_VERSION_STR}"
        )

  def system_requirements(self):
    self._ensure_dotnet()
    self._endure_vulkan_sdk()

  def build_requirements(self):
    self.tool_requires("cmake/[>=3.20]")

  # def test_requirements(self):
  #   if self.options.build_tests:
  #     self.test_requires("gtest/1.17.0")

  #   if self.options.build_benchmarks:
  #     self.test_requires("benchmark/1.9.4")

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
    self.requires(f"spirv-cross/{self.REQUIRED_VULKAN_VERSION_STR}")
    # self.requires("spirv-headers/1.2.198.0")
    # self.requires("spirv-tools/1.4.309.0")
    # self.requires("slang/0.9")
    self.requires("vulkan-memory-allocator/3.3.0")
    self.requires(f"vulkan-headers/{self.REQUIRED_VULKAN_VERSION_STR}")
    self.requires("stb/cci.20230920")
    self.requires("range-v3/0.12.0", transitive_headers=True)
    self.requires("freetype/2.14.1")
    self.requires("openal-soft/1.25.1")
    self.requires("drwav/0.13.14")
    self.requires("drmp3/0.6.38")
    self.requires("imgui/1.91.8-docking")
    self.requires("implot/0.16-docking")
    self.requires("imnodes/0.5.0-docking")
    self.requires("imguizmo/1.83-docking")
    # self.requires("portable-file-dialogs/0.1.0")
    # self.requires("easy_profiler/2.1.0", transitive_headers=True)
    self.requires("tsl-robin-map/1.3.0")
    self.requires("lz4/1.10.0")
    self.requires("assimp/5.4.3")
    # self.requires("bullet3/3.25")
    self.requires("meshoptimizer/0.25")
    self.requires("sol2/3.5.0")
    self.requires("magic_enum/0.9.7")
    self.requires("zstd/1.5.7")
    self.requires("tracy/0.13.1")

    if self.options.build_tests:
      self.test_requires("gtest/1.17.0")

    if self.options.build_benchmarks:
      self.test_requires("benchmark/1.9.4")

  def configure(self):
    self.options["imgui"].wchar32 = True

  def generate(self):
    deps = CMakeDeps(self)
    toolchain = CMakeToolchain(self)

    deps.generate()
    toolchain.generate()

  def build(self):
    cmake = CMake(self)

    cmake.configure({
      "SBX_BUILD_DEMO": "ON" if self.options.build_demo else "OFF",
      "SBX_BUILD_SHARED": "ON" if self.options.shared else "OFF",
      "SBX_BUILD_TESTS": "ON" if self.options.build_tests else "OFF",
      "SBX_BUILD_BENCHMARKS": "ON" if self.options.build_benchmarks else "OFF"
    })
  
    cmake.build()

  def package(self):
    cmake = CMake(self)
    cmake.install()

    dotnet_build_dir = os.path.join(self.build_folder, "_dotnet")
    dotnet_package_dir = os.path.join(self.package_folder, "bin", "dotnet")

    if os.path.isdir(dotnet_build_dir):
      mkdir(self, dotnet_package_dir)

      copy(self,
        pattern="*.dll",
        src=dotnet_build_dir,
        dst=dotnet_package_dir,
        keep_path=False
      )
      copy(self,
        pattern="*.deps.json",
        src=dotnet_build_dir,
        dst=dotnet_package_dir,
        keep_path=False
      )
      copy(self,
        pattern="*.runtimeconfig.json",
        src=dotnet_build_dir,
        dst=dotnet_package_dir,
        keep_path=False
      )
      copy(self,
        pattern="*.pdb",
        src=dotnet_build_dir,
        dst=dotnet_package_dir,
        keep_path=False
      )

  def package_info(self):
    self.cpp_info.set_property("cmake_file_name", "libsbx")
    self.cpp_info.set_property("cmake_target_name", "libsbx::libsbx")

    self.cpp_info.bindirs.append("bin/dotnet")
    
    self.cpp_info.components["utility"].requires = [
      "fmt::fmt", 
      "spdlog::spdlog",
      "magic_enum::magic_enum",
      "lz4::lz4"
    ]

    self.cpp_info.components["math"].requires = [
      "fmt::fmt",
      "yaml-cpp::yaml-cpp",
      "range-v3::range-v3"
    ]

    # if self.options.build_tests:
    #   self.cpp_info.components["math"].requires.append("GTest::gtest")

    self.cpp_info.components["core"].requires = [
      "range-v3::range-v3"
    ]

    self.cpp_info.components["devices"].requires = [
      "glfw::glfw"
    ]

    self.cpp_info.components["graphics"].requires = [
      "vulkan-memory-allocator::vulkan-memory-allocator",
      "vulkan-headers::vulkan-headers",
      "stb::stb",
      "spirv-cross::spirv-cross"
    ]

    self.cpp_info.components["models"].requires = [
      "tinyobjloader::tinyobjloader",
      "nlohmann_json::nlohmann_json",
      "base64::base64",
      "tsl-robin-map::tsl-robin-map",
      "assimp::assimp",
      "meshoptimizer::meshoptimizer"
    ]

    self.cpp_info.components["scripting"].requires = [
      "sol2::sol2"
    ]

    self.cpp_info.components["ui"].requires = [
      "freetype::freetype"
    ]
