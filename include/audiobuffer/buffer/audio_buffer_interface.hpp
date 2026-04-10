#pragma once

// *****************************************************************************

#include <cstdint>
#include <functional>

#include "../internal/macro.hpp"

#include "./types.hpp"
#include "./audio_buffer_data.h"

// *****************************************************************************

namespace audiobuffer {

// *****************************************************************************

/// @brief Structure to pass when calling a copy command.
struct CopyArgs {
  /// @brief How many frames to copy. A value of `0` will copy all.
  frame_count_t frame_count = 0;
  /// @brief How many channels to copy. A value of `0` will copy all.
  channel_count_t channel_count = 0;

  /// @brief Offset where to start copying frames from.
  frame_count_t src_offset = 0;
  /// @brief Offset where to start copying channels from.
  channel_count_t src_channel_offset = 0;

  /// @brief Offset where to start copying frames to.
  frame_count_t dst_offset = 0;
  /// @brief Offset where to start copying channels to.
  channel_count_t dst_channel_offset = 0;

  // TODO: Remove?
  bool pad = false;
};

static const CopyArgs COPY_ARGS_DEFAULT = CopyArgs();

// *****************************************************************************

///
/// @brief Common interface for audio buffers.
///
class AudioBufferInterface
{
  public: virtual ~AudioBufferInterface() {}

  ///
  /// @brief Checks whether the audio data is not null and has a non-zero size.
  ///
  virtual bool has_data() const noexcept
  =0;

  ///
  /// @brief Gets the raw data structure managed by the audio buffer instance.
  ///
  /// @note Mainly for internal use or for interfacing with other languages.
  ///
  virtual AudioBufferData* get_data() const noexcept
  =0;

  ///
  /// @brief Gets the format ID value.
  ///
  virtual format_id_t get_format_id() const noexcept
  =0;

  ///
  /// @brief Gets the amount of frames (samples per channel) in the audio buffer.
  ///
  virtual frame_count_t get_frame_count() const noexcept
  =0;

  ///
  /// @brief Gets the amount of channels in the audio buffer.
  ///
  virtual channel_count_t get_channel_count() const noexcept
  =0;

  ///
  /// @brief Checks whether the audio buffer is a reference and is not managed.
  ///
  virtual bool is_reference() const noexcept
  =0;

  // TODO: Add can_resize() and set_resize_block()

  ///
  /// @brief Clears the buffer with DC center values.
  ///
  virtual void clear() noexcept
  =0;

  ///
  /// @brief Resizes contents of the audio buffer.
  ///
  /// @param frame_count The new amount of frames (samples per channel).
  ///   A value of zero retains the current amount of frames.
  /// @param channel_count The new amount of channels.
  ///   A value of zero retains the current amount of channels.
  ///
  /// @return Whether the operation was successful.
  ///
  virtual bool resize(frame_count_t frame_count, channel_count_t channel_count=0) audiobuffer__noexcept
  =0;

  ///
  /// @brief Duplicates contents from one channel to another.
  ///
  /// @param src_channel_index Channel to copy from.
  /// @param dst_channel_index Channel to copy to.
  ///
  /// @return Whether the operation was successful.
  ///
  virtual bool duplicate_channel(channel_count_t src_channel_index, channel_count_t dst_channel_index) noexcept
  =0;

  ///
  /// @brief Copies contents from another audio buffer to this audio buffer.
  ///
  /// @param src Audio buffer to copy from.
  /// @param args Arguments to adjust the copy operation.
  ///
  /// @return Whether the operation was successful.
  ///
  virtual bool copy_from(AudioBufferInterface* src, CopyArgs args=COPY_ARGS_DEFAULT) audiobuffer__noexcept
  =0;

  ///
  /// @brief Copies contents from this audio buffer to another audio buffer.
  ///
  /// @param dst Audio buffer to copy to.
  /// @param args Arguments to adjust the copy operation.
  ///
  /// @return Whether the operation was successful.
  ///
  virtual bool copy_to(AudioBufferInterface* dst, CopyArgs args=COPY_ARGS_DEFAULT) audiobuffer__noexcept
  =0;

};

// *****************************************************************************

} // namespace audiobuffer
