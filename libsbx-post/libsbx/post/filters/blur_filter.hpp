// SPDX-License-Identifier: MIT
#ifndef LIBSBX_POST_FILTERS_BLUR_FILTER_HPP_
#define LIBSBX_POST_FILTERS_BLUR_FILTER_HPP_

#include <libsbx/utility/enum.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/post/filter.hpp>

namespace sbx::post {

enum class blur_type : std::uint32_t {
  gaussian_5 = 0,
  gaussian_9 = 1,
  gaussian_13 = 2
}; // enum class blur_type

template<blur_type Type>
class blur_filter final : public filter {

  using base = filter;

  static auto _specialization_info(const std::uint32_t id, const std::uint32_t value) -> const VkSpecializationInfo* {
    static auto specialization_map_entry = VkSpecializationMapEntry{};
    specialization_map_entry.constantID = id;
    specialization_map_entry.offset = 0u;
    specialization_map_entry.size = sizeof(std::uint32_t);

    static auto specialization_data = std::uint32_t{value};

    static auto specialization_info = VkSpecializationInfo{};
    specialization_info.mapEntryCount = 1u;
    specialization_info.pMapEntries = &specialization_map_entry;
    specialization_info.dataSize = sizeof(std::uint32_t);
    specialization_info.pData = &specialization_data;

    return &specialization_info;
  }

public:

  inline static constexpr auto type = Type;

  blur_filter(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, const std::string& attachment_name, const math::vector2& direction)
  : base{attachments, path, base::default_pipeline_definition, _specialization_info(0u, utility::to_underlying(Type))},
    _push_handler{base::pipeline()},
    _attachment_name{attachment_name},
    _direction{direction} { }

  ~blur_filter() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    auto& pipeline = base::pipeline();
    auto& descriptor_handler = base::descriptor_handler();

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    
    pipeline.bind(command_buffer);

    _push_handler.push("direction", _direction);
    // _push_handler.push("type", utility::to_underlying(type));

    descriptor_handler.push("image", graphics_module.attachment(_attachment_name));

    if (!descriptor_handler.update(pipeline)) {
      return;
    }

    descriptor_handler.bind_descriptors(command_buffer);
    _push_handler.bind(command_buffer);

    command_buffer.draw(3, 1, 0, 0);
  }

private:

  graphics::push_handler _push_handler;

  std::string _attachment_name;
  math::vector2 _direction;

}; // class blur_filter

using blur_filter_gaussian_5 = blur_filter<blur_type::gaussian_5>;

using blur_filter_gaussian_9 = blur_filter<blur_type::gaussian_9>;

using blur_filter_gaussian_13 = blur_filter<blur_type::gaussian_13>;

} // namespace sbx::post

#endif // LIBSBX_POST_FILTERS_BLUR_FILTER_HPP_
