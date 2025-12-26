#pragma once

// *****************************************************************************

#include <cstdint>
#include <limits>
#include <type_traits>
#include <cassert>

#include "./audio_buffer_data.h"

// *****************************************************************************

namespace audiobuffer {

// *****************************************************************************

/// @brief Packs the format of the audio buffer data.
typedef ab_format_id_t format_id_t;

/// @brief For defining the channel count.
typedef ab_channel_count_t channel_count_t;

/// @brief For defining the buffer size.
typedef ab_buffer_size_t buffer_size_t;

typedef ab_AudioBufferData AudioBufferData;

// *****************************************************************************

} // namespace audiobuffer
