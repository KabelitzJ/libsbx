// SPDX-License-Identifier: MIT
#include <span>
#include <ranges>
#include <stacktrace>

#include <easy/profiler.h>

#include <range/v3/all.hpp>

#include <libsbx/utility/target.hpp>
#include <libsbx/utility/memory_tracker.hpp>

#include <libsbx/core/core.hpp>
#include <libsbx/core/engine.hpp>
#include <libsbx/core/exit.hpp>

#include <demo/application.hpp>

auto main(int argc, const char** argv) -> int {
  EASY_PROFILER_ENABLE;
  EASY_MAIN_THREAD;

  profiler::startListen();

  auto args = std::vector<std::string_view>{argv, argv + argc};

  EASY_BLOCK("main");

  sbx::utility::memory_tracker::instance().initialize({
    .track_allocations = true,
    .track_deallocations = true,
    .capture_callstacks = false,
    .budget_warning_threshold = 0
  });

  SBX_MEMORY_TRACKER_SCOPE("demo", "main");

  try {
    auto engine = std::make_unique<sbx::core::engine>(args);

    engine->run<demo::application>();
  } catch(const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure; 
  }

  EASY_END_BLOCK;

  profiler::stopListen();

  profiler::dumpBlocksToFile("libsbx.profile");

  sbx::utility::memory_tracker::instance().shutdown();

  return sbx::core::exit::success;
}
