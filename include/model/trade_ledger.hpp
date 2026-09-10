#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mde::model {

struct DailyStats {
  std::optional<double> _open;
  std::optional<double> _high;
  std::optional<double> _low;
  std::optional<double> _close;
  std::uint64_t _totalVolume = 0;
};

// Generic per-symbol trade ledger: records trades keyed by an
// exchange-assigned ID, supports cancelling or replacing an earlier trade
// (both require recomputing OHLC from the still-live trades, since either
// could remove or change the day's High/Low), and derives DailyStats from
// whatever's still live.
//
// Reused by mde::model::OrderBook (Integrated Feed Order Execution/
// Non-Displayed Trade/Cross Trade) and directly by examples/stock-summary
// (TAQ NYSE Trades product Trade/Trade Cancel/Trade Correction messages)
// -- same ledger semantics, different feeds, never mixed into the same
// instance (their ID spaces aren't guaranteed disjoint across feeds).
class TradeLedger {
public:
  void RecordTrade(const std::string &symbol, std::uint64_t id, double price,
                   std::uint64_t volume);

  // Marks `id` cancelled and recomputes DailyStats from the still-live
  // trades. Returns false (and increments GetFailureCount()) if `id` was
  // never recorded for `symbol`, or was already cancelled.
  bool CancelTrade(const std::string &symbol, std::uint64_t id);

  // Adjusts a trade's volume in place without touching its price -- for
  // corrections that only restate volume (e.g. Integrated Cross
  // Correction). Returns false (and increments GetFailureCount()) if `id`
  // was never recorded.
  bool AdjustVolume(const std::string &symbol, std::uint64_t id,
                    std::uint64_t newVolume);

  // Cancels `oldId` and records a new trade under `newId` with the
  // corrected price/volume -- for corrections that fully restate a trade
  // under a new ID (e.g. Trades-product Trade Correction). Returns false
  // (and increments GetFailureCount()) if `oldId` was never recorded; the
  // new trade is still recorded either way.
  bool ReplaceTrade(const std::string &symbol, std::uint64_t oldId,
                    std::uint64_t newId, double price, std::uint64_t volume);

  DailyStats GetDailyStats(const std::string &symbol) const;
  std::vector<std::string> GetSymbols() const;

  // Cancel/Replace referencing an ID this ledger never saw recorded.
  std::uint64_t GetFailureCount() const { return _failures; }

private:
  struct TradeRecord {
    double _price;
    std::uint64_t _volume;
    bool _cancelled = false;
  };

  struct SymbolLedger {
    std::vector<TradeRecord> _log;
    std::unordered_map<std::uint64_t, std::size_t> _byId;
    DailyStats _stats;
  };

  SymbolLedger &GetOrCreate(const std::string &symbol);
  // Pushes a raw record with no stats side effect; callers on the rare
  // (cancel/correction) path always follow up with Recompute().
  static std::size_t AppendRaw(SymbolLedger &ledger, double price,
                               std::uint64_t volume);
  // O(1) hot-path stats fold-in for a trade that will never be
  // cancelled/corrected in the same call.
  static void FoldIntoStats(DailyStats &stats, double price,
                            std::uint64_t volume);
  // O(trades for the symbol) rebuild from the still-live log entries.
  // Only called on cancel/correction -- rare.
  static void Recompute(SymbolLedger &ledger);

  std::unordered_map<std::string, SymbolLedger> _ledgers;
  std::uint64_t _failures = 0;
};

} // namespace mde::model
