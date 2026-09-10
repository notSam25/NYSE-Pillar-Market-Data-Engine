#include <cmath>
#include <cstdint>
#include <engine.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <model/order_book.hpp>
#include <model/trade_ledger.hpp>
#include <schema/nyse/reference_types.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

// Replays the Integrated Feed through mde::Engine + mde::model::OrderBook
// (order-book reconstruction), separately replays the TAQ NYSE Trades
// product through a standalone mde::model::TradeLedger (OHLCV), and
// compares the latter against the independent Stock Summary (msgType 223)
// reference feed for the same day.
//
// Trades, not Integrated, is the OHLCV source: verified against real data
// that STOCKSUM's TotalVolume is computed from the Trades product's Trade
// (220) messages, not from Integrated's Order Execution/Non-Displayed
// Trade/Cross Trade -- see schema/nyse/reference_types.hpp. Integrated's
// own Order Execution still independently drives resting-order removal in
// OrderBook; that's a genuinely separate concern from OHLCV.
//
// Usage: stock-summary [integrated_feed.csv] [trades.csv] [stocksum.csv]
// Defaults to the channel-1 sample data under data/.

namespace {

struct ReferenceSummary {
  double _high = 0;
  double _low = 0;
  double _open = 0;
  double _close = 0;
  std::uint64_t _totalVolume = 0;
};

std::unordered_map<std::string, ReferenceSummary>
LoadStockSummary(const std::filesystem::path &path) {
  std::unordered_map<std::string, ReferenceSummary> summaries;
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open " + path.string());
  }

  std::string line;
  while (std::getline(file, line)) {
    mde::schema::nyse::CSVReader reader{line};
    const auto msgType = reader.GetField(0);
    if (!msgType || *msgType != "223") {
      continue;
    }

    try {
      mde::schema::nyse::messages::StockSummary summary{&reader};
      // Republished every 60s with cumulative state -- later records for
      // the same symbol supersede earlier ones.
      summaries[summary._symbol] =
          ReferenceSummary{summary._highPrice, summary._lowPrice,
                           summary._open, summary._close,
                           summary._totalVolume};
    } catch (const mde::schema::nyse::CSVReader::Error &) {
      continue;
    }
  }

  return summaries;
}

mde::model::TradeLedger LoadTrades(const std::filesystem::path &path) {
  mde::model::TradeLedger tape;
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open " + path.string());
  }

  std::string line;
  while (std::getline(file, line)) {
    mde::schema::nyse::CSVReader reader{line};
    const auto msgType = reader.GetField(0);
    if (!msgType) {
      continue;
    }

    try {
      if (*msgType == "220") {
        mde::schema::nyse::messages::TaqTrade trade{&reader};
        tape.RecordTrade(trade._header._symbol, trade._tradeId, trade._price,
                         trade._volume);
      } else if (*msgType == "221") {
        // Same wire layout as Integrated's Trade Cancel (112).
        mde::schema::nyse::messages::TradeCancel cancel{&reader};
        tape.CancelTrade(cancel._header._symbol, cancel._tradeId);
      } else if (*msgType == "222") {
        mde::schema::nyse::messages::TaqTradeCorrection correction{&reader};
        tape.ReplaceTrade(correction._header._symbol,
                          correction._originalTradeId, correction._tradeId,
                          correction._price, correction._volume);
      }
    } catch (const mde::schema::nyse::CSVReader::Error &) {
      continue;
    }
  }

  return tape;
}

bool NearlyEqual(double a, double b) { return std::fabs(a - b) < 1e-6; }

} // namespace

int main(int argc, char **argv) {
  // "Encountered unimplemented MsgType" is expected noise -- 6 of the 14
  // Integrated Feed msgTypes are intentionally out of scope (see
  // include/model/order_book.hpp). Not an error condition for this driver.
  spdlog::set_level(spdlog::level::off);

  const std::filesystem::path dataDir = NYSE_TAQ_DATA_DIR;
  const std::filesystem::path integratedPath =
      argc > 1 ? std::filesystem::path(argv[1])
               : dataDir / "EQY_US_NYSE_IBF_1_20260401.csv";
  const std::filesystem::path tradesPath =
      argc > 2 ? std::filesystem::path(argv[2])
               : dataDir / "EQY_US_TAQ_NYSE_1_TRADES_20260401.csv";
  const std::filesystem::path stockSumPath =
      argc > 3 ? std::filesystem::path(argv[3])
               : dataDir / "EQY_US_NYSE_STOCKSUM_20260401.csv";

  if (!std::filesystem::exists(integratedPath) ||
      !std::filesystem::exists(tradesPath) ||
      !std::filesystem::exists(stockSumPath)) {
    std::cerr << "Missing input data. Expected:\n  " << integratedPath
              << "\n  " << tradesPath << "\n  " << stockSumPath << "\n";
    return 1;
  }

  std::cout << "Loading Stock Summary reference from " << stockSumPath
            << "...\n";
  const auto reference = LoadStockSummary(stockSumPath);
  std::cout << "  " << reference.size() << " symbols in reference\n";

  mde::model::OrderBook book;
  std::cout << "Replaying Integrated Feed (order book) from "
            << integratedPath << "...\n";
  mde::Engine engine(mde::IngestData::NYSE_PILLAR, integratedPath,
                     [&book](mde::messages::MessageType type, void *data) {
                       book.OnMessage(type, data);
                     });
  engine.Join();
  std::cout << "  " << engine.GetTotal() << " lines parsed, "
            << engine.GetEgressCount() << " messages delivered in "
            << engine.GetElapsedSeconds() << "s\n";
  std::cout << "  book_build_failures=" << book.GetBookBuildFailures()
            << "\n";

  std::cout << "Replaying Trades product (OHLCV) from " << tradesPath
            << "...\n";
  const auto tape = LoadTrades(tradesPath);
  std::cout << "  " << tape.GetSymbols().size()
            << " symbols traded, trade_ledger_failures="
            << tape.GetFailureCount() << "\n";

  std::size_t compared = 0;
  std::size_t openMatch = 0, highMatch = 0, lowMatch = 0, closeMatch = 0;
  std::size_t volumeMatch = 0;
  std::uint64_t volumeSum = 0, refVolumeSum = 0;
  std::size_t volumeExamples = 0;

  for (const auto &symbol : tape.GetSymbols()) {
    const auto refIt = reference.find(symbol);
    if (refIt == reference.end()) {
      continue;
    }

    const auto stats = tape.GetDailyStats(symbol);
    if (!stats._open) {
      continue;
    }

    compared++;
    const auto &ref = refIt->second;
    openMatch += NearlyEqual(*stats._open, ref._open);
    highMatch += NearlyEqual(*stats._high, ref._high);
    lowMatch += NearlyEqual(*stats._low, ref._low);
    closeMatch += NearlyEqual(*stats._close, ref._close);
    volumeMatch += stats._totalVolume == ref._totalVolume;
    volumeSum += stats._totalVolume;
    refVolumeSum += ref._totalVolume;

    if (stats._totalVolume != ref._totalVolume && volumeExamples < 5) {
      volumeExamples++;
      std::cout << "  volume mismatch " << symbol
                << ": trades=" << stats._totalVolume
                << " stocksum=" << ref._totalVolume << "\n";
    }
  }

  std::cout << "\n=== Stock Summary Validation ===\n";
  std::cout << "Symbols traded with a STOCKSUM entry: " << compared << "\n";
  std::cout << "Open  match: " << openMatch << "/" << compared << "\n";
  std::cout << "High  match: " << highMatch << "/" << compared << "\n";
  std::cout << "Low   match: " << lowMatch << "/" << compared << "\n";
  std::cout << "Close match: " << closeMatch << "/" << compared << "\n";
  std::cout << "Volume match (exact): " << volumeMatch << "/" << compared
            << "\n";
  std::cout << "Total Trades-derived volume: " << volumeSum << "\n";
  std::cout << "Total STOCKSUM volume:       " << refVolumeSum << "\n";
  if (refVolumeSum > 0) {
    std::cout << "Trades volume is "
              << (100.0 * static_cast<double>(volumeSum) /
                  static_cast<double>(refVolumeSum))
              << "% of STOCKSUM volume\n";
  }

  return 0;
}
