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

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/pipeline/pipeline.hpp>

#include <libsbx/graphics/task.hpp>
#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/render_graph.hpp>
#include <libsbx/graphics/draw_list.hpp>
#include <libsbx/graphics/viewport.hpp>

namespace sbx::graphics {

class renderer : utility::noncopyable {

public:

  renderer() = default;

  virtual ~renderer() = default;

  auto render(command_buffer& command_buffer, const swapchain& swapchain) -> void {
    for (auto& [key, draw_list] : _draw_lists) {
      draw_list->clear();
      draw_list->update();
    }

    _graph.execute(command_buffer, swapchain, [this, &command_buffer](const pass_handle& pass) -> void {
      for (const auto& subrenderer : _subrenderers[pass.index]) {
        subrenderer->render(command_buffer);
      }
    });
  }

  auto execute_tasks(command_buffer& command_buffer) -> void {
    for (const auto& task : _tasks) {
      task->execute(command_buffer);
    }
  }

  auto resize(const viewport::type flags) -> void {
    _graph.resize(flags);
  }

  auto attachment(const std::string& name) const -> const descriptor& {
    return _graph.find_attachment(name);
  }

  template<typename Type>
  requires (std::is_base_of_v<draw_list, Type>)
  auto draw_list(const utility::hashed_string& name) -> Type& {
    if (auto entry = _draw_lists.find(name); entry != _draw_lists.end()) {
      return *static_cast<Type*>(entry->second.get());
    }

    throw utility::runtime_error{"Draw list with name '{}' not found", name.str()};
  }

protected:

  template<typename Type, typename... Args>
  requires (std::is_constructible_v<Type, const std::vector<sbx::graphics::attachment_description>&, Args...>)
  auto add_subrenderer(const pass_handle handle, Args&&... args) -> Type& {
    _subrenderers.resize(std::max(_subrenderers.size(), static_cast<std::size_t>(handle.index + 1)));

    auto& subrenderers = _subrenderers[handle.index];

    subrenderers.emplace_back(std::make_unique<Type>(_graph.attachment_descriptions(handle), std::forward<Args>(args)...));

    return *static_cast<Type*>(subrenderers.back().get());
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

  std::vector<std::unique_ptr<graphics::task>> _tasks;

  std::vector<std::vector<std::unique_ptr<subrenderer>>> _subrenderers;

  std::unordered_map<utility::hashed_string, std::unique_ptr<graphics::draw_list>> _draw_lists;

  render_graph _graph;

}; // class renderer

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_RENDERER_HPP_
