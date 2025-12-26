#pragma once

// *****************************************************************************

#include <cmath>

// *****************************************************************************

namespace audiobuffer::internal {

// *****************************************************************************

///
/// @brief Implements constexpr std::abs.
///
/// @tparam T Numeric type.
/// @param x
/// @return
///
template<class T>
inline constexpr auto constexpr_abs(T const& x) noexcept
{ return x < 0 ? -x : x; }

///
/// @brief Clamps a value.
///
/// @tparam T Numeric type.
/// @param x Value.
/// @param min Minimum value.
/// @param max Maximum value.
///
/// @return The clamped value.
///
/// @author Jorge
/// @cite https://stackoverflow.com/a/16659263
///
template <class T>
inline constexpr T clamp(T x, T min, T max) noexcept
{
  const T t = x < min ? min : x;
  return t > max ? max : t;
}

///
/// @brief Calculates/inverts 2's complement in a platform-agnostic way.
///
/// @tparam SRC_T Integer with a different signedness than the output type.
/// @tparam DST_T Integer with a different signedness than the input type.
///
/// @param input Input integer.
///
/// @return Output integer.
///
template <typename SRC_T, typename DST_T>
DST_T inline scaled_inv_2s_complement(SRC_T input) noexcept
{
  static_assert(std::is_integral_v<SRC_T>, "Input type is not an integer.");
  static_assert(std::is_integral_v<DST_T>, "Output type is not an integer.");
  static_assert(
    std::is_signed_v<SRC_T> != std::is_signed_v<DST_T>,
    "Input and output types must have different signedness."
  );

  constexpr int SRC_BITS = sizeof(SRC_T)*8;
  constexpr int DST_BITS = sizeof(DST_T)*8;
  constexpr int VALUE_SHIFT = constexpr_abs(DST_BITS - SRC_BITS);

  #define __calc_middle(__t, __bits) \
     (static_cast<__t>(1) << (__bits-2)) + \
    ((static_cast<__t>(1) << (__bits-2)) - 1)
  // #enddefine

  constexpr SRC_T SRC_MIDDLE = __calc_middle(SRC_T, SRC_BITS);
  constexpr DST_T DST_MIDDLE = __calc_middle(DST_T, DST_BITS);

  #undef __calc_middle

  DST_T output;

  if constexpr (std::is_unsigned_v<SRC_T>)
  {
    bool is_positive = input >= SRC_MIDDLE;
    if (is_positive) {
      input = input - SRC_MIDDLE - 1;
    }

    // Shift right.
    if constexpr (DST_BITS < SRC_BITS) {
      input >>= VALUE_SHIFT;
    }

    output = static_cast<DST_T>(input);
    if (!is_positive) {
      constexpr DST_T adjust_middle = DST_BITS < SRC_BITS
        ? DST_MIDDLE
        : static_cast<DST_T>(SRC_MIDDLE);
      output = output - adjust_middle - 1;
    }

    // Shift left.
    if constexpr (DST_BITS > SRC_BITS) {
      output <<= VALUE_SHIFT;
      // Range correction: positive range has less values than negative.
      output += is_positive
        ? (1 << (input & VALUE_SHIFT)) - 1
        : 0;
    }
  }
  // Is signed.
  else
  {
    bool is_negative = input < 0;
    if (is_negative) {
      input = input + SRC_MIDDLE + 1;
    }

    // Shift right.
    if constexpr (DST_BITS < SRC_BITS) {
      input >>= VALUE_SHIFT;
    }

    output = static_cast<DST_T>(input);
    if (!is_negative) {
      constexpr DST_T adjust_middle = DST_BITS < SRC_BITS
        ? DST_MIDDLE
        : static_cast<DST_T>(SRC_MIDDLE);
      output = output + adjust_middle + 1;
    }

    // Shift left.
    if constexpr (DST_BITS > SRC_BITS) {
      output <<= VALUE_SHIFT;
      // Range correction: positive range has less values than negative.
      output += !is_negative
        ? (1 << (input & VALUE_SHIFT)) - 1
        : 0;
    }
  }

  return output;
}

template<typename T, int SRC_BITS, int DST_BITS>
inline constexpr auto scale_int(T input) noexcept
{
  T scaled_value = input;
  constexpr T middle = (T(1) << (sizeof(T)*8-2)) + ((T(1) << (sizeof(T)*8-2)) - 1);

  constexpr auto SHIFT_AMOUNT = constexpr_abs(DST_BITS - SRC_BITS);

  // Bitshift left; cast to target type first, then shift.
  if constexpr (DST_BITS > SRC_BITS) {
    scaled_value <<= SHIFT_AMOUNT;
    // Range correction: positive range has less values than negative.
    scaled_value += input > middle
      ? (1 << (input & SHIFT_AMOUNT)) - 1
      : 0;
  }
  // Bitshift right; shift first, then cast to target type.
  if constexpr (DST_BITS < SRC_BITS) {
    scaled_value >>= SHIFT_AMOUNT;
  }

  return scaled_value;
}

// *****************************************************************************

} // namespace audiobuffer::internal
