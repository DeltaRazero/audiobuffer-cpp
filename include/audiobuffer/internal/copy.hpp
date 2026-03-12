#pragma once

// *****************************************************************************

#include <cmath>
#include <cstdlib>

#include "../buffer/audio_buffer_interface.hpp"
#include "../descriptor.hpp"

#include "./macro.hpp"
#include "./util.hpp"

// *****************************************************************************

namespace audiobuffer::internal {

// *****************************************************************************

CopyArgs sanitize_copy_args(AudioBufferData& src_data, AudioBufferData& dst_data, CopyArgs& copy_args) noexcept
{
  copy_args.src_offset = std::min(copy_args.src_offset, src_data.buffer_size-1);
  copy_args.dst_offset = std::min(copy_args.dst_offset, dst_data.buffer_size-1);

  auto max_size = std::min(
    src_data.buffer_size - copy_args.src_offset,
    dst_data.buffer_size - copy_args.dst_offset
  );
  copy_args.size = copy_args.size == 0
    ? max_size
    : std::min(copy_args.size, max_size)
  ;

  copy_args.src_channel_offset = std::min<channel_count_t>(copy_args.src_channel_offset, src_data.channel_count-1);
  copy_args.dst_channel_offset = std::min<channel_count_t>(copy_args.dst_channel_offset, dst_data.channel_count-1);

  auto max_channel_count = std::min<channel_count_t>(
    src_data.channel_count - copy_args.src_channel_offset,
    dst_data.channel_count - copy_args.dst_channel_offset
  );
  copy_args.channel_count = copy_args.channel_count == 0
    ? max_channel_count
    : std::min(copy_args.channel_count, max_channel_count)
  ;

  return copy_args;
}

// *****************************************************************************

///
/// @brief Copies audio buffer data.
///
/// @tparam SRC_SAMPLE_T Type of the source audio buffer data.
/// @tparam DST_SAMPLE_T Type of the target audio buffer data.
///
/// @param src Pointer to the source audio buffer data.
/// @param dst Pointer to the target audio buffer data.
///
/// @return Whether the operation was successful.
///
template <typename SRC_SAMPLE_T, typename DST_SAMPLE_T>
bool copy_audio_buffer_data(AudioBufferData& src, AudioBufferData& dst, CopyArgs& copy_args) audiobuffer__noexcept
{
  copy_args = sanitize_copy_args(src, dst, copy_args);

  using SRC_DESCRIPTOR = SampleDescriptor<SRC_SAMPLE_T>;
  using DST_DESCRIPTOR = SampleDescriptor<DST_SAMPLE_T>;

  if (!(src.format_id == SRC_DESCRIPTOR::FORMAT_ID && dst.format_id == DST_DESCRIPTOR::FORMAT_ID)) {
    #if (audiobuffer__disable_exceptions)
      return false;
    #else
      throw std::runtime_error("Cannot copy audio buffer: input or target data does not match format type.");
    #endif
  }

  auto src_channels = reinterpret_cast<SRC_SAMPLE_T**>(src.channels);
  auto dst_channels = reinterpret_cast<DST_SAMPLE_T**>(dst.channels);

  // Amount of frames to pad with zeros.
  auto pad_size = src.buffer_size < dst.buffer_size
    ? dst.buffer_size - src.buffer_size
    : 0;
  if (!copy_args.pad) {
    pad_size = 0;
  }

  buffer_size_t   i;
  channel_count_t c;

  buffer_size_t i_src;
  buffer_size_t i_dst;

  SRC_SAMPLE_T* src_channel;
  DST_SAMPLE_T* dst_channel;

  for (c=0; c<copy_args.channel_count; c++)
  {
    src_channel = src_channels[c + copy_args.src_channel_offset];
    dst_channel = dst_channels[c + copy_args.dst_channel_offset];

    for (i=0; i<copy_args.size; i++)
    {
      i_src = i + copy_args.src_offset;
      i_dst = i + copy_args.dst_offset;

      // Same format; no conversion needed.
      if constexpr (SRC_DESCRIPTOR::FORMAT_ID == DST_DESCRIPTOR::FORMAT_ID) {
        dst_channel[i_dst] = src_channel[i_src];
      }

      // (U)INT -> (U)INT
      else if constexpr (
        (SRC_DESCRIPTOR::TYPE == SampleType::INT || SRC_DESCRIPTOR::TYPE == SampleType::UINT)
        &&
        (DST_DESCRIPTOR::TYPE == SampleType::INT || DST_DESCRIPTOR::TYPE == SampleType::UINT)
      ) {
        // Same signed types.
        if constexpr (SRC_DESCRIPTOR::TYPE == DST_DESCRIPTOR::TYPE)
        {
          constexpr auto SRC_BITS = SRC_DESCRIPTOR::BIT_DEPTH;
          constexpr auto DST_BITS = DST_DESCRIPTOR::BIT_DEPTH;

          constexpr auto SHIFT_AMOUNT = constexpr_abs(DST_BITS - SRC_BITS);

          // Bitshift left; cast to target type first, then shift.
          if constexpr (DST_BITS > SRC_BITS) {
            dst_channel[i_dst] = (static_cast<DST_SAMPLE_T>(src_channel[i_src]) << SHIFT_AMOUNT);

            // Range correction: positive range has less values than negative.
            if (src_channel[i_src] > SRC_DESCRIPTOR::CENTER) {
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

              DST_SAMPLE_T pre_shifted = static_cast<DST_SAMPLE_T>(src_channel[i_dst]);

              for (int i=0; i<POS_CORRECTION_SHIFTS; i++) {
                dst_channel[i_dst] += (pre_shifted << POS_CORRECTION_SHIFTS) << (7 * i);
              }
              dst_channel[i_dst] |= POS_POSTFIX;
            }
          }
          // Bitshift right; shift first, then cast to target type.
          if constexpr (DST_BITS < SRC_BITS) {
            dst_channel[i_dst] = static_cast<DST_SAMPLE_T>(src_channel[i_src] >> SHIFT_AMOUNT);
          }
        }
        // Sign conversion needed.
        else {
          dst_channel[i_dst] = scaled_inv_2s_complement<SRC_SAMPLE_T, DST_SAMPLE_T>(src_channel[i_src]);
        }
      }

      // (U)INT -> FLOAT
      else if constexpr (
        (SRC_DESCRIPTOR::TYPE == SampleType::INT || SRC_DESCRIPTOR::TYPE == SampleType::UINT)
        &&
        (DST_DESCRIPTOR::TYPE == SampleType::IEEE_FLOAT)
      ) {
        auto value = src_channel[i_src];
        bool is_positive = value >= SRC_DESCRIPTOR::CENTER;

        if constexpr (SRC_DESCRIPTOR::TYPE == SampleType::INT) {
          // NOTE: Loss of precision occurs here when the source integer type
          // has a larger bit-depth than the the bit-depth of the exponent part
          // of the target floating-point type.
          // E.g., int32 and float32 (which has 24 bits for the exponent part).
          dst_channel[i_dst] = is_positive
            ?  static_cast<DST_SAMPLE_T>(value) / static_cast<DST_SAMPLE_T>(SRC_DESCRIPTOR::MAX)
            : -static_cast<DST_SAMPLE_T>(value) / static_cast<DST_SAMPLE_T>(SRC_DESCRIPTOR::MIN);
        }
        // Unsigned integer.
        else {
          dst_channel[i_dst] = is_positive
            ?  static_cast<DST_SAMPLE_T>(value - SRC_DESCRIPTOR::CENTER) / static_cast<DST_SAMPLE_T>(SRC_DESCRIPTOR::CENTER - 1)
            : -static_cast<DST_SAMPLE_T>(SRC_DESCRIPTOR::CENTER - value) / static_cast<DST_SAMPLE_T>(SRC_DESCRIPTOR::CENTER);
        }
      }

      // FLOAT -> (U)INT
      else if constexpr (
        (SRC_DESCRIPTOR::TYPE == SampleType::IEEE_FLOAT)
        &&
        (DST_DESCRIPTOR::TYPE == SampleType::INT || DST_DESCRIPTOR::TYPE == SampleType::UINT)
      ) {
        auto value = clamp<SRC_SAMPLE_T>(src_channel[i_src], SRC_DESCRIPTOR::MIN, SRC_DESCRIPTOR::MAX);
        bool is_positive = value >= SRC_DESCRIPTOR::CENTER;

        if constexpr (DST_DESCRIPTOR::TYPE == SampleType::INT) {
          dst_channel[i_dst] = is_positive
            ? static_cast<DST_SAMPLE_T>( value * static_cast<SRC_SAMPLE_T>(DST_DESCRIPTOR::MAX))
            : static_cast<DST_SAMPLE_T>(-value * static_cast<SRC_SAMPLE_T>(DST_DESCRIPTOR::MIN));
        }
        else {
          dst_channel[i_dst] = is_positive
            ? static_cast<DST_SAMPLE_T>(value * static_cast<SRC_SAMPLE_T>(DST_DESCRIPTOR::CENTER - 1)) + DST_DESCRIPTOR::CENTER
            : DST_DESCRIPTOR::CENTER - static_cast<DST_SAMPLE_T>(-value * static_cast<SRC_SAMPLE_T>(DST_DESCRIPTOR::CENTER));
        }
        // Ensure the converted value hasn't overflowed when there's a lack of
        // precision, e.g. float32 to int32 would overflow on int32::max.
        dst_channel[i_dst] -= 1 * (is_positive && dst_channel[i_dst] < DST_DESCRIPTOR::CENTER);
      }

      // FLOAT -> FLOAT
      else if constexpr (
        (SRC_DESCRIPTOR::TYPE == SampleType::IEEE_FLOAT)
        &&
        (DST_DESCRIPTOR::TYPE == SampleType::IEEE_FLOAT)
      ) {
        // Compiler will handle resizing.
        dst_channel[i_dst] = static_cast<DST_SAMPLE_T>(src_channel[i_src]);
      }

    }

    // Padding if needed.
    for (buffer_size_t i=copy_args.size; i<copy_args.size+pad_size; i++) {
      dst_channel[i] = DST_DESCRIPTOR::CENTER;
    }
  }

  return true;
}

// *****************************************************************************

} // namespace audiobuffer::internal
