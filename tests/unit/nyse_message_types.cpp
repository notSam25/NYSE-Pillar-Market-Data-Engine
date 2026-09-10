#include <engine.hpp>
#include <gtest/gtest.h>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

// Round-trips one hand-built CSV line per new msgType (34, 100-104, 114)
// through the full Ingest -> Parser::Emit -> MessageQueue -> Egress ->
// EngineCallback pipeline, and checks the public mde::messages struct the
// callback receives matches what was encoded. Complements nyse_csv.cpp
// (msgType 3) and nyse_sample_data.cpp (real-file integration).

namespace {

struct CapturedMessages {
  std::optional<mde::messages::SecurityStatus> _securityStatus;
  std::optional<mde::messages::AddOrder> _addOrder;
  std::optional<mde::messages::ModifyOrder> _modifyOrder;
  std::optional<mde::messages::DeleteOrder> _deleteOrder;
  std::optional<mde::messages::OrderExecution> _orderExecution;
  std::optional<mde::messages::ReplaceOrder> _replaceOrder;
  std::optional<mde::messages::RetailPriceImprovement> _rpi;
  std::optional<mde::messages::Imbalance> _imbalance;
  std::optional<mde::messages::AddOrderRefresh> _addOrderRefresh;
  std::optional<mde::messages::NonDisplayedTrade> _nonDisplayedTrade;
  std::optional<mde::messages::CrossTrade> _crossTrade;
  std::optional<mde::messages::TradeCancel> _tradeCancel;
  std::optional<mde::messages::CrossCorrection> _crossCorrection;
};

CapturedMessages RunLines(const std::vector<std::string> &lines) {
  CapturedMessages captured;

  auto engine = mde::Engine(
      mde::IngestData::NYSE_PILLAR, lines,
      [&captured](mde::messages::MessageType type, void *data) {
        using mde::messages::MessageType;
        switch (type) {
        case MessageType::SecurityStatus:
          captured._securityStatus =
              *static_cast<mde::messages::SecurityStatus *>(data);
          break;
        case MessageType::AddOrder:
          captured._addOrder = *static_cast<mde::messages::AddOrder *>(data);
          break;
        case MessageType::ModifyOrder:
          captured._modifyOrder =
              *static_cast<mde::messages::ModifyOrder *>(data);
          break;
        case MessageType::DeleteOrder:
          captured._deleteOrder =
              *static_cast<mde::messages::DeleteOrder *>(data);
          break;
        case MessageType::OrderExecution:
          captured._orderExecution =
              *static_cast<mde::messages::OrderExecution *>(data);
          break;
        case MessageType::ReplaceOrder:
          captured._replaceOrder =
              *static_cast<mde::messages::ReplaceOrder *>(data);
          break;
        case MessageType::RetailPriceImprovement:
          captured._rpi =
              *static_cast<mde::messages::RetailPriceImprovement *>(data);
          break;
        case MessageType::Imbalance:
          captured._imbalance =
              *static_cast<mde::messages::Imbalance *>(data);
          break;
        case MessageType::AddOrderRefresh:
          captured._addOrderRefresh =
              *static_cast<mde::messages::AddOrderRefresh *>(data);
          break;
        case MessageType::NonDisplayedTrade:
          captured._nonDisplayedTrade =
              *static_cast<mde::messages::NonDisplayedTrade *>(data);
          break;
        case MessageType::CrossTrade:
          captured._crossTrade =
              *static_cast<mde::messages::CrossTrade *>(data);
          break;
        case MessageType::TradeCancel:
          captured._tradeCancel =
              *static_cast<mde::messages::TradeCancel *>(data);
          break;
        case MessageType::CrossCorrection:
          captured._crossCorrection =
              *static_cast<mde::messages::CrossCorrection *>(data);
          break;
        default:
          break;
        }
      });
  engine.Join();
  return captured;
}

} // namespace

TEST(NYSE_MessageTypes, SecurityStatus) {
  spdlog::set_level(spdlog::level::off);
  const auto captured = RunLines(
      {"34,1,09:30:00.000000000,ABC,1,4,I,10.5,11.5,N,100,093000,E,O"});

  ASSERT_TRUE(captured._securityStatus.has_value());
  const auto &msg = *captured._securityStatus;
  EXPECT_EQ(msg._symbol, "ABC");
  EXPECT_EQ(msg._symbolSeqNum, 1u);
  EXPECT_EQ(msg._securityStatus, '4');
  ASSERT_TRUE(msg._haltCondition.has_value());
  EXPECT_EQ(*msg._haltCondition, 'I');
  ASSERT_TRUE(msg._price1.has_value());
  EXPECT_DOUBLE_EQ(*msg._price1, 10.5);
  ASSERT_TRUE(msg._price2.has_value());
  EXPECT_DOUBLE_EQ(*msg._price2, 11.5);
}

TEST(NYSE_MessageTypes, SecurityStatusBlankOptionalFields) {
  spdlog::set_level(spdlog::level::off);
  // Halt with none of the conditional fields populated -- spec §2.2.6:
  // default/not-applicable values are published as an empty CSV field.
  const auto captured =
      RunLines({"34,1,09:30:00.000000000,ABC,1,5,,,,,,,,"});

  ASSERT_TRUE(captured._securityStatus.has_value());
  const auto &msg = *captured._securityStatus;
  EXPECT_EQ(msg._securityStatus, '5');
  EXPECT_FALSE(msg._haltCondition.has_value());
  EXPECT_FALSE(msg._price1.has_value());
  EXPECT_FALSE(msg._price2.has_value());
  EXPECT_FALSE(msg._ssrTriggeringExchangeId.has_value());
  EXPECT_FALSE(msg._ssrTriggeringVolume.has_value());
  EXPECT_FALSE(msg._time.has_value());
  EXPECT_FALSE(msg._ssrState.has_value());
  EXPECT_FALSE(msg._marketState.has_value());
}

TEST(NYSE_MessageTypes, AddOrder) {
  spdlog::set_level(spdlog::level::off);
  const auto captured =
      RunLines({"100,1,09:30:00.000000001,ABC,1,555,25.50,100,B,FIRM,0"});

  ASSERT_TRUE(captured._addOrder.has_value());
  const auto &msg = *captured._addOrder;
  EXPECT_EQ(msg._symbol, "ABC");
  EXPECT_EQ(msg._orderId, 555u);
  EXPECT_DOUBLE_EQ(msg._price, 25.50);
  EXPECT_EQ(msg._volume, 100u);
  EXPECT_EQ(msg._side, 'B');
  EXPECT_EQ(msg._firmId, "FIRM");
}

TEST(NYSE_MessageTypes, ModifyOrder) {
  spdlog::set_level(spdlog::level::off);
  const auto captured =
      RunLines({"101,1,09:30:01.000000000,ABC,1,555,26.00,50,0,B,0"});

  ASSERT_TRUE(captured._modifyOrder.has_value());
  const auto &msg = *captured._modifyOrder;
  EXPECT_EQ(msg._orderId, 555u);
  EXPECT_DOUBLE_EQ(msg._price, 26.00);
  EXPECT_EQ(msg._volume, 50u);
  EXPECT_EQ(msg._side, 'B');
}

TEST(NYSE_MessageTypes, DeleteOrder) {
  spdlog::set_level(spdlog::level::off);
  const auto captured = RunLines({"102,1,09:30:02.000000000,ABC,1,555,0"});

  ASSERT_TRUE(captured._deleteOrder.has_value());
  EXPECT_EQ(captured._deleteOrder->_orderId, 555u);
  EXPECT_EQ(captured._deleteOrder->_symbol, "ABC");
}

TEST(NYSE_MessageTypes, OrderExecution) {
  spdlog::set_level(spdlog::level::off);
  const auto captured = RunLines(
      {"103,1,09:30:03.000000000,ABC,1,555,999,25.75,50,1,0,@,,,"});

  ASSERT_TRUE(captured._orderExecution.has_value());
  const auto &msg = *captured._orderExecution;
  EXPECT_EQ(msg._orderId, 555u);
  EXPECT_EQ(msg._tradeId, 999u);
  EXPECT_DOUBLE_EQ(msg._price, 25.75);
  EXPECT_EQ(msg._volume, 50u);
  EXPECT_TRUE(msg._printedToSip);
}

TEST(NYSE_MessageTypes, ReplaceOrder) {
  spdlog::set_level(spdlog::level::off);
  const auto captured = RunLines(
      {"104,1,09:30:04.000000000,ABC,1,555,556,26.10,75,B,0"});

  ASSERT_TRUE(captured._replaceOrder.has_value());
  const auto &msg = *captured._replaceOrder;
  EXPECT_EQ(msg._orderId, 555u);
  EXPECT_EQ(msg._newOrderId, 556u);
  EXPECT_DOUBLE_EQ(msg._price, 26.10);
  EXPECT_EQ(msg._volume, 75u);
  EXPECT_EQ(msg._side, 'B');
}

TEST(NYSE_MessageTypes, RetailPriceImprovement) {
  spdlog::set_level(spdlog::level::off);
  const auto captured =
      RunLines({"114,1,09:30:05.000000000,ABC,1,A"});

  ASSERT_TRUE(captured._rpi.has_value());
  ASSERT_TRUE(captured._rpi->_rpiIndicator.has_value());
  EXPECT_EQ(*captured._rpi->_rpiIndicator, 'A');
}

TEST(NYSE_MessageTypes, RetailPriceImprovementBlankIndicator) {
  spdlog::set_level(spdlog::level::off);
  // Space means "no retail interest" and is published blank per §2.2.6.
  const auto captured = RunLines({"114,1,09:30:05.000000000,ABC,1,"});

  ASSERT_TRUE(captured._rpi.has_value());
  EXPECT_FALSE(captured._rpi->_rpiIndicator.has_value());
}

TEST(NYSE_MessageTypes, Imbalance) {
  spdlog::set_level(spdlog::level::off);
  const auto captured = RunLines(
      {"105,1,09:30:00.000000000,ABC,1,10.50,1000,500,0,0930,M,B,10.45,"
       "10.50,0,10.48,10.60,10.40,1,0,0,0, , "});

  ASSERT_TRUE(captured._imbalance.has_value());
  const auto &msg = *captured._imbalance;
  EXPECT_EQ(msg._symbol, "ABC");
  EXPECT_DOUBLE_EQ(msg._referencePrice, 10.50);
  EXPECT_EQ(msg._pairedQty, 1000u);
  EXPECT_EQ(msg._totalImbalanceQty, 500u);
  EXPECT_EQ(msg._auctionType, 'M');
  EXPECT_EQ(msg._imbalanceSide, 'B');
  EXPECT_EQ(msg._auctionStatus, 1u);
}

TEST(NYSE_MessageTypes, AddOrderRefresh) {
  spdlog::set_level(spdlog::level::off);
  const auto captured = RunLines(
      {"106,1,09:30:00.000000001,ABC,1,777,25.50,100,B,FIRM,0"});

  ASSERT_TRUE(captured._addOrderRefresh.has_value());
  const auto &msg = *captured._addOrderRefresh;
  EXPECT_EQ(msg._orderId, 777u);
  EXPECT_DOUBLE_EQ(msg._price, 25.50);
  EXPECT_EQ(msg._volume, 100u);
  EXPECT_EQ(msg._side, 'B');
  EXPECT_EQ(msg._firmId, "FIRM");
}

TEST(NYSE_MessageTypes, NonDisplayedTrade) {
  spdlog::set_level(spdlog::level::off);
  const auto captured = RunLines(
      {"110,1,09:30:00.000000000,ABC,1,42,25.60,50,0,@,O, ,I"});

  ASSERT_TRUE(captured._nonDisplayedTrade.has_value());
  const auto &msg = *captured._nonDisplayedTrade;
  EXPECT_EQ(msg._tradeId, 42u);
  EXPECT_DOUBLE_EQ(msg._price, 25.60);
  EXPECT_EQ(msg._volume, 50u);
  EXPECT_FALSE(msg._printedToSip);
}

TEST(NYSE_MessageTypes, CrossTrade) {
  spdlog::set_level(spdlog::level::off);
  const auto captured =
      RunLines({"111,1,09:30:00.000000000,ABC,1,999,25.70,1000,O"});

  ASSERT_TRUE(captured._crossTrade.has_value());
  const auto &msg = *captured._crossTrade;
  EXPECT_EQ(msg._crossId, 999u);
  EXPECT_DOUBLE_EQ(msg._price, 25.70);
  EXPECT_EQ(msg._volume, 1000u);
  EXPECT_EQ(msg._crossType, 'O');
}

TEST(NYSE_MessageTypes, TradeCancel) {
  spdlog::set_level(spdlog::level::off);
  const auto captured =
      RunLines({"112,1,09:30:00.000000000,ABC,1,42"});

  ASSERT_TRUE(captured._tradeCancel.has_value());
  EXPECT_EQ(captured._tradeCancel->_tradeId, 42u);
}

TEST(NYSE_MessageTypes, CrossCorrection) {
  spdlog::set_level(spdlog::level::off);
  const auto captured =
      RunLines({"113,1,09:30:00.000000000,ABC,1,999,900"});

  ASSERT_TRUE(captured._crossCorrection.has_value());
  EXPECT_EQ(captured._crossCorrection->_crossId, 999u);
  EXPECT_EQ(captured._crossCorrection->_volume, 900u);
}
