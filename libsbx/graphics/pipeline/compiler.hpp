// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_PIPELINE_COMPILER_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_COMPILER_HPP_

#include <vulkan/vulkan.hpp>

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/target.hpp>

#include <libsbx/memory/blob.hpp>

namespace sbx::graphics {

class compiler {

public:

  struct define {
    std::string key;
    std::string value;
  }; // struct define

  struct compile_request {

    struct per_stage {
      std::string entry_point{"main"};
      std::vector<std::string> specializations{};
    }; // struct per_stage

    std::filesystem::path path{};
    std::vector<define> defines{};
    std::unordered_map<SlangStage, compile_request::per_stage> per_stage{};

  }; // struct compile_request

  struct compile_result {
    std::unordered_map<SlangStage, std::vector<std::uint32_t>> code;
  }; // struct compile_result

  compiler();

  ~compiler();

  auto compile(const compile_request& compile_request) -> compile_result;

private:

  static auto _read_file(const std::filesystem::path& path) -> std::string;

  auto _create_session(const compile_request& compile_request, SlangStage stage) -> Slang::ComPtr<slang::ISession>;

  Slang::ComPtr<slang::IGlobalSession> _global_session;

}; // class compiler

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_COMPILER_HPP_
