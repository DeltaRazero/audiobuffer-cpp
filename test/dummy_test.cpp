#include <gtest/gtest.h>
#include <audiobuffer.hpp>

TEST(AudioBufferTests, DummyTest) {
  auto static_buffer = ::audiobuffer::StaticAudioBuffer<std::int16_t, 2, 256>();
  EXPECT_TRUE(static_buffer.get_buffer_size() == 256);
}
