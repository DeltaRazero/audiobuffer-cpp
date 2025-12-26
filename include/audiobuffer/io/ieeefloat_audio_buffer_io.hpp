#pragma once

// *****************************************************************************

#include <cstdint>
#include <iostream>
#include <tuple>
#include <memory>

#include "../../../third-party/numio-cpp/include/numio.hpp"

#include "./audio_buffer_io_base.hpp"

// *****************************************************************************

namespace audiobuffer::io {

// *****************************************************************************

template<
  typename T,
  unsigned int EXPONENT_DEPTH_V=SampleDescriptor<T>::EXPONENT_DEPTH,
  unsigned int FRACTION_DEPTH_V=SampleDescriptor<T>::FRACTION_DEPTH,
  bool ALIGNED_V=false,
  template<typename> class ALLOCATOR_T=std::allocator
>
class IEEEFloatAudioBufferIO : public AudioBufferIOBase<T, ALLOCATOR_T>
{
  static_assert(std::is_floating_point_v<T>, "Type must be an integer.");

  protected:

  using NUMIO_TYPE = numio::FloatIO<T, EXPONENT_DEPTH_V, FRACTION_DEPTH_V, ALIGNED_V>;

  public:

  IEEEFloatAudioBufferIO(std::iostream& stream, ::audiobuffer::AudioBufferInterface& audio_buffer)
    : AudioBufferIOBase<T, ALLOCATOR_T>(stream, audio_buffer, NUMIO_TYPE::N_IO_BYTES)
  {
    // this->set_stream(stream, source_audio_buffer);
  }

  protected:

  T _unpack1(std::size_t& io_buffer_offset) override
  {
    return NUMIO_TYPE::unpack(this->_io_buffer, io_buffer_offset);
  }

  void _pack1(T& value, std::size_t& io_buffer_offset) override
  {
    NUMIO_TYPE::pack(
      value,
      this->_io_buffer,
      io_buffer_offset
    );
    return;
  }
};

// *****************************************************************************

} // namespace audiobuffer::io
