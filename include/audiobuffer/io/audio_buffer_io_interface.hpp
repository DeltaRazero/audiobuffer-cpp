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

///
/// @brief Common interface for audio buffer I/O streams.
///
class AudioBufferIOInterface
{
  public: virtual ~AudioBufferIOInterface() {};

  ///
  /// @brief Sets the stream to read from/write to.
  ///
  /// @param stream The stream object, implementing `std::iostream`.
  /// @param audio_buffer The audio buffer to from/write to.
  ///
  /// @warning Channel count read from/written to the stream will not be updated
  ///   when the I/O audio buffer is reconfigured. Remaining channels will be
  ///   padded with DC center values. If you want to change the channel count,
  ///   you will need to call `set_stream()` again.
  ///
  virtual void set_stream(std::iostream& stream, ::audiobuffer::AudioBufferInterface& audio_buffer)
  =0;

  ///
  /// @brief Sets the size of the buffer used to buffer I/O operations.
  ///
  /// @param io_buffer_size Size of the buffer in bytes.
  ///
  virtual void set_io_buffer_size(std::size_t io_buffer_size)
  =0;

  ///
  /// @brief Changes the current read/write position.
  ///
  /// @param samples Amount of samples per channel.
  /// @param direction Seek direction.
  /// @param offset Raw offset in the stream.
  /// @param offset_direction Seek direction of the raw offset.
  ///
  virtual void seek(
    std::streampos samples,
    std::ios_base::seekdir direction=std::ios::beg,
    std::streamoff offset=0,
    std::ios_base::seekdir offset_direction=std::ios::beg
  )
  =0;

  ///
  /// @brief Reads samples to the I/O audio buffer.
  ///
  /// @param size Amount of samples per channel.
  /// @param offset Offset where to write the samples to in the I/O audio buffer.
  ///
  /// @return Amount of samples read.
  ///
  /// @warning `size` and `offset` will be limited to the size and offset of the
  ///   the I/O audio buffer if the values are larger.
  ///
  virtual std::size_t read(std::size_t size=0, std::size_t offset=0)
  =0;

  ///
  /// @brief Writes samples from the I/O audio buffer.
  ///
  /// @param size Amount of samples per channel.
  /// @param offset Offset where to read the samples from in the I/O audio buffer.
  ///
  /// @return Amount of samples written.
  ///
  /// @warning `size` and `offset` will be limited to the size and offset of the
  ///   the I/O audio buffer if the values are larger.
  ///
  virtual std::size_t write(std::size_t size=0, std::size_t offset=0)
  =0;
};

// *****************************************************************************

} // namespace audiobuffer::io
