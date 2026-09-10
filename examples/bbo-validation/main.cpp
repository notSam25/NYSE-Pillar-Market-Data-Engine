#include <cmath>
#include <cstdint>
#include <engine.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <model/order_book.hpp>
#include <schema/nyse/reference_types.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <unordered_map>

// Replays the Integrated Feed through mde::Engine + mde::model::OrderBook,
// freezing each symbol's top-of-book the moment the feed passes that
// symbol's last real BBO quote timestamp, then compares the frozen
// snapshot against the independent BBO (msgType 140 Quote) reference feed
// for the same market/day.
//
// Point-in-time alignment, not "final state after the whole file": an
// earlier version compared post-EOF book state against BBO's last quote
// and got ~75% price match with ~15% volume-at-level match -- the gap was
// order activity between the BBO snapshot's timestamp and end-of-file,
// not a reconstruction defect (book_build_failures was already 0). This
// freezes the book at the matching instant instead.
//
// Usage: bbo-validation [integrated_feed.csv] [bbo.csv]
// Defaults to the channel-1 sample data under data/.

namespace {

struct ReferenceQuote {
  double _askPrice = 0;
  std::uint64_t _askVolume = 0;
  double _bidPrice = 0;
  std::uint64_t _bidVolume = 0;
  std::uint64_t _nanosSinceMidnight = 0;
};

std::unordered_map<std::string, ReferenceQuote>
LoadLastQuote(const std::filesystem::path &path) {
  std::unordered_map<std::string, ReferenceQuote> quotes;
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open " + path.string());
  }

  std::string line;
  while (std::getline(file, line)) {
    mde::schema::nyse::CSVReader reader{line};
    const auto msgType = reader.GetField(0);
    if (!msgType || *msgType != "140") {
      continue;
    }

    try {
      mde::schema::nyse::messages::Quote quote{&reader};
      // TAQ file records are in real-time-feed order, so the last *real*
      // Quote seen for a symbol is its end-of-day top of book. Spec §17:
      // "At the scheduled closing time, NYSE publishes an R-quote with
      // prices and volumes set to 0" -- that zeroed sentinel is always the
      // literal last record per symbol, so it's explicitly skipped rather
      // than treated as the reference top-of-book.
      if (quote._askPrice == 0.0 && quote._bidPrice == 0.0) {
        continue;
      }
      quotes[quote._header._symbol] = ReferenceQuote{
          quote._askPrice, quote._askVolume, quote._bidPrice,
          quote._bidVolume, quote._header._sourceTime._nanosSinceMidnight};
    } catch (const mde::schema::nyse::CSVReader::Error &) {
      continue;
    }
  }

  return quotes;
}

bool NearlyEqual(double a, double b) { return std::fabs(a - b) < 1e-6; }

// (symbol, nanosSinceMidnight) for every Integrated message type that can
// move the displayed book. Empty symbol/false means "doesn't affect the
// book" (e.g. Security Status, Imbalance, trade-ledger-only messages).
struct BookEventMeta {
  std::string_view _symbol;
  std::uint64_t _nanosSinceMidnight = 0;
  bool _mutatesBook = false;
};

BookEventMeta ExtractBookEventMeta(mde::messages::MessageType type,
                                   void *data) {
  using mde::messages::MessageType;
  switch (type) {
  case MessageType::AddOrder: {
    auto *m = static_cast<mde::messages::AddOrder *>(data);
    return {m->_symbol, m->_nanosSinceMidnight, true};
  }
  case MessageType::ModifyOrder: {
    auto *m = static_cast<mde::messages::ModifyOrder *>(data);
    return {m->_symbol, m->_nanosSinceMidnight, true};
  }
  case MessageType::DeleteOrder: {
    auto *m = static_cast<mde::messages::DeleteOrder *>(data);
    return {m->_symbol, m->_nanosSinceMidnight, true};
  }
  case MessageType::OrderExecution: {
    auto *m = static_cast<mde::messages::OrderExecution *>(data);
    return {m->_symbol, m->_nanosSinceMidnight, true};
  }
  case MessageType::ReplaceOrder: {
    auto *m = static_cast<mde::messages::ReplaceOrder *>(data);
    return {m->_symbol, m->_nanosSinceMidnight, true};
  }
  case MessageType::AddOrderRefresh: {
    auto *m = static_cast<mde::messages::AddOrderRefresh *>(data);
    return {m->_symbol, m->_nanosSinceMidnight, true};
  }
  default:
    return {};
  }
}

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
  const std::filesystem::path bboPath =
      argc > 2 ? std::filesystem::path(argv[2])
               : dataDir / "EQY_US_TAQ_NYSE_1_BBO_20260401.csv";

  if (!std::filesystem::exists(integratedPath) ||
      !std::filesystem::exists(bboPath)) {
    std::cerr << "Missing input data. Expected:\n  " << integratedPath
              << "\n  " << bboPath << "\n";
    return 1;
  }

  std::cout << "Loading BBO reference from " << bboPath << "...\n";
  const auto reference = LoadLastQuote(bboPath);
  std::cout << "  " << reference.size() << " symbols in reference\n";

  mde::model::OrderBook book;
  std::unordered_map<std::string, mde::model::TopOfBook> frozen;

  std::cout << "Replaying Integrated Feed from " << integratedPath
            << " (freezing each symbol's book at its BBO reference "
               "timestamp)...\n";
  mde::Engine engine(
      mde::IngestData::NYSE_PILLAR, integratedPath,
      [&book, &frozen, &reference](mde::messages::MessageType type,
                                   void *data) {
        book.OnMessage(type, data);

        const auto meta = ExtractBookEventMeta(type, data);
        if (!meta._mutatesBook) {
          return;
        }
        const auto refIt = reference.find(std::string(meta._symbol));
        if (refIt == reference.end()) {
          return;
        }
        if (meta._nanosSinceMidnight <= refIt->second._nanosSinceMidnight) {
          frozen[std::string(meta._symbol)] =
              book.GetTopOfBook(std::string(meta._symbol));
        }
      });
  engine.Join();

  std::cout << "  " << engine.GetTotal() << " lines parsed, "
            << engine.GetEgressCount() << " messages delivered in "
            << engine.GetElapsedSeconds() << "s\n";

  std::size_t compared = 0;
  std::size_t bidPriceMatch = 0, bidVolumeMatch = 0;
  std::size_t askPriceMatch = 0, askVolumeMatch = 0;
  std::size_t mismatchExamples = 0;

  for (const auto &[symbol, top] : frozen) {
    const auto refIt = reference.find(symbol);
    if (refIt == reference.end()) {
      continue;
    }
    if (!top._bestBid && !top._bestAsk) {
      continue;
    }

    compared++;
    const auto &ref = refIt->second;

    const bool bidPriceOk =
        top._bestBid && NearlyEqual(top._bestBid->_price, ref._bidPrice);
    const bool bidVolumeOk =
        top._bestBid && top._bestBid->_aggregateVolume == ref._bidVolume;
    const bool askPriceOk =
        top._bestAsk && NearlyEqual(top._bestAsk->_price, ref._askPrice);
    const bool askVolumeOk =
        top._bestAsk && top._bestAsk->_aggregateVolume == ref._askVolume;

    bidPriceMatch += bidPriceOk;
    bidVolumeMatch += bidVolumeOk;
    askPriceMatch += askPriceOk;
    askVolumeMatch += askVolumeOk;

    if (!(bidPriceOk && askPriceOk) && mismatchExamples < 5) {
      mismatchExamples++;
      std::cout << "  mismatch " << symbol
                << ": book bid=" << (top._bestBid ? top._bestBid->_price : 0)
                << " ask=" << (top._bestAsk ? top._bestAsk->_price : 0)
                << " | bbo bid=" << ref._bidPrice << " ask=" << ref._askPrice
                << "\n";
    }
  }

  std::cout << "\n=== BBO Validation ===\n";
  std::cout << "Symbols with a frozen snapshot and a BBO entry: " << compared
            << "\n";
  std::cout << "Bid price  match: " << bidPriceMatch << "/" << compared
            << "\n";
  std::cout << "Bid volume match: " << bidVolumeMatch << "/" << compared
            << "\n";
  std::cout << "Ask price  match: " << askPriceMatch << "/" << compared
            << "\n";
  std::cout << "Ask volume match: " << askVolumeMatch << "/" << compared
            << "\n";
  std::cout << "(book_build_failures=" << book.GetBookBuildFailures()
            << " -- Modify/Delete/Execute/Replace referencing an unseen "
               "OrderID; the single best correctness signal for order-book "
               "reconstruction)\n";

  return 0;
}
