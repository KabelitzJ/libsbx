// SPDX-License-Identifier: MIT
// #ifndef LIBSBX_SCENES_HIERARCHY_MODULE_HPP_
// #define LIBSBX_SCENES_HIERARCHY_MODULE_HPP_

// #include <easy/profiler.h>

// #include <libsbx/utility/timer.hpp>

// #include <libsbx/core/module.hpp>
// #include <libsbx/core/engine.hpp>
// #include <libsbx/core/profiler.hpp>


// #include <libsbx/scenes/scenes_module.hpp>

// #include <libsbx/scenes/components/relationship.hpp>
// #include <libsbx/scenes/components/global_transform.hpp>
// #include <libsbx/scenes/components/transform.hpp>

// namespace sbx::scenes {

// class hierarchy_module final : public core::module<hierarchy_module> {

//   friend class scene;

//   inline static const auto is_registered = register_module(stage::post);

//   struct stack_entry {
//     scenes::node node;
//     math::matrix4x4 parent_world;
//     bool is_parent_dirty;
//   }; // struct stack_entry

// public:

//   hierarchy_module() {
    
//   }

//   ~hierarchy_module() override = default;

//   auto update() -> void override {
//     SBX_PROFILE_SCOPE("hierarchy_module");

//     auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//     auto& scene = scenes_module.scene();

//     auto root = scene.root();

//     EASY_BLOCK("updating global transforms");

//     auto stack = std::vector<stack_entry>{};
//     stack.reserve(256u);

//     stack.push_back({root, math::matrix4x4::identity, false});

//     while (!stack.empty()) {
//       const auto [current, parent_matrix, is_parent_dirty] = stack.back();
//       stack.pop_back();

//       auto& transform = scene.get_component<scenes::transform>(current);
//       auto& global_transform = scene.get_component<scenes::global_transform>(current);
//       const auto& relationship = scene.get_component<const scenes::relationship>(current);

//       const auto is_dirty = is_parent_dirty || transform.is_dirty();

//       if (is_dirty) {
//         global_transform.model = parent_matrix * transform.local_transform();
//         global_transform.normal = math::matrix4x4::transposed(math::matrix4x4::inverted(global_transform.model));
//         transform.clear_is_dirty();
//       }

//       for (const auto child : relationship.children()) {
//         if (child != scenes::node::null) {
//           stack.push_back({child, global_transform.model, is_dirty});
//         }
//       }
//     }

//     EASY_END_BLOCK;
//   }

// }; // class hierarchy_module

// } // namespace sbx::scenes

// #endif // LIBSBX_SCENES_HIERARCHY_MODULE_HPP_
