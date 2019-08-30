/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <quic/congestion_control/Pacer.h>
#include <folly/portability/GTest.h>

using namespace testing;

namespace quic {
namespace test {

class PacerTest : public Test {
 protected:
  QuicConnectionStateBase conn{QuicNodeType::Client};
  DefaultPacer pacer{conn, conn.transportSettings.minCwndInMss};
};

TEST_F(PacerTest, WriteBeforeScheduled) {
  EXPECT_EQ(
      conn.transportSettings.writeConnectionDataPacketsLimit,
      pacer.updateAndGetWriteBatchSize(Clock::now()));
  EXPECT_EQ(0us, pacer.getTimeUntilNextWrite());
}

TEST_F(PacerTest, RateCalculator) {
  pacer.setPacingRateCalculator([](const QuicConnectionStateBase&,
                                   uint64_t,
                                   uint64_t,
                                   std::chrono::microseconds) {
    return PacingRate::Builder().setInterval(1234us).setBurstSize(4321).build();
  });
  pacer.refreshPacingRate(200000, 200us);
  EXPECT_EQ(1234us, pacer.getTimeUntilNextWrite());
  EXPECT_EQ(4321, pacer.updateAndGetWriteBatchSize(Clock::now()));
}

TEST_F(PacerTest, CompensateTimerDrift) {
  pacer.setPacingRateCalculator([](const QuicConnectionStateBase&,
                                   uint64_t,
                                   uint64_t,
                                   std::chrono::microseconds) {
    return PacingRate::Builder().setInterval(1000us).setBurstSize(10).build();
  });
  auto currentTime = Clock::now();
  pacer.refreshPacingRate(20, 100us); // These two values do not matter here
  pacer.onPacedWriteScheduled(currentTime);
  EXPECT_EQ(20, pacer.updateAndGetWriteBatchSize(currentTime + 1000us));

  // Query batch size again without calling onPacedWriteScheduled won't do timer
  // drift compensation
  EXPECT_EQ(10, pacer.updateAndGetWriteBatchSize(currentTime + 2000us));
}

TEST_F(PacerTest, NextWriteTime) {
  EXPECT_EQ(0us, pacer.getTimeUntilNextWrite());

  pacer.setPacingRateCalculator([](const QuicConnectionStateBase&,
                                   uint64_t,
                                   uint64_t,
                                   std::chrono::microseconds rtt) {
    return PacingRate::Builder().setInterval(rtt).setBurstSize(10).build();
  });
  pacer.refreshPacingRate(20, 1000us);
  EXPECT_EQ(1000us, pacer.getTimeUntilNextWrite());
}

} // namespace test
} // namespace quic