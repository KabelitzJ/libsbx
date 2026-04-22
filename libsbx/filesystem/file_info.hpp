// SPDX-License-Identifier: MIT
#ifndef LIBSBX_FILESYSTEM_FILE_INFO_HPP_
#define LIBSBX_FILESYSTEM_FILE_INFO_HPP_

#include <string>
#include <filesystem>

namespace sbx::filesystem {

class file_info final {

public:

  file_info(const std::string& alias_path, const std::string& base_path, const std::string& file_name) {
    _initialize(alias_path, base_path, file_name);
  }

  file_info() = delete;

  ~file_info() = default;
  
  inline auto filename() const -> const std::string& {
    return _filename;
  }
  
  inline auto base_filename() const -> const std::string& {
    return _base_filename;
  }

  inline auto extension() const -> const std::string& {
    return _extension;
  }

  inline auto file_path() const -> const std::string& {
    return _filepath;
  }

  inline auto virtual_path() const -> const std::string& {
    return _virtual_path;
  }


  inline auto native_path() const -> const std::string& {
    return _native_path;
  }

private:

  auto _initialize(const std::string& alias_path, const std::string& base_path, const std::string& file_name) -> void {
    auto stripped_file_name = std::string{file_name};
  
    if (!base_path.empty() && file_name.find(base_path) == 0) {
      stripped_file_name = file_name.substr(base_path.length());
    } else if (!alias_path.empty() && file_name.find(alias_path) == 0) {
      stripped_file_name = file_name.substr(alias_path.length());
    }

    while (!stripped_file_name.empty() && (stripped_file_name.front() == '/' || stripped_file_name.front() == '\\')) {
      stripped_file_name.erase(stripped_file_name.begin());
    }

    const auto file_path = std::filesystem::path{stripped_file_name};

    _filepath = file_path.generic_string();
    _virtual_path = (std::filesystem::path{alias_path} / file_path).generic_string();
    _native_path = (std::filesystem::path{base_path} / file_path).generic_string();

    _filename = file_path.filename().string();
    _extension = file_path.has_extension() ? file_path.extension().string() : "";
    _base_filename = file_path.stem().string();
  }

  std::string _filename;
  std::string _base_filename;
  std::string _extension;
  
  std::string _filepath;
  std::string _virtual_path;
  std::string _native_path;

}; // class file_info

inline auto operator==(const file_info& lhs, const file_info& rhs) -> bool {
  return lhs.virtual_path() == rhs.virtual_path();
}
    
inline auto operator<(const file_info& lhs, const file_info& rhs) -> bool {
  return lhs.virtual_path() < rhs.virtual_path();
}

} // namespace sbx::filesystem

#endif // LIBSBX_FILESYSTEM_FILE_INFO_HPP_
