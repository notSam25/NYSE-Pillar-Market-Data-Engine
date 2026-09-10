#pragma once
#include "nyse_types.hpp"

// Wire structs for the TAQ NYSE BBO (msgType 140), TAQ NYSE Stock Summary
// (msgType 223), and TAQ NYSE Trades (msgTypes 220/221/222) products.
// These are NOT ingested by mde::Engine -- per project scope, the Engine
// only ingests the Integrated Feed. These exist so examples/bbo-validation
// and examples/stock-summary can decode the independent BBO/StockSummary/
// Trades reference files directly with the same CSVReader machinery, to
// check the Engine-derived order book against them.

namespace mde::schema::nyse::messages {

// TAQ Products Client Spec v2.4 §25 "Stock Summary Message" (msgType 223).
// Published once per minute per symbol on a separate channel from the
// order/trade stream. No SymbolSeqNum field (unlike the per-symbol header
// used elsewhere), so this does not reuse PerSymbolHeader.
#define STOCK_SUMMARY_MEMBERS(X)                                              \
  X(_header, MessageHeader)                                                   \
  X(_sourceTime, mde::schema::nyse::SourceTime)                               \
  X(_symbol, std::string)                                                     \
  X(_highPrice, double)                                                       \
  X(_lowPrice, double)                                                        \
  X(_open, double)                                                            \
  X(_close, double)                                                           \
  X(_totalVolume, std::uint64_t)

DEFINE_STRUCT(StockSummary, STOCK_SUMMARY_MEMBERS)

// TAQ Products Client Spec v2.4 §17 "Quote Message" (msgType 140) -- the
// BBO feed's core message, published on any top-of-book change.
#define QUOTE_MEMBERS(X)                                                      \
  X(_header, PerSymbolHeader)                                                 \
  X(_askPrice, double)                                                        \
  X(_askVolume, std::uint64_t)                                                \
  X(_bidPrice, double)                                                        \
  X(_bidVolume, std::uint64_t)                                                \
  X(_quoteCondition, char)                                                    \
  X(_rpiIndicator, std::optional<char>)

DEFINE_STRUCT(Quote, QUOTE_MEMBERS)

// TAQ Products Client Spec v2.4 §23 "Trade Message" (msgType 220), TAQ
// NYSE Trades product. STOCKSUM's TotalVolume is computed from this
// stream, not from the Integrated Feed's Order Execution/Non-Displayed
// Trade/Cross Trade messages -- verified against real sample data (whole-
// file Trade volume matches STOCKSUM's total to within 0.001%, and an
// individual symbol's Trade volume matched its STOCKSUM volume exactly).
#define TAQ_TRADE_MEMBERS(X)                                                  \
  X(_header, PerSymbolHeader)                                                 \
  X(_tradeId, std::uint64_t)                                                  \
  X(_price, double)                                                           \
  X(_volume, std::uint64_t)                                                   \
  X(_tradeCondition1, std::optional<char>)                                    \
  X(_tradeCondition2, std::optional<char>)                                    \
  X(_tradeCondition3, std::optional<char>)                                    \
  X(_tradeCondition4, std::optional<char>)

DEFINE_STRUCT(TaqTrade, TAQ_TRADE_MEMBERS)

// Msg Type 221 "Trade Cancel for TAQ Trades" has the identical 6-field
// layout as Integrated's Trade Cancel (msgType 112) -- reuse that struct
// rather than duplicating it (see nyse_types.hpp).

// TAQ Products Client Spec v2.4 §24 "Trade Correction Message" (msgType
// 222). Field-order column in the spec table has a numbering artifact
// (TradeCond1 through TradeCond4 all listed under duplicate order
// numbers); field layout below follows the same "always contiguous,
// doc-table-numbering-is-buggy" pattern already confirmed against real
// data for several other message types in this file/nyse_types.hpp.
// Unverified against real data -- msgType 222 has zero occurrences in the
// available sample day.
#define TAQ_TRADE_CORRECTION_MEMBERS(X)                                       \
  X(_header, PerSymbolHeader)                                                 \
  X(_originalTradeId, std::uint64_t)                                         \
  X(_tradeId, std::uint64_t)                                                  \
  X(_price, double)                                                           \
  X(_volume, std::uint64_t)                                                   \
  X(_tradeCondition1, std::optional<char>)                                    \
  X(_tradeCondition2, std::optional<char>)                                    \
  X(_tradeCondition3, std::optional<char>)                                    \
  X(_tradeCondition4, std::optional<char>)

DEFINE_STRUCT(TaqTradeCorrection, TAQ_TRADE_CORRECTION_MEMBERS)

} // namespace mde::schema::nyse::messages
