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
  // :: PUBLIC CONSTANTS :: //

  public:

  static constexpr std::size_t DEFAULT_IO_BUFFER_SIZE = 1024 * 8192;

  // :: PROTECTED ATTRIBUTES :: //

  protected:

  // Size of a single sample in number of bytes.
  const std::size_t _SIZEOF_IO_SAMPLE;

  // The binary buffer for doing actual disk I/O.
  std::size_t _io_buffer_size;
  char* _io_buffer;

  // sizeof_io_sample * stream_channel_count
  int _io_divider;

  // Amount of bytes and frames to read per full rw operation.
  std::size_t _rw_amount_bytes;
  std::size_t _rw_amount_frames;

  // The stream we read from/write to.
  std::iostream* _stream;
  // The amount of channels we expect to read/want to write in the stream.
  channel_count_t _stream_channel_count;

  // The audio buffer provided by the user which we will read to or write from.
  AudioBufferInterface* _ab_raw;
  // We cache the last known sizes for speed and safety reasons.
  channel_count_t _ab_channel_count;
  frame_count_t   _ab_frame_count;

  // We can't interact with the audio buffer provided by the user directly, but
  // we instead pump data between this intermediate buffer, which will be a
  // reference when the sample type is the same as the type of this IO class.
  AudioBuffer<T, ALLOCATOR_T> _ab_interm;

  // :: CONSTRUCTOR :: //

  public:

  ///
  /// @param sizeof_io_sample The size of a single sample in bytes.
  /// @param stream The stream object, implementing `std::iostream`.
  /// @param stream_channel_count The amount of channels in the stream.
  /// @param io_buffer_size The size of the buffer used to buffer I/O operations.
  ///
  AudioBufferIOBase(
    std::size_t sizeof_io_sample,
    std::iostream& stream,
    channel_count_t stream_channel_count,
    std::size_t io_buffer_size=DEFAULT_IO_BUFFER_SIZE
  )
    : _SIZEOF_IO_SAMPLE(sizeof_io_sample)
  {
    this->_io_buffer_size = 0;
    this->_io_buffer = nullptr;

    this->_io_divider = 1;

    this->_rw_amount_bytes  = 0;
    this->_rw_amount_frames = 0;

    this->_stream = nullptr;
    this->_stream_channel_count = 0;

    this->_ab_raw = nullptr;
    this->_ab_channel_count = 0;
    this->_ab_frame_count   = 0;

    this->set_stream(stream, stream_channel_count);
    this->set_io_buffer_size(io_buffer_size);
  }

  ~AudioBufferIOBase()
  {
    this->_cleanup_io_buffer();
  }

  // :: INTERFACE METHODS :: //

  void set_stream(std::iostream& stream, channel_count_t stream_channel_count) override final
  {
    this->_stream = &stream;
    this->_stream_channel_count = stream_channel_count;

    // Prevent division by zero if no channel count.
    this->_io_divider = this->_SIZEOF_IO_SAMPLE * this->_stream_channel_count;
    if (!this->_io_divider) {
      this->_io_divider = 1;
    }

    return;
  }

  void set_io_buffer_size(std::size_t io_buffer_size) override final
  {
    // Ensure minimum size to functionally operate.
    io_buffer_size = std::max(io_buffer_size, static_cast<std::size_t>(this->_io_divider));
    // For safety, enforce a maximum size.
    io_buffer_size = std::min(
      io_buffer_size,
      static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())
    );
    // If sizes are the same and the I/O buffer is already allocated, no further
    // actions needed.
    if (this->_io_buffer && this->_io_buffer_size == io_buffer_size) {
      return;
    }

    // Whole division so we ensure we never have incomplete frames to rw.
    this->_rw_amount_bytes  =         io_buffer_size / this->_io_divider * this->_io_divider;
    this->_rw_amount_frames = this->_rw_amount_bytes / this->_io_divider;

    this->_cleanup_io_buffer();
    {
      ALLOCATOR_T<char> io_buffer_alloc;
      using io_buffer_alloc_t = std::allocator_traits<decltype(io_buffer_alloc)>;

      this->_io_buffer      = io_buffer_alloc_t::allocate(io_buffer_alloc, io_buffer_size);
      this->_io_buffer_size = io_buffer_size;
    }

    this->_update_ab_meta();
    return;
  }

  void seek(
    std::streampos frames,
    std::ios_base::seekdir direction=std::ios::beg,
    std::streamoff offset=0,
    std::ios_base::seekdir offset_direction=std::ios::beg
  ) override final
  {
    if (!this->_is_io_available()) {
      return;
    }
    this->_update_ab_meta();

    // Offset seek.
    this->_stream->seekg(offset, offset_direction);
    this->_stream->seekp(offset, offset_direction);
    // Frame seek.
    this->_stream->seekg(frames * this->_SIZEOF_IO_SAMPLE * this->_stream_channel_count, direction);
    this->_stream->seekp(frames * this->_SIZEOF_IO_SAMPLE * this->_stream_channel_count, direction);

    return;
  }

  std::size_t read(audiobuffer::AudioBufferInterface& audio_buffer, std::size_t frames=0, std::size_t offset=0) override
  {
    this->_set_ab(audio_buffer);
    if (!(this->_is_io_available() && this->_is_ab_available())) {
      return 0;
    }
    if (!this->_io_divider) {
      return 0;
    }

    std::tie(frames, offset) = this->_sanitize_rw_params(frames, offset);
    if (!frames) {
      return 0;
    }

    CopyArgs copy_args;

    auto bytes_per_read  = this->_rw_amount_bytes;
    auto frames_per_read = this->_rw_amount_frames;

    frame_count_t   i;
    channel_count_t c;
    // When using a referenced intermediate audio buffer, we can just use offsets.
    frame_count_t ref_offset = 0;

    std::size_t current_frame = 0;
    std::size_t current_byte  = 0;
    while (current_frame < frames)
    {
      if (current_frame + frames_per_read > frames) {
        frames_per_read = frames - current_frame;
        bytes_per_read  = frames_per_read * this->_io_divider;
      }

      // Read new data from the stream.
      this->_stream->read(this->_io_buffer, bytes_per_read);
      current_byte = 0;
      // If we hit unexpected EOF.
      std::size_t amount_bytes_read = this->_stream->gcount();
      bool eof = amount_bytes_read != bytes_per_read;
      if (eof) {
        // Process what we still have read.
        frames_per_read = amount_bytes_read
          ? bytes_per_read / this->_io_divider
          : 0;
      }

      std::size_t intermediate_size = this->_ab_interm.get_frame_count();
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
        for (c=0; c<this->_ab_channel_count; c++)
        {
          auto channel = this->_ab_interm[c];

          // If the stream has less channels than the audio buffer to read to,
          // fill with center values.
          if (c >= this->_stream_channel_count) {
            for (i=0; i<intermediate_size; i++) {
              channel[ref_offset+i] = SampleDescriptor<T>::CENTER;
            }
            continue;
          }

          std::size_t byte_offset = current_byte + (this->_SIZEOF_IO_SAMPLE * c);
          for (i=0; i<intermediate_size; i++) {
            channel[ref_offset+i] = this->_unpack1(byte_offset + (i * this->_io_divider));
          }
        }

        // Copy the intermediate data to the user audio buffer.
        if (!this->_ab_interm.is_reference()) {
          copy_args.size       = intermediate_size;
          copy_args.dst_offset = current_frame;
          this->_ab_interm.copy_to(this->_ab_raw, copy_args);
        }

        current_frame += intermediate_size;
        current_byte  += intermediate_size * this->_io_divider;
        ref_offset = this->_ab_interm.is_reference()
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

  std::size_t write(audiobuffer::AudioBufferInterface& audio_buffer, std::size_t frames=0, std::size_t offset=0) override
  {
    this->_set_ab(audio_buffer);
    if (!(this->_is_io_available() && this->_is_ab_available())) {
      return 0;
    }
    if (!this->_io_divider) {
      return 0;
    }

    std::tie(frames, offset) = this->_sanitize_rw_params(frames, offset);
    if (!frames) {
      return 0;
    }

    CopyArgs copy_args;

    auto frames_per_write = this->_rw_amount_frames;
    auto bytes_per_write  = this->_rw_amount_bytes;

    frame_count_t   i;
    channel_count_t c;
    // When using a referenced intermediate audio buffer, we can just use offsets.
    frame_count_t ref_offset = 0;

    std::size_t current_frame = 0;
    std::size_t current_byte  = 0;
    while (current_frame < frames)
    {
      if (current_frame + frames_per_write > frames) {
        frames_per_write = frames - current_frame;
        bytes_per_write  = frames_per_write * this->_io_divider;
      }

      std::size_t intermediate_size = this->_ab_interm.get_frame_count();
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

        if (!this->_ab_interm.is_reference()) {
          // Copy the intermediate data from the user audio buffer.
          copy_args.size       = intermediate_size;
          copy_args.src_offset = current_frame;
          this->_ab_interm.copy_from(this->_ab_raw, copy_args);
        }

        // Process data as interleaved samples.
        for (c=0; c<this->_stream_channel_count; c++)
        {
          std::size_t byte_offset = current_byte + (this->_SIZEOF_IO_SAMPLE * c);

          // If the stream has more channels than the audio buffer to write from,
          // fill with center values.
          if (c >= this->_ab_channel_count) {
            static auto center = SampleDescriptor<T>::CENTER;
            for (i=0; i<intermediate_size; i++) {
              this->_pack1(center, byte_offset + (i * this->_io_divider));
            }
            continue;
          }

          auto channel = this->_ab_interm[c];
          for (i=0; i<intermediate_size; i++) {
            this->_pack1(channel[ref_offset+i], byte_offset + (i * this->_io_divider));
          }
        }
        current_frame += intermediate_size;
        current_byte  += intermediate_size * this->_io_divider;
        ref_offset = this->_ab_interm.is_reference()
          ? current_frame
          : 0;
      }

      // Get current position to check how many bytes we've been able to write.
      auto pos = this->_stream->tellp();
      // Write new data to the stream.
      this->_stream->write(this->_io_buffer, bytes_per_write);
      current_byte = 0;
      // Check the amount of bytes actually written.
      std::size_t amount_bytes_written = this->_stream->tellp() - pos;
      if (amount_bytes_written != bytes_per_write)
      {
        // If we were not able to write successfully, adjust the current
        // frame position and stop trying to write.
        auto amount_frames_written = amount_bytes_written
          ? amount_bytes_written / this->_io_divider
          : 0;
        current_frame -= (frames_per_write - amount_frames_written);
        break;
      }
    }

    // Sync read position with write position.
    this->_stream->seekg(this->_stream->tellp());
    return current_frame;
  }

  // :: PROTECTED ABSTRACT METHODS :: //

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

  // :: PRIVATE METHODS :: //

  ///
  /// @brief Cleans up the I/O buffer and the intermediate audio buffer.
  ///
  void _cleanup_io_buffer()
  {
    if (this->_io_buffer) {
      ALLOCATOR_T<char> io_buffer_alloc;
      using io_buffer_alloc_t = std::allocator_traits<decltype(io_buffer_alloc)>;

      io_buffer_alloc_t::deallocate(io_buffer_alloc, this->_io_buffer, this->_io_buffer_size);
      this->_io_buffer = nullptr;
    }
    // Cleanup dynamically allocated memory in the intermediate audio buffer.
    if (!this->_ab_interm.is_reference()) {
      this->_ab_interm.resize(0, 0);
    }
    return;
  }

  ///
  /// @brief Checks whether the I/O stream and I/O audio buffer are available.
  ///
  bool _is_io_available() const
  { return static_cast<bool>(this->_stream && this->_ab_raw); }

  ///
  /// @brief Checks whether the I/O audio buffer can be read from/written to.
  ///
  bool _is_ab_available() const
  { return static_cast<bool>(this->_ab_raw && this->_ab_channel_count && this->_ab_frame_count); }

  ///
  /// @brief Sets the source buffer.
  ///
  /// @param audio_buffer The source buffer to read from/write to.
  ///
  void _set_ab(audiobuffer::AudioBufferInterface& audio_buffer)
  {
    if (!audio_buffer.has_data()) {
      this->_ab_raw = nullptr;
      this->_update_ab_meta();
    }

    bool reset_interm = false;
    if (!this->_ab_raw) {
      reset_interm = true;
    }
    else if (this->_ab_raw->get_format_id() != audio_buffer.get_format_id()) {
      reset_interm = true;
    }
    if (reset_interm) {
      this->_ab_raw = &audio_buffer;
      // Try to get a reference if the intermediate type is of the same type as
      // the user input type, else create a new intermediate audio buffer.
      auto ab_ref = audiobuffer::AudioBuffer<T, ALLOCATOR_T>::from_reference(this->_ab_raw->get_data());
      this->_ab_interm = ab_ref.has_value()
        ? std::move(ab_ref).value()
        : audiobuffer::AudioBuffer<T, ALLOCATOR_T>(0, 0);
    }

    this->_update_ab_meta();
    return;
  }

  ///
  /// @brief Updates audio buffer metadata and intermediate buffer sizes.
  ///
  void _update_ab_meta()
  {
    if (!this->_is_io_available()) {
      this->_ab_channel_count = 0;
      this->_ab_frame_count   = 0;
      return;
    }

    auto channel_count = this->_ab_raw->get_channel_count();
    auto frame_count   = this->_ab_raw->get_frame_count();
    // No changes needed.
    if (this->_ab_channel_count == channel_count && this->_ab_frame_count == frame_count) {
      return;
    }
    this->_ab_channel_count = channel_count;
    this->_ab_frame_count   = frame_count;

    // No resizing needed if the intermediate audio buffer is a reference.
    if (this->_ab_interm.is_reference()) {
      return;
    }

    // Enforce a maximum intermediate audio buffer size.
    std::size_t ab_interm_size = std::min(
      this->_io_buffer_size / this->_io_divider,
      static_cast<std::size_t>(1024*16) / this->_io_divider
    );
    this->_ab_interm.resize(
      static_cast<frame_count_t>(ab_interm_size),
      this->_ab_channel_count
    );

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
  std::tuple<std::size_t, std::size_t> _sanitize_rw_params(std::size_t size, std::size_t offset)
  {
    if (!this->_ab_frame_count || offset >= this->_ab_frame_count) {
      return std::make_tuple(0, 0);
    }
    // Full buffer size.
    if (size == 0) {
      size = this->_ab_frame_count;
    }
    if (size + offset > this->_ab_frame_count) {
      size = this->_ab_frame_count - offset;
    }

    return std::make_tuple(size, offset);
  }

};

// *****************************************************************************

} // namespace audiobuffer::io
