#pragma once

// *****************************************************************************

#include <cstdint>
#include <limits>
#include <type_traits>
#include <cassert>

// *****************************************************************************

namespace audiobuffer {

// *****************************************************************************

enum class SampleType {
  UNSUPPORTED = 0,
  INT,
  UINT,
  IEEE_FLOAT,
};

// *****************************************************************************

// Generic implementation, which triggers for unsupported types.
template<typename SAMPLE_T, typename = void>
struct SampleDescriptor
{
  static_assert(false, "Unsupported sample type.");
};

// Integer sample types.
template<typename SAMPLE_T>
struct SampleDescriptor<SAMPLE_T, std::enable_if_t<std::is_integral_v<SAMPLE_T>>>
{
  typedef SAMPLE_T sample_t;

  static constexpr SampleType TYPE = std::is_signed_v<sample_t>
    ? SampleType::INT
    : SampleType::UINT;

  static constexpr int BIT_DEPTH = sizeof(sample_t)*8; // Assume byte-aligned type.

  static constexpr std::uint_fast32_t FORMAT_ID = (static_cast<std::uint_fast32_t>(TYPE) << 8) + BIT_DEPTH;

  static constexpr sample_t MAX = []() {
    // Last bit is sign-bit for signed integers.
    auto bit_depth = BIT_DEPTH - (1 * std::is_signed_v<sample_t>);
    return
      (static_cast<sample_t>(1) << (bit_depth-1)) - 1 +
      (static_cast<sample_t>(1) << (bit_depth-1));
  }();

  static constexpr sample_t CENTER = std::is_signed_v<sample_t>
    ? 0
    : MAX / 2 + 1;

  static constexpr sample_t MIN = std::is_signed_v<sample_t>
    ? static_cast<sample_t>(-MAX - 1)
    : static_cast<sample_t>(0);
};

// IEEE floating point sample types.
template<typename SAMPLE_T>
struct SampleDescriptor<SAMPLE_T, std::enable_if_t<std::is_floating_point_v<SAMPLE_T> && std::numeric_limits<SAMPLE_T>::is_iec559>>
{
  typedef SAMPLE_T sample_t;

  static constexpr SampleType TYPE = SampleType::IEEE_FLOAT;

  static constexpr int BIT_DEPTH = sizeof(sample_t)*8; // Assume byte-aligned type.
  static constexpr int FRACTION_DEPTH = std::numeric_limits<sample_t>::digits - 1;
  static constexpr int EXPONENT_DEPTH = BIT_DEPTH - FRACTION_DEPTH - 1; // -1 is the signbit.

  static constexpr std::uint_fast32_t FORMAT_ID = (static_cast<std::uint_fast32_t>(TYPE) << 8) + BIT_DEPTH;

  static constexpr sample_t MAX    = static_cast<sample_t>( 1.0);
  static constexpr sample_t CENTER = static_cast<sample_t>( 0.0);
  static constexpr sample_t MIN    = static_cast<sample_t>(-1.0);
};

// *****************************************************************************

} // namespace audiobuffer
