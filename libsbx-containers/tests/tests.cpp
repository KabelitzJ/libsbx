// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <libsbx/containers/task_graph.hpp>

auto main(int argc, char* argv[]) -> int {
  testing::InitGoogleTest(&argc, argv);

  // auto render_graph = sbx::render_graph{"render_graph"};

  // auto [shadow, g_buffer, deferred] = render_graph.emplace(
  //   [](auto& ctx) {
  //     auto shadow_pass = ctx.pass("shadow");

  //     shadow_pass.output("shadow_map", 0, type::image, default_clear_color, format::r8g8b8a8_unorm);

  //     return shadow_pass;
  //   },
  //   [](auto& ctx){
  //     auto g_buffer_pass = ctx.pass("g_buffer");

  //     g_buffer_pass.output("depth", 0, type::depth);
  //     g_buffer_pass.output("albedo", 1, type::image, default_clear_color, format::r8g8b8a8_unorm);
  //     g_buffer_pass.output("normal", 2, type::image, default_clear_color, format::r8g8b8a8_unorm);

  //     return g_buffer_pass;
  //   },
  //   [](auto& ctx){
  //     auto deferred_pass = ctx.pass("deferred");

  //     deferred_pass.input("shadow");
  //     deferred_pass.input("depth");
  //     deferred_pass.input("albedo");
  //     deferred_pass.input("normal");

  //     deferred_pass.output("swapchain", 0, type::swapchain, default_clear_color, format::r8g8b8a8_unorm);
  //   }
  // );

  // deferred.succeed(shadow, g_buffer);

  return RUN_ALL_TESTS();
}
