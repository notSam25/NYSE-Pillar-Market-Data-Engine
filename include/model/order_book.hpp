#pragma once
#include "../message.hpp"
#include "trade_ledger.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mde::model {

struct PriceLevel {
  double _price;
  std::uint64_t _aggregateVolume;
};

struct TopOfBook {
  std::optional<PriceLevel> _bestBid;
  std::optional<PriceLevel> _bestAsk;
};

// Order-by-order limit order book reconstruction driven by the full
// Integrated Feed order-book and trade message set: Symbol Index Mapping,
// Security Status, Add/Modify/Delete/Execute/Replace Order, Add Order
// Refresh, Retail Price Improvement, Imbalance, Non-Displayed Trade, Cross
// Trade, Trade Cancel, and Cross Correction. Order IDs are scoped
// per-symbol per spec ("unique for this symbol for today only"), so every
// lookup is keyed on (symbol, orderId) via a per-symbol book; TradeID and
// CrossID are assumed to share that same per-symbol scoping.
//
// Displayed-book mechanics (bid/ask levels) are driven only by the Add/
// Modify/Delete/Execute/Replace/Refresh order messages -- Non-Displayed
// Trade and Cross Trade never touch resting orders (spec: "customers who
// are only interested in building a book of displayed orders may safely
// ignore Non-Displayed Trade messages"), they only add to the trade
// ledger that GetDailyStats() is computed from.
//
// NOT thread-safe. Feed it from a single EngineCallback -- by this
// project's design that's the Engine's Egress thread, so exactly one
// caller drives OnMessage() at a time. Read accessors after
// Engine::Join() (or from within that same callback).
class OrderBook {
public:
  // Dispatches on `type`, casting `data` to the matching mde::messages
  // struct. Pass this as (or wrap it in) an EngineCallback.
  void OnMessage(mde::messages::MessageType type, void *data);

  TopOfBook GetTopOfBook(const std::string &symbol) const;
  DailyStats GetDailyStats(const std::string &symbol) const;
  bool IsHalted(const std::string &symbol) const;
  std::optional<mde::messages::SymbolIndexMapping>
  GetSymbolInfo(const std::string &symbol) const;
  std::optional<mde::messages::Imbalance>
  GetImbalance(const std::string &symbol) const;

  std::vector<std::string> GetSymbols() const;
  std::size_t GetActiveBookCount() const { return _books.size(); }
  std::uint64_t GetLiveOrderCount() const;

  // Modify/Delete/Execute/Replace referencing an OrderID this book never
  // saw added, or removing more resting volume than is at a price level.
  // README: "the single best correctness signal for order-book
  // reconstruction".
  std::uint64_t GetBookBuildFailures() const { return _bookBuildFailures; }

  // Trade Cancel/Cross Correction referencing a TradeID/CrossID this book
  // never saw traded. Analogous to GetBookBuildFailures() but for the
  // trade ledger rather than the resting-order book.
  std::uint64_t GetTradeLedgerFailures() const {
    return _tradeLedgerFailures;
  }

private:
  struct LiveOrder {
    double _price;
    std::uint64_t _volume;
    char _side;
  };

  struct TradeRecord {
    double _price;
    std::uint64_t _volume;
    bool _cancelled = false;
  };

  struct SymbolBook {
    std::unordered_map<std::uint64_t, LiveOrder> _orders;
    std::map<double, std::uint64_t, std::greater<double>> _bidLevels;
    std::map<double, std::uint64_t> _askLevels;
    DailyStats _stats;
    bool _halted = false;
    std::optional<char> _lastRpiIndicator;
    std::optional<mde::messages::SymbolIndexMapping> _info;
    std::optional<mde::messages::Imbalance> _lastImbalance;

    // Every OrderExecution/NonDisplayedTrade/CrossTrade, in arrival
    // (sequence) order, so a Trade Cancel/Cross Correction can find and
    // adjust its entry and DailyStats can be recomputed from the
    // still-live subset.
    std::vector<TradeRecord> _tradeLog;
    std::unordered_map<std::uint64_t, std::size_t> _tradesById;
    std::unordered_map<std::uint64_t, std::size_t> _crossesById;
  };

  SymbolBook &GetOrCreateBook(const std::string &symbol);
  static void AddLevel(SymbolBook &book, char side, double price,
                       std::uint64_t volume);
  // Returns false (and leaves the book untouched) if `volume` exceeds what
  // is resting at `price` -- callers treat that as a book-build failure.
  static bool RemoveLevel(SymbolBook &book, char side, double price,
                          std::uint64_t volume);

  // Appends a trade to the log and folds it into DailyStats incrementally.
  // Returns the log index, for callers that need to key it by TradeID/
  // CrossID.
  static std::size_t AppendTrade(SymbolBook &book, double price,
                                 std::uint64_t volume);
  // Rebuilds DailyStats from scratch over the still-live (non-cancelled)
  // trade log. Only called on cancellation -- O(trades for the symbol),
  // acceptable since cancels are rare.
  static void RecomputeStats(SymbolBook &book);

  void ApplySymbolIndexMapping(const mde::messages::SymbolIndexMapping &msg);
  void ApplySecurityStatus(const mde::messages::SecurityStatus &msg);
  void ApplyAddOrder(const mde::messages::AddOrder &msg);
  void ApplyModifyOrder(const mde::messages::ModifyOrder &msg);
  void ApplyDeleteOrder(const mde::messages::DeleteOrder &msg);
  void ApplyOrderExecution(const mde::messages::OrderExecution &msg);
  void ApplyReplaceOrder(const mde::messages::ReplaceOrder &msg);
  void ApplyRetailPriceImprovement(
      const mde::messages::RetailPriceImprovement &msg);
  void ApplyImbalance(const mde::messages::Imbalance &msg);
  void ApplyAddOrderRefresh(const mde::messages::AddOrderRefresh &msg);
  void ApplyNonDisplayedTrade(const mde::messages::NonDisplayedTrade &msg);
  void ApplyCrossTrade(const mde::messages::CrossTrade &msg);
  void ApplyTradeCancel(const mde::messages::TradeCancel &msg);
  void ApplyCrossCorrection(const mde::messages::CrossCorrection &msg);

  std::unordered_map<std::string, SymbolBook> _books;
  std::uint64_t _bookBuildFailures = 0;
  std::uint64_t _tradeLedgerFailures = 0;
};

} // namespace mde::model
