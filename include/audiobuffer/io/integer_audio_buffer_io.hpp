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

///
/// @brief Audio buffer I/O stream for integer data.
///
/// @tparam T Integer storage sample type.
/// @tparam BIT_DEPTH_V Amount of bits of the integer, defaults to the bit depth
///   of `T`.
/// @tparam ALIGNED_V Whether the data is byte-aligned, defaults to `false`.
/// @tparam ALLOCATOR_T Allocator class, defaults to `std::allocator`.
///
template<
  typename T,
  unsigned int BIT_DEPTH_V=SampleDescriptor<T>::BIT_DEPTH,
  bool ALIGNED_V=false,
  template<typename> class ALLOCATOR_T=std::allocator
>
class IntegerAudioBufferIO : public AudioBufferIOBase<T, ALLOCATOR_T>
{
  static_assert(std::is_integral_v<T>, "Type must be integer.");

  protected:

  using NUMIO_TYPE = numio::IntIO<T, BIT_DEPTH_V, ALIGNED_V>;
  static constexpr bool SAME_DEPTH = BIT_DEPTH_V == SampleDescriptor<T>::BIT_DEPTH;

  public:

  IntegerAudioBufferIO(
    std::iostream& stream,
    ::audiobuffer::AudioBufferInterface& audio_buffer,
    std::size_t io_buffer_size=IntegerAudioBufferIO::DEFAULT_IO_BUFFER_SIZE
  )
    : AudioBufferIOBase<T, ALLOCATOR_T>(stream, audio_buffer, io_buffer_size, NUMIO_TYPE::N_IO_BYTES)
  {}

  protected:

  T _unpack1(std::size_t io_buffer_offset) override final
  {
    if constexpr (SAME_DEPTH) {
      return NUMIO_TYPE::unpack(this->_io_buffer, io_buffer_offset);
    }
    else {
      return ::audiobuffer::internal::scale_int<T, BIT_DEPTH_V, SampleDescriptor<T>::BIT_DEPTH>(
        NUMIO_TYPE::unpack(this->_io_buffer, io_buffer_offset)
      );
    }
  }

  void _pack1(T& value, std::size_t io_buffer_offset) override final
  {
    if constexpr (SAME_DEPTH)
    {
      NUMIO_TYPE::pack(
        value,
        this->_io_buffer,
        io_buffer_offset
      );
    }
    else
    {
      NUMIO_TYPE::pack(
        ::audiobuffer::internal::scale_int<T, SampleDescriptor<T>::BIT_DEPTH, BIT_DEPTH_V>(
          value
        ),
        this->_io_buffer,
        io_buffer_offset
      );
    }
    return;
  }
};

// *****************************************************************************

} // namespace audiobuffer::io
