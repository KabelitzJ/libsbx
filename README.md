<p align="center">
  <img src="images/logo.png" alt="Sandbox Engine Logo" width="400"/>
</p>

<h1 align="center">Sandbox Game Engine</h1>

<p align="center">
  A modular, Vulkan-based game engine built with modern C++20
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C%2B%2B20-blue?logo=c%2B%2B&logoColor=blue" alt="C++20"/>
  <img src="https://img.shields.io/badge/Version-0.2.0-red?logo=git" alt="Version 0.2.0"/>
  <img src="https://img.shields.io/badge/License-MIT-green?logo=opensourceinitiative&logoColor=green" alt="MIT License"/>
  <img src="https://img.shields.io/github/actions/workflow/status/KabelitzJ/sandbox/gh_pages.yml?logo=github&label=Deploy%20docs" alt="Docs Build"/>
</p>

---

Sandbox is a game engine written from the ground up in **C++20**, designed as both a learning tool and a practical framework for real-time 3D applications. It features a Vulkan-based deferred PBR rendering pipeline, a render graph architecture, and a modular design built around an Entity-Component-System core.

> **Note:** Active development happens on the [`development`](https://github.com/KabelitzJ/sandbox/tree/development) branch.

<p align="center">
  <img src="images/screenshot.png" alt="Engine Screenshot" width="720"/>
</p>

## Features

**Rendering**

- Vulkan-based deferred PBR shading pipeline
- Image-Based Lighting (IBL) — compute-generated BRDF LUT, irradiance, and prefiltered environment maps
- Cascaded Shadow Maps (4 cascades at varying resolutions)
- Weighted Blended Order-Independent Transparency (WBOIT)
- Skybox rendering with HDR cube maps
- Post-processing chain: tonemapping, FXAA, bloom (WIP)
- Render graph with automatic dependency resolution and pass ordering
- GPU compute passes for skinning and particle simulation

**Engine**

- Entity-Component-System architecture
- Scene graph with YAML-based scene loading
- Skeletal animation system with state machines and blend transitions
- GPU-driven particle system with configurable emitters
- C# scripting via .NET integration
- Audio engine
- Editor and gizmo tools
- Debug rendering and grid overlay
- Profiling support (easy_profiler)

**In progress:** SSAO, bloom, physics engine, networking, AI systems

## Architecture

The engine is split into independent modules, each prefixed with `libsbx-`:

| Module | Purpose |
|---|---|
| `core` | Engine lifecycle, application base class, CLI, module system |
| `ecs` | Entity-Component-System framework |
| `graphics` | Vulkan rendering, pipelines, render graph, compute passes |
| `models` | Static mesh loading, materials, draw lists |
| `animations` | Skeletal animation, GPU skinning, state machines |
| `particles` | GPU particle simulation and rendering |
| `scenes` | Scene graph, transforms, skybox, debug/grid rendering |
| `physics` | Physics simulation |
| `audio` | Audio playback |
| `scripting` | C# scripting via .NET |
| `devices` | Window management and input handling |
| `ui` | User interface rendering |
| `editor` | In-engine editor tools |
| `gizmos` | Debug visualization |
| `post` | Post-processing filters (tonemap, FXAA, blur, SSAO, bloom) |
| `signals` | Event / signal system |
| `math` | Vectors, matrices, transforms, noise, colors |
| `memory` | Custom allocators, observer pointers |
| `io` | File I/O and resource loading |
| `assets` | Asset pipeline and management |
| `bitmaps` | Image loading and manipulation |
| `sprites` | 2D sprite rendering |
| `containers` | Custom container types |
| `units` | Type-safe unit system |
| `utility` | Logging, timers, string IDs, general utilities |

## Getting Started

### Prerequisites

- A **C++20** compatible compiler (MinGW via [MSYS2](https://www.msys2.org/) recommended on Windows)
- [CMake](https://cmake.org/) 3.x+
- [Conan](https://conan.io/) (C++ package manager)
- [Vulkan SDK](https://vulkan.lunarg.com/)
- [.NET SDK](https://dotnet.microsoft.com/) (for C# scripting)

### Clone

```bash
git clone -b development https://github.com/KabelitzJ/sandbox.git
cd sandbox
```

### Install Dependencies

```bash
conan install . --profile=default --build=missing
```

### Build

```bash
cmake . -B build/debug -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
```

> Adjust the generator (`-G`) and build type to match your toolchain.

### Run the Demo

```bash
./build/debug/bin/demo.exe
```

## Usage Examples

The project includes a full demo application in [`demo/demo/`](https://github.com/KabelitzJ/sandbox/tree/development/demo/demo) that showcases the engine's capabilities.

### Creating an Application

Derive from `sbx::core::application` to set up modules, load assets, and build your scene:

```cpp
// demo/demo/application.hpp

#include <libsbx/core/core.hpp>
#include <libsbx/devices/devices.hpp>
#include <libsbx/graphics/graphics.hpp>
#include <libsbx/scenes/scenes.hpp>
#include <libsbx/animations/animations_module.hpp>
#include <libsbx/particles/particle_emitter.hpp>

namespace demo {

class application : public sbx::core::application {

public:

  application();
  ~application() override = default;

  auto update() -> void override;
  auto fixed_update() -> void override;
  auto is_paused() const -> bool override;

private:

  auto _generate_brdf(const std::uint32_t size) -> void;
  auto _generate_irradiance(const std::uint32_t size) -> void;
  auto _generate_prefiltered(const std::uint32_t size) -> void;

  bool _is_paused;
  sbx::math::angle _rotation;

  sbx::graphics::image2d_handle _brdf;
  sbx::graphics::cube_image2d_handle _irradiance;
  sbx::graphics::cube_image2d_handle _prefiltered;

}; // class application

} // namespace demo
```

The constructor sets up the full scene — loading meshes, textures, and materials; generating IBL maps via compute shaders; configuring skeletal animations with state machine transitions; spawning particle emitters; and instantiating C# scripts on scene nodes:

```cpp
application::application() {
  // ...

  // Set up the renderer
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  graphics_module.set_renderer<demo::renderer>();

  // Load the scene from YAML
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.load_scene("res://scenes/scene.yaml");

  // Load PBR textures
  scene.add_image("helmet_albedo", "res://textures/helmet/albedo.jpg", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("helmet_normal", "res://textures/helmet/normal.jpg", sbx::graphics::format::r8g8b8a8_unorm);
  scene.add_image("helmet_mrao", "res://textures/helmet/mrao2.jpg", sbx::graphics::format::r8g8b8a8_unorm);
  scene.add_image("helmet_emissive", "res://textures/helmet/emissive.jpg", sbx::graphics::format::r8g8b8a8_srgb);

  // Generate IBL maps on the GPU
  _generate_brdf(512);
  _generate_irradiance(64);
  _generate_prefiltered(512);

  // Configure a PBR material
  auto& helmet_material = scene.add_material<sbx::models::material>("helmet");
  helmet_material.albedo.image = scene.get_image("helmet_albedo");
  helmet_material.normal.image = scene.get_image("helmet_normal");
  helmet_material.mrao.image = scene.get_image("helmet_mrao");
  helmet_material.emissive.image = scene.get_image("helmet_emissive");
  helmet_material.emissive_strength = 16.0f;

  // Set up skeletal animation with state transitions
  auto& fox_animator = scene.add_component<sbx::animations::animator>(fox);
  fox_animator.add_state({"Walk", scene.get_animation("Walk"), true, 0.5f});
  fox_animator.add_state({"Survey", scene.get_animation("Survey"), true, 0.5f});
  fox_animator.add_state({"Run", scene.get_animation("Run"), true, 0.5f});

  fox_animator.add_transition({"Walk", "Run", 0.15f, [](const auto& a) {
    if (auto v = a.float_parameter("speed"); v) return *v >= 2.0f;
    return false;
  }});

  // Attach a particle emitter to the fox's tail bone
  auto tail = animations_module.find_skeleton_node(fox, "b_Tail03_014");

  auto tail_emitter = scene.create_child_node(tail, "TailEmitter");

  auto& particles = scene.add_component<sbx::particles::particle_emitter>(tail_emitter);
  particles.max_particles = 1000;
  particles.emission_rate = 100.0f;
  particles.initial_color = sbx::math::color{255u, 140u, 0u, 250u};
  particles.end_color = sbx::math::color{255u, 69u, 0u, 0u};

  // Create a skybox
  auto camera_node = scene.camera();

  scene.add_component<sbx::scenes::skybox>(camera_node, scene.get_cube_image("skybox"), _brdf, _irradiance, _prefiltered);

  // Attach a C# script to the camera
  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  scripting_module.instantiate(camera_anchor, "build/.../Demo.dll", "Demo.CameraController");

  // ...
}
```

### Setting Up the Renderer

The renderer defines the full render graph — shadow passes, deferred G-buffer, transparency, resolve, and post-processing:

```cpp
renderer::renderer() {
  // G-buffer attachments
  auto depth    = create_attachment("depth", attachment::type::depth);
  auto albedo   = create_attachment("albedo", attachment::type::image, ...);
  auto position = create_attachment("position", attachment::type::image, ...);
  auto normal   = create_attachment("normal", attachment::type::image, ...);
  auto material = create_attachment("material", attachment::type::image, ...);
  auto emissive = create_attachment("emissive", attachment::type::image, ...);

  // 4-cascade shadow maps
  auto shadow0 = create_attachment("shadow0", ...);
  // ...

  // Compute passes
  auto skinning_pass  = create_pass([&](auto& ctx) { return ctx.compute_pass("skinning"); });
  auto particles_pass = create_pass([&](auto& ctx) { return ctx.compute_pass("particles"); });

  // Graphics passes with explicit dependencies
  auto deferred_pass = create_pass([&](auto& ctx) {
    auto pass = ctx.graphics_pass("deferred");
    pass.depends_on(skinning_pass);
    pass.writes(depth, albedo, position, normal, material, emissive, ...);
    return pass;
  });

  // ... shadow passes, transparency pass, resolve pass ...

  build_render_graph();

  // Bind subrenderers to passes
  add_subrenderer<static_mesh_material_subrenderer>(deferred_pass, ...);
  add_subrenderer<skinned_mesh_material_subrenderer>(deferred_pass, ...);
  add_subrenderer<particle_subrenderer>(transparency_pass, ...);
  add_subrenderer<resolve_opaque_filter>(resolve_pass, ...);
  add_subrenderer<skybox_subrenderer>(resolve_pass, ...);
  add_subrenderer<tonemap_filter>(tonemap_pass, ...);
  add_subrenderer<fxaa_filter>(fxaa_pass, ...);
  add_subrenderer<editor_subrenderer>(editor_pass, ...);
}
```

### Entry Point

The engine is bootstrapped from a simple `main` that creates the engine instance and runs the application:

```cpp
#include <libsbx/core/core.hpp>
#include <libsbx/core/engine.hpp>

#include <demo/application.hpp>

auto main(int argc, const char** argv) -> int {
  auto args = std::vector<std::string_view>{argv, argv + argc};

  try {
    auto engine = std::make_unique<sbx::core::engine>(args);

    engine->run<demo::application>();
  } catch(const std::exception& exception) {
    sbx::utility::logger<"demo">::error("{}", exception.what());

    return sbx::core::exit::failure; 
  }

  return sbx::core::exit::success;
}
```

## Gallery

<p align="center">
  <img src="images/dynamic_lighting.gif" alt="Dynamic Lighting" width="720"/><br/>
  <em>Real-time dynamic lighting</em>
</p>

<p align="center">
  <img src="images/shadow.png" alt="Shadow Mapping" width="720"/><br/>
  <em>Shadow mapping</em>
</p>

## Contributing

Contributions are welcome! To get involved:

1. Fork the repository
2. Create a feature branch off `development`
3. Make your changes
4. Open a pull request

Found a bug or have a suggestion? [Open an issue](https://github.com/KabelitzJ/sandbox/issues).

## License

This project is licensed under the [MIT License](LICENSE).

Feel free to use this project for your own purposes. If you do, send me a message — I'd love to see what you've built with it.

## Contact

GitHub: [KabelitzJ](https://github.com/KabelitzJ)
