#pragma once
#include "csv.hpp"

namespace mde::schema::nyse::messages {

#define MESSAGE_HEADER_MEMBERS(X)                                              \
  X(_msgType, std::uint8_t)                                                    \
  X(_sequenceNumber, std::uint64_t)

DEFINE_STRUCT(MessageHeader, MESSAGE_HEADER_MEMBERS)

#define SYMBOL_INDEX_MAPPING_MEMBERS(X)                                        \
  X(_header, MessageHeader)                                                    \
  X(_symbol, std::string)                                                      \
  X(_marketId, std::uint8_t)                                                   \
  X(_systemId, std::uint8_t)                                                   \
  X(_exchangeCode, char)                                                       \
  X(_securityType, char)                                                       \
  X(_lotSize, std::uint64_t)                                                   \
  X(_prevClosePrice, double)                                                   \
  X(_prevCloseVolume, std::uint64_t)                                           \
  X(_priceResolution, std::uint8_t)                                            \
  X(_roundLot, char)                                                           \
  X(_mpv, double)                                                              \
  X(_unitOfTrade, std::uint8_t)

DEFINE_STRUCT(SymbolIndexMapping, SYMBOL_INDEX_MAPPING_MEMBERS)

// Shared prefix for every per-symbol Integrated Feed message (msgTypes 34,
// 100-104, 106, 110-114, 140): MsgType, SequenceNumber, SourceTime, Symbol,
// SymbolSeqNum. Symbol Index Mapping (3) is the one exception and does not
// use this
#define PER_SYMBOL_HEADER_MEMBERS(X)                                           \
  X(_header, MessageHeader)                                                    \
  X(_sourceTime, mde::schema::nyse::SourceTime)                                \
  X(_symbol, std::string)                                                      \
  X(_symbolSeqNum, std::uint64_t)

DEFINE_STRUCT(PerSymbolHeader, PER_SYMBOL_HEADER_MEMBERS)

#define SECURITY_STATUS_MEMBERS(X)                                             \
  X(_header, PerSymbolHeader)                                                  \
  X(_securityStatus, char)                                                     \
  X(_haltCondition, std::optional<char>)                                       \
  X(_price1, std::optional<double>)                                            \
  X(_price2, std::optional<double>)                                            \
  X(_ssrTriggeringExchangeId, std::optional<char>)                             \
  X(_ssrTriggeringVolume, std::optional<std::uint64_t>)                        \
  X(_time, std::optional<std::uint64_t>)                                       \
  X(_ssrState, std::optional<char>)                                            \
  X(_marketState, std::optional<char>)

DEFINE_STRUCT(SecurityStatus, SECURITY_STATUS_MEMBERS)

#define ADD_ORDER_MEMBERS(X)                                                   \
  X(_header, PerSymbolHeader)                                                  \
  X(_orderId, std::uint64_t)                                                   \
  X(_price, double)                                                            \
  X(_volume, std::uint64_t)                                                    \
  X(_side, char)                                                               \
  X(_firmId, std::string)                                                      \
  X(_reserved, std::optional<std::uint64_t>)

DEFINE_STRUCT(AddOrder, ADD_ORDER_MEMBERS)

#define MODIFY_ORDER_MEMBERS(X)                                                \
  X(_header, PerSymbolHeader)                                                  \
  X(_orderId, std::uint64_t)                                                   \
  X(_price, double)                                                            \
  X(_volume, std::uint64_t)                                                    \
  X(_positionChange, std::optional<std::uint64_t>)                             \
  X(_side, char)                                                               \
  X(_reserved2, std::optional<std::uint64_t>)

DEFINE_STRUCT(ModifyOrder, MODIFY_ORDER_MEMBERS)

#define DELETE_ORDER_MEMBERS(X)                                                \
  X(_header, PerSymbolHeader)                                                  \
  X(_orderId, std::uint64_t)                                                   \
  X(_reserved, std::optional<std::uint64_t>)

DEFINE_STRUCT(DeleteOrder, DELETE_ORDER_MEMBERS)

#define ORDER_EXECUTION_MEMBERS(X)                                             \
  X(_header, PerSymbolHeader)                                                  \
  X(_orderId, std::uint64_t)                                                   \
  X(_tradeId, std::uint64_t)                                                   \
  X(_price, double)                                                            \
  X(_volume, std::uint64_t)                                                    \
  X(_printableFlag, std::uint8_t)                                              \
  X(_reserved1, std::optional<std::uint64_t>)                                  \
  X(_tradeCondition1, std::optional<char>)                                     \
  X(_tradeCondition2, std::optional<char>)                                     \
  X(_tradeCondition3, std::optional<char>)                                     \
  X(_tradeCondition4, std::optional<char>)

DEFINE_STRUCT(OrderExecution, ORDER_EXECUTION_MEMBERS)

#define REPLACE_ORDER_MEMBERS(X)                                               \
  X(_header, PerSymbolHeader)                                                  \
  X(_orderId, std::uint64_t)                                                   \
  X(_newOrderId, std::uint64_t)                                                \
  X(_price, double)                                                            \
  X(_volume, std::uint64_t)                                                    \
  X(_side, char)                                                               \
  X(_reserved2, std::optional<std::uint64_t>)

DEFINE_STRUCT(ReplaceOrder, REPLACE_ORDER_MEMBERS)

#define RETAIL_PRICE_IMPROVEMENT_MEMBERS(X)                                    \
  X(_header, PerSymbolHeader)                                                  \
  X(_rpiIndicator, std::optional<char>)

DEFINE_STRUCT(RetailPriceImprovement, RETAIL_PRICE_IMPROVEMENT_MEMBERS)

#define IMBALANCE_MEMBERS(X)                                                   \
  X(_header, PerSymbolHeader)                                                  \
  X(_referencePrice, double)                                                   \
  X(_pairedQty, std::uint64_t)                                                 \
  X(_totalImbalanceQty, std::uint64_t)                                         \
  X(_marketImbalanceQty, std::uint64_t)                                        \
  X(_auctionTime, std::uint64_t)                                               \
  X(_auctionType, char)                                                        \
  X(_imbalanceSide, char)                                                      \
  X(_continuousBookClearingPrice, double)                                      \
  X(_auctionInterestClearingPrice, double)                                     \
  X(_ssrFilingPrice, double)                                                   \
  X(_indicativeMatchPrice, double)                                             \
  X(_upperCollar, double)                                                      \
  X(_lowerCollar, double)                                                      \
  X(_auctionStatus, std::uint8_t)                                              \
  X(_freezeStatus, std::uint8_t)                                               \
  X(_numExtensions, std::uint64_t)                                             \
  X(_unpairedQuantity, std::uint64_t)                                          \
  X(_unpairedSide, char)                                                       \
  X(_significantImbalance, char)

DEFINE_STRUCT(Imbalance, IMBALANCE_MEMBERS)

// Same field layout as Add Order (100) sent in response to a Refresh
// Request or backup failover, for every order currently on the book
#define ADD_ORDER_REFRESH_MEMBERS(X)                                           \
  X(_header, PerSymbolHeader)                                                  \
  X(_orderId, std::uint64_t)                                                   \
  X(_price, double)                                                            \
  X(_volume, std::uint64_t)                                                    \
  X(_side, char)                                                               \
  X(_firmId, std::string)                                                      \
  X(_reserved, std::optional<std::uint64_t>)

DEFINE_STRUCT(AddOrderRefresh, ADD_ORDER_REFRESH_MEMBERS)

#define NON_DISPLAYED_TRADE_MEMBERS(X)                                         \
  X(_header, PerSymbolHeader)                                                  \
  X(_tradeId, std::uint64_t)                                                   \
  X(_price, double)                                                            \
  X(_volume, std::uint64_t)                                                    \
  X(_printableFlag, std::uint8_t)                                              \
  X(_tradeCondition1, std::optional<char>)                                     \
  X(_tradeCondition2, std::optional<char>)                                     \
  X(_tradeCondition3, std::optional<char>)                                     \
  X(_tradeCondition4, std::optional<char>)

DEFINE_STRUCT(NonDisplayedTrade, NON_DISPLAYED_TRADE_MEMBERS)

#define CROSS_TRADE_MEMBERS(X)                                                 \
  X(_header, PerSymbolHeader)                                                  \
  X(_crossId, std::uint64_t)                                                   \
  X(_price, double)                                                            \
  X(_volume, std::uint64_t)                                                    \
  X(_crossType, char)

DEFINE_STRUCT(CrossTrade, CROSS_TRADE_MEMBERS)

#define TRADE_CANCEL_MEMBERS(X)                                                \
  X(_header, PerSymbolHeader)                                                  \
  X(_tradeId, std::uint64_t)

DEFINE_STRUCT(TradeCancel, TRADE_CANCEL_MEMBERS)

#define CROSS_CORRECTION_MEMBERS(X)                                            \
  X(_header, PerSymbolHeader)                                                  \
  X(_crossId, std::uint64_t)                                                   \
  X(_volume, std::uint64_t)

DEFINE_STRUCT(CrossCorrection, CROSS_CORRECTION_MEMBERS)

} // namespace mde::schema::nyse::messages
