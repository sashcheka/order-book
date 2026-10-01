#include "orderbook/order_book.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

using namespace orderbook;

TEST(OrderBookTest, EmptyBookQueries) {
  OrderBook book;
  EXPECT_FALSE(book.best_bid());
  EXPECT_FALSE(book.best_ask());
  EXPECT_FALSE(book.spread());
  EXPECT_EQ(book.order_count(), 0);
  EXPECT_TRUE(book.snapshot().bids.empty());
  EXPECT_TRUE(book.snapshot().asks.empty());
}

TEST(OrderBookTest, NonCrossingOrdersAndQueries) {
  OrderBook book;
  EXPECT_TRUE(book.add_limit_order(1, Side::buy, 99, 4).empty());
  EXPECT_TRUE(book.add_limit_order(2, Side::sell, 105, 3).empty());
  EXPECT_EQ(book.best_bid(), 99);
  EXPECT_EQ(book.best_ask(), 105);
  EXPECT_EQ(book.spread(), 6);
  EXPECT_EQ(book.order_count(), 2);
}

TEST(OrderBookTest, ExactAndPartialFills) {
  OrderBook book;
  (void)book.add_limit_order(1, Side::sell, 100, 10);
  auto trades = book.add_limit_order(2, Side::buy, 100, 4);
  ASSERT_EQ(trades.size(), 1);
  EXPECT_EQ(trades[0].price, 100);
  EXPECT_EQ(trades[0].quantity, 4);
  EXPECT_EQ(trades[0].buy_order_id, 2);
  EXPECT_EQ(trades[0].sell_order_id, 1);
  EXPECT_EQ(book.order_count(), 1);
  EXPECT_EQ(book.snapshot().asks[0].quantity, 6);

  trades = book.add_limit_order(3, Side::buy, 100, 6);
  ASSERT_EQ(trades.size(), 1);
  EXPECT_EQ(trades[0].quantity, 6);
  EXPECT_EQ(book.order_count(), 0);
}

TEST(OrderBookTest, PricePriorityAcrossMultipleLevels) {
  OrderBook book;
  (void)book.add_limit_order(1, Side::sell, 102, 10);
  (void)book.add_limit_order(2, Side::sell, 100, 2);
  (void)book.add_limit_order(3, Side::sell, 101, 3);
  auto trades = book.add_limit_order(4, Side::buy, 105, 6);
  ASSERT_EQ(trades.size(), 3);
  EXPECT_EQ(trades[0].price, 100);
  EXPECT_EQ(trades[0].quantity, 2);
  EXPECT_EQ(trades[1].price, 101);
  EXPECT_EQ(trades[1].quantity, 3);
  EXPECT_EQ(trades[2].price, 102);
  EXPECT_EQ(trades[2].quantity, 1);
  EXPECT_EQ(book.snapshot().asks[0].quantity, 9);

  OrderBook bids;
  (void)bids.add_limit_order(10, Side::buy, 99, 5);
  (void)bids.add_limit_order(11, Side::buy, 101, 2);
  (void)bids.add_limit_order(12, Side::buy, 100, 3);
  trades = bids.add_limit_order(13, Side::sell, 98, 4);
  ASSERT_EQ(trades.size(), 2);
  EXPECT_EQ(trades[0].price, 101);
  EXPECT_EQ(trades[0].quantity, 2);
  EXPECT_EQ(trades[1].price, 100);
  EXPECT_EQ(trades[1].quantity, 2);
}

TEST(OrderBookTest, FIFOAtSamePrice) {
  OrderBook book;
  (void)book.add_limit_order(1, Side::sell, 100, 2);
  (void)book.add_limit_order(2, Side::sell, 100, 3);
  auto trades = book.add_limit_order(3, Side::buy, 100, 4);
  ASSERT_EQ(trades.size(), 2);
  EXPECT_EQ(trades[0].sell_order_id, 1);
  EXPECT_EQ(trades[0].quantity, 2);
  EXPECT_EQ(trades[1].sell_order_id, 2);
  EXPECT_EQ(trades[1].quantity, 2);
  EXPECT_EQ(book.snapshot().asks[0].order_count, 1);
}

TEST(OrderBookTest, CancelOrderAndRemoveEmptyLevel) {
  OrderBook book;
  (void)book.add_limit_order(1, Side::buy, 101, 3);
  (void)book.add_limit_order(2, Side::buy, 101, 4);
  (void)book.add_limit_order(3, Side::buy, 101, 5);
  (void)book.add_limit_order(4, Side::buy, 99, 8);
  EXPECT_TRUE(book.cancel_order(2));
  EXPECT_EQ(book.snapshot().bids[0].quantity, 8);
  auto trades = book.add_limit_order(5, Side::sell, 101, 8);
  ASSERT_EQ(trades.size(), 2);
  EXPECT_EQ(trades[0].buy_order_id, 1);
  EXPECT_EQ(trades[0].quantity, 3);
  EXPECT_EQ(trades[1].buy_order_id, 3);
  EXPECT_EQ(trades[1].quantity, 5);
  EXPECT_FALSE(book.cancel_order(1));
  EXPECT_EQ(book.best_bid(), 99);
  EXPECT_FALSE(book.cancel_order(99));
  EXPECT_EQ(book.order_count(), 1);
}

TEST(OrderBookTest, MarketOrdersAndRestingPriceRule) {
  OrderBook book;
  (void)book.add_limit_order(1, Side::sell, 100, 2);
  (void)book.add_limit_order(2, Side::sell, 101, 3);
  auto trades = book.add_market_order(3, Side::buy, 7);
  ASSERT_EQ(trades.size(), 2);
  EXPECT_EQ(trades[0].price, 100);
  EXPECT_EQ(trades[0].quantity, 2);
  EXPECT_EQ(trades[1].price, 101);
  EXPECT_EQ(trades[1].quantity, 3);
  EXPECT_EQ(book.order_count(), 0);

  (void)book.add_limit_order(4, Side::buy, 99, 2);
  (void)book.add_limit_order(5, Side::buy, 98, 3);
  trades = book.add_market_order(6, Side::sell, 4);
  ASSERT_EQ(trades.size(), 2);
  EXPECT_EQ(trades[0].price, 99);
  EXPECT_EQ(trades[0].quantity, 2);
  EXPECT_EQ(trades[1].price, 98);
  EXPECT_EQ(trades[1].quantity, 2);
  EXPECT_EQ(book.snapshot().bids[0].quantity, 1);
}

TEST(OrderBookTest, InvalidOrdersAndOrderIdReuse) {
  OrderBook book;
  EXPECT_THROW((void)book.add_limit_order(1, Side::buy, 0, 1),
               std::invalid_argument);
  EXPECT_THROW((void)book.add_limit_order(1, Side::buy, 1, 0),
               std::invalid_argument);
  (void)book.add_limit_order(1, Side::buy, 10, 1);
  EXPECT_THROW((void)book.add_limit_order(1, Side::sell, 11, 1),
               std::invalid_argument);
  EXPECT_TRUE(book.cancel_order(1));
  EXPECT_THROW((void)book.add_market_order(1, Side::buy, 1),
               std::invalid_argument);
  EXPECT_THROW((void)book.add_market_order(2, Side::sell, 0),
               std::invalid_argument);
}

TEST(OrderBookTest, SnapshotsRespectDepthAndAggregateLevels) {
  OrderBook book;
  (void)book.add_limit_order(1, Side::buy, 102, 2);
  (void)book.add_limit_order(2, Side::buy, 102, 3);
  (void)book.add_limit_order(3, Side::buy, 101, 7);
  const auto snapshot = book.snapshot(1);
  ASSERT_EQ(snapshot.bids.size(), 1);
  EXPECT_EQ(snapshot.bids[0].price, 102);
  EXPECT_EQ(snapshot.bids[0].quantity, 5);
  EXPECT_EQ(snapshot.bids[0].order_count, 2);
}

TEST(OrderBookTest, AggregateQuantityOverflowIsRejectedWithoutMutation) {
  const auto verify_side = [](Side side) {
    OrderBook book;
    const auto max_quantity = std::numeric_limits<Quantity>::max();
    (void)book.add_limit_order(1, side, 100, max_quantity);
    EXPECT_THROW((void)book.add_limit_order(2, side, 100, 1),
                 std::overflow_error);

    const auto snapshot = book.snapshot();
    const auto& levels = side == Side::buy ? snapshot.bids : snapshot.asks;
    ASSERT_EQ(levels.size(), 1);
    EXPECT_EQ(levels[0].quantity, max_quantity);
    EXPECT_EQ(book.order_count(), 1);

    EXPECT_TRUE(book.cancel_order(1));
    (void)book.add_limit_order(2, side, 100, 1);
    EXPECT_EQ(book.order_count(), 1);
    EXPECT_EQ(book.snapshot().bids.size() + book.snapshot().asks.size(), 1);
  };

  verify_side(Side::buy);
  verify_side(Side::sell);
}
