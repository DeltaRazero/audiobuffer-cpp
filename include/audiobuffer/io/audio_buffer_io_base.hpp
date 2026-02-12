#pragma once

// *****************************************************************************

#include <cstdint>
#include <iostream>
#include <tuple>
#include <memory>

#include "../buffer/audio_buffer_interface.hpp"
#include "../buffer/audio_buffer.hpp"

#include "./audio_buffer_io_interface.hpp"

// *****************************************************************************

namespace audiobuffer::io {

// *****************************************************************************

///
/// @brief Base class for providing I/O using audio buffer containers.
///
/// @tparam T The sample type.
/// @tparam ALLOCATOR_T Allocator class, defaults to `std::allocator`.
///
template <typename T, template<typename> class ALLOCATOR_T=std::allocator>
class AudioBufferIOBase : public AudioBufferIOInterface
{
  public:

  static constexpr std::size_t DEFAULT_IO_BUFFER_SIZE = 1024 * 8192;

  protected:

  // Size of a single sample when reading/writing from the I/O buffer.
  const std::size_t _sizeof_io_sample;

  // The input buffer from which we will read from or write to.
  AudioBufferInterface* _src_ab;
  // We keep track of the last known sizes to ensure we don't overflow/underflow.
  channel_count_t _src_ab_channel_count;
  buffer_size_t   _src_ab_buffer_size;

  // We use this audio buffer to copy from/to the source audio buffer.
  AudioBuffer<T, ALLOCATOR_T> _interm_ab;

  // Buffer so we can read/write data in a large chunk for faster I/O performance.
  std::size_t _io_buffer_size;
  char* _io_buffer;

  // The actual stream we read/write to.
  std::iostream*  _stream;
  channel_count_t _stream_channel_count;

  public:

  ///
  /// @param stream The stream object, implementing `std::iostream`.
  /// @param audio_buffer The audio buffer to read from/write to.
  /// @param io_buffer_size The size of the buffer used to buffer I/O operations.
  /// @param sizeof_io_sample The size of a single sample in bytes.
  ///
  AudioBufferIOBase(
    std::iostream& stream,
    ::audiobuffer::AudioBufferInterface& audio_buffer,
    std::size_t io_buffer_size,
    std::size_t sizeof_io_sample
  )
  : _sizeof_io_sample(sizeof_io_sample)
  {
    this->_stream = nullptr;
    this->_src_ab = nullptr;

    this->_src_ab_buffer_size   = 0;
    this->_src_ab_channel_count = 0;

    this->_io_buffer      = nullptr;
    this->_io_buffer_size = io_buffer_size;
    // This will enforce a minimum value for `io+buffer_size`.
    this->set_stream(stream, audio_buffer);
  }

  ~AudioBufferIOBase()
  {
    this->_cleanup();
  }

  public:

  void set_stream(std::iostream& stream, ::audiobuffer::AudioBufferInterface& audio_buffer) override final
  {
    // If we already had a stream set before, close and cleanup.
    if (this->_stream) {
      this->_cleanup();
    }

    this->_stream = &stream;
    this->_src_ab = &audio_buffer;

    // If the input audio buffer has no data, set variables to these values to
    // skip read/write operations.
    if (!audio_buffer.has_data()) {
      this->_stream_channel_count = 0;
      this->_src_ab = nullptr;
      return;
    }

    // Try to get a reference if the intermediate type is of the same type as
    // the user input type, else create a new intermediate buffer.
    auto ab_ref = ::audiobuffer::AudioBuffer<T, ALLOCATOR_T>::from_reference(audio_buffer.get_data());
    this->_interm_ab = ab_ref.has_value()
      ? std::move(ab_ref).value()
      : ::audiobuffer::AudioBuffer<T, ALLOCATOR_T>(0, 0);

    this->_stream_channel_count = audio_buffer.get_channel_count();

    // Ensure that the I/O buffer is available.
    this->set_io_buffer_size(this->_io_buffer_size);
  }

  void set_io_buffer_size(std::size_t io_buffer_size) override final
  {
    // Ensure minimum size to functionally operate.
    io_buffer_size = std::max(
      io_buffer_size,
      this->_sizeof_io_sample * this->_src_ab_channel_count
    );
    // For safety, enforce a maximum size.
    io_buffer_size = std::min(
      io_buffer_size,
      static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())
    );
    // If sizes are the same and the I/O buffer is already located, no further
    // actions needed.
    if (this->_io_buffer_size == io_buffer_size && this->_io_buffer) {
      return;
    }

    this->_cleanup();
    {
      ALLOCATOR_T<char> io_buffer_alloc;
      using io_buffer_alloc_t = std::allocator_traits<decltype(io_buffer_alloc)>;

      this->_io_buffer      = io_buffer_alloc_t::allocate(io_buffer_alloc, io_buffer_size);
      this->_io_buffer_size = io_buffer_size;
    }
    this->_update_ab_sizes();
    return;
  }

  protected:

  ///
  /// @brief Unpacks a single value.
  ///
  /// @param io_buffer_offset Offset of the packed data in the I/O buffer.
  ///
  /// @return The unpacked value.
  ///
  virtual T _unpack1(std::size_t io_buffer_offset)
  =0;

  ///
  /// @brief Packs a single value.
  ///
  /// @param value The value to pack.
  /// @param io_buffer_offset Offset of the packed data in the I/O buffer.
  ///
  virtual void _pack1(T& value, std::size_t io_buffer_offset)
  =0;

  public:

  void seek(
    std::streampos frames,
    std::ios_base::seekdir direction=std::ios::beg,
    std::streamoff offset=0,
    std::ios_base::seekdir offset_direction=std::ios::beg
  ) override final
  {
    if (!this->_stream) {
      return;
    }
    this->_update_ab_sizes();

    // Offset seek.
    this->_stream->seekg(offset, offset_direction);
    this->_stream->seekp(offset, offset_direction);
    // Frame seek.
    this->_stream->seekg(frames * this->_src_ab_buffer_size * this->_src_ab_channel_count, direction);
    this->_stream->seekp(frames * this->_src_ab_buffer_size * this->_src_ab_channel_count, direction);

    return;
  }

  std::size_t read(std::size_t size=0, std::size_t offset=0) override
  {
    if (!(this->_is_io_available() && this->_is_src_ab_data_available())) {
      return 0;
    }

    std::tie(size, offset) = this->_sanitize_size_params(size, offset);
    if (!size) {
      return 0;
    }
    this->_update_ab_sizes();

    CopyArgs copy_args;

    auto io_divider = this->_stream_channel_count * this->_sizeof_io_sample;
    if (!io_divider) {
      return 0;
    }
    // Whole division and back to get the amount of bytes to fill the I/O buffer
    // so no frames (samples of all channels) are read incompletely.
    std::size_t amount_bytes_per_read = this->_io_buffer_size / io_divider * io_divider;
    std::size_t frames_per_read       = amount_bytes_per_read / io_divider;

    buffer_size_t   i;
    channel_count_t c;
    // When using a referenced intermediate audio buffer, we can just use offsets.
    buffer_size_t ref_offset = 0;

    std::size_t current_frame   = 0;
    std::size_t current_io_byte = 0;
    while (current_frame < size)
    {
      if (current_frame + frames_per_read > size) {
        frames_per_read       = size - current_frame;
        amount_bytes_per_read = frames_per_read * io_divider;
      }

      // Read new data from the stream.
      this->_stream->read(this->_io_buffer, amount_bytes_per_read);
      current_io_byte = 0;
      // If we hit unexpected EOF.
      std::size_t amount_bytes_read = this->_stream->gcount();
      bool eof = amount_bytes_read != amount_bytes_per_read;
      if (eof) {
        // Process what we still have read.
        frames_per_read = amount_bytes_read
          ? amount_bytes_per_read / io_divider
          : 0;
      }

      std::size_t intermediate_size = this->_interm_ab.get_buffer_size();
      // Must be able to contain at least one frame (sample for all channels).
      if (!intermediate_size) {
        break;
      }
      std::size_t intermediate_passes = frames_per_read / intermediate_size;
      std::size_t intermediate_mod    = frames_per_read % intermediate_size;
      if (intermediate_mod) {
        intermediate_passes += 1;
      }
      for (std::size_t ip=0; ip<intermediate_passes; ip++)
      {
        if (intermediate_mod && ip == intermediate_passes-1) {
          intermediate_size = intermediate_mod;
        }

        // Process data as interleaved samples.
        for (c=0; c<this->_src_ab_channel_count; c++)
        {
          auto channel = this->_interm_ab[c];

          // If channel count was changed by the user between read calls, fill
          // with center values.
          if (c >= this->_stream_channel_count) {
            for (i=0; i<intermediate_size; i++) {
              channel[ref_offset+i] = SampleDescriptor<T>::CENTER;
            }
            continue;
          }

          std::size_t io_byte_offset = current_io_byte + (this->_sizeof_io_sample * c);
          for (i=0; i<intermediate_size; i++) {
            channel[ref_offset+i] = this->_unpack1(io_byte_offset + (i * io_divider));
          }
        }

        if (!this->_interm_ab.is_reference()) {
          // Copy the intermediate data to the source audio buffer.
          copy_args.size       = intermediate_size;
          copy_args.dst_offset = current_frame;
          this->_interm_ab.copy_to(this->_src_ab, copy_args);
        }

        current_frame   += intermediate_size;
        current_io_byte += intermediate_size * io_divider;
        ref_offset = this->_interm_ab.is_reference()
          ? current_frame
          : 0;
      }

      if (eof) {
        break;
      }
    }

    // Sync write position with read position.
    this->_stream->seekp(this->_stream->tellg());

    return current_frame;
  }

  std::size_t write(std::size_t size=0, std::size_t offset=0) override
  {
    if (!(this->_is_io_available() && this->_is_src_ab_data_available())) {
      return 0;
    }

    std::tie(size, offset) = this->_sanitize_size_params(size, offset);
    if (!size) {
      return 0;
    }
    this->_update_ab_sizes();

    CopyArgs copy_args;

    auto io_divider = this->_stream_channel_count * this->_sizeof_io_sample;
    if (!io_divider) {
      return 0;
    }
    // Whole division and back to get the amount of bytes to fill the I/O buffer
    // so no frames (samples of all channels) are written incompletely.
    std::size_t amount_bytes_per_write = this->_io_buffer_size  / io_divider * io_divider;
    std::size_t frames_per_write       = amount_bytes_per_write / io_divider;

    buffer_size_t   i;
    channel_count_t c;
    // When using a referenced intermediate audio buffer, we can just use offsets.
    buffer_size_t ref_offset = 0;

    std::size_t current_frame   = 0;
    std::size_t current_io_byte = 0;
    while (current_frame < size)
    {
      if (current_frame + frames_per_write > size) {
        frames_per_write = size - current_frame;
        amount_bytes_per_write = frames_per_write * io_divider;
      }

      std::size_t intermediate_size = this->_interm_ab.get_buffer_size();
      // Must be able to contain at least one frame (sample for all channels).
      if (!intermediate_size) {
        break;
      }
      std::size_t intermediate_passes = frames_per_write / intermediate_size;
      std::size_t intermediate_mod    = frames_per_write % intermediate_size;
      if (intermediate_mod) {
        intermediate_passes += 1;
      }
      for (std::size_t ip=0; ip<intermediate_passes; ip++)
      {
        if (intermediate_mod && ip == intermediate_passes-1) {
          intermediate_size = intermediate_mod;
        }

        // If we can access the source audio buffer directly, no copy operations
        // are needed.
        if (!this->_interm_ab.is_reference()) {
          // Copy the intermediate data from the source audio buffer.
          copy_args.size       = intermediate_size;
          copy_args.src_offset = current_frame;
          this->_interm_ab.copy_from(this->_src_ab, copy_args);
        }

        // Process data as interleaved samples.
        for (c=0; c<this->_stream_channel_count; c++)
        {
          std::size_t io_byte_offset = current_io_byte + (this->_sizeof_io_sample * c);

          // If channel count was changed by the user between write calls, fill
          // with center values.
          if (c >= this->_src_ab_channel_count) {
            static auto center = SampleDescriptor<T>::CENTER;
            for (i=0; i<intermediate_size; i++) {
              this->_pack1(center, io_byte_offset + (i * io_divider));
            }
            continue;
          }

          auto channel = this->_interm_ab[c];
          for (i=0; i<intermediate_size; i++) {
            this->_pack1(channel[ref_offset+i], io_byte_offset + (i * io_divider));
          }
        }
        current_frame   += intermediate_size;
        current_io_byte += intermediate_size * io_divider;
        ref_offset = this->_interm_ab.is_reference()
          ? current_frame
          : 0;
      }

      {
        // Get current position to check how many bytes we've been able to write.
        auto pos = this->_stream->tellp();
        // Write new data to the stream.
        this->_stream->write(this->_io_buffer, amount_bytes_per_write);
        current_io_byte = 0;
        // Check the amount of bytes actually written.
        std::size_t amount_bytes_written = this->_stream->tellp() - pos;
        if (amount_bytes_written != amount_bytes_per_write)
        {
          // If we were not able to write successfully, adjust the current
          // frame position and stop trying to write.
          auto amount_frames_written = amount_bytes_written
            ? amount_bytes_written / io_divider
            : 0;
          current_frame -= (frames_per_write - amount_frames_written);
          break;
        }
      }
    }

    // Sync read position with write position.
    this->_stream->seekg(this->_stream->tellp());

    return current_frame;
  }

  protected:

  ///
  /// @brief Checks whether the I/O stream and I/O audio buffer are available.
  ///
  bool _is_io_available() const
  { return static_cast<bool>(this->_stream && this->_src_ab); }

  ///
  /// @brief Checks whether the I/O audio buffer can be read from/written to.
  ///
  bool _is_src_ab_data_available() const
  { return static_cast<bool>(this->_src_ab_buffer_size && this->_src_ab_channel_count); }

  ///
  /// @brief Updates audio buffer metadata and intermediate buffer sizes.
  ///
  void _update_ab_sizes()
  {
    if (!this->_is_io_available()) {
      this->_src_ab_channel_count = 0;
      this->_src_ab_buffer_size   = 0;
    }

    auto channel_count = this->_src_ab->get_channel_count();
    auto buffer_size   = this->_src_ab->get_buffer_size();
    // No changes needed.
    if (this->_src_ab_channel_count == channel_count && this->_src_ab_buffer_size == buffer_size) {
      return;
    }

    this->_src_ab_channel_count = channel_count;
    this->_src_ab_buffer_size   = buffer_size;

    // No resizing needed if the intermediate audio buffer is a reference.
    if (this->_interm_ab.is_reference()) {
      return;
    }

    auto divider = channel_count * this->_sizeof_io_sample;
    // Prevent division by zero if no channel count.
    if (!divider) {
      divider = 1;
    }
    // Enforce a maximum intermediate audio buffer size.
    std::size_t interm_ab_size = std::min(
      this->_io_buffer_size / divider,
      (1024*16) / divider
    );
    this->_interm_ab.resize(
      static_cast<buffer_size_t>(interm_ab_size),
      channel_count
    );

    return;
  }

  ///
  /// @brief Cleans up the I/O buffer and the intermediate audio buffer.
  ///
  void _cleanup()
  {
    if (this->_io_buffer) {
      ALLOCATOR_T<char> io_buffer_alloc;
      using io_buffer_alloc_t = std::allocator_traits<decltype(io_buffer_alloc)>;

      io_buffer_alloc_t::deallocate(io_buffer_alloc, this->_io_buffer, this->_io_buffer_size);
      this->_io_buffer = nullptr;
    }
    // Cleanup dynamically allocated memory in the intermediate audio buffer.
    if (!this->_interm_ab.is_reference()) {
      this->_interm_ab.resize(0, 0);
    }
    return;
  }

  ///
  /// @brief Sanitizes size parameters of I/O operations.
  ///
  /// @param size Amount of frames to read/write.
  /// @param offset Offset in the audio buffer to read from/write to.
  ///
  /// @return Tuple with sanitized size and offset values.
  ///
  std::tuple<std::size_t, std::size_t> _sanitize_size_params(std::size_t size, std::size_t offset)
  {
    // Always check that we have up-to-date metadata.
    this->_update_ab_sizes();

    if (!this->_src_ab_buffer_size || offset >= this->_src_ab_buffer_size) {
      return std::make_tuple(0, 0);
    }
    // Full buffer size.
    if (size == 0) {
      size = this->_src_ab_buffer_size;
    }
    if (size + offset > this->_src_ab_buffer_size) {
      size = this->_src_ab_buffer_size - offset;
    }

    return std::make_tuple(size, offset);
  }

};

// *****************************************************************************

} // namespace audiobuffer::io
