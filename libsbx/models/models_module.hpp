// models_module.hpp
#ifndef LIBSBX_MODELS_MODELS_MODULE_HPP_
#define LIBSBX_MODELS_MODELS_MODULE_HPP_

#include <libsbx/core/module.hpp>

namespace sbx::models {

class models_module final : public core::module<models_module> {

  inline static const auto is_registered = register_module(stage::normal);

public:

  models_module();

  ~models_module() override;

  auto update() -> void override;

}; // class models_module

} // namespace sbx::models

#endif // LIBSBX_MODELS_MODELS_MODULE_HPP_
