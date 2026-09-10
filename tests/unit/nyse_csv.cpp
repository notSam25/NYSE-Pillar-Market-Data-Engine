#include "spdlog/common.h"
#include <atomic>
#include <engine.hpp>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

TEST(NYSE_TAQ_CSV, number_letter_empty_test) {
  spdlog::set_level(spdlog::level::trace);

  const std::array<std::string, 5> CSVTestData = {
      "123,456", "", "1,b", ",,", "3,2,WMB,1,1,N,C,100,72.78,0,0,N,.0001,1"};

  std::atomic<uint64_t> deliveredCount{0};
  auto engine = mde::Engine(
      mde::IngestData::NYSE_PILLAR,
      std::vector(CSVTestData.begin(), CSVTestData.end()),
      [&deliveredCount](mde::messages::MessageType, void *) {
        deliveredCount++;
      });

  engine.Join();

  EXPECT_EQ(engine.GetSuccess(), 1);
  EXPECT_EQ(engine.GetTotal(), CSVTestData.size());
  EXPECT_EQ(deliveredCount.load(), 1);
}
