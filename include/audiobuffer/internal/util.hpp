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

  // We can skip processing if it's the center value.
  DST_T output;

  constexpr int POS_CORRECTION_SHIFTS = []() {
    int shifts = (DST_BITS / SRC_BITS) - 1;
    if (shifts < 0) {
      shifts = 0;
    }
    return shifts;
  }();
  // FIXME: There should be a way to spread out offset incrementally so
  //   the values map correctly. Haven't found the exact pattern when
  //   more values should be added yet. For the time being this only
  //   adds miniscule offset (at most 0.003%).
  constexpr int POS_POSTFIX = []() {
    int add = (1 << POS_CORRECTION_SHIFTS) - 1;
    if (add <= 0) {
      add = 1;
    }
    return add;
  }();

  if constexpr (std::is_unsigned_v<SRC_T>)
  {
    constexpr SRC_T SRC_CENTER = 1 << (SRC_BITS - 1);
    constexpr DST_T DST_MAX = (static_cast<DST_T>(1) << (DST_BITS-2)) - 1 + (static_cast<DST_T>(1) << (DST_BITS-2));

    if (input == SRC_CENTER) {
      return static_cast<DST_T>(0);
    }

    bool is_positive = input > SRC_CENTER;
    if (is_positive) {
      input -= SRC_CENTER;
    }

    // Shift right.
    if constexpr (DST_BITS < SRC_BITS) {
      input >>= VALUE_SHIFT;
    }

    output = static_cast<DST_T>(input);
    if (!is_positive) {
      // We need to make the value a signed negative value again.
      constexpr DST_T NEG_OFFSET = DST_BITS < SRC_BITS
        ? DST_MAX
        : static_cast<DST_T>(SRC_CENTER - 1);
      output = output - NEG_OFFSET - 1;
    }

    // Shift left.
    if constexpr (DST_BITS > SRC_BITS) {
      DST_T pre_shifted = output;

      output <<= VALUE_SHIFT;
      // Range correction: positive range has less values than negative.
      if (is_positive) {
        for (int i=0; i<POS_CORRECTION_SHIFTS; i++) {
          output += (pre_shifted << POS_CORRECTION_SHIFTS) << (7 * i);
        }
        output |= POS_POSTFIX;
      }
    }
  }
  // Is signed.
  else
  {
    constexpr DST_T DST_CENTER = 1 << (DST_BITS - 1);
    constexpr SRC_T SRC_MAX = (static_cast<SRC_T>(1) << (SRC_BITS-2)) - 1 + (static_cast<SRC_T>(1) << (SRC_BITS-2));

    if (input == 0) {
      return static_cast<DST_T>(DST_CENTER);
    }

    bool is_negative = input < 0;
    if (is_negative) {
      input = input + SRC_MAX + 1;
    }

    // Shift right.
    if constexpr (DST_BITS < SRC_BITS) {
      input >>= VALUE_SHIFT;
    }

    output = static_cast<DST_T>(input);
    if (!is_negative) {
      // We need to make the value an unsigned positive value again.
      constexpr DST_T POS_OFFSET = DST_BITS < SRC_BITS
        ? DST_CENTER - 1
        : static_cast<DST_T>(SRC_MAX);
      output = output + POS_OFFSET + 1;
    }

    // Shift left.
    if constexpr (DST_BITS > SRC_BITS) {
      DST_T pre_shifted = static_cast<DST_T>(input);

      output <<= VALUE_SHIFT;
      // Range correction: positive range has less values than negative.
      if (!is_negative) {
        for (int i=0; i<POS_CORRECTION_SHIFTS; i++) {
          output += (pre_shifted << POS_CORRECTION_SHIFTS) << (7 * i);
        }
        output |= POS_POSTFIX;
      }
    }
  }

  return output;
}

template<typename T, int SRC_BITS, int DST_BITS>
inline constexpr auto scale_int(T input) noexcept
{
  T scaled_value = input;
  constexpr T center = (T(1) << (sizeof(T)*8-2)) + ((T(1) << (sizeof(T)*8-2)) - 1);

  constexpr auto SHIFT_AMOUNT = constexpr_abs(DST_BITS - SRC_BITS);

  // Bitshift left; cast to target type first, then shift.
  if constexpr (DST_BITS > SRC_BITS) {
    scaled_value <<= SHIFT_AMOUNT;

    // Range correction: positive range has less values than negative.
    if (input > center) {
      constexpr int POS_CORRECTION_SHIFTS = []() {
        int shifts = (DST_BITS / SRC_BITS) - 1;
        if (shifts < 0) {
          shifts = 0;
        }
        return shifts;
      }();
      // FIXME: There should be a way to spread out offset incrementally so
      //   the values map correctly. Haven't found the exact pattern when
      //   more values should be added yet. For the time being this only
      //   adds miniscule offset (at most 0.003%).
      constexpr int POS_POSTFIX = []() {
        int add = (1 << POS_CORRECTION_SHIFTS) - 1;
        if (add <= 0) {
          add = 1;
        }
        return add;
      }();

      for (int i=0; i<POS_CORRECTION_SHIFTS; i++) {
        scaled_value += (input << POS_CORRECTION_SHIFTS) << (7 * i);
      }
      scaled_value |= POS_POSTFIX;
    }
  }
  // Bitshift right; shift first, then cast to target type.
  if constexpr (DST_BITS < SRC_BITS) {
    scaled_value >>= SHIFT_AMOUNT;
  }

  return scaled_value;
}

// *****************************************************************************

} // namespace audiobuffer::internal
