#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace mde {

// Public, exchange-agnostic representation of decoded market data. A
// Processor (e.g. mde::schema::nyse::Parser) decodes an exchange-specific
// 'wire' message and maps it onto one of these before handing it to the message
// queue. Note that many design decisions made here, namely the use of
// std::optional, heavily rely on data from NYSE Pillar TAQ spec sheet for
// guidence, and may require modification to support more specs
namespace messages {

enum class MessageType : std::uint8_t {
  SymbolIndexMapping = 0,
  SecurityStatus,
  AddOrder,
  ModifyOrder,
  DeleteOrder,
  OrderExecution,
  ReplaceOrder,
  RetailPriceImprovement,
  Imbalance,
  AddOrderRefresh,
  NonDisplayedTrade,
  CrossTrade,
  TradeCancel,
  CrossCorrection,
};

struct SymbolIndexMapping {
  std::string _symbol;
  std::uint8_t _marketId;
  std::uint8_t _systemId;
  char _exchangeCode;
  char _securityType;
  std::uint64_t _lotSize;
  double _prevClosePrice;
  std::uint64_t _prevCloseVolume;
  std::uint8_t _priceResolution;
  char _roundLot;
  double _mpv;
  std::uint8_t _unitOfTrade;
};

// A field is std::nullopt where the exchange published it blank (its
// default/not-applicable value; TAQ Products Client Spec 2.2.6)
struct SecurityStatus {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  char _securityStatus;
  std::optional<char> _haltCondition;
  std::optional<double> _price1;
  std::optional<double> _price2;
  std::optional<char> _ssrTriggeringExchangeId;
  std::optional<std::uint64_t> _ssrTriggeringVolume;
  std::optional<std::uint64_t> _time;
  std::optional<char> _ssrState;
  std::optional<char> _marketState;
};

struct AddOrder {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  std::uint64_t _orderId;
  double _price;
  std::uint64_t _volume;
  char _side;
  std::string _firmId;
};

struct ModifyOrder {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  std::uint64_t _orderId;
  double _price;
  std::uint64_t _volume;
  char _side;
};

struct DeleteOrder {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  std::uint64_t _orderId;
};

struct OrderExecution {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  std::uint64_t _orderId;
  std::uint64_t _tradeId;
  double _price;
  std::uint64_t _volume;
  bool _printedToSip;
};

struct ReplaceOrder {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  std::uint64_t _orderId;
  std::uint64_t _newOrderId;
  double _price;
  std::uint64_t _volume;
  char _side;
};

struct RetailPriceImprovement {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  std::optional<char> _rpiIndicator;
};

struct Imbalance {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  double _referencePrice;
  std::uint64_t _pairedQty;
  std::uint64_t _totalImbalanceQty;
  std::uint64_t _marketImbalanceQty;
  std::uint64_t _auctionTime;
  char _auctionType;
  char _imbalanceSide;
  double _continuousBookClearingPrice;
  double _auctionInterestClearingPrice;
  double _ssrFilingPrice;
  double _indicativeMatchPrice;
  double _upperCollar;
  double _lowerCollar;
  std::uint8_t _auctionStatus;
  std::uint8_t _freezeStatus;
  std::uint64_t _numExtensions;
  std::uint64_t _unpairedQuantity;
  char _unpairedSide;
  char _significantImbalance;
};

// Same fields as AddOrder, sent in response to a Refresh Request or
// backup failover, for every order currently resting on the book
struct AddOrderRefresh {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  std::uint64_t _orderId;
  double _price;
  std::uint64_t _volume;
  char _side;
  std::string _firmId;
};

// A match between two non-displayed orders. Doesn't move the displayed
// book; only volume
struct NonDisplayedTrade {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  std::uint64_t _tradeId;
  double _price;
  std::uint64_t _volume;
  bool _printedToSip;
};

// Bulk auction print. Doesn't move the displayed book; only volume
struct CrossTrade {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  std::uint64_t _crossId;
  double _price;
  std::uint64_t _volume;
  char _crossType;
};

// Cancels an earlier OrderExecution or NonDisplayedTrade, identified by
// its TradeID
struct TradeCancel {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  std::uint64_t _tradeId;
};

// Corrects the volume of an earlier CrossTrade, identified by its `_crossID`
struct CrossCorrection {
  std::uint64_t _nanosSinceMidnight;
  std::string _symbol;
  std::uint64_t _symbolSeqNum;
  std::uint64_t _crossId;
  std::uint64_t _volume;
};

} // namespace messages

// Type-less normalized message pushed through MessageQueue. `_data` owns the
// payload and the Egress thread hands the raw pointer to the EngineCallback for
// the duration of that one call only. Consumers must copy out anything they
// need to keep, as it's free'd after the callback is invoked
struct Message {
  messages::MessageType _type;
  std::shared_ptr<void> _data;
};

template <typename T>
Message MakeMessage(messages::MessageType type, T payload) {
  return Message{type, std::make_shared<T>(std::move(payload))};
}

} // namespace mde
