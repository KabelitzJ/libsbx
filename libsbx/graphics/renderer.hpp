// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_RENDERER_HPP_
#define LIBSBX_GRAPHICS_RENDERER_HPP_

#include <memory>
#include <vector>
#include <typeindex>

#include <easy/profiler.h>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/concepts.hpp>
#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/type_id.hpp>
#include <libsbx/utility/type_name.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/pipeline/pipeline.hpp>

#include <libsbx/graphics/task.hpp>
#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/render_graph.hpp>
#include <libsbx/graphics/draw_list.hpp>
#include <libsbx/graphics/viewport.hpp>

namespace sbx::graphics {

namespace detail {

struct graphics_type_id_scope { };

} // namespace detail

/**
 * @brief A scoped type ID generator for the libsbx-graphics scope.
 *
 * @tparam Type The type for which the ID is generated.
 */
template<typename Type>
using type_id = utility::scoped_type_id<detail::graphics_type_id_scope, Type>;

class renderer : utility::noncopyable {

public:

  renderer() = default;

  virtual ~renderer() = default;

  auto render(command_buffer& command_buffer, const swapchain& swapchain) -> void {
    for (auto& [key, draw_list] : _draw_lists) {
      draw_list->clear();
      draw_list->update();
    }

    auto compute_callback = [this, &command_buffer](const pass_handle& pass) -> void {
      if (pass.index >= _tasks.size()) {
        return;
      }

      for (const auto& task : _tasks[pass.index]) {
        task->execute(command_buffer);
      }
    };

    auto pass_callback = [this, &command_buffer](const pass_handle& pass) -> void {
      if (pass.index >= _subrenderers.size()) {
        return;
      }

      for (const auto& subrenderer : _subrenderers[pass.index]) {
        subrenderer->render(command_buffer);
      }
    };

    _graph.execute(command_buffer, swapchain, pass_callback, compute_callback);
  }

  auto resize(const std::string& viewport_name) -> void {
    _graph.resize(viewport_name);
  }

  auto attachment(const std::string& name) const -> const descriptor& {
    return _graph.find_attachment(name);
  }

  template<typename Type>
  requires (std::is_base_of_v<graphics::draw_list, Type>)
  auto draw_list(const utility::hashed_string& name) -> Type& {
    if (auto entry = _draw_lists.find(name); entry != _draw_lists.end()) {
      return *static_cast<Type*>(entry->second.get());
    }

    throw utility::runtime_error{"Draw list with name '{}' not found", name.str()};
  }

  template<typename Type>
  requires (std::is_base_of_v<graphics::task, Type>)
  auto task() -> Type& {
    const auto type = type_id<Type>::value();

    if (auto entry = _task_by_id.find(type); entry != _task_by_id.end()) {
      auto [pass, index] = entry->second;

      return *static_cast<Type*>(_tasks[pass][index].get());
    }

    throw utility::runtime_error{"Draw list with name '{}' not found", utility::type_name<Type>()};
  }

protected:

  template<typename Type, typename... Args>
  requires (std::is_base_of_v<graphics::subrenderer, Type> && std::is_constructible_v<Type, const std::vector<sbx::graphics::attachment_description>&, Args...>)
  auto add_subrenderer(const pass_handle handle, Args&&... args) -> Type& {
    utility::assert_that(handle.is_valid(), "Invalid pass handle in add_subrenderer()");

    if (_graph.pass_kind(handle) != pass_node::kind::graphics) {
      throw utility::runtime_error{"add_subrenderer() can only be used with graphics passes"};
    }

    _subrenderers.resize(std::max(_subrenderers.size(), static_cast<std::size_t>(handle.index + 1)));

    auto& subrenderers = _subrenderers[handle.index];

    subrenderers.emplace_back(std::make_unique<Type>(_graph.attachment_descriptions(handle), std::forward<Args>(args)...));

    return *static_cast<Type*>(subrenderers.back().get());
  }

  template<typename Type, typename... Args>
  requires (std::is_base_of_v<graphics::task, Type> && std::is_constructible_v<Type, Args...>)
  auto add_task(const pass_handle handle, Args&&... args) -> Type& {
    utility::assert_that(handle.is_valid(), "Invalid pass handle in add_compute_task()");

    const auto type = type_id<Type>::value();

    if (_graph.pass_kind(handle) != pass_node::kind::compute) {
      throw utility::runtime_error{"add_compute_task() can only be used with compute passes"};
    }

    _tasks.resize(std::max(_tasks.size(), static_cast<std::size_t>(handle.index + 1)));

    auto& tasks = _tasks[handle.index];

    _task_by_id.emplace(type, std::make_pair(handle.index, tasks.size()));

    tasks.emplace_back(std::make_unique<Type>(std::forward<Args>(args)...));

    return *static_cast<Type*>(tasks.back().get());
  }

  template<typename Type, typename... Args>
  requires (std::is_constructible_v<Type, Args...>)
  auto add_draw_list(const utility::hashed_string& name, Args&&... args) -> Type& {
    auto result = _draw_lists.emplace(name, std::make_unique<Type>(std::forward<Args>(args)...));

    return *static_cast<Type*>(result.first->second.get());
  }

  template<typename... Args>
  auto create_attachment(Args&&... args) -> attachment_handle {
    return _graph.create_attachment(std::forward<Args>(args)...);
  }

  template<typename Callable>
  auto create_pass(Callable&& callable) -> pass_handle {
    return _graph.create_pass(std::forward<Callable>(callable));
  }

  auto build_render_graph() -> void {
    _graph.build();
  }

  auto attachment_descriptions(const pass_handle handle) const -> std::vector<attachment_description> {
    return _graph.attachment_descriptions(handle);
  }

private:

  std::vector<std::vector<std::unique_ptr<graphics::subrenderer>>> _subrenderers;
  std::vector<std::vector<std::unique_ptr<graphics::task>>> _tasks;

  std::unordered_map<std::uint32_t, std::pair<std::uint32_t, std::size_t>> _task_by_id;

  std::unordered_map<utility::hashed_string, std::unique_ptr<graphics::draw_list>> _draw_lists;

  render_graph _graph;

}; // class renderer

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_RENDERER_HPP_
