#include "spdlog/common.h"
#include <engine.hpp>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

TEST(Test, BasicTest) {
  spdlog::set_level(spdlog::level::trace);
  const auto engine = mde::Engine(mde::IngestData::NYSE_PILLAR, "/tmp/");

  EXPECT_EQ(true, !false);
}
