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
/// @brief Audio buffer I/O stream for IEEE Floating Point data.
///
/// @tparam T Floating point storage sample type.
/// @tparam EXPONENT_DEPTH_V Amount of bits of the exponent part, defaults to
///   the exponent depth of `T`.
/// @tparam FRACTION_DEPTH_V Amount of bits of the fraction part, defaults to
///   the fraction depth of `T`.
/// @tparam ALIGNED_V Whether the data is byte-aligned, defaults to `false`.
/// @tparam ALLOCATOR_T Allocator class, defaults to `std::allocator`.
///
template<
  typename T,
  unsigned int EXPONENT_DEPTH_V=SampleDescriptor<T>::EXPONENT_DEPTH,
  unsigned int FRACTION_DEPTH_V=SampleDescriptor<T>::FRACTION_DEPTH,
  bool ALIGNED_V=false,
  template<typename> class ALLOCATOR_T=std::allocator
>
class IEEEFloatAudioBufferIO : public AudioBufferIOBase<T, ALLOCATOR_T>
{
  static_assert(std::is_floating_point_v<T>, "Type must be floating point.");

  protected:

  using NUMIO_TYPE = numio::FloatIO<T, EXPONENT_DEPTH_V, FRACTION_DEPTH_V, ALIGNED_V>;

  public:

  ///
  /// @param stream The stream object, implementing `std::iostream`.
  /// @param io_buffer_size The size of the buffer used to buffer I/O operations.
  ///
  IEEEFloatAudioBufferIO(
    std::iostream& stream,
    channel_count_t stream_channel_count,
    std::size_t io_buffer_size=IEEEFloatAudioBufferIO::DEFAULT_IO_BUFFER_SIZE
  )
    : AudioBufferIOBase<T, ALLOCATOR_T>(
        NUMIO_TYPE::N_IO_BYTES,
        stream,
        stream_channel_count,
        io_buffer_size
      )
  {}

  protected:

  T _unpack1(std::size_t io_buffer_offset) override final
  {
    return NUMIO_TYPE::unpack(this->_io_buffer, io_buffer_offset);
  }

  void _pack1(T& value, std::size_t io_buffer_offset) override final
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
