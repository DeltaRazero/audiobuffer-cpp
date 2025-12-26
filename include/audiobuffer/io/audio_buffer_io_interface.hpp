#pragma once

// *****************************************************************************

#include <cstdint>
#include <iostream>
#include <tuple>
#include <memory>

#include "../buffer/audio_buffer_interface.hpp"

// *****************************************************************************

namespace audiobuffer::io {

// *****************************************************************************

class AudioBufferIOInterface
{
  public: virtual ~AudioBufferIOInterface() {};

  // TODO: Maybe not include this in the interface?
  virtual void set_stream(std::iostream& stream, ::audiobuffer::AudioBufferInterface& audio_buffer)
  =0;

  virtual void set_io_buffer_size(std::size_t io_buffer_size)
  =0;

  virtual std::size_t seek(std::streamsize size)
  =0;

  ///
  /// @brief Reads samples.
  ///
  /// @param size Amount of samples per channel.
  /// @param offset Offset where to put in audio buffer object.
  ///
  /// @return Amount of samples read.
  ///
  virtual std::size_t read(std::size_t size=0, std::size_t offset=0)
  =0;

  virtual std::size_t write(std::size_t size=0, std::size_t offset=0)
  =0;
};

// *****************************************************************************

} // namespace audiobuffer::io
