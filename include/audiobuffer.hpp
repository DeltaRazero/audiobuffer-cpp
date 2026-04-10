#pragma once

// *****************************************************************************

#define NUMIO_IGNORE_AUTO_ENDIAN

// *****************************************************************************

#include "./audiobuffer/descriptor.hpp"
#include "./audiobuffer/sample_type.hpp"

#include "./audiobuffer/buffer/audio_buffer_interface.hpp"
#include "./audiobuffer/buffer/audio_buffer.hpp"
#include "./audiobuffer/buffer/static_audio_buffer.hpp"

#include "./audiobuffer/io/audio_buffer_io_interface.hpp"
#include "./audiobuffer/io/ieeefloat_audio_buffer_io.hpp"
#include "./audiobuffer/io/integer_audio_buffer_io.hpp"
