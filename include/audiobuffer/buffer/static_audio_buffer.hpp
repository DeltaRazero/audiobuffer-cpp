#pragma once

// *****************************************************************************

#include <array>
#include <cstdint>
#include <cstdlib>

#include "../internal/macro.hpp"

#include "./audio_buffer_base.hpp"

// *****************************************************************************

namespace audiobuffer {

// *****************************************************************************

///
/// @brief An audio buffer container for storing a fixed size sequence of channels and samples.
///
/// @tparam T The sample type.
/// @tparam CHANNEL_COUNT_V The amount of channels the audio buffer should have.
/// @tparam FRAME_COUNT_V The amount of frames (samples per channel) the audio buffer should have.
///
template <typename T, channel_count_t CHANNEL_COUNT_V, frame_count_t FRAME_COUNT_V>
class StaticAudioBuffer : public AudioBufferBase<T>
{
  // :: PRIVATE ATTRIBUTES :: //

  private:
    std::array<typename AudioBufferBase<T>::SAMPLE_T*, CHANNEL_COUNT_V> _channel_table;
    std::array<typename AudioBufferBase<T>::SAMPLE_T , CHANNEL_COUNT_V*FRAME_COUNT_V> _buffer_data;

    // Pre-allocated data struct where we can point `this->_data` to.
    AudioBufferData _fixed_data;

  // :: CONSTRUCTOR :: //

  public:

  StaticAudioBuffer() :
    _fixed_data({ .format_id = AudioBufferBase<T>::DESCRIPTOR::FORMAT_ID, .resizable = false })
  {
    this->_is_managed = false;

    // Reference the auto-managed data to the data pointer of the base class.
    this->_data = &this->_fixed_data;

    // Set buffer memory location and metadata.
    this->_fixed_data.buffer      = static_cast<void*>(this->_buffer_data.data());
    this->_fixed_data.frame_count = FRAME_COUNT_V;

    // Reference channel table memory location and metadata, use the base class
    // method to build the channel table.
    this->_fixed_data.channels = reinterpret_cast<void**>(this->_channel_table.data());
    this->_fixed_data.channel_count = CHANNEL_COUNT_V;
    this->_build_channel_table();

    // Since the data is not managed dynamically, there is no deallocator.
    this->_fixed_data.deallocate = nullptr;

    // Initialize with DC center values.
    this->clear();
  }

  ~StaticAudioBuffer()
  {
    // Clean values for extra safety.
    this->_fixed_data.buffer      = nullptr;
    this->_fixed_data.frame_count = 0;
    this->_fixed_data.channels      = nullptr;
    this->_fixed_data.channel_count = 0;

    this->_data = nullptr;
  }

  // :: INTERFACE METHODS :: //

  bool resize(frame_count_t frame_count, channel_count_t channel_count=0) audiobuffer__noexcept override final
  {
    // Fixed-size audio buffers can not be resized.
    #if (audiobuffer__disable_exceptions)
      return false;
    #else
      throw std::runtime_error("Cannot resize audio buffer: fixed-size audio buffers cannot be resized.");
    #endif
  }
};

// *****************************************************************************

} // namespace audiobuffer
