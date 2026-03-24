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
  /// @param stream_channel_count The amount of channels in the stream..
  ///
  virtual void set_stream(std::iostream& stream, channel_count_t stream_channel_count)
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
  /// @param frames Amount of frames (samples per channel).
  /// @param direction Seek direction.
  /// @param offset Raw offset in the stream.
  /// @param offset_direction Seek direction of the raw offset.
  ///
  virtual void seek(
    std::streampos frames,
    std::ios_base::seekdir direction=std::ios::beg,
    std::streamoff offset=0,
    std::ios_base::seekdir offset_direction=std::ios::beg
  )
  =0;

  ///
  /// @brief Reads frames to the I/O audio buffer.
  ///
  /// @param audio_buffer The buffer to read to.
  /// @param frames Amount of frames (samples per channel). If not set, will be
  ///   the size of the audio buffer to read to.
  /// @param offset Offset where to write the frames to in the I/O audio buffer.
  ///   If not set, will be at the start of the audio buffer to read to.
  ///
  /// @return Amount of frames read.
  ///
  /// @warning `size` and `offset` will be limited to the size and offset of the
  ///   the I/O audio buffer if the values are larger.
  ///
  virtual std::size_t read(audiobuffer::AudioBufferInterface& audio_buffer, frame_count_t frames=0, frame_count_t offset=0)
  =0;

  ///
  /// @brief Writes frames from the I/O audio buffer.
  ///
  /// @param audio_buffer The buffer to write from.
  /// @param frames Amount of frames (samples per channel). If not set, will be
  ///   the size of the audio buffer to write from.
  /// @param offset Offset where to read the frames from in the I/O audio buffer.
  ///   If not set, will be at the start of the audio buffer to write from.
  ///
  /// @return Amount of frames written.
  ///
  /// @warning `size` and `offset` will be limited to the size and offset of the
  ///   the I/O audio buffer if the values are larger.
  ///
  virtual std::size_t write(audiobuffer::AudioBufferInterface& audio_buffer, frame_count_t frames=0, frame_count_t offset=0)
  =0;
};

// *****************************************************************************

} // namespace audiobuffer::io
