#include "orderbook/order_book.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace orderbook;

namespace {
int failures = 0;

void check(bool condition, const char* expression, const char* test) {
  if (!condition) {
    std::cerr << test << ": assertion failed: " << expression << '\n';
    ++failures;
  }
}

#define CHECK(expr) check(static_cast<bool>(expr), #expr, __func__)

template <typename Exception, typename Function>
void expect_throw(Function function, const char* test) {
  try {
    function();
    std::cerr << test << ": expected exception was not thrown\n";
    ++failures;
  } catch (const Exception&) {
  } catch (...) {
    std::cerr << test << ": unexpected exception type\n";
    ++failures;
  }
}

void empty_book_queries() {
  OrderBook book;
  CHECK(!book.best_bid());
  CHECK(!book.best_ask());
  CHECK(!book.spread());
  CHECK(book.order_count() == 0);
  CHECK(book.snapshot().bids.empty());
  CHECK(book.snapshot().asks.empty());
}

void non_crossing_and_queries() {
  OrderBook book;
  CHECK(book.add_limit_order(1, Side::buy, 99, 4).empty());
  CHECK(book.add_limit_order(2, Side::sell, 105, 3).empty());
  CHECK(book.best_bid() == 99);
  CHECK(book.best_ask() == 105);
  CHECK(book.spread() == 6);
  CHECK(book.order_count() == 2);
}

void exact_and_partial_fills() {
  OrderBook book;
  (void)book.add_limit_order(1, Side::sell, 100, 10);
  auto trades = book.add_limit_order(2, Side::buy, 100, 4);
  CHECK(trades.size() == 1);
  CHECK(trades[0].price == 100 && trades[0].quantity == 4);
  CHECK(trades[0].buy_order_id == 2 && trades[0].sell_order_id == 1);
  CHECK(book.order_count() == 1);
  CHECK(book.snapshot().asks[0].quantity == 6);

  trades = book.add_limit_order(3, Side::buy, 100, 6);
  CHECK(trades.size() == 1 && trades[0].quantity == 6);
  CHECK(book.order_count() == 0);
}

void price_priority_and_multiple_levels() {
  OrderBook book;
  (void)book.add_limit_order(1, Side::sell, 102, 10);
  (void)book.add_limit_order(2, Side::sell, 100, 2);
  (void)book.add_limit_order(3, Side::sell, 101, 3);
  auto trades = book.add_limit_order(4, Side::buy, 105, 6);
  CHECK(trades.size() == 3);
  CHECK(trades[0].price == 100 && trades[0].quantity == 2);
  CHECK(trades[1].price == 101 && trades[1].quantity == 3);
  CHECK(trades[2].price == 102 && trades[2].quantity == 1);
  CHECK(book.snapshot().asks[0].quantity == 9);

  OrderBook bids;
  (void)bids.add_limit_order(10, Side::buy, 99, 5);
  (void)bids.add_limit_order(11, Side::buy, 101, 2);
  (void)bids.add_limit_order(12, Side::buy, 100, 3);
  trades = bids.add_limit_order(13, Side::sell, 98, 4);
  CHECK(trades.size() == 2);
  CHECK(trades[0].price == 101 && trades[0].quantity == 2);
  CHECK(trades[1].price == 100 && trades[1].quantity == 2);
}

void fifo_at_same_price() {
  OrderBook book;
  (void)book.add_limit_order(1, Side::sell, 100, 2);
  (void)book.add_limit_order(2, Side::sell, 100, 3);
  auto trades = book.add_limit_order(3, Side::buy, 100, 4);
  CHECK(trades.size() == 2);
  CHECK(trades[0].sell_order_id == 1 && trades[0].quantity == 2);
  CHECK(trades[1].sell_order_id == 2 && trades[1].quantity == 2);
  CHECK(book.snapshot().asks[0].order_count == 1);
}

void cancel_and_remove_empty_level() {
  OrderBook book;
  (void)book.add_limit_order(1, Side::buy, 101, 3);
  (void)book.add_limit_order(2, Side::buy, 101, 4);
  (void)book.add_limit_order(3, Side::buy, 101, 5);
  (void)book.add_limit_order(4, Side::buy, 99, 8);
  CHECK(book.cancel_order(2));  // Remove the middle order while preserving FIFO.
  CHECK(book.snapshot().bids[0].quantity == 8);
  auto trades = book.add_limit_order(5, Side::sell, 101, 8);
  CHECK(trades.size() == 2);
  CHECK(trades[0].buy_order_id == 1 && trades[0].quantity == 3);
  CHECK(trades[1].buy_order_id == 3 && trades[1].quantity == 5);
  CHECK(!book.cancel_order(1));
  CHECK(book.best_bid() == 99);
  CHECK(!book.cancel_order(99));
  CHECK(book.order_count() == 1);
}

void market_orders_and_price_rule() {
  OrderBook book;
  (void)book.add_limit_order(1, Side::sell, 100, 2);
  (void)book.add_limit_order(2, Side::sell, 101, 3);
  auto trades = book.add_market_order(3, Side::buy, 7);
  CHECK(trades.size() == 2);
  CHECK(trades[0].price == 100 && trades[0].quantity == 2);
  CHECK(trades[1].price == 101 && trades[1].quantity == 3);
  CHECK(book.order_count() == 0);

  (void)book.add_limit_order(4, Side::buy, 99, 2);
  (void)book.add_limit_order(5, Side::buy, 98, 3);
  trades = book.add_market_order(6, Side::sell, 4);
  CHECK(trades.size() == 2);
  CHECK(trades[0].price == 99 && trades[0].quantity == 2);
  CHECK(trades[1].price == 98 && trades[1].quantity == 2);
  CHECK(book.snapshot().bids[0].quantity == 1);
}

void invalid_orders_and_id_reuse() {
  OrderBook book;
  expect_throw<std::invalid_argument>(
      [&] { (void)book.add_limit_order(1, Side::buy, 0, 1); }, __func__);
  expect_throw<std::invalid_argument>(
      [&] { (void)book.add_limit_order(1, Side::buy, 1, 0); }, __func__);
  (void)book.add_limit_order(1, Side::buy, 10, 1);
  expect_throw<std::invalid_argument>(
      [&] { (void)book.add_limit_order(1, Side::sell, 11, 1); }, __func__);
  CHECK(book.cancel_order(1));
  expect_throw<std::invalid_argument>(
      [&] { (void)book.add_market_order(1, Side::buy, 1); }, __func__);
  expect_throw<std::invalid_argument>(
      [&] { (void)book.add_market_order(2, Side::sell, 0); }, __func__);
}

void snapshots_respect_depth_and_aggregate() {
  OrderBook book;
  (void)book.add_limit_order(1, Side::buy, 102, 2);
  (void)book.add_limit_order(2, Side::buy, 102, 3);
  (void)book.add_limit_order(3, Side::buy, 101, 7);
  auto snapshot = book.snapshot(1);
  CHECK(snapshot.bids.size() == 1);
  CHECK(snapshot.bids[0].price == 102);
  CHECK(snapshot.bids[0].quantity == 5);
  CHECK(snapshot.bids[0].order_count == 2);
}

}  // namespace

int main() {
  const std::vector<std::pair<const char*, std::function<void()>>> tests{
      {"empty_book_queries", empty_book_queries},
      {"non_crossing_and_queries", non_crossing_and_queries},
      {"exact_and_partial_fills", exact_and_partial_fills},
      {"price_priority_and_multiple_levels", price_priority_and_multiple_levels},
      {"fifo_at_same_price", fifo_at_same_price},
      {"cancel_and_remove_empty_level", cancel_and_remove_empty_level},
      {"market_orders_and_price_rule", market_orders_and_price_rule},
      {"invalid_orders_and_id_reuse", invalid_orders_and_id_reuse},
      {"snapshots_respect_depth_and_aggregate", snapshots_respect_depth_and_aggregate},
  };
  for (const auto& [name, test] : tests) {
    const int failures_before = failures;
    test();
    std::cout << (failures == failures_before ? "PASS " : "FAIL ") << name << '\n';
  }
  if (failures != 0) {
    std::cerr << failures << " assertion(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << tests.size() << " tests passed\n";
  return EXIT_SUCCESS;
}
