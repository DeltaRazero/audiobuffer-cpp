#pragma once

// *****************************************************************************

#include <cstdint>
#include <limits>
#include <type_traits>
#include <cassert>

#include "./sample_type.hpp"

// *****************************************************************************

namespace audiobuffer {

// *****************************************************************************

///
/// @brief Describes descriptive data about a sample type.
///
/// @tparam SAMPLE_T An integer sample type.
///


///
/// @brief Provides descriptive data about a sample type.
///
/// @tparam SAMPLE_T A sample type.
///
template<typename SAMPLE_T, typename=void>
struct SampleDescriptor
{
  static_assert(false, "Unsupported sample type.");
};

///
/// @brief Provides descriptive data about an integer sample type.
///
/// @tparam SAMPLE_T An integer sample type.
///
template<typename SAMPLE_T>
struct SampleDescriptor<SAMPLE_T, std::enable_if_t<std::is_integral_v<SAMPLE_T>>>
{
  ///
  /// @brief The sample type.
  ///
  typedef SAMPLE_T sample_t;

  ///
  /// @brief Whether the sample type is signed or not.
  ///
  static constexpr bool IS_SIGNED = std::is_signed_v<SAMPLE_T>;
  ///
  /// @brief ....
  ///
  static constexpr SampleType TYPE = IS_SIGNED
    ? SampleType::INT
    : SampleType::UINT;

  static constexpr int BIT_DEPTH = sizeof(sample_t)*8; // Assume byte-aligned type.

  static constexpr std::uint_fast32_t FORMAT_ID = (std::uint_fast32_t(TYPE) << 8) + BIT_DEPTH;

  static constexpr sample_t MAX = []() {
    // Last bit is sign-bit for signed integers.
    auto bit_depth = BIT_DEPTH - (1 * IS_SIGNED);
    return
      (sample_t(1) << (bit_depth-1)) - 1 +
      (sample_t(1) << (bit_depth-1));
  }();
  static constexpr sample_t CENTER = IS_SIGNED
    ? 0
    : MAX / 2 + 1;
  static constexpr sample_t MIN = IS_SIGNED
    ? sample_t(-MAX - 1)
    : sample_t(0);
};

///
/// @brief Provides descriptive data about an IEEE Floating Point sample type.
///
/// @tparam SAMPLE_T An IEEE Floating Point sample type.
///
template<typename SAMPLE_T>
struct SampleDescriptor<SAMPLE_T, std::enable_if_t<std::is_floating_point_v<SAMPLE_T> && std::numeric_limits<SAMPLE_T>::is_iec559>>
{
  typedef SAMPLE_T sample_t;

  static constexpr SampleType TYPE = SampleType::IEEE_FLOAT;

  static constexpr int BIT_DEPTH = sizeof(sample_t)*8; // Assume byte-aligned type.
  static constexpr int FRACTION_DEPTH = std::numeric_limits<sample_t>::digits - 1;
  static constexpr int EXPONENT_DEPTH = BIT_DEPTH - FRACTION_DEPTH - 1; // -1 is the signbit.

  static constexpr std::uint_fast32_t FORMAT_ID = (std::uint_fast32_t(TYPE) << 8) + BIT_DEPTH;

  static constexpr sample_t MAX    = sample_t( 1.0);
  static constexpr sample_t CENTER = sample_t( 0.0);
  static constexpr sample_t MIN    = sample_t(-1.0);
};

// *****************************************************************************

} // namespace audiobuffer
