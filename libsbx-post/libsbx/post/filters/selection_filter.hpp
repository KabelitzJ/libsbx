#ifndef LIBSBX_POST_SELECTION_FILTER_HPP_
#define LIBSBX_POST_SELECTION_FILTER_HPP_

#include <libsbx/core/profiler.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/post/filter.hpp>

namespace sbx::post {

class selection_filter final : public filter {

  using base = filter;

public:

  selection_filter(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, std::vector<std::pair<std::string, std::string>>&& attachment_names)
  : base{attachments, path},
    _push_handler{base::pipeline()},
    _attachment_names{std::move(attachment_names)} { }

  ~selection_filter() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    SBX_PROFILE_SCOPE("selection_filter::render");

    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    auto& pipeline = base::pipeline();
    auto& descriptor_handler = base::descriptor_handler();

    pipeline.bind(command_buffer);

    _push_handler.push("color", sbx::math::color{1.0, 0.86, 0.49, 1.0});
    _push_handler.push("thickness", 0.5f);

    descriptor_handler.push("scene", scene.uniform_handler());
    // descriptor_handler.push("resolve_image", graphics_module.attachment(_image));
    // descriptor_handler.push("object_id_image", graphics_module.attachment(_object_id_image));
    // descriptor_handler.push("linear_depth_image", graphics_module.attachment(_linear_depth_image));

    for (const auto& [name, attachment] : _attachment_names) {
      descriptor_handler.push(name, graphics_module.attachment(attachment));
    }

    if (!descriptor_handler.update(pipeline)) {
      return;
    }

    descriptor_handler.bind_descriptors(command_buffer);
    _push_handler.bind(command_buffer);

    command_buffer.draw(3, 1, 0, 0);
  }

private:

  graphics::push_handler _push_handler;

  std::vector<std::pair<std::string, std::string>> _attachment_names;

  std::string _image;
  std::string _object_id_image;
  std::string _linear_depth_image;

}; // class selection_filter

} // namespace sbx::post 

#endif // LIBSBX_POST_SELECTION_FILTER_HPP_
