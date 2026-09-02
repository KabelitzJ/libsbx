import os

from conan import ConanFile
from conan.tools.files import copy
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class LibsbxConan(ConanFile):

    name = "libsbx"
    description = "A modular, Vulkan-based game engine built with modern C++26"
    license = "MIT"
    url = "https://github.com/KabelitzJ/libsbx"
    homepage = "https://kabelitzj.github.io/libsbx/"
    topics = ("graphics", "3D", "vulkan")

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
            
        self.options["imgui/*"].use_wchar32 = True

    def layout(self):
        is_multi_config = self.settings.compiler == "msvc"

        self.folders.build_folder_vars = ["settings.arch", "settings.compiler"]

        cmake_layout(self)

        self.folders.build = os.path.join("build", str(self.settings.arch), str(self.settings.compiler))

        if not is_multi_config:
            self.folders.build = os.path.join(self.folders.build, str(self.settings.build_type).lower())

        self.folders.generators = os.path.join(self.folders.build, "dependencies")

    def requirements(self):
        self.requires("fastgltf/0.9.0")
        self.requires("fmt/12.1.0", transitive_headers=True)
        self.requires("glfw/3.3.8", transitive_headers=True)
        self.requires("gtest/1.17.0")
        self.requires("imgui/1.92.8-docking", transitive_headers=True)
        self.requires("imguizmo/1.10-docking", transitive_headers=True)
        self.requires("lz4/1.10.0")
        self.requires("meshoptimizer/1.0")
        self.requires("nlohmann_json/3.11.3", transitive_headers=True)
        self.requires("spdlog/1.17.0", transitive_headers=True)
        self.requires("stb/cci.20240531")
        self.requires("tracy/0.13.1", transitive_headers=True)
        self.requires("vulkan-memory-allocator/3.3.0", transitive_headers=True)
        self.requires("yaml-cpp/0.7.0", transitive_headers=True)
        self.requires("zstd/1.5.7")

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.presets_prefix = ""
        toolchain.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

