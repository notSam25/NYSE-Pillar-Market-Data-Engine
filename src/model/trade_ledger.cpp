#include <model/trade_ledger.hpp>

namespace mde::model {

TradeLedger::SymbolLedger &TradeLedger::GetOrCreate(const std::string &symbol) {
  return _ledgers[symbol];
}

std::size_t TradeLedger::AppendRaw(SymbolLedger &ledger, double price,
                                   std::uint64_t volume) {
  ledger._log.push_back(TradeRecord{price, volume, false});
  return ledger._log.size() - 1;
}

void TradeLedger::FoldIntoStats(DailyStats &stats, double price,
                                std::uint64_t volume) {
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
}

void TradeLedger::Recompute(SymbolLedger &ledger) {
  DailyStats stats;
  for (const auto &trade : ledger._log) {
    if (trade._cancelled) {
      continue;
    }
    FoldIntoStats(stats, trade._price, trade._volume);
  }
  ledger._stats = stats;
}

void TradeLedger::RecordTrade(const std::string &symbol, std::uint64_t id,
                              double price, std::uint64_t volume) {
  auto &ledger = GetOrCreate(symbol);
  ledger._byId[id] = AppendRaw(ledger, price, volume);
  FoldIntoStats(ledger._stats, price, volume);
}

bool TradeLedger::CancelTrade(const std::string &symbol, std::uint64_t id) {
  auto &ledger = GetOrCreate(symbol);
  auto it = ledger._byId.find(id);
  if (it == ledger._byId.end() || ledger._log[it->second]._cancelled) {
    _failures++;
    return false;
  }

  ledger._log[it->second]._cancelled = true;
  Recompute(ledger);
  return true;
}

bool TradeLedger::AdjustVolume(const std::string &symbol, std::uint64_t id,
                               std::uint64_t newVolume) {
  auto &ledger = GetOrCreate(symbol);
  auto it = ledger._byId.find(id);
  if (it == ledger._byId.end()) {
    _failures++;
    return false;
  }

  // Price is untouched, so High/Low/Open/Close never move -- apply the
  // volume delta directly instead of paying for a full Recompute().
  TradeRecord &trade = ledger._log[it->second];
  const auto delta = static_cast<std::int64_t>(newVolume) -
                     static_cast<std::int64_t>(trade._volume);
  ledger._stats._totalVolume = static_cast<std::uint64_t>(
      static_cast<std::int64_t>(ledger._stats._totalVolume) + delta);
  trade._volume = newVolume;
  return true;
}

bool TradeLedger::ReplaceTrade(const std::string &symbol, std::uint64_t oldId,
                               std::uint64_t newId, double price,
                               std::uint64_t volume) {
  auto &ledger = GetOrCreate(symbol);

  bool foundOld = false;
  auto it = ledger._byId.find(oldId);
  if (it != ledger._byId.end() && !ledger._log[it->second]._cancelled) {
    ledger._log[it->second]._cancelled = true;
    foundOld = true;
  } else {
    _failures++;
  }

  ledger._byId[newId] = AppendRaw(ledger, price, volume);
  Recompute(ledger);
  return foundOld;
}

DailyStats TradeLedger::GetDailyStats(const std::string &symbol) const {
  auto it = _ledgers.find(symbol);
  return it == _ledgers.end() ? DailyStats{} : it->second._stats;
}

std::vector<std::string> TradeLedger::GetSymbols() const {
  std::vector<std::string> symbols;
  symbols.reserve(_ledgers.size());
  for (const auto &[symbol, ledger] : _ledgers) {
    symbols.push_back(symbol);
  }
  return symbols;
}

} // namespace mde::model
