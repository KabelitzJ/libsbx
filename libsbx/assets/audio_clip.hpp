// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_AUDIO_CLIP_HPP_
#define LIBSBX_ASSETS_AUDIO_CLIP_HPP_

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>

namespace sbx::assets {

/** @brief How a cooked clip's payload is stored. */
enum class audio_clip_encoding : std::uint8_t {
  pcm_s16, // interleaved signed 16-bit frames, ready for one shared OpenAL buffer
  vorbis   // the source's Ogg Vorbis bytes, decoded while playing
}; // enum class audio_clip_encoding

/**
 * @brief Decoded or still-encoded audio held on the CPU; the audio module turns it into OpenAL buffers.
 *
 * For a vorbis clip, channels() is the output channel count after import settings (1 when forced to mono), which the streaming decoder downmixes to.
 */
class audio_clip final : public loadable {

  friend class asset_residency;

public:

  audio_clip() = default;

  [[nodiscard]] auto sample_rate() const noexcept -> std::uint32_t {
    return _sample_rate;
  }

  [[nodiscard]] auto channels() const noexcept -> std::uint32_t {
    return _channels;
  }

  [[nodiscard]] auto frame_count() const noexcept -> std::uint64_t {
    return _frame_count;
  }

  [[nodiscard]] auto duration() const noexcept -> std::float_t {
    return _sample_rate > 0u ? static_cast<std::float_t>(_frame_count) / static_cast<std::float_t>(_sample_rate) : 0.0f;
  }

  [[nodiscard]] auto encoding() const noexcept -> audio_clip_encoding {
    return _encoding;
  }

  [[nodiscard]] auto samples() const noexcept -> std::span<const std::int16_t> {
    return _samples;
  }

  [[nodiscard]] auto encoded() const noexcept -> std::span<const std::byte> {
    return _encoded;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

private:

  // Fills a placeholder clip once its cooked content arrives (main thread). Bumps generation() so the audio module re-uploads its buffer.
  auto _finalize_content(std::uint32_t sample_rate, std::uint32_t channels, std::uint64_t frame_count, audio_clip_encoding encoding, std::vector<std::int16_t> samples, std::vector<std::byte> encoded) -> void {
    _sample_rate = sample_rate;
    _channels = channels;
    _frame_count = frame_count;
    _encoding = encoding;
    _samples = std::move(samples);
    _encoded = std::move(encoded);
    _bump_generation();
  }

  std::uint32_t _sample_rate{0u};
  std::uint32_t _channels{0u};
  std::uint64_t _frame_count{0u};
  audio_clip_encoding _encoding{audio_clip_encoding::pcm_s16};
  std::vector<std::int16_t> _samples{};
  std::vector<std::byte> _encoded{};
  math::uuid _id{math::uuid::nil()};

}; // class audio_clip

using audio_clip_handle = asset_handle<audio_clip>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_AUDIO_CLIP_HPP_
