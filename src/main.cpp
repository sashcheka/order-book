#include "orderbook/order_book.hpp"

#include <iostream>

int main() {
  orderbook::OrderBook book;
  (void)book.add_limit_order(1, orderbook::Side::sell, 100, 10);
  const auto trades = book.add_limit_order(2, orderbook::Side::buy, 105, 4);
  for (const auto& trade : trades) {
    std::cout << "TRADE " << trade.quantity << " @ " << trade.price << '\n';
  }
  for (const auto& level : book.snapshot().asks) {
    std::cout << "ASK " << level.quantity << " @ " << level.price << '\n';
  }
  return 0;
}
