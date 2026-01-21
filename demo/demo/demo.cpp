#include <span>
#include <ranges>
#include <stacktrace>

#include <easy/profiler.h>

#include <range/v3/all.hpp>

#include <libsbx/utility/target.hpp>

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

  return sbx::core::exit::success;
}
