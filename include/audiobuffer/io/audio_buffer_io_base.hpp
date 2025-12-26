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
/// @brief An audio buffer container which offers fixed time access to individual channels and samples in any order.
///
/// @tparam T The sample type.
/// @tparam ALLOCATOR_T Allocator class, defaults to `std::allocator`.
///
template <typename T, template<typename> class ALLOCATOR_T=std::allocator>
class AudioBufferIOBase : public AudioBufferIOInterface
{
  protected:

  // The input buffer from which we will read from or write to.
  AudioBufferInterface* _source_audio_buffer;

  // The buffer we'll use to convert between...
  AudioBuffer<T, ALLOCATOR_T> _intermediate_audio_buffer;
  bool _intermediate_audio_buffer_is_reference;

  std::size_t _io_buffer_size;
  char* _io_buffer;

  // Size of a single sample when reading/writing from the I/O buffer.
  // TODO: Make this a const.
  const std::size_t _sizeof_io_sample;

  std::iostream* _stream;

  public:

  AudioBufferIOBase(std::iostream& stream, ::audiobuffer::AudioBufferInterface& audio_buffer, std::size_t sizeof_io_sample)
  : _sizeof_io_sample(sizeof_io_sample)
  {
    this->_stream              = nullptr;
    this->_source_audio_buffer = nullptr;

    this->_io_buffer = nullptr;
    this->_io_buffer_size = 1024 * 8192;

    this->_intermediate_audio_buffer_is_reference = false;

    this->set_stream(stream, audio_buffer);
  }

  ~AudioBufferIOBase()
  {
    this->_cleanup();
  }

  public:

  // TODO: Add note that changing channel count in source buffer after stream has been set is not supported.
  void set_stream(std::iostream& stream, ::audiobuffer::AudioBufferInterface& audio_buffer) override
  {
    if (this->_stream) {
      this->_cleanup();
    }

    this->_stream = &stream;
    this->_source_audio_buffer = &audio_buffer;

    // TODO: Check has_data();
    // TODO: Use get_format_id();

    auto opt_buffer = ::audiobuffer::AudioBuffer<T, ALLOCATOR_T>::from_reference(audio_buffer.get_data());

    // if (audio_buffer.get_data()->format_id == SampleDescriptor<T>::FORMAT_ID) {
    if (opt_buffer.has_value()) {
      this->_intermediate_audio_buffer = std::move(opt_buffer).value();
      this->_intermediate_audio_buffer_is_reference = true;
    }
    else {
      this->_intermediate_audio_buffer = ::audiobuffer::AudioBuffer<T, ALLOCATOR_T>(0, 0);
      this->_intermediate_audio_buffer_is_reference = false;
    }

    // Ensure I/O buffer available.
    this->set_io_buffer_size(this->_io_buffer_size);

    // TODO: This is temporary.
    // this->set_io_buffer_size(source_audio_buffer->get_buffer_size() * source_audio_buffer->get_channel_count() * this->_sizeof_io_sample);
  }

  void set_io_buffer_size(std::size_t io_buffer_size) override
  {
    if (this->_io_buffer == nullptr) {
      this->_io_buffer_size = io_buffer_size;
    }
    else if (this->_io_buffer_size == io_buffer_size) {
      return;
    }
    this->_cleanup();

    {
      ALLOCATOR_T<char> io_buffer_alloc;
      using io_buffer_alloc_t = std::allocator_traits<decltype(io_buffer_alloc)>;

      this->_io_buffer      = io_buffer_alloc_t::allocate(io_buffer_alloc, io_buffer_size);
      this->_io_buffer_size = io_buffer_size;
    }

    if (this->_intermediate_audio_buffer_is_reference) {
      return;
    }

    auto channel_count = this->_source_audio_buffer->get_channel_count();
    auto divider = channel_count * this->_sizeof_io_sample;

    // Prevent division by zero if no channel count.
    if (!divider) {
      divider = 1;
    }

    // TODO: Limit IO buffer size to the size+channel_count of the source buffer?

    this->_intermediate_audio_buffer.resize(
      std::min<buffer_size_t>(
        io_buffer_size / divider,
        // TODO: Make comment that we enforce a maximum for the intermediate audio buffer.
        (1024*16) / divider
      ),
      channel_count
    );

    return;
  }

  std::size_t seek(std::streamsize size) override
  {
    if (!this->_stream) {
      return 0;
    }

    // TODO: Calc position delta?
    // auto a = this->_stream->tellg

    // TODO: Null check?
    auto channel_count = this->_intermediate_audio_buffer_is_reference
      ? this->_source_audio_buffer->get_channel_count()
      : this->_intermediate_audio_buffer.get_channel_count();

    this->_stream->seekg(
      size * this->_sizeof_io_sample * channel_count,
      std::ios_base::cur
    );

    return size;
  }

  std::size_t read(std::size_t size=0, std::size_t offset=0) override
  {
    // TODO: Make method to check if stuff is set.
    if (!(this->_stream && this->_source_audio_buffer)) {
      return 0;
    }

    // TODO: Check if stream is readable?
    // TODO: Method to check if buffer size is set.

    std::tie(size, offset) = this->_sanitize_io_params(size, offset);
    // If size is still 0, we don't do anything.
    if (size == 0) {
      return 0;
    }

    CopyArgs copy_args;

    channel_count_t channel_count = this->_intermediate_audio_buffer.get_channel_count();
    // Cache value to speed up performance.
    auto io_divider = channel_count * this->_sizeof_io_sample;
    // Whole division and back to get the amount of bytes to fill the I/O buffer
    // so no samples or channels are read incompletely.
    std::size_t amount_bytes_per_read = this->_io_buffer_size / io_divider * io_divider;
    std::size_t samples_per_read      = amount_bytes_per_read / io_divider;

    std::size_t amount_bytes_read;

    buffer_size_t   i;
    channel_count_t c;

    // TODO: Copy comment.
    buffer_size_t ref_offset = 0;

    bool eof = false;

    std::size_t current_sample  = 0;
    std::size_t current_io_byte = 0;
    while (current_sample < size)
    {
      if (current_sample + samples_per_read > size) {
        samples_per_read      = size - current_sample;
        amount_bytes_per_read = samples_per_read * io_divider;
      }

      // Read new data from the stream.
      this->_stream->read(this->_io_buffer, amount_bytes_per_read);
      current_io_byte = 0;
      // If we hit unexpected EOF.
      amount_bytes_read = this->_stream->gcount();
      if (amount_bytes_read != amount_bytes_per_read) {
        eof = true;
        // Process what we still have read.
        samples_per_read = amount_bytes_read / io_divider;
      }

      std::size_t intermediate_size   = this->_intermediate_audio_buffer.get_buffer_size();
      std::size_t intermediate_passes = samples_per_read / intermediate_size;
      std::size_t intermediate_mod    = samples_per_read % intermediate_size;
      if (intermediate_mod) {
        intermediate_passes += 1;
      }
      for (std::size_t ip=0; ip<intermediate_passes; ip++)
      {
        if (intermediate_mod && ip == intermediate_passes-1) {
          intermediate_size = intermediate_mod;
        }

        // Process data as interleaved samples.
        for (i=ref_offset; i<intermediate_size+ref_offset; i++) {
          for (c=0; c<channel_count; c++) {
            this->_intermediate_audio_buffer[c][i] = this->_unpack1(current_io_byte);
            current_io_byte += this->_sizeof_io_sample;
          }
        }

        if (!this->_intermediate_audio_buffer_is_reference) {
          // Copy the intermediate data to the source audio buffer.
          copy_args.size       = intermediate_size;
          copy_args.dst_offset = current_sample;
          this->_intermediate_audio_buffer.copy_to(this->_source_audio_buffer, copy_args);
        }

        current_sample += intermediate_size;
        ref_offset = this->_intermediate_audio_buffer_is_reference
          ? current_sample
          : 0;
      }

      // Unexpected EOF.
      if (eof) {
        break;
      }
    }

    return current_sample;
  }

  std::size_t write(std::size_t size=0, std::size_t offset=0) override
  {
    // TODO: Make method to check if stuff is set.
    if (!(this->_stream && this->_source_audio_buffer)) {
      return 0;
    }

    // TODO: Check if stream is writable?

    std::tie(size, offset) = this->_sanitize_io_params(size, offset);
    // If size is still 0, we don't do anything.
    if (size == 0) {
      return 0;
    }

    CopyArgs copy_args;

    channel_count_t channel_count = this->_intermediate_audio_buffer.get_channel_count();
    // Cache value to speed up performance.
    auto io_divider = channel_count * this->_sizeof_io_sample;
    // Whole division and back to get the amount of bytes to fill the I/O buffer
    // so no samples or channels are written incompletely.
    std::size_t amount_bytes_per_write = this->_io_buffer_size  / io_divider * io_divider;
    std::size_t samples_per_write      = amount_bytes_per_write / io_divider;

    // TODO: Necessary to check if we've written successfully?

    buffer_size_t   i;
    channel_count_t c;

    // If we can use the source audio buffer directly.
    buffer_size_t ref_offset = 0;

    std::size_t current_sample  = 0;
    std::size_t current_io_byte = 0;
    while (current_sample < size)
    {
      if (current_sample + samples_per_write > size) {
        samples_per_write = size - current_sample;
        amount_bytes_per_write = samples_per_write * io_divider;
      }

      std::size_t intermediate_size   = this->_intermediate_audio_buffer.get_buffer_size();
      std::size_t intermediate_passes = samples_per_write / intermediate_size;
      std::size_t intermediate_mod    = samples_per_write % intermediate_size;
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
        if (!this->_intermediate_audio_buffer_is_reference) {
          // Copy the intermediate data from the source audio buffer.
          copy_args.size       = intermediate_size;
          copy_args.src_offset = current_sample;
          this->_intermediate_audio_buffer.copy_from(this->_source_audio_buffer, copy_args);
        }

        // Process data as interleaved samples.
        for (i=0+ref_offset; i<intermediate_size+ref_offset; i++) {
          for (c=0; c<channel_count; c++) {
            this->_pack1(this->_intermediate_audio_buffer[c][i], current_io_byte);
            current_io_byte += this->_sizeof_io_sample;
          }
        }

        current_sample += intermediate_size;
        ref_offset = this->_intermediate_audio_buffer_is_reference
          ? current_sample
          : 0;
      }

      // Write new data to the stream.
      this->_stream->write(this->_io_buffer, amount_bytes_per_write);
      current_io_byte = 0;
    }

    return current_sample;
  }

  protected:

  virtual T _unpack1(std::size_t& io_buffer_offset)
  =0;

  virtual void _pack1(T& value, std::size_t& io_buffer_offset)
  =0;

  protected:

  std::tuple<std::size_t, std::size_t> _sanitize_io_params(std::size_t size, std::size_t offset)
  {
    // TODO: If size is bigger than source size, calculate the offset where copying should start from.
    // TODO: Maybe also use f.seek() to seek the offsets?

    auto source_size = this->_source_audio_buffer->get_buffer_size();

    // Full buffer size.
    if (size == 0) {
      size = source_size;
    }
    // Ensure size and offset within bounds.
    if (size + offset > source_size) {
      if (offset > source_size) {
        offset -= source_size;
      }
      size = source_size - offset;
    }

    return std::make_tuple(size, offset);
  }

  void _cleanup()
  {
    if (this->_io_buffer) {
      ALLOCATOR_T<char> io_buffer_alloc;
      using io_buffer_alloc_t = std::allocator_traits<decltype(io_buffer_alloc)>;

      io_buffer_alloc_t::deallocate(io_buffer_alloc, this->_io_buffer, this->_io_buffer_size);
      this->_io_buffer = nullptr;
    }
    // Cleanup dynamically allocated memory in the intermediate audio buffer.
    if (!this->_intermediate_audio_buffer_is_reference) {
      this->_intermediate_audio_buffer.resize(0, 0);
    }

    return;
  }

};

// *****************************************************************************

} // namespace audiobuffer::io
