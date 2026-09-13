<picture>
  <source media="(prefers-color-scheme: dark)" srcset="images/logo-dark.png" width="400">
  <source media="(prefers-color-scheme: light)" srcset="images/logo-light.png" width="400">
  <img alt="libsbx logo" src="images/logo-dark.png" width="400">
</picture>

A modular, Vulkan-based game engine built with modern C++26

![C++26](https://img.shields.io/badge/Language-C%2B%2B26-blue?logo=c%2B%2B&logoColor=blue)
![Version 0.1.0](https://img.shields.io/badge/Version-0.1.0-red?logo=git)
![MIT License](https://img.shields.io/badge/License-MIT-green?logo=opensourceinitiative&logoColor=green)
![Docs Build](https://img.shields.io/github/actions/workflow/status/KabelitzJ/sandbox/gh_pages.yml?logo=github&label=Deploy%20docs)

Sandbox (`libsbx`) is a game engine written from the ground up in **C++26**. It ships as a single static library plus three applications built on top of it: an in-game **runtime**, a full **editor**, and a **launcher** for picking and starting projects. Scripting is done in C# via a custom .NET host.

## Features

**Rendering**

- Vulkan render graph with automatic pass ordering (depth pre-pass, opaque, transparent accumulate/resolve, shadows, bloom, tonemap)
- Cascaded shadow maps
- Skinned mesh rendering with GPU compute skinning
- GPU particle simulation and rendering
- IBL baking (BRDF LUT, irradiance, prefiltered environment maps)
- Immediate-mode debug draw and editor grid overlay
- ImGui-based editor UI, plus an in-house retained-mode canvas UI (SDF text, widgets, screen- and world-space canvases)

**Engine**

- Entity-Component-System core (`ecs`)
- Scene graph with node/component model and YAML scene serialization
- Asset pipeline: cooking, manifests, residency/streaming, prefabs
- Physics: GJK/EPA collision, contact solver, and a Recast/Detour-style navmesh + crowd-agent system (`physics/nav`)
- Terrain module with heightmap generation and chunking
- C# scripting via a custom .NET (nethost) integration, with a source-generated interop layer
- Reflection system for structs/enums, used by the inspector and serializer
- Cross-platform input, windowing, and process handling (`platform`)
- Tracy profiling integration

## Layout

```
libsbx/     engine library (see module table below)
runtime/    standalone player application
editor/     full scene/asset editor application
launcher/   project picker / launcher application
dotnet/     C# side: Sbx.Managed (native interop), Sbx.Core (user-facing API), Sbx.Compiler
shaders/    Slang shader sources
tests/      GTest unit tests
```

### Engine modules (`libsbx/`)

All modules compile into one `libsbx` static (or shared, via `SBX_BUILD_SHARED`) library target.

| Module | Purpose |
|---|---|
| `assets` | Asset cooking, loading, manifests, residency, materials, meshes, prefabs, IBL baking |
| `canvas` | Retained-mode UI: layout, draw lists, screen/world canvases |
| `cli` | Command-line argument parsing |
| `containers` | Custom containers: dense_map, octree, dynamic_tree, ring/stable/static vector |
| `core` | Engine lifecycle, application base class, module system, project handling |
| `ecs` | Entity-Component-System framework |
| `filesystem` | Virtual filesystem abstraction over disk/archive sources |
| `graphics` | Vulkan device/pipeline/resource layer, render graph primitives, bindless resources |
| `math` | Vectors, matrices, transforms, color, noise, UUIDs |
| `memory` | Allocators, aligned storage, memory tracking |
| `particles` | CPU-side particle spawning/simulation |
| `physics` | Collision (GJK/EPA), contact solving, raycasting, navmesh + crowd navigation |
| `platform` | Windowing, input, process spawning |
| `reflection` | Struct/enum reflection used by the editor and serializer |
| `render` | Compositor, render passes, shadow/skinning/particle rendering, ImGui backend, UI system |
| `scenes` | Scene graph, node/component storage, YAML scene serialization |
| `scripting` | C# hosting (.NET/nethost), managed object marshalling, script compiler |
| `signals` | Event/signal/connection system |
| `terrain` | Heightmap generation and terrain chunking |
| `units` | Type-safe physical units (length, mass, time, velocity, ...) |
| `utility` | Logging, hashing, compression, timers, general helpers |

### Applications

- **`runtime/`** — minimal executable that boots the engine and runs a cooked project.
- **`editor/`** — ImGui-based editor: hierarchy, inspector, asset browser, animation graph, navigation baking, play-mode preview, prefab editing, and more (`editor/editor/panels/`).
- **`launcher/`** — lightweight project selector that launches the editor or runtime for a chosen project.

## Getting Started

### Prerequisites

- A C++26-capable compiler (GCC or Clang on Linux; MSVC on Windows)
- [CMake](https://cmake.org/) 3.25+
- [Conan](https://conan.io/) 2.x
- [Vulkan SDK](https://vulkan.lunarg.com/)
- [Slang](https://github.com/shader-slang/slang) shader compiler
- .NET SDK (net9.0) for the C# scripting side

### Build

```bash
git clone git@github.com:KabelitzJ/sandbox.git
cd sandbox

conan install . --build=missing -s build_type=Debug
cmake --preset x86_64-gcc-debug   # name is <arch>-<compiler>-<build_type>, from CMakeUserPresets.json
cmake --build --preset x86_64-gcc-debug
```

Useful CMake options (all default to `ON` except where noted):

| Option | Purpose |
|---|---|
| `SBX_BUILD_RUNTIME` | Build the runtime executable |
| `SBX_BUILD_EDITOR` | Build the editor executable |
| `SBX_BUILD_LAUNCHER` | Build the launcher executable |
| `SBX_BUILD_TESTS` | Build GTest unit tests |
| `SBX_BUILD_SHARED` | Build `libsbx` as a shared library (default `OFF`) |
| `SBX_ENABLE_PROFILING` | Enable Tracy profiling (default `ON` in Debug) |

### Entry point

Both the runtime and editor derive an application from `sbx::core::application` and boot it through `sbx::core::engine`:

```cpp
#include <libsbx/core/core.hpp>
#include <libsbx/core/engine.hpp>

auto main(int argc, const char** argv) -> int {
  auto args = std::vector<std::string_view>{argv, argv + argc};

  try {
    auto engine = std::make_unique<sbx::core::engine>(args);
    engine->run<my::application>();
  } catch (const std::exception& exception) {
    sbx::utility::logger<"app">::error("{}", exception.what());
    return sbx::core::exit::failure;
  }

  return sbx::core::exit::success;
}
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Open a pull request

Found a bug or have a suggestion? [Open an issue](https://github.com/KabelitzJ/sandbox/issues).

## License

This project is licensed under the [MIT License](LICENSE).

## Contact

GitHub: [KabelitzJ](https://github.com/KabelitzJ)
