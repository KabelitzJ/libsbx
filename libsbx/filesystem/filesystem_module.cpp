// SPDX-License-Identifier: MIT
#include <libsbx/filesystem/filesystem_module.hpp>

namespace sbx::filesystem {

filesystem_module::filesystem_module()
: _filesystem{std::make_unique<virtual_filesystem>()} {
    
}

filesystem_module::~filesystem_module() {

}

auto filesystem_module::update() -> void {
  SBX_PROFILE_SCOPE("filesystem_module::update");
}

} // namespace sbx::filesystem
