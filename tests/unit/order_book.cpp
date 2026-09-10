#include <gtest/gtest.h>
#include <model/order_book.hpp>
#include <optional>

// Pure unit tests against mde::model::OrderBook via hand-built public
// messages -- no CSV/Engine involved. Covers the book-mutation semantics
// documented in include/model/order_book.hpp.

namespace {
using mde::messages::AddOrder;
using mde::messages::AddOrderRefresh;
using mde::messages::CrossCorrection;
using mde::messages::CrossTrade;
using mde::messages::DeleteOrder;
using mde::messages::Imbalance;
using mde::messages::ModifyOrder;
using mde::messages::NonDisplayedTrade;
using mde::messages::OrderExecution;
using mde::messages::ReplaceOrder;
using mde::messages::SecurityStatus;
using mde::messages::TradeCancel;
} // namespace

TEST(OrderBook, AddOrderSetsTopOfBook) {
  mde::model::OrderBook book;
  AddOrder add{0, "ABC", 1, 100, 10.00, 500, 'B', "FIRM"};
  book.OnMessage(mde::messages::MessageType::AddOrder, &add);

  const auto top = book.GetTopOfBook("ABC");
  ASSERT_TRUE(top._bestBid.has_value());
  EXPECT_DOUBLE_EQ(top._bestBid->_price, 10.00);
  EXPECT_EQ(top._bestBid->_aggregateVolume, 500u);
  EXPECT_FALSE(top._bestAsk.has_value());
  EXPECT_EQ(book.GetLiveOrderCount(), 1u);
}

TEST(OrderBook, TwoOrdersSamePriceAggregate) {
  mde::model::OrderBook book;
  AddOrder first{0, "ABC", 1, 100, 10.00, 300, 'B', "F1"};
  AddOrder second{0, "ABC", 2, 101, 10.00, 200, 'B', "F2"};
  book.OnMessage(mde::messages::MessageType::AddOrder, &first);
  book.OnMessage(mde::messages::MessageType::AddOrder, &second);

  const auto top = book.GetTopOfBook("ABC");
  ASSERT_TRUE(top._bestBid.has_value());
  EXPECT_EQ(top._bestBid->_aggregateVolume, 500u);
}

TEST(OrderBook, BestBidIsHighestBestAskIsLowest) {
  mde::model::OrderBook book;
  AddOrder lowBid{0, "ABC", 1, 100, 9.90, 100, 'B', "F"};
  AddOrder highBid{0, "ABC", 2, 101, 10.00, 100, 'B', "F"};
  AddOrder lowAsk{0, "ABC", 3, 102, 10.10, 100, 'S', "F"};
  AddOrder highAsk{0, "ABC", 4, 103, 10.20, 100, 'S', "F"};
  book.OnMessage(mde::messages::MessageType::AddOrder, &lowBid);
  book.OnMessage(mde::messages::MessageType::AddOrder, &highBid);
  book.OnMessage(mde::messages::MessageType::AddOrder, &lowAsk);
  book.OnMessage(mde::messages::MessageType::AddOrder, &highAsk);

  const auto top = book.GetTopOfBook("ABC");
  ASSERT_TRUE(top._bestBid.has_value());
  ASSERT_TRUE(top._bestAsk.has_value());
  EXPECT_DOUBLE_EQ(top._bestBid->_price, 10.00);
  EXPECT_DOUBLE_EQ(top._bestAsk->_price, 10.10);
}

TEST(OrderBook, ModifyOrderMovesLevel) {
  mde::model::OrderBook book;
  AddOrder add{0, "ABC", 1, 100, 10.00, 500, 'B', "F"};
  book.OnMessage(mde::messages::MessageType::AddOrder, &add);

  ModifyOrder modify{0, "ABC", 2, 100, 10.05, 200, 'B'};
  book.OnMessage(mde::messages::MessageType::ModifyOrder, &modify);

  const auto top = book.GetTopOfBook("ABC");
  ASSERT_TRUE(top._bestBid.has_value());
  EXPECT_DOUBLE_EQ(top._bestBid->_price, 10.05);
  EXPECT_EQ(top._bestBid->_aggregateVolume, 200u);
  EXPECT_EQ(book.GetBookBuildFailures(), 0u);
}

TEST(OrderBook, DeleteOrderRemovesLevel) {
  mde::model::OrderBook book;
  AddOrder add{0, "ABC", 1, 100, 10.00, 500, 'B', "F"};
  book.OnMessage(mde::messages::MessageType::AddOrder, &add);

  DeleteOrder del{0, "ABC", 2, 100};
  book.OnMessage(mde::messages::MessageType::DeleteOrder, &del);

  const auto top = book.GetTopOfBook("ABC");
  EXPECT_FALSE(top._bestBid.has_value());
  EXPECT_EQ(book.GetLiveOrderCount(), 0u);
  EXPECT_EQ(book.GetBookBuildFailures(), 0u);
}

TEST(OrderBook, PartialExecutionReducesRestingVolume) {
  mde::model::OrderBook book;
  AddOrder add{0, "ABC", 1, 100, 10.00, 500, 'B', "F"};
  book.OnMessage(mde::messages::MessageType::AddOrder, &add);

  OrderExecution exec{0, "ABC", 2, 100, 900, 10.00, 200, true};
  book.OnMessage(mde::messages::MessageType::OrderExecution, &exec);

  const auto top = book.GetTopOfBook("ABC");
  ASSERT_TRUE(top._bestBid.has_value());
  EXPECT_EQ(top._bestBid->_aggregateVolume, 300u);
  EXPECT_EQ(book.GetLiveOrderCount(), 1u);

  const auto stats = book.GetDailyStats("ABC");
  ASSERT_TRUE(stats._open.has_value());
  EXPECT_DOUBLE_EQ(*stats._open, 10.00);
  EXPECT_EQ(stats._totalVolume, 200u);
}

TEST(OrderBook, FullExecutionRemovesOrder) {
  mde::model::OrderBook book;
  AddOrder add{0, "ABC", 1, 100, 10.00, 500, 'B', "F"};
  book.OnMessage(mde::messages::MessageType::AddOrder, &add);

  OrderExecution exec{0, "ABC", 2, 100, 900, 10.00, 500, true};
  book.OnMessage(mde::messages::MessageType::OrderExecution, &exec);

  const auto top = book.GetTopOfBook("ABC");
  EXPECT_FALSE(top._bestBid.has_value());
  EXPECT_EQ(book.GetLiveOrderCount(), 0u);
}

TEST(OrderBook, ReplaceOrderSwapsIdAndPrice) {
  mde::model::OrderBook book;
  AddOrder add{0, "ABC", 1, 100, 10.00, 500, 'B', "F"};
  book.OnMessage(mde::messages::MessageType::AddOrder, &add);

  ReplaceOrder replace{0, "ABC", 2, 100, 200, 10.25, 400, 'B'};
  book.OnMessage(mde::messages::MessageType::ReplaceOrder, &replace);

  const auto top = book.GetTopOfBook("ABC");
  ASSERT_TRUE(top._bestBid.has_value());
  EXPECT_DOUBLE_EQ(top._bestBid->_price, 10.25);
  EXPECT_EQ(top._bestBid->_aggregateVolume, 400u);
  EXPECT_EQ(book.GetLiveOrderCount(), 1u);
  EXPECT_EQ(book.GetBookBuildFailures(), 0u);
}

TEST(OrderBook, UnknownOrderIdIncrementsBookBuildFailures) {
  mde::model::OrderBook book;

  ModifyOrder modify{0, "ABC", 1, 999, 10.05, 200, 'B'};
  book.OnMessage(mde::messages::MessageType::ModifyOrder, &modify);
  EXPECT_EQ(book.GetBookBuildFailures(), 1u);

  DeleteOrder del{0, "ABC", 2, 999};
  book.OnMessage(mde::messages::MessageType::DeleteOrder, &del);
  EXPECT_EQ(book.GetBookBuildFailures(), 2u);

  OrderExecution exec{0, "ABC", 3, 999, 1, 10.00, 100, true};
  book.OnMessage(mde::messages::MessageType::OrderExecution, &exec);
  EXPECT_EQ(book.GetBookBuildFailures(), 3u);
}

TEST(OrderBook, SecurityStatusHaltResume) {
  mde::model::OrderBook book;
  EXPECT_FALSE(book.IsHalted("ABC"));

  SecurityStatus halt{0,          "ABC",       1,
                      '4',        std::nullopt, std::nullopt,
                      std::nullopt, std::nullopt, std::nullopt,
                      std::nullopt, std::nullopt, std::nullopt};
  book.OnMessage(mde::messages::MessageType::SecurityStatus, &halt);
  EXPECT_TRUE(book.IsHalted("ABC"));

  SecurityStatus resume{0,          "ABC",       2,
                        '5',        std::nullopt, std::nullopt,
                        std::nullopt, std::nullopt, std::nullopt,
                        std::nullopt, std::nullopt, std::nullopt};
  book.OnMessage(mde::messages::MessageType::SecurityStatus, &resume);
  EXPECT_FALSE(book.IsHalted("ABC"));
}

TEST(OrderBook, OrderExecutionCancelRemovesVolumeAndRecomputesExtremes) {
  mde::model::OrderBook book;
  AddOrder add{0, "ABC", 1, 100, 10.00, 500, 'B', "F"};
  book.OnMessage(mde::messages::MessageType::AddOrder, &add);

  // Two executions: the second trades at a higher price, becoming High.
  OrderExecution exec1{0, "ABC", 2, 100, 900, 10.00, 100, true};
  OrderExecution exec2{0, "ABC", 3, 100, 901, 10.50, 100, true};
  book.OnMessage(mde::messages::MessageType::OrderExecution, &exec1);
  book.OnMessage(mde::messages::MessageType::OrderExecution, &exec2);

  auto stats = book.GetDailyStats("ABC");
  ASSERT_TRUE(stats._high.has_value());
  EXPECT_DOUBLE_EQ(*stats._high, 10.50);
  EXPECT_EQ(stats._totalVolume, 200u);

  // Cancelling the trade that set the high must pull High back down and
  // remove its volume -- a plain volume subtraction would leave a stale
  // extreme.
  TradeCancel cancel{0, "ABC", 4, 901};
  book.OnMessage(mde::messages::MessageType::TradeCancel, &cancel);

  stats = book.GetDailyStats("ABC");
  ASSERT_TRUE(stats._high.has_value());
  EXPECT_DOUBLE_EQ(*stats._high, 10.00);
  EXPECT_DOUBLE_EQ(*stats._close, 10.00);
  EXPECT_EQ(stats._totalVolume, 100u);
  EXPECT_EQ(book.GetTradeLedgerFailures(), 0u);
}

TEST(OrderBook, UnknownTradeIdCancelIncrementsTradeLedgerFailures) {
  mde::model::OrderBook book;
  TradeCancel cancel{0, "ABC", 1, 999};
  book.OnMessage(mde::messages::MessageType::TradeCancel, &cancel);
  EXPECT_EQ(book.GetTradeLedgerFailures(), 1u);
}

TEST(OrderBook, NonDisplayedTradeAddsVolumeButNotBook) {
  mde::model::OrderBook book;
  NonDisplayedTrade trade{0, "ABC", 1, 55, 10.25, 300, true};
  book.OnMessage(mde::messages::MessageType::NonDisplayedTrade, &trade);

  EXPECT_FALSE(book.GetTopOfBook("ABC")._bestBid.has_value());
  EXPECT_EQ(book.GetLiveOrderCount(), 0u);

  const auto stats = book.GetDailyStats("ABC");
  ASSERT_TRUE(stats._close.has_value());
  EXPECT_DOUBLE_EQ(*stats._close, 10.25);
  EXPECT_EQ(stats._totalVolume, 300u);
}

TEST(OrderBook, CrossTradeAddsVolumeButNotBook) {
  mde::model::OrderBook book;
  CrossTrade cross{0, "ABC", 1, 777, 10.30, 5000, 'O'};
  book.OnMessage(mde::messages::MessageType::CrossTrade, &cross);

  EXPECT_FALSE(book.GetTopOfBook("ABC")._bestAsk.has_value());
  EXPECT_EQ(book.GetLiveOrderCount(), 0u);

  const auto stats = book.GetDailyStats("ABC");
  EXPECT_EQ(stats._totalVolume, 5000u);
}

TEST(OrderBook, CrossCorrectionAdjustsVolumeOnly) {
  mde::model::OrderBook book;
  CrossTrade cross{0, "ABC", 1, 777, 10.30, 5000, 'O'};
  book.OnMessage(mde::messages::MessageType::CrossTrade, &cross);

  CrossCorrection correction{0, "ABC", 2, 777, 4500};
  book.OnMessage(mde::messages::MessageType::CrossCorrection, &correction);

  const auto stats = book.GetDailyStats("ABC");
  EXPECT_EQ(stats._totalVolume, 4500u);
  ASSERT_TRUE(stats._close.has_value());
  EXPECT_DOUBLE_EQ(*stats._close, 10.30); // price untouched by correction
  EXPECT_EQ(book.GetTradeLedgerFailures(), 0u);
}

TEST(OrderBook, UnknownCrossIdCorrectionIncrementsTradeLedgerFailures) {
  mde::model::OrderBook book;
  CrossCorrection correction{0, "ABC", 1, 999, 100};
  book.OnMessage(mde::messages::MessageType::CrossCorrection, &correction);
  EXPECT_EQ(book.GetTradeLedgerFailures(), 1u);
}

TEST(OrderBook, AddOrderRefreshBehavesLikeAddOrder) {
  mde::model::OrderBook book;
  AddOrderRefresh refresh{0, "ABC", 1, 100, 10.00, 500, 'B', "F"};
  book.OnMessage(mde::messages::MessageType::AddOrderRefresh, &refresh);

  const auto top = book.GetTopOfBook("ABC");
  ASSERT_TRUE(top._bestBid.has_value());
  EXPECT_DOUBLE_EQ(top._bestBid->_price, 10.00);
  EXPECT_EQ(top._bestBid->_aggregateVolume, 500u);
  EXPECT_EQ(book.GetLiveOrderCount(), 1u);
}

TEST(OrderBook, ImbalanceIsStoredPerSymbol) {
  mde::model::OrderBook book;
  EXPECT_FALSE(book.GetImbalance("ABC").has_value());

  Imbalance imbalance{
      ._nanosSinceMidnight = 0,
      ._symbol = "ABC",
      ._symbolSeqNum = 1,
      ._referencePrice = 10.50,
      ._pairedQty = 1000,
      ._totalImbalanceQty = 500,
      ._marketImbalanceQty = 0,
      ._auctionTime = 930,
      ._auctionType = 'M',
      ._imbalanceSide = 'B',
      ._continuousBookClearingPrice = 10.45,
      ._auctionInterestClearingPrice = 10.50,
      ._ssrFilingPrice = 0,
      ._indicativeMatchPrice = 10.48,
      ._upperCollar = 10.60,
      ._lowerCollar = 10.40,
      ._auctionStatus = 1,
      ._freezeStatus = 0,
      ._numExtensions = 0,
      ._unpairedQuantity = 0,
      ._unpairedSide = ' ',
      ._significantImbalance = ' ',
  };
  book.OnMessage(mde::messages::MessageType::Imbalance, &imbalance);

  const auto stored = book.GetImbalance("ABC");
  ASSERT_TRUE(stored.has_value());
  EXPECT_DOUBLE_EQ(stored->_referencePrice, 10.50);
  EXPECT_EQ(stored->_totalImbalanceQty, 500u);
  EXPECT_EQ(stored->_imbalanceSide, 'B');
}
