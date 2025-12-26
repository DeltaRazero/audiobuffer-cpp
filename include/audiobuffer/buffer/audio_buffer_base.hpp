#pragma once

// *****************************************************************************

#include <cstdint>
#include <cstdlib>

#include "../internal/macro.hpp"
#include "../internal/copy.hpp"
#include "../descriptor.hpp"

#include "./audio_buffer_interface.hpp"

// *****************************************************************************

namespace audiobuffer {

// *****************************************************************************

template <typename T>
class AudioBufferBase : public AudioBufferInterface
{
  // :: PUBLIC META ATTRIBUTES :: //

  public:

    /// @brief Sample type.
    using SAMPLE_T = T;
    /// @brief Descriptor containing metadata about the used sample type.
    using DESCRIPTOR = SampleDescriptor<SAMPLE_T>;

  // :: PROTECTED ATTRIBUTES :: //

  protected:

  AudioBufferData* _data;

  // :: OPERATORS :: //

  public:

  ///
  /// @brief Index operator.
  ///
  /// @param channel_index Channel to index.
  ///
  /// @return The pointer to the specified channel.
  ///
  /// @warning This operation is not null safe. If you wish to check if you are
  ///   within bounds, you must check your value against `get_channel_count()`
  ///   manually.
  ///
  SAMPLE_T* operator[] (channel_count_t channel_index) const noexcept
  { return this->get_channel(channel_index); }

  // :: INTERFACE METHODS :: //

  public:

  bool has_data() const noexcept override
  {
    if (!this->_data) {
      return false;
    }
    return static_cast<bool>(this->_data->buffer) && static_cast<bool>(this->_data->channels);
  }

  AudioBufferData* get_data() const noexcept override
  { return this->_data; }

  format_id_t get_format_id() const noexcept override
  { return this->has_data() ? this->_data->format_id : 0; }

  buffer_size_t get_buffer_size() const noexcept override
  { return this->has_data() ? this->_data->buffer_size : 0; }

  channel_count_t get_channel_count() const noexcept override
  { return this->has_data() ? this->_data->channel_count : 0; }

  void clear() noexcept override
  {
    for (channel_count_t c=0; c<this->get_channel_count(); c++) {
      auto channel = this->get_channel(c);
      for (buffer_size_t i=0; i<this->get_buffer_size(); i++) {
        channel[i] = DESCRIPTOR::CENTER;
      }
    }
    return;
  }

  bool duplicate_channel(channel_count_t src_channel_index, channel_count_t dst_channel_index) noexcept override
  {
    // Do nothing if the channels are the same
    if (src_channel_index == dst_channel_index) {
      return true;
    }
    // Check if args in range.
    if (src_channel_index > this->get_channel_count() || dst_channel_index > this->get_channel_count()) {
      return false;
    }

    auto src_channel = this->get_channel(src_channel_index);
    auto dst_channel = this->get_channel(dst_channel_index);

    for (buffer_size_t i=0; i<this->_data->buffer_size; i++) {
      dst_channel[i] = src_channel[i];
    }

    return true;
  }

  bool copy_from(AudioBufferInterface* src, CopyArgs args=COPY_ARGS_DEFAULT) audiobuffer__noexcept override
  {
    // Check if both buffers point to valid audio buffer data instances.
    if (!src) {
      return false;
    }
    if (!(src->get_data() && this->_data)) {
      return false;
    }

    auto is_copied = this->_on_copy_from(*src, args);
    if (!is_copied) {
      #if (audiobuffer__disable_exceptions)
        return false;
      #else
        throw std::runtime_error("Cannot copy audio buffer: the audio type is unsupported.");
      #endif
    };

    return true;
  }

  bool copy_to(AudioBufferInterface* dst, CopyArgs args=COPY_ARGS_DEFAULT) audiobuffer__noexcept override
  {
    // Check if both buffers point to valid audio buffer data instances.
    if (!dst) {
      return false;
    }
    if (!(this->_data && dst->get_data())) {
      return false;
    }

    auto is_copied = this->_on_copy_to(*dst, args);
    if (!is_copied) {
      #if (audiobuffer__disable_exceptions)
        return false;
      #else
        throw std::runtime_error("Cannot copy audio buffer: the audio type is unsupported.");
      #endif
    };

    return true;
  }

  // :: PUBLIC METHODS :: //

  public:

  ///
  /// @brief Index operator.
  ///
  /// @param channel_index Channel to index.
  ///
  /// @return The pointer to the specified channel.
  ///
  /// @warning This operation is not null safe. If you wish to check if data is
  ///   present, you must check against `has_data()` manually.
  ///
  SAMPLE_T** get_channels() const noexcept
  { return reinterpret_cast<SAMPLE_T**>(this->_data->channels); }

  ///
  /// @brief Index operator.
  ///
  /// @param channel_index Channel to index.
  ///
  /// @return The pointer to the specified channel.
  ///
  /// @warning This operation is not null safe. If you wish to check if you are
  ///   within bounds, you must check your value against `get_channel_count()`
  ///   manually.
  ///
  SAMPLE_T* get_channel(channel_count_t channel_index) const noexcept
  { return this->get_channels()[channel_index]; }

  // TODO: Interleaved iterator + non interleaved iterator

  // :: PROTECTED METHODS :: //

  ///
  /// @brief Callback when contents from another audio buffer needs to be copied
  ///   to this audio buffer.
  ///
  /// @param src Audio buffer to copy from.
  /// @param args Arguments to adjust the copy operation.
  ///
  /// @return Whether a copy operation was carried out.
  ///
  virtual bool _on_copy_from(AudioBufferInterface& src, CopyArgs& args) audiobuffer__noexcept
  {
    bool is_copied = false;
    #ifndef audiobuffer__no_copy_defaults
      // Define preprocessor macro function for convenience
      #define FORMAT_CASE(__src_sample_t)\
      case (SampleDescriptor<__src_sample_t>::FORMAT_ID):\
        is_copied = ::audiobuffer::internal::copy_audio_buffer_data<__src_sample_t, SAMPLE_T>(*src.get_data(), *this->_data, args);\
        break;
      //#enddefine
      switch (src.get_data()->format_id)
      {
        FORMAT_CASE(std::uint8_t);
        FORMAT_CASE(std:: int8_t);
        FORMAT_CASE(std::int16_t);
        FORMAT_CASE(std::int32_t);
        FORMAT_CASE(float );
        FORMAT_CASE(double);

        default:
          break;
      }
      #undef FORMAT_CASE
    #endif
    return is_copied;
  }

  ///
  /// @brief Callback when contents from this audio buffer needs to be copied
  ///   to another audio buffer.
  ///
  /// @param dst Audio buffer to copy to.
  /// @param args Arguments to adjust the copy operation.
  ///
  /// @return Whether a copy operation was carried out.
  ///
  virtual bool _on_copy_to(AudioBufferInterface& dst, CopyArgs& args) audiobuffer__noexcept
  {
    bool is_copied = false;
    #ifndef audiobuffer__no_copy_defaults
      // Define preprocessor macro function for convenience
      #define FORMAT_CASE(__dst_sample_t)\
      case (SampleDescriptor<__dst_sample_t>::FORMAT_ID):\
        is_copied = ::audiobuffer::internal::copy_audio_buffer_data<SAMPLE_T, __dst_sample_t>(*this->_data, *dst.get_data(), args);\
        break;
      //#enddefine
      switch (dst.get_data()->format_id)
      {
        FORMAT_CASE(std::uint8_t);
        FORMAT_CASE(std:: int8_t);
        FORMAT_CASE(std::int16_t);
        FORMAT_CASE(std::int32_t);
        FORMAT_CASE(float );
        FORMAT_CASE(double);

        default:
          break;
      }
      #undef FORMAT_CASE
    #endif
    return is_copied;
  }

  // :: HELPER METHODS :: //

  protected:

  ///
  /// @brief Populates the channel table.
  ///
  void _build_channel_table() noexcept
  {
    if (!this->has_data()) {
      return;
    }

    auto channel_count = this->_data->channel_count;
    auto buffer_size   = this->_data->buffer_size;

    for (channel_count_t c=0; c<channel_count; c++) {
      this->_data->channels[c] = &reinterpret_cast<SAMPLE_T*>(this->_data->buffer)[buffer_size*c];
    }
    return;
  }

};

// *****************************************************************************

} // namespace audiobuffer
