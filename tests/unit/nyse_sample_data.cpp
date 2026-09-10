#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <engine.hpp>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <schema/nyse/csv.hpp>
#include <schema/nyse/parse.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace {

std::vector<std::string> ReadFirstLines(const std::filesystem::path &path,
                                        std::size_t count) {
  std::ifstream file(path);
  std::vector<std::string> lines;
  std::string line;

  while (lines.size() < count && std::getline(file, line)) {
    lines.push_back(line);
  }

  return lines;
}

} // namespace

// Feeds the first 1000 lines of the real NYSE Pillar TAQ sample file through
// the Engine end-to-end and checks that every line was accounted for, and
// that every line whose declared message type is currently implemented (the
// full Integrated Feed set: Symbol Index Mapping, Security Status, Add/
// Modify/Delete/Execute/Replace Order, Add Order Refresh, Retail Price
// Improvement, Imbalance, Non-Displayed Trade, Cross Trade, Trade Cancel,
// Cross Correction) was parsed successfully. The expected success count is
// derived from the sample data itself rather than hardcoded, so the
// assertion tracks the data instead of going stale as more message types
// are implemented.
TEST(NYSE_TAQ_CSV, first_thousand_lines_of_sample_data) {
  spdlog::set_level(spdlog::level::off);

  const std::filesystem::path dataFile =
      std::filesystem::path(NYSE_TAQ_DATA_DIR) /
      "EQY_US_NYSE_IBF_1_20260401.csv";

  if (!std::filesystem::exists(dataFile)) {
    GTEST_SKIP() << "TAQ sample data not present at " << dataFile;
  }

  constexpr std::size_t kLineCount = 1000;
  const auto lines = ReadFirstLines(dataFile, kLineCount);
  ASSERT_EQ(lines.size(), kLineCount)
      << "sample data file has fewer than " << kLineCount << " lines";

  const std::array<std::uint8_t, 14> implementedMsgTypes{
      SymbolIndexMapping,  SecurityStatusMessage, AddOrder,
      ModifyOrder,         DeleteOrder,           OrderExecution,
      ReplaceOrder,        RetailPriceImprovement, Imbalance,
      AddOrderRefresh,     NonDisplayedTrade,      CrossTrade,
      TradeCancel,         CrossCorrection,
  };

  std::size_t expectedSuccess = 0;
  for (const auto &line : lines) {
    mde::schema::nyse::CSVReader reader{line};
    const auto msgTypeField = reader.GetField(0);
    ASSERT_TRUE(msgTypeField.has_value());

    const auto msgType =
        mde::schema::nyse::parse_std_type<std::uint8_t>(*msgTypeField);
    ASSERT_TRUE(msgType.has_value());

    if (std::find(implementedMsgTypes.begin(), implementedMsgTypes.end(),
                 *msgType) != implementedMsgTypes.end()) {
      expectedSuccess++;
    }
  }

  std::atomic<uint64_t> deliveredCount{0};
  auto engine = mde::Engine(
      mde::IngestData::NYSE_PILLAR, lines,
      [&deliveredCount](mde::messages::MessageType, void *) {
        deliveredCount++;
      });
  engine.Join();

  EXPECT_EQ(engine.GetTotal(), kLineCount);
  EXPECT_EQ(engine.GetSuccess(), expectedSuccess);
  EXPECT_EQ(deliveredCount.load(), expectedSuccess);
}
