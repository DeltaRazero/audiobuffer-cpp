#include <string>
#include <limits>

#include <omp.h>
#include <gtest/gtest.h>

#define audiobuffer__enable_nonstd_unsigned

#include <audiobuffer.hpp>

#undef audiobuffer__enable_nonstd_unsigned


template <typename SRC_T, typename DST_T>
void test_int_scaling_case(SRC_T& value, long double& neg_scale_factor, long double& pos_scale_factor, DST_T& max_diff)
{
  auto src_buffer = ::audiobuffer::StaticAudioBuffer<SRC_T, 1, 1>();
  auto dst_buffer = ::audiobuffer::StaticAudioBuffer<DST_T, 1, 1>();

  src_buffer[0][0] = value;

  src_buffer.copy_to(&dst_buffer);
  auto copy_to_val = dst_buffer[0][0];

  dst_buffer.copy_from(&src_buffer);
  auto copy_from_val = dst_buffer[0][0];

  // First check if the operations aren't destructive.
  EXPECT_EQ(copy_to_val, copy_from_val);

  // Calculate expected scaled value. This is a much more expensive and slower
  // way to calculate it compared to what is used in the library itself, but
  // is easier and more precise.
  auto scaled_value = static_cast<long double>(value);
  if constexpr (!::audiobuffer::SampleDescriptor<SRC_T>::IS_SIGNED) {
    // Change it to signed-equivalent value when unsigned type.
    scaled_value -= ::audiobuffer::SampleDescriptor<SRC_T>::CENTER;
  }

  scaled_value = value < ::audiobuffer::SampleDescriptor<SRC_T>::CENTER
    ? scaled_value * neg_scale_factor
    : scaled_value * pos_scale_factor
  ;
  scaled_value = std::round(scaled_value);

  if constexpr (!::audiobuffer::SampleDescriptor<DST_T>::IS_SIGNED) {
    // Change it to unsigned-equivalent value when signed type.
    scaled_value += ::audiobuffer::SampleDescriptor<DST_T>::CENTER;
  }
  // Clamp to min and max of destination types for safety.
  DST_T scaled_value_final = std::clamp(
    static_cast<DST_T>(scaled_value),
    std::numeric_limits<DST_T>::min(),
    std::numeric_limits<DST_T>::max()
  );

  if (value == ::audiobuffer::SampleDescriptor<SRC_T>::CENTER) {
    scaled_value_final = ::audiobuffer::SampleDescriptor<DST_T>::CENTER;
  }

  // Negative values.
  if (value < ::audiobuffer::SampleDescriptor<SRC_T>::CENTER) {
    bool eq = copy_from_val == scaled_value_final;

    // When downscaling, off-by-one is allowed. When upscaling, it must match perfectly.
    if (::audiobuffer::SampleDescriptor<DST_T>::BIT_DEPTH < ::audiobuffer::SampleDescriptor<SRC_T>::BIT_DEPTH) {
      if (-copy_from_val - -scaled_value_final == max_diff) {
        eq = true;
      }
    }

    EXPECT_TRUE(eq);
  }
  // Center and max values must match perfectly.
  else if (value == ::audiobuffer::SampleDescriptor<SRC_T>::CENTER || value == ::audiobuffer::SampleDescriptor<SRC_T>::MAX) {
    EXPECT_EQ(copy_from_val, scaled_value_final);
  }
  // Positive values can be off by a miniscule amount to allow for fast, but
  // slightly imprecise integer-only operations.
  else {
    // Check that the value is correctly mapped to the positive values.
    EXPECT_GE(copy_from_val, ::audiobuffer::SampleDescriptor<DST_T>::CENTER);

    if (copy_from_val < scaled_value_final) {
      EXPECT_LE(
        scaled_value_final - copy_from_val,
        max_diff
      );
    }
    else {
      EXPECT_LE(
        copy_from_val - scaled_value_final,
        max_diff
      );
    }
  }
}

template <typename SRC_T, typename DST_T>
void test_int_scaling()
{
  // Negative has more values than positive.
  long double neg_scale_factor =
    (static_cast<long double>(std::numeric_limits<DST_T>::min()) - static_cast<long double>(::audiobuffer::SampleDescriptor<DST_T>::CENTER))
    /
    (static_cast<long double>(std::numeric_limits<SRC_T>::min()) - static_cast<long double>(::audiobuffer::SampleDescriptor<SRC_T>::CENTER))
  ;
  long double pos_scale_factor =
    (static_cast<long double>(std::numeric_limits<DST_T>::max()) - static_cast<long double>(::audiobuffer::SampleDescriptor<DST_T>::CENTER))
    /
    (static_cast<long double>(std::numeric_limits<SRC_T>::max()) - static_cast<long double>(::audiobuffer::SampleDescriptor<SRC_T>::CENTER))
  ;

  DST_T max_diff = ::audiobuffer::SampleDescriptor<DST_T>::BIT_DEPTH < ::audiobuffer::SampleDescriptor<SRC_T>::BIT_DEPTH
    // Downscaling may only have at most off-by-one.
    ? 1
    // Upscaling.
    : static_cast<DST_T>(
      std::pow(
        2,
        (static_cast<double>(::audiobuffer::SampleDescriptor<DST_T>::BIT_DEPTH) / 8.0) - 1.0
      )
    )
  ;

  int max_threads = std::max(
    omp_get_max_threads() - 1,
    1
  );

  if constexpr (::audiobuffer::SampleDescriptor<SRC_T>::IS_SIGNED)
  {
    // Negative values.
    // NOTE: OpenMP requires positive values for the iterator values.
    #pragma omp parallel for num_threads(max_threads)
    for (SRC_T i=0; i<std::numeric_limits<SRC_T>::max(); i++) {
      SRC_T value = std::numeric_limits<SRC_T>::min() - i;
      test_int_scaling_case<SRC_T, DST_T>(
        value,
        neg_scale_factor,
        pos_scale_factor,
        max_diff
      );
    }
    // Center value.
    {
      SRC_T value = 0;
      test_int_scaling_case<SRC_T, DST_T>(
        value,
        neg_scale_factor,
        pos_scale_factor,
        max_diff
      );
    }
    // Positive values.
    #pragma omp parallel for num_threads(max_threads)
    for (SRC_T i=0; i<std::numeric_limits<SRC_T>::max(); i++) {
      SRC_T value = i;
      test_int_scaling_case<SRC_T, DST_T>(
        value,
        neg_scale_factor,
        pos_scale_factor,
        max_diff
      );
    }
  }
  else
  {
    #pragma omp parallel for num_threads(max_threads)
    for (SRC_T i=0; i<::audiobuffer::SampleDescriptor<SRC_T>::CENTER; i++) {
      SRC_T value = i;
      test_int_scaling_case<SRC_T, DST_T>(
        value,
        neg_scale_factor,
        pos_scale_factor,
        max_diff
      );
    }
    // Max value.
    {
      SRC_T value = std::numeric_limits<SRC_T>::max();
      test_int_scaling_case<SRC_T, DST_T>(
        value,
        neg_scale_factor,
        pos_scale_factor,
        max_diff
      );
    }
  }
}

// i8

TEST(AudioBufferTests, IntScalingTest_i8_i16) {
  test_int_scaling<std::int8_t, std::int16_t>();
}

TEST(AudioBufferTests, IntScalingTest_i8_i32) {
  test_int_scaling<std::int8_t, std::int32_t>();
}

TEST(AudioBufferTests, IntScalingTest_i8_u16) {
  test_int_scaling<std::int8_t, std::uint16_t>();
}

TEST(AudioBufferTests, IntScalingTest_i8_u32) {
  test_int_scaling<std::int8_t, std::uint32_t>();
}

// u8

TEST(AudioBufferTests, IntScalingTest_u8_i16) {
  test_int_scaling<std::uint8_t, std::int16_t>();
}

TEST(AudioBufferTests, IntScalingTest_u8_i32) {
  test_int_scaling<std::uint8_t, std::int32_t>();
}

TEST(AudioBufferTests, IntScalingTest_u8_u16) {
  test_int_scaling<std::uint8_t, std::uint16_t>();
}

TEST(AudioBufferTests, IntScalingTest_u8_u32) {
  test_int_scaling<std::uint8_t, std::uint32_t>();
}

// i16

TEST(AudioBufferTests, IntScalingTest_i16_i8) {
  test_int_scaling<std::int16_t, std::int8_t>();
}

TEST(AudioBufferTests, IntScalingTest_i16_i32) {
  test_int_scaling<std::int16_t, std::int32_t>();
}

TEST(AudioBufferTests, IntScalingTest_i16_u8) {
  test_int_scaling<std::int16_t, std::uint8_t>();
}

TEST(AudioBufferTests, IntScalingTest_i16_u32) {
  test_int_scaling<std::int16_t, std::uint32_t>();
}

// u16

TEST(AudioBufferTests, IntScalingTest_u16_i8) {
  test_int_scaling<std::uint16_t, std::int8_t>();
}

TEST(AudioBufferTests, IntScalingTest_u16_i32) {
  test_int_scaling<std::uint16_t, std::int32_t>();
}

TEST(AudioBufferTests, IntScalingTest_u16_u8) {
  test_int_scaling<std::uint16_t, std::uint8_t>();
}

TEST(AudioBufferTests, IntScalingTest_u16_u32) {
  test_int_scaling<std::uint16_t, std::uint32_t>();
}

// i32

TEST(AudioBufferTests, IntScalingTest_i32_i8) {
  test_int_scaling<std::int32_t, std::int8_t>();
}

TEST(AudioBufferTests, IntScalingTest_i32_i16) {
  test_int_scaling<std::int32_t, std::int16_t>();
}

TEST(AudioBufferTests, IntScalingTest_i32_u8) {
  test_int_scaling<std::int32_t, std::uint8_t>();
}

TEST(AudioBufferTests, IntScalingTest_i32_u16) {
  test_int_scaling<std::int32_t, std::uint16_t>();
}

// u32

TEST(AudioBufferTests, IntScalingTest_u32_i8) {
  test_int_scaling<std::uint32_t, std::int8_t>();
}

TEST(AudioBufferTests, IntScalingTest_u32_i16) {
  test_int_scaling<std::uint32_t, std::int16_t>();
}

TEST(AudioBufferTests, IntScalingTest_u32_u8) {
  test_int_scaling<std::uint32_t, std::uint8_t>();
}

TEST(AudioBufferTests, IntScalingTest_u32_u16) {
  test_int_scaling<std::uint32_t, std::uint16_t>();
}
