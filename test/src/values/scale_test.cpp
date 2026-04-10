#include <string>
#include <limits>

#include <omp.h>
#include <gtest/gtest.h>

#include <audiobuffer.hpp>

// *****************************************************************************

template <typename SRC_T, typename DST_T>
void test_int_scaling_case(const SRC_T& value, long double const& neg_scale_factor, long double const& pos_scale_factor)
{
  using SRC_DESCRIPTOR = audiobuffer::SampleDescriptor<SRC_T>;
  using DST_DESCRIPTOR = audiobuffer::SampleDescriptor<DST_T>;

  auto src_buffer = audiobuffer::StaticAudioBuffer<SRC_T, 1, 1>();
  auto dst_buffer = audiobuffer::StaticAudioBuffer<DST_T, 1, 1>();

  src_buffer[0][0] = value;

  src_buffer.copy_to(&dst_buffer);
  auto copy_to_val = dst_buffer[0][0];

  dst_buffer.copy_from(&src_buffer);
  auto copy_from_val = dst_buffer[0][0];

  // First check if the operations aren't destructive.
  EXPECT_EQ(copy_to_val, copy_from_val);

  DST_T expected_value;
  bool match_perfectly = false;
  switch (value)
  {
    case SRC_DESCRIPTOR::MIN:
      expected_value = DST_DESCRIPTOR::MIN;
      match_perfectly = true;
      break;

    case SRC_DESCRIPTOR::CENTER:
      expected_value = DST_DESCRIPTOR::CENTER;
      match_perfectly = true;
      break;

    case SRC_DESCRIPTOR::MAX:
      expected_value = DST_DESCRIPTOR::MAX;
      match_perfectly = true;
      break;

    // Calculate expected scaled value. This is a much more expensive and slower
    // way to calculate it compared to what is used in the library itself, but
    // is easier to calculate and is always precise.
    default:
      auto scaled_value = static_cast<long double>(value);

      // Change it to signed-equivalent value when unsigned type.
      if constexpr (!SRC_DESCRIPTOR::IS_SIGNED) {
        scaled_value -= SRC_DESCRIPTOR::CENTER;
      }

      scaled_value = std::round(value < audiobuffer::SampleDescriptor<SRC_T>::CENTER
        ? scaled_value * neg_scale_factor
        : scaled_value * pos_scale_factor
      );

      // Change it to unsigned-equivalent value when signed type.
      if constexpr (!DST_DESCRIPTOR::IS_SIGNED) {
        scaled_value += DST_DESCRIPTOR::CENTER;
      }

      // Clamp to min and max of destination types for safety.
      expected_value = std::clamp(
        static_cast<DST_T>(scaled_value),
        std::numeric_limits<DST_T>::min(),
        std::numeric_limits<DST_T>::max()
      );
      break;
  }

  if (match_perfectly) {
    EXPECT_EQ(copy_from_val, expected_value);
    return;
  }

  // Since the library uses integer-only math for the scaling routines for speed
  // and portability reasons, we allow off-by-one.
  DST_T diff = copy_from_val < expected_value
    ? expected_value - copy_from_val
    : copy_from_val - expected_value;
  EXPECT_TRUE(diff >= static_cast<DST_T>(0) && diff <= static_cast<DST_T>(1));
}

template <typename SRC_T, typename DST_T>
void test_int_scaling()
{
  using SRC_DESCRIPTOR = audiobuffer::SampleDescriptor<SRC_T>;
  using DST_DESCRIPTOR = audiobuffer::SampleDescriptor<DST_T>;

  // Negative has more values than positive.
  const long double neg_scale_factor =
    (static_cast<long double>(DST_DESCRIPTOR::MIN) - static_cast<long double>(DST_DESCRIPTOR::CENTER))
    /
    (static_cast<long double>(SRC_DESCRIPTOR::MIN) - static_cast<long double>(SRC_DESCRIPTOR::CENTER))
  ;
  const long double pos_scale_factor =
    (static_cast<long double>(DST_DESCRIPTOR::MAX) - static_cast<long double>(DST_DESCRIPTOR::CENTER))
    /
    (static_cast<long double>(SRC_DESCRIPTOR::MAX) - static_cast<long double>(SRC_DESCRIPTOR::CENTER))
  ;

  int max_threads = std::max(
    omp_get_max_threads() - 1,
    1
  );
  constexpr int MAX_BATCH_SIZE = (1024*1024) > SRC_DESCRIPTOR::MAX
    ? SRC_DESCRIPTOR::MAX
    : (1024*1024);

  SRC_T current = SRC_DESCRIPTOR::MIN;
  while (current < SRC_DESCRIPTOR::MAX)
  {
    int batch_size = (current >= 0 && SRC_DESCRIPTOR::MAX - MAX_BATCH_SIZE < current)
      ? batch_size = SRC_DESCRIPTOR::MAX - current
      : MAX_BATCH_SIZE;

    #pragma omp parallel for num_threads(max_threads)
    for (int i=0; i<=batch_size; i++) {
      SRC_T value = current + static_cast<SRC_T>(i);
      test_int_scaling_case<SRC_T, DST_T>(
        value,
        neg_scale_factor,
        pos_scale_factor
      );
    }

    current += batch_size;
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
