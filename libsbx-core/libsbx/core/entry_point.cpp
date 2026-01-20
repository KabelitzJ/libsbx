#include <libsbx/core/entry_point.hpp>

#include <easy/profiler.h>

#include <span>
#include <ranges>
#include <stacktrace>

#include <range/v3/all.hpp>

#include <libsbx/utility/target.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/exit.hpp>

auto main(int argc, const char** argv) -> int {
  EASY_PROFILER_ENABLE;
  EASY_MAIN_THREAD;
  profiler::startListen();

  auto args = std::vector<std::string_view>{argv, argv + argc};

  if constexpr (sbx::utility::is_build_configuration_debug_v) {
    sbx::utility::logger<"core">::debug("Cli args:");

    for (const auto& arg : args) {
      sbx::utility::logger<"core">::debug("  {}", arg);
    }
  }

  EASY_BLOCK("main");

  try {
    auto engine = std::make_unique<sbx::core::engine>(args, sbx::core::create_application);

    engine->run();
  } catch(const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure; 
  }

  EASY_END_BLOCK;

  profiler::stopListen();

  profiler::dumpBlocksToFile("libsbx.profile");

  return sbx::core::exit::success;
}
