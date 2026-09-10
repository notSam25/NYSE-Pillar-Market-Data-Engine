#include "../parse.hpp"
#include "csv.hpp"
#include "nyse_types.hpp"
#include <cstdint>
#include <memory>
#include <spdlog/spdlog.h>

// msgType constants per TAQ Products Client Spec v2.4 §2.1 (Integrated Feed
// subset). Note: 102 is Delete Order, not Replace -- Replace is 104.
constexpr uint8_t SymbolIndexMapping = 3u;
constexpr uint8_t SecurityStatusMessage = 34u;
constexpr uint8_t AddOrder = 100u;
constexpr uint8_t ModifyOrder = 101u;
constexpr uint8_t DeleteOrder = 102u;
constexpr uint8_t OrderExecution = 103u;
constexpr uint8_t ReplaceOrder = 104u;
constexpr uint8_t Imbalance = 105u;
constexpr uint8_t AddOrderRefresh = 106u;
constexpr uint8_t NonDisplayedTrade = 110u;
constexpr uint8_t CrossTrade = 111u;
constexpr uint8_t TradeCancel = 112u;
constexpr uint8_t CrossCorrection = 113u;
constexpr uint8_t RetailPriceImprovement = 114u;
#include <format>

namespace mde::schema::nyse {
class Parser final : public mde::schema::Parser {
public:
  explicit Parser(mde::MessageQueue &queue) : mde::schema::Parser(queue) {}
  ~Parser() override = default;

  ParseError ParseNext(
      std::unique_ptr<const std::vector<uint8_t>> data) noexcept override {
    try {
      const std::string_view text{reinterpret_cast<const char *>(data->data()),
                                  data->size()};
      mde::schema::nyse::CSVReader csvReader{text};

      messages::MessageHeader messageHeader{&csvReader};

      RecordSequence(messageHeader._msgType, messageHeader._sequenceNumber);
      switch (messageHeader._msgType) {
      case ::SymbolIndexMapping: {
        messages::SymbolIndexMapping wireMessage{&csvReader};
        Emit(mde::messages::MessageType::SymbolIndexMapping,
             mde::messages::SymbolIndexMapping{
                 ._symbol = wireMessage._symbol,
                 ._marketId = wireMessage._marketId,
                 ._systemId = wireMessage._systemId,
                 ._exchangeCode = wireMessage._exchangeCode,
                 ._securityType = wireMessage._securityType,
                 ._lotSize = wireMessage._lotSize,
                 ._prevClosePrice = wireMessage._prevClosePrice,
                 ._prevCloseVolume = wireMessage._prevCloseVolume,
                 ._priceResolution = wireMessage._priceResolution,
                 ._roundLot = wireMessage._roundLot,
                 ._mpv = wireMessage._mpv,
                 ._unitOfTrade = wireMessage._unitOfTrade,
             });
        break;
      }
      case ::SecurityStatusMessage: {
        messages::SecurityStatus wireMessage{&csvReader};
        Emit(mde::messages::MessageType::SecurityStatus,
             mde::messages::SecurityStatus{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._securityStatus = wireMessage._securityStatus,
                 ._haltCondition = wireMessage._haltCondition,
                 ._price1 = wireMessage._price1,
                 ._price2 = wireMessage._price2,
                 ._ssrTriggeringExchangeId =
                     wireMessage._ssrTriggeringExchangeId,
                 ._ssrTriggeringVolume = wireMessage._ssrTriggeringVolume,
                 ._time = wireMessage._time,
                 ._ssrState = wireMessage._ssrState,
                 ._marketState = wireMessage._marketState,
             });
        break;
      }
      case ::AddOrder: {
        messages::AddOrder wireMessage{&csvReader};
        Emit(mde::messages::MessageType::AddOrder,
             mde::messages::AddOrder{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._orderId = wireMessage._orderId,
                 ._price = wireMessage._price,
                 ._volume = wireMessage._volume,
                 ._side = wireMessage._side,
                 ._firmId = wireMessage._firmId,
             });
        break;
      }
      case ::ModifyOrder: {
        messages::ModifyOrder wireMessage{&csvReader};
        Emit(mde::messages::MessageType::ModifyOrder,
             mde::messages::ModifyOrder{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._orderId = wireMessage._orderId,
                 ._price = wireMessage._price,
                 ._volume = wireMessage._volume,
                 ._side = wireMessage._side,
             });
        break;
      }
      case ::DeleteOrder: {
        messages::DeleteOrder wireMessage{&csvReader};
        Emit(mde::messages::MessageType::DeleteOrder,
             mde::messages::DeleteOrder{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._orderId = wireMessage._orderId,
             });
        break;
      }
      case ::OrderExecution: {
        messages::OrderExecution wireMessage{&csvReader};
        Emit(mde::messages::MessageType::OrderExecution,
             mde::messages::OrderExecution{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._orderId = wireMessage._orderId,
                 ._tradeId = wireMessage._tradeId,
                 ._price = wireMessage._price,
                 ._volume = wireMessage._volume,
                 ._printedToSip = wireMessage._printableFlag != 0,
             });
        break;
      }
      case ::ReplaceOrder: {
        messages::ReplaceOrder wireMessage{&csvReader};
        Emit(mde::messages::MessageType::ReplaceOrder,
             mde::messages::ReplaceOrder{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._orderId = wireMessage._orderId,
                 ._newOrderId = wireMessage._newOrderId,
                 ._price = wireMessage._price,
                 ._volume = wireMessage._volume,
                 ._side = wireMessage._side,
             });
        break;
      }
      case ::RetailPriceImprovement: {
        messages::RetailPriceImprovement wireMessage{&csvReader};
        Emit(mde::messages::MessageType::RetailPriceImprovement,
             mde::messages::RetailPriceImprovement{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._rpiIndicator = wireMessage._rpiIndicator,
             });
        break;
      }
      case ::Imbalance: {
        messages::Imbalance wireMessage{&csvReader};
        Emit(mde::messages::MessageType::Imbalance,
             mde::messages::Imbalance{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._referencePrice = wireMessage._referencePrice,
                 ._pairedQty = wireMessage._pairedQty,
                 ._totalImbalanceQty = wireMessage._totalImbalanceQty,
                 ._marketImbalanceQty = wireMessage._marketImbalanceQty,
                 ._auctionTime = wireMessage._auctionTime,
                 ._auctionType = wireMessage._auctionType,
                 ._imbalanceSide = wireMessage._imbalanceSide,
                 ._continuousBookClearingPrice =
                     wireMessage._continuousBookClearingPrice,
                 ._auctionInterestClearingPrice =
                     wireMessage._auctionInterestClearingPrice,
                 ._ssrFilingPrice = wireMessage._ssrFilingPrice,
                 ._indicativeMatchPrice = wireMessage._indicativeMatchPrice,
                 ._upperCollar = wireMessage._upperCollar,
                 ._lowerCollar = wireMessage._lowerCollar,
                 ._auctionStatus = wireMessage._auctionStatus,
                 ._freezeStatus = wireMessage._freezeStatus,
                 ._numExtensions = wireMessage._numExtensions,
                 ._unpairedQuantity = wireMessage._unpairedQuantity,
                 ._unpairedSide = wireMessage._unpairedSide,
                 ._significantImbalance = wireMessage._significantImbalance,
             });
        break;
      }
      case ::AddOrderRefresh: {
        messages::AddOrderRefresh wireMessage{&csvReader};
        Emit(mde::messages::MessageType::AddOrderRefresh,
             mde::messages::AddOrderRefresh{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._orderId = wireMessage._orderId,
                 ._price = wireMessage._price,
                 ._volume = wireMessage._volume,
                 ._side = wireMessage._side,
                 ._firmId = wireMessage._firmId,
             });
        break;
      }
      case ::NonDisplayedTrade: {
        messages::NonDisplayedTrade wireMessage{&csvReader};
        Emit(mde::messages::MessageType::NonDisplayedTrade,
             mde::messages::NonDisplayedTrade{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._tradeId = wireMessage._tradeId,
                 ._price = wireMessage._price,
                 ._volume = wireMessage._volume,
                 ._printedToSip = wireMessage._printableFlag != 0,
             });
        break;
      }
      case ::CrossTrade: {
        messages::CrossTrade wireMessage{&csvReader};
        Emit(mde::messages::MessageType::CrossTrade,
             mde::messages::CrossTrade{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._crossId = wireMessage._crossId,
                 ._price = wireMessage._price,
                 ._volume = wireMessage._volume,
                 ._crossType = wireMessage._crossType,
             });
        break;
      }
      case ::TradeCancel: {
        messages::TradeCancel wireMessage{&csvReader};
        Emit(mde::messages::MessageType::TradeCancel,
             mde::messages::TradeCancel{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._tradeId = wireMessage._tradeId,
             });
        break;
      }
      case ::CrossCorrection: {
        messages::CrossCorrection wireMessage{&csvReader};
        Emit(mde::messages::MessageType::CrossCorrection,
             mde::messages::CrossCorrection{
                 ._nanosSinceMidnight =
                     wireMessage._header._sourceTime._nanosSinceMidnight,
                 ._symbol = wireMessage._header._symbol,
                 ._symbolSeqNum = wireMessage._header._symbolSeqNum,
                 ._crossId = wireMessage._crossId,
                 ._volume = wireMessage._volume,
             });
        break;
      }
      default: {
        spdlog::warn(std::format("Encountered unimplemented MsgType: {}",
                                 messageHeader._msgType));
        return ParseError::unknown_msg_type;
      }
      }

    } catch (const CSVReader::Error &e) {
      spdlog::warn(std::format("Failed to parse message: {} (field {})",
                               e.what(), e.fieldIndex));
      return ParseError::message_decode;
    } catch (const std::exception &e) {
      spdlog::warn(std::format("Failed to parse MessageHeader: {}", e.what()));
      return ParseError::unknown;
    }
    return ParseError::none;
  };
};
} // namespace mde::schema::nyse
