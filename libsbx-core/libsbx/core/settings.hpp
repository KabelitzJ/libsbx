#ifndef LIBSBX_CORE_SETTINGS_HPP_
#define LIBSBX_CORE_SETTINGS_HPP_

#include <any>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <string>
#include <unordered_map>
#include <variant>


#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/memory/observer_ptr.hpp>

namespace sbx::core {

namespace prototype {

template<std::default_initializable Type>
struct setting_range {
  Type min{};
  Type max{};
}; // struct setting_range

template<typename Type>
struct setting_key {
  std::string_view name;
  std::optional<setting_range<Type>> range{std::nullopt};
}; // struct setting_key

class settings {

public:

  template<typename Type>
  static auto set(setting_key<Type> key, Type value) -> void {
    auto lock = std::scoped_lock{_mutex};

    _data[std::string{key.name}] = std::move(value);
  }

  template<typename Type>
  static auto get(setting_key<Type> key) -> Type {
    auto lock = std::scoped_lock{_mutex};

    auto entry = _data.find(std::string{key.name});

    if (entry == _data.end()) {
      throw std::runtime_error("setting not found");
    }

    return std::any_cast<Type>(entry->second);
  }

  template<typename Type>
  static Type get_or(setting_key<Type> key, const Type& fallback) {
    auto lock = std::scoped_lock{_mutex};

    auto entry = _data.find(std::string{key.name});

    if (entry == _data.end()) {
      return fallback;
    }

    return std::any_cast<Type>(entry->second);
  }

private:

  static inline std::unordered_map<std::string, std::any> _data;
  static inline std::mutex _mutex;

}; // class settings

}; // namespace prototype

template<typename Type>
concept setting_type = std::is_same_v<Type, bool> || std::is_same_v<Type, std::uint32_t> || std::is_same_v<Type, std::int32_t> || std::is_same_v<Type, std::float_t> || std::is_same_v<Type, std::string>;

class settings {

public:

  using entry_type = std::variant<std::monostate, bool, std::uint32_t, std::int32_t, std::float_t, std::string>;

  struct value_type {
    entry_type entry;
    entry_type min;
    entry_type max;
  };

  struct group_entry {
    utility::hashed_string name;
    value_type& value;
  }; // struct group_entry

  struct group {
    utility::hashed_string name;
    std::vector<group_entry> entries;
  }; // struct group

  settings() = default;

  template<setting_type Type>
  auto set(const utility::hashed_string& key, const Type& value) -> void {
    _settings[key] = value_type{value, std::monostate{}, std::monostate{}};
  }

  template<setting_type Type>
  auto set(const utility::hashed_string& key, const Type& value, const Type& min, const Type& max) -> void {
    _settings[key] = value_type{value, min, max};
  }

  template<setting_type Type>
  auto get(const utility::hashed_string& key) const -> memory::observer_ptr<const Type> {
    if (auto entry = _settings.find(key); entry != _settings.end()) {
      return std::get_if<Type>(&entry->second.entry);
    }

    return nullptr;
  }

  // template<setting_type Type>
  // auto get_or_set(const utility::hashed_string& key, const Type& default_value) -> memory::observer_ptr<const Type> {
  //   if (auto entry = _settings.find(key); entry != _settings.end()) {
  //     return std::get_if<Type>(&entry->second);
  //   }

  //   set(key, default_value);
    
  //   return get<Type>(key);
  // }

  template<typename Callable>
  requires (std::is_invocable_v<Callable, const utility::hashed_string&, group&>)
  auto for_each(Callable&& callable) -> void {
    auto grouped = std::unordered_map<utility::hashed_string, group>{};

    for (auto& [key, value] : _settings) {
      const auto position = key.rfind("::");

      const auto has_namespace = (position != utility::hashed_string::npos);

      const auto group_name = has_namespace ? key.substr(0, position) : key;
      const auto entry_name = has_namespace ? key.substr(position + 2u) : key;

      grouped[group_name].name = group_name;
      grouped[group_name].entries.emplace_back(group_entry{entry_name, value});
    }

    for (auto& [group_name, group] : grouped) {
      std::invoke(callable, group_name, group);
    }
  }

private:

  std::unordered_map<utility::hashed_string, value_type> _settings;

}; // class settings

} // namespace sbx::core

#endif // LIBSBX_CORE_SETTINGS_HPP_
