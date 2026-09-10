#include <model/order_book.hpp>

namespace mde::model {

void OrderBook::OnMessage(mde::messages::MessageType type, void *data) {
  using mde::messages::MessageType;

  switch (type) {
  case MessageType::SymbolIndexMapping:
    ApplySymbolIndexMapping(
        *static_cast<const mde::messages::SymbolIndexMapping *>(data));
    return;
  case MessageType::SecurityStatus:
    ApplySecurityStatus(
        *static_cast<const mde::messages::SecurityStatus *>(data));
    return;
  case MessageType::AddOrder:
    ApplyAddOrder(*static_cast<const mde::messages::AddOrder *>(data));
    return;
  case MessageType::ModifyOrder:
    ApplyModifyOrder(*static_cast<const mde::messages::ModifyOrder *>(data));
    return;
  case MessageType::DeleteOrder:
    ApplyDeleteOrder(*static_cast<const mde::messages::DeleteOrder *>(data));
    return;
  case MessageType::OrderExecution:
    ApplyOrderExecution(
        *static_cast<const mde::messages::OrderExecution *>(data));
    return;
  case MessageType::ReplaceOrder:
    ApplyReplaceOrder(
        *static_cast<const mde::messages::ReplaceOrder *>(data));
    return;
  case MessageType::RetailPriceImprovement:
    ApplyRetailPriceImprovement(
        *static_cast<const mde::messages::RetailPriceImprovement *>(data));
    return;
  case MessageType::Imbalance:
    ApplyImbalance(*static_cast<const mde::messages::Imbalance *>(data));
    return;
  case MessageType::AddOrderRefresh:
    ApplyAddOrderRefresh(
        *static_cast<const mde::messages::AddOrderRefresh *>(data));
    return;
  case MessageType::NonDisplayedTrade:
    ApplyNonDisplayedTrade(
        *static_cast<const mde::messages::NonDisplayedTrade *>(data));
    return;
  case MessageType::CrossTrade:
    ApplyCrossTrade(*static_cast<const mde::messages::CrossTrade *>(data));
    return;
  case MessageType::TradeCancel:
    ApplyTradeCancel(
        *static_cast<const mde::messages::TradeCancel *>(data));
    return;
  case MessageType::CrossCorrection:
    ApplyCrossCorrection(
        *static_cast<const mde::messages::CrossCorrection *>(data));
    return;
  }
}

OrderBook::SymbolBook &OrderBook::GetOrCreateBook(const std::string &symbol) {
  return _books[symbol];
}

void OrderBook::AddLevel(SymbolBook &book, char side, double price,
                         std::uint64_t volume) {
  if (side == 'B') {
    book._bidLevels[price] += volume;
  } else {
    book._askLevels[price] += volume;
  }
}

bool OrderBook::RemoveLevel(SymbolBook &book, char side, double price,
                            std::uint64_t volume) {
  if (side == 'B') {
    auto it = book._bidLevels.find(price);
    if (it == book._bidLevels.end() || it->second < volume) {
      return false;
    }
    it->second -= volume;
    if (it->second == 0) {
      book._bidLevels.erase(it);
    }
    return true;
  }

  auto it = book._askLevels.find(price);
  if (it == book._askLevels.end() || it->second < volume) {
    return false;
  }
  it->second -= volume;
  if (it->second == 0) {
    book._askLevels.erase(it);
  }
  return true;
}

std::size_t OrderBook::AppendTrade(SymbolBook &book, double price,
                                   std::uint64_t volume) {
  auto &stats = book._stats;

  if (!stats._open) {
    stats._open = price;
  }
  if (!stats._high || price > *stats._high) {
    stats._high = price;
  }
  if (!stats._low || price < *stats._low) {
    stats._low = price;
  }
  stats._close = price;
  stats._totalVolume += volume;

  book._tradeLog.push_back(TradeRecord{price, volume, false});
  return book._tradeLog.size() - 1;
}

void OrderBook::RecomputeStats(SymbolBook &book) {
  DailyStats stats;
  for (const auto &trade : book._tradeLog) {
    if (trade._cancelled) {
      continue;
    }
    if (!stats._open) {
      stats._open = trade._price;
    }
    if (!stats._high || trade._price > *stats._high) {
      stats._high = trade._price;
    }
    if (!stats._low || trade._price < *stats._low) {
      stats._low = trade._price;
    }
    stats._close = trade._price;
    stats._totalVolume += trade._volume;
  }
  book._stats = stats;
}

void OrderBook::ApplySymbolIndexMapping(
    const mde::messages::SymbolIndexMapping &msg) {
  GetOrCreateBook(msg._symbol)._info = msg;
}

void OrderBook::ApplySecurityStatus(
    const mde::messages::SecurityStatus &msg) {
  auto &book = GetOrCreateBook(msg._symbol);

  // '4' = Trading Halt, '5' = Resume (spec §4). The Security Status field
  // is multiplexed with SSR/session/price-indication codes; only these two
  // values change halted state.
  if (msg._securityStatus == '4') {
    book._halted = true;
  } else if (msg._securityStatus == '5') {
    book._halted = false;
  }
}

void OrderBook::ApplyAddOrder(const mde::messages::AddOrder &msg) {
  auto &book = GetOrCreateBook(msg._symbol);
  book._orders[msg._orderId] = LiveOrder{msg._price, msg._volume, msg._side};
  AddLevel(book, msg._side, msg._price, msg._volume);
}

void OrderBook::ApplyModifyOrder(const mde::messages::ModifyOrder &msg) {
  auto &book = GetOrCreateBook(msg._symbol);
  auto it = book._orders.find(msg._orderId);
  if (it == book._orders.end()) {
    _bookBuildFailures++;
    return;
  }

  LiveOrder &order = it->second;
  if (!RemoveLevel(book, order._side, order._price, order._volume)) {
    _bookBuildFailures++;
  }

  order._price = msg._price;
  order._volume = msg._volume;
  AddLevel(book, order._side, order._price, order._volume);
}

void OrderBook::ApplyDeleteOrder(const mde::messages::DeleteOrder &msg) {
  auto &book = GetOrCreateBook(msg._symbol);
  auto it = book._orders.find(msg._orderId);
  if (it == book._orders.end()) {
    _bookBuildFailures++;
    return;
  }

  if (!RemoveLevel(book, it->second._side, it->second._price,
                   it->second._volume)) {
    _bookBuildFailures++;
  }
  book._orders.erase(it);
}

void OrderBook::ApplyOrderExecution(
    const mde::messages::OrderExecution &msg) {
  auto &book = GetOrCreateBook(msg._symbol);
  book._tradesById[msg._tradeId] =
      AppendTrade(book, msg._price, msg._volume);

  auto it = book._orders.find(msg._orderId);
  if (it == book._orders.end()) {
    _bookBuildFailures++;
    return;
  }

  LiveOrder &order = it->second;
  // Spec §8: an execution can report fewer shares than the resting order;
  // remaining shares keep their original (resting) price, which is what
  // the book level is keyed on -- not the execution's own Price field.
  const std::uint64_t executed =
      msg._volume > order._volume ? order._volume : msg._volume;

  if (!RemoveLevel(book, order._side, order._price, executed)) {
    _bookBuildFailures++;
  }

  if (msg._volume >= order._volume) {
    book._orders.erase(it);
  } else {
    order._volume -= msg._volume;
  }
}

void OrderBook::ApplyReplaceOrder(const mde::messages::ReplaceOrder &msg) {
  auto &book = GetOrCreateBook(msg._symbol);
  auto it = book._orders.find(msg._orderId);
  if (it == book._orders.end()) {
    _bookBuildFailures++;
  } else {
    if (!RemoveLevel(book, it->second._side, it->second._price,
                     it->second._volume)) {
      _bookBuildFailures++;
    }
    book._orders.erase(it);
  }

  book._orders[msg._newOrderId] =
      LiveOrder{msg._price, msg._volume, msg._side};
  AddLevel(book, msg._side, msg._price, msg._volume);
}

void OrderBook::ApplyRetailPriceImprovement(
    const mde::messages::RetailPriceImprovement &msg) {
  GetOrCreateBook(msg._symbol)._lastRpiIndicator = msg._rpiIndicator;
}

void OrderBook::ApplyImbalance(const mde::messages::Imbalance &msg) {
  GetOrCreateBook(msg._symbol)._lastImbalance = msg;
}

void OrderBook::ApplyAddOrderRefresh(
    const mde::messages::AddOrderRefresh &msg) {
  // Same book effect as Add Order (100) -- sent for every order resting
  // on the book as part of a refresh response or backup failover.
  auto &book = GetOrCreateBook(msg._symbol);
  book._orders[msg._orderId] = LiveOrder{msg._price, msg._volume, msg._side};
  AddLevel(book, msg._side, msg._price, msg._volume);
}

void OrderBook::ApplyNonDisplayedTrade(
    const mde::messages::NonDisplayedTrade &msg) {
  // Spec §12: a match between two non-displayed orders. Never touches the
  // resting/displayed book -- only the trade ledger and DailyStats.
  auto &book = GetOrCreateBook(msg._symbol);
  book._tradesById[msg._tradeId] =
      AppendTrade(book, msg._price, msg._volume);
}

void OrderBook::ApplyCrossTrade(const mde::messages::CrossTrade &msg) {
  // Spec §13: bulk auction print. Never touches the resting/displayed
  // book -- only the trade ledger and DailyStats. Corrected (not
  // cancelled) via Cross Correction, keyed by CrossID rather than
  // TradeID.
  auto &book = GetOrCreateBook(msg._symbol);
  book._crossesById[msg._crossId] =
      AppendTrade(book, msg._price, msg._volume);
}

void OrderBook::ApplyTradeCancel(const mde::messages::TradeCancel &msg) {
  auto &book = GetOrCreateBook(msg._symbol);
  auto it = book._tradesById.find(msg._tradeId);
  if (it == book._tradesById.end() ||
      book._tradeLog[it->second]._cancelled) {
    _tradeLedgerFailures++;
    return;
  }

  book._tradeLog[it->second]._cancelled = true;
  RecomputeStats(book);
}

void OrderBook::ApplyCrossCorrection(
    const mde::messages::CrossCorrection &msg) {
  auto &book = GetOrCreateBook(msg._symbol);
  auto it = book._crossesById.find(msg._crossId);
  if (it == book._crossesById.end()) {
    _tradeLedgerFailures++;
    return;
  }

  // Spec §15: corrects only the Cross Trade's volume, not its price, so
  // High/Low/Open/Close never need to move -- just apply the volume
  // delta directly instead of paying for a full RecomputeStats() rescan.
  TradeRecord &trade = book._tradeLog[it->second];
  const auto delta = static_cast<std::int64_t>(msg._volume) -
                     static_cast<std::int64_t>(trade._volume);
  book._stats._totalVolume = static_cast<std::uint64_t>(
      static_cast<std::int64_t>(book._stats._totalVolume) + delta);
  trade._volume = msg._volume;
}

TopOfBook OrderBook::GetTopOfBook(const std::string &symbol) const {
  TopOfBook top;
  auto it = _books.find(symbol);
  if (it == _books.end()) {
    return top;
  }

  const auto &book = it->second;
  if (!book._bidLevels.empty()) {
    top._bestBid = PriceLevel{book._bidLevels.begin()->first,
                              book._bidLevels.begin()->second};
  }
  if (!book._askLevels.empty()) {
    top._bestAsk = PriceLevel{book._askLevels.begin()->first,
                              book._askLevels.begin()->second};
  }
  return top;
}

DailyStats OrderBook::GetDailyStats(const std::string &symbol) const {
  auto it = _books.find(symbol);
  return it == _books.end() ? DailyStats{} : it->second._stats;
}

bool OrderBook::IsHalted(const std::string &symbol) const {
  auto it = _books.find(symbol);
  return it != _books.end() && it->second._halted;
}

std::optional<mde::messages::SymbolIndexMapping>
OrderBook::GetSymbolInfo(const std::string &symbol) const {
  auto it = _books.find(symbol);
  return it == _books.end() ? std::nullopt : it->second._info;
}

std::optional<mde::messages::Imbalance>
OrderBook::GetImbalance(const std::string &symbol) const {
  auto it = _books.find(symbol);
  return it == _books.end() ? std::nullopt : it->second._lastImbalance;
}

std::vector<std::string> OrderBook::GetSymbols() const {
  std::vector<std::string> symbols;
  symbols.reserve(_books.size());
  for (const auto &[symbol, book] : _books) {
    symbols.push_back(symbol);
  }
  return symbols;
}

std::uint64_t OrderBook::GetLiveOrderCount() const {
  std::uint64_t total = 0;
  for (const auto &[symbol, book] : _books) {
    total += book._orders.size();
  }
  return total;
}

} // namespace mde::model
