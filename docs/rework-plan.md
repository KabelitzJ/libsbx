# libsbx rework — architecture plan

Status: agreed 2026-07-19. This document guides the rewrite on the `rework` branch.

## Goals

- Clean module layering: `platform` → `graphics` → `assets` / `renderer`.
- Vulkan with **dynamic rendering only** (no `VkRenderPass`/framebuffers) and a **bindless** resource model.
- **VMA** for all GPU memory, **slang** as the only shader language, reflection via the **slang reflection API** (spirv-cross is dropped).
- UUID-based asset system with **import + binary cache** (cook on first touch, fast binary loads afterwards).
- **Forward+ renderer with clustered (3D froxel) light culling**, shader-per-material, draw lists batched by material/submesh into as few indirect draws as possible.
- Mesh import via **fastgltf** (glTF is the canonical interchange format).

## Decisions (with rationale)

| Decision | Choice | Why |
|---|---|---|
| Shader reflection | slang `ProgramLayout` API | One toolchain; understands slang semantics; drops spirv-cross |
| Asset flow | Import once → binary cache keyed by uuid | Fast startup; importers can be heavy (mips, meshoptimizer, LODs) |
| Light culling | Clustered / froxels | Works for transparents & volumetrics; no depth dependency for the light grid |
| Mesh import | fastgltf | Small, fast, modern; pairs with the cook step |
| Descriptors | Global bindless tables (update-after-bind) | Replaces descriptor/uniform/storage handler machinery entirely |
| Geometry | Global mega vertex/index buffers | Batches never rebind geometry; indirect draws index into shared buffers |

## Module layering

```
foundation: utility, containers, math (uuid), memory, signals, ecs, core, filesystem
  └─ platform    — window, input, events (GLFW)
      └─ graphics — Vulkan RHI: device, swapchain, VMA, bindless, slang, pipelines, render graph
          ├─ assets   — uuid database, importers, binary cache, async loading
          └─ renderer — Forward+, materials, draw lists, batching  (depends on graphics + assets)
```

Each layer only includes headers from layers below it. `graphics` knows nothing about
scenes/entities/assets; `renderer` is the only module that ties everything together.

## core module system (implemented 2026-07-19)

The engine is **explicitly composed at compile time** — static self-registration
(`is_registered` inline statics, `module_manager`, `type_id`-based registries) is gone.
Rationale: libsbx is an editor-based engine; game-side extensibility lives in scripts,
shaders, and assets — not C++ module registration. The C++ module set is closed and
known at the composition root:

```cpp
auto engine = sbx::core::basic_engine<
  sbx::platform::platform_module,
  sbx::graphics::graphics_module,
  sbx::assets::assets_module,
  sbx::renderer::renderer_module
>{args};

engine.run<demo::application>();
```

Applications normally don't spell this out: `libsbx/engine.hpp` ships
`sbx::engine`, the library-owned default composition (grows as modules land).
Custom lists are for deviating apps (headless tools, tests).

- `core::basic_engine<Modules...>` (template) owns the `engine` core **as a member**
  (composition, not inheritance — no `_base` class exists) plus the modules in
  guaranteed left-to-right construction order via `detail::module_storage` (std::tuple
  is not order-guaranteed); destruction is reverse order. List order = update order
  within each stage. The main loop lives here so stage dispatch is `_dispatch<Stage>()`
  — fully compile-time, no virtuals anywhere. Loop order per frame: `pre` → fixed steps
  (clamped accumulator, max 0.25 s catch-up) → `application::update` → `update` →
  `post` → `render`.
- `core::engine` (non-template, engine.cpp, befriends `basic_engine`) owns timing
  state, cli args, quit, the application, and the static access surface used
  everywhere: `core::engine::get_module<M>()`, `core::engine::delta_time()`,
  `core::engine::fixed_delta_time()` (inside fixed hooks), `core::engine::time()`,
  `core::engine::quit()`, `core::engine::get_application<T>()` — mirroring the
  pre-rework call style.
- Modules are plain default-constructible classes — no base class, no virtuals. Stage
  participation is opt-in by defining parameterless hooks detected via concepts:
  `pre_update` / `fixed_update` / `update` / `post_update` / `render`.
- Dependencies: `using dependencies = core::dependency_list<...>;` — checked by
  `static_assert` (every dependency must appear earlier in the module list); duplicate
  modules are a compile error too.
- `engine::get_module<M>()` resolves via a per-type instance pointer published as each
  module finishes construction and unpublished only **after** its destruction completes
  (`module_slot` uses raw storage + construct_at/destroy_at). Like the old engine,
  `get_module` is therefore valid everywhere — including destructors of module-owned
  objects (e.g. `surface::~surface` resolves the graphics module).

## utility: logger & profiler (reworked 2026-07-19)

- Logger: per-tag `spdlog::logger` instances (named after the tag, rendered via `%n`)
  sharing global sinks from a lazily created `logger_context` (magic static — no
  static-init construction). Single-pass, level-gated formatting (the old design
  eagerly `fmt::format`ed args even for disabled levels, twice). Sinks are `_mt`
  (thread pool ahead), `flush_on(warn)`, file at CWD-relative `logs/sbx.log`
  (the old hardcoded `./demo/logs/` path knew about the demo app from library code).
  Ring-buffer sink kept for a future editor console via `utility::logged_lines()`.
- Profiler: tracy integrated behind `SBX_ENABLE_PROFILING` (CMake option; defaults
  ON for Debug, OFF otherwise). When OFF the tracy dependency is not even fetched
  (`"condition"` key in dependencies.json) and no tracy code is linked. When ON,
  TracyClient links and publicly propagates `TRACY_ENABLE`, from which
  `utility/profiler.hpp` derives `SBX_ENABLE_PROFILING`. Built with
  `TRACY_ON_DEMAND` (zones recorded only while a Tracy server is connected).
  Engine loop has zones per stage + `FrameMark`; connect the Tracy UI to profile.

## platform module (`libsbx/platform/`) — implemented 2026-07-19

Port of the old `devices` module into `sbx::platform` (the old `libsbx/devices/`
directory is deleted):

- **glfw is a private implementation detail** — no public platform header includes
  it. The enums (`key`, `mouse_button`, `input_action`, `input_mod`) carry literal
  values matching the glfw constants, verified by static_asserts in window.cpp;
  window.hpp only forward-declares `GLFWwindow`.
- `window` — signals for window/input events; windowed at the size given in
  `window_create_info` (the old video-mode size override was dropped). Exposes the
  `GLFWwindow*` handle; the graphics `surface` is created via
  `glfwCreateWindowSurface` (decision revised 2026-07-20 — simpler than the
  native-handle variant approach, glfw already abstracts win32/x11/wayland).
- `input` — static keyboard/mouse state; press→repeat transitions run in
  `platform_module::pre_update`. The old viewport/capture state
  (`_active_viewport_*`, `_is_active`, `_is_captured` — imgui-era workarounds) was
  intentionally not ported; viewport-local input mapping is the editor's concern later.
- `events` — window/input event structs dispatched through `signals`.
- `platform_module` — owns the glfw context + window; `pre_update` transitions input
  state and polls events; `required_instance_extensions()` ready for the graphics module.

Verified end-to-end: WM_CLOSE → glfw close callback → `on_window_closed` signal →
application lambda → `engine::quit()` → clean exit. (Debugging this surfaced a
foundation bug: `units::quantity::operator-=` added instead of subtracting, freezing
the fixed-timestep loop — fixed.)

## graphics module (`libsbx/graphics/`)

Scene-agnostic RHI. Suggested layout:

- `devices/` — **implemented 2026-07-20**: `instance` (validation layer + debug
  messenger owned privately — members/functions behind `#if SBX_BUILD_TYPE_DEBUG`,
  zero footprint in release; extension entry points via vkGetInstanceProcAddr).
  `logical_device::set_debug_name(...)` names vulkan objects for validation/RenderDoc
  (PFN via vkGetDeviceProcAddr, inline no-op in release).
  Features split like the old engine: `required()` hard floor
  filters device selection, `optional()` (wide lines, geometry/tess, 8/16-bit types,
  texture compression, compute shader derivatives, ...) merges enable-if-available
  via `features::enabled()`.
  `physical_device` (scored selection, requires 1.3 + required feature set),
  `logical_device` (graphics/dedicated-compute/dedicated-transfer queues with
  graphics-family fallback), VMA `allocator`, and `devices/features.hpp` — the
  single `features` chain used for both device selection and creation.
  Core functions come from the system loader (`find_package(Vulkan)`, own wrapper
  api — no volk); `graphics/vulkan.hpp` is the only header that includes vulkan.
  Verified: RTX 2070 SUPER selected, queues graphics 0 / compute 2 / transfer 1.
- `swapchain` — dynamic-rendering presentation, frames in flight (2), per-frame command
  buffers/sync, per-frame deletion queue. Timeline semaphores for queue sync.
- `resources/` — `buffer`, `image` (all through VMA), staging/upload ring + transfer queue
  submission, mip generation.
- `bindless/` — the heart of the design. One global descriptor set (set 0) with
  update-after-bind variable-count arrays:
  - binding 0: `sampled_image[]`
  - binding 1: `sampler[]`
  - binding 2: `storage_image[]`
  - buffers via **buffer device address** (BDA) — no buffer table needed; push constants
    and material/instance data carry `uint64` addresses or `uint32` handles.
  - Handle types are plain `uint32` indices with a free-list; destruction goes through the
    per-frame deletion queue.
- `shaders/` — slang global session; compiles `.slang` → SPIR-V; reflection from
  `slang::ProgramLayout` produces: push-constant ranges, entry points/stages, parameter
  names/offsets/types for material parameter blocks, and validation against the fixed
  bindless set layout. Shader cache keyed by (path, defines, entry) hash; disk-cache the
  SPIR-V blobs alongside reflection data.
- `pipelines/` — `graphics_pipeline` (built with `VkPipelineRenderingCreateInfo`),
  `compute_pipeline`, pipeline cache keyed by (shader hash, render state, attachment
  formats). Pipeline layout is trivial everywhere: bindless set + push constants.
- `render_graph` — slim pass system: declare passes with read/written resources,
  transient attachment pool, automatic image/buffer barriers and layout transitions,
  linear execution order (no reordering needed initially).

Vulkan feature baseline (already required by SDK version 1.4.x): `dynamicRendering`,
`descriptorIndexing` (+ update-after-bind, partially bound, variable count),
`bufferDeviceAddress`, `timelineSemaphore`, `synchronization2`, `drawIndirectCount`.

## assets module (`libsbx/assets/`)

- `asset_database` — uuid → `asset_metadata` (type, source path, import settings,
  dependency uuids, content hash). Sidecar `.meta` files next to sources hold the uuid +
  import settings; the database scans/loads them.
- `asset_handle<T>` — typed, ref-counted; resolves through the database.
- Loading: thread-pool import/decode off the main thread; GPU uploads handed to the
  graphics staging ring on the main/render thread.
- **Binary cache**: first load of a source asset runs the importer and writes a cooked
  binary (`cache/<uuid>.bin`, versioned header + content hash of the source). Subsequent
  loads memory-map/read the cooked file. Source change (hash mismatch) or importer
  version bump → re-import.
- Asset types:
  - `texture` — stb decode → mips → (later: BC compression) → cooked blob.
  - `mesh` — fastgltf import → meshoptimizer (optimize, LOD chain) → cooked vertex/index
    blobs per submesh + bounds.
  - `material` — references a shader asset uuid + parameter values + texture uuids;
    authored as a small yaml/json file, cooked to binary.
  - `shader` — `.slang` source; cooked = SPIR-V + serialized reflection.

## renderer module (`libsbx/renderer/`)

- **Material system** — shader-per-material (builtin `pbr.slang`, `unlit.slang`, plus
  user surface shaders). `material_key` (evolves the old design): shader hash, alpha mode,
  double-sided, vertex stream mask, feature flags → cached pipeline. Material parameter
  values live in a global GPU material table (storage buffer of `material_data` with
  bindless texture indices); a material instance is just an index.
- **Scene inputs** — transforms + renderable (mesh uuid, per-submesh material uuid)
  from the ecs; octree for spatial queries.
- **Draw list build (per view)**:
  1. Frustum cull via octree.
  2. Bucket survivors by `material_key` (pipeline) → mesh → submesh.
  3. Write per-instance data (transform index, material index, entity id) to a storage
     buffer; write one `VkDrawIndexedIndirectCommand` per (submesh × material) group with
     `instanceCount` = group size.
  4. One `vkCmdDrawIndexedIndirect` (later `...Count`) per pipeline bucket — geometry
     never rebinds thanks to the mega buffers.
- **Frame graph**:
  `depth prepass → cluster light assignment (compute) → opaque forward → skybox →
  transparents (back-to-front) → post stack`
  - Clustered culling: view-space froxel grid (e.g. 16×9×24), compute pass builds
    per-cluster light index lists into storage buffers; forward shaders fetch the
    cluster by fragment position/depth.
- Per-view data (camera, cluster params) in a per-frame uniform/storage buffer referenced
  by BDA from push constants.

## Milestones

- **M0** — platform complete (input/events ported); swapchain clear via dynamic rendering.
- **M1** — graphics core: VMA, frames in flight, slang compile + reflection, triangle.
- **M2** — bindless tables + staging uploads: textured quad, zero per-draw descriptors.
- **M3** — assets: uuid database + binary cache, texture & mesh import, render a mesh.
- **M4** — materials + draw lists: many instances, few indirect draws, unlit shader.
- **M5** — Forward+: depth prepass, clustered light culling, PBR, point/dir lights.
- **M6** — polish: LOD selection, GPU culling (compute writes indirect commands +
  `drawIndirectCount`), transparents, post stack, shader hot reload.

## Dependency changes

- Add: `fastgltf` (dependencies.json).
- Keep: fmt, spdlog, yaml-cpp, glfw, stb, meshoptimizer, Vulkan-Headers, VMA, slang (Find module), nethost.
- Dropped vs old engine: spirv-cross, assimp, tinyobjloader (fastgltf replaces), conan (fetch_dependencies replaces).

## Carried over from the old engine (conceptually)

- Reflection-driven pipeline creation (`graphics/pipeline/shader.*`) → now via slang.
- `material_key` + `basic_material_draw_list` batching (`models/`) → renderer draw lists.
- Bindless-style `material_data` with image/sampler indices → global material table.
- Render graph / pass concept → slim render graph without subrenderer registry coupling.
- Assets thread pool + uuid handles → assets module with binary cache added.
