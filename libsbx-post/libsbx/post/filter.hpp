#ifndef LIBSBX_POST_FILTER_HPP_
#define LIBSBX_POST_FILTER_HPP_

#include <string>
#include <map>

#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/pipeline/vertex_input_description.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>


namespace sbx::post {

class filter : public graphics::subrenderer {

public:

  using pipeline_type = graphics::graphics_pipeline;

  filter(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, const graphics::pipeline_definition& pipeline_definition = default_pipeline_definition, const VkSpecializationInfo* specialization_info = nullptr)
  : graphics::subrenderer{},
    _pipeline{path, attachments, pipeline_definition, specialization_info},
    _descriptor_handler{_pipeline, 0u} { }

  virtual ~filter() override = default;

  auto descriptor_handler() noexcept -> graphics::descriptor_handler& {
    return _descriptor_handler;
  }

  auto pipeline() noexcept -> pipeline_type& {
    return _pipeline;
  }

  auto attachment(const std::string& descriptor_name, const graphics::descriptor& descriptor) -> const graphics::descriptor& {
    if (auto it = _descriptors.find(descriptor_name); it != _descriptors.end()) {
      return *(it->second);
    }

    return descriptor;
  }

  auto attachment(const std::string& descriptor_name, const std::string& render_attachment) -> const graphics::descriptor& {
    if (auto it = _descriptors.find(descriptor_name); it != _descriptors.end()) {
      return *(it->second);
    }

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    return graphics_module.attachment(render_attachment);
  }

  auto set_attachment(const std::string& descriptor_name, const graphics::descriptor& descriptor) -> void {
    if (auto entry = _descriptors.find(descriptor_name); entry != _descriptors.end()) {
      entry->second = &descriptor;
    } else {
      _descriptors.emplace(descriptor_name, &descriptor);
    }
  }

  auto remove_attachment(const std::string& descriptor_name) -> bool {
    if (auto it = _descriptors.find(descriptor_name); it != _descriptors.end()) {
      _descriptors.erase(it);
      return true;
    }

    return false;
  }

protected:

  inline static const auto default_pipeline_definition = graphics::pipeline_definition{
    .depth = graphics::depth::disabled,
    .uses_transparency = false,
    .rasterization_state = graphics::rasterization_state{
      .polygon_mode = graphics::polygon_mode::fill,
      .cull_mode = graphics::cull_mode::none,
      .front_face = graphics::front_face::counter_clockwise
    }
  };

private:

  std::map<std::string, memory::observer_ptr<const graphics::descriptor>> _descriptors;

  pipeline_type _pipeline;
  graphics::descriptor_handler _descriptor_handler;

}; // class filter

} // namespace sbx::post

#endif // LIBSBX_POST_FILTER_HPP_
