#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// *****************************************************************************

#include <stdint.h>

// *****************************************************************************

/// @brief Packs the format of the audio buffer data.
typedef uint32_t ab_format_id_t;

/// @brief For defining the channel count.
typedef uint8_t ab_channel_count_t;

/// @brief For defining the buffer size.
typedef uint32_t ab_buffer_size_t;

// *****************************************************************************

struct ab_AudioBufferData
{
  /// ID of the sample format.
  const ab_format_id_t format_id;

  /// If the audio buffer supports resizing.
  const bool resizable;

  /// Amount of channels.
  ab_channel_count_t channel_count;
  /// Amount of frames (samples per channel).
  ab_buffer_size_t buffer_size;

  /// Raw pointer to the buffer sample data.
  void* buffer;

  /// Raw pointer to the buffer channel table.
  void** channels;

  /// Function pointer to deallocate the buffer data;
  void (*deallocate)(ab_AudioBufferData* abd);

  /// User-adjustable flag whether to block resizing operations.
  bool block_resize;

};

// *****************************************************************************

#ifdef __cplusplus
}
#endif
