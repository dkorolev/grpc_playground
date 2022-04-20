#include "3rdparty/gtest/gtest-main.h"
#include "schema_smoke.pb.h"

TEST(Proto, Smoke) {
  test_proto::TheAnswer answer;
  answer.set_value(42);
  EXPECT_EQ(42, answer.value());
}
