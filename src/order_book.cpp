#include "orderbook/order_book.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace orderbook {
namespace {

bool is_valid_side(Side side) {
  return side == Side::buy || side == Side::sell;
}

}  // namespace

void OrderBook::validate_new_order(OrderId id, Side side, Quantity quantity) {
  if (!is_valid_side(side)) {
    throw std::invalid_argument("invalid order side");
  }
  if (quantity == 0) {
    throw std::invalid_argument("order quantity must be positive");
  }
  if (!used_ids_.insert(id).second) {
    throw std::invalid_argument("order ID has already been used");
  }
}

std::vector<Trade> OrderBook::add_limit_order(OrderId id, Side side, Price price,
                                             Quantity quantity) {
  if (price <= 0) {
    throw std::invalid_argument("limit price must be positive");
  }
  validate_new_order(id, side, quantity);
  Order incoming{id, side, OrderType::limit, price, quantity, quantity,
                 next_sequence_++};
  auto trades = match(incoming);
  if (incoming.remaining_quantity > 0) {
    rest(std::move(incoming));
  }
  return trades;
}

std::vector<Trade> OrderBook::add_market_order(OrderId id, Side side,
                                              Quantity quantity) {
  validate_new_order(id, side, quantity);
  Order incoming{id, side, OrderType::market, 0, quantity, quantity,
                 next_sequence_++};
  return match(incoming);
}

std::vector<Trade> OrderBook::match(Order& incoming) {
  std::vector<Trade> trades;
  const bool is_buy = incoming.side == Side::buy;

  while (incoming.remaining_quantity > 0) {
    Price resting_price{};
    Order* resting_order{};
    PriceLevel* level{};

    if (is_buy) {
      if (asks_.empty()) break;
      auto it = asks_.begin();
      resting_price = it->first;
      if (incoming.type == OrderType::limit && incoming.price < resting_price) break;
      level = &it->second;
      resting_order = &level->orders.front();
    } else {
      if (bids_.empty()) break;
      auto it = bids_.begin();
      resting_price = it->first;
      if (incoming.type == OrderType::limit && incoming.price > resting_price) break;
      level = &it->second;
      resting_order = &level->orders.front();
    }

    const Quantity fill = std::min(incoming.remaining_quantity,
                                   resting_order->remaining_quantity);
    trades.push_back(Trade{next_trade_id_++,
                           is_buy ? incoming.id : resting_order->id,
                           is_buy ? resting_order->id : incoming.id,
                           resting_price, fill});
    incoming.remaining_quantity -= fill;
    resting_order->remaining_quantity -= fill;
    level->total_quantity -= fill;

    if (resting_order->remaining_quantity == 0) {
      const OrderId filled_id = resting_order->id;
      if (is_buy) {
        asks_.begin()->second.orders.pop_front();
        locations_.erase(filled_id);
        if (asks_.begin()->second.orders.empty()) asks_.erase(asks_.begin());
      } else {
        bids_.begin()->second.orders.pop_front();
        locations_.erase(filled_id);
        if (bids_.begin()->second.orders.empty()) bids_.erase(bids_.begin());
      }
    }
  }
  return trades;
}

void OrderBook::rest(Order&& order) {
  if (order.side == Side::buy) {
    auto& level = bids_[order.price];
    level.orders.push_back(std::move(order));
    auto it = std::prev(level.orders.end());
    level.total_quantity += it->remaining_quantity;
    locations_.emplace(it->id, Location{Side::buy, it->price, it});
  } else {
    auto& level = asks_[order.price];
    level.orders.push_back(std::move(order));
    auto it = std::prev(level.orders.end());
    level.total_quantity += it->remaining_quantity;
    locations_.emplace(it->id, Location{Side::sell, it->price, it});
  }
}

void OrderBook::erase_order(Location& location) {
  if (location.side == Side::buy) {
    auto level = bids_.find(location.price);
    level->second.total_quantity -= location.order->remaining_quantity;
    level->second.orders.erase(location.order);
    if (level->second.orders.empty()) bids_.erase(level);
  } else {
    auto level = asks_.find(location.price);
    level->second.total_quantity -= location.order->remaining_quantity;
    level->second.orders.erase(location.order);
    if (level->second.orders.empty()) asks_.erase(level);
  }
}

bool OrderBook::cancel_order(OrderId id) {
  auto location = locations_.find(id);
  if (location == locations_.end()) return false;
  erase_order(location->second);
  locations_.erase(location);
  return true;
}

std::optional<Price> OrderBook::best_bid() const {
  if (bids_.empty()) return std::nullopt;
  return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
  if (asks_.empty()) return std::nullopt;
  return asks_.begin()->first;
}

std::optional<Price> OrderBook::spread() const {
  const auto bid = best_bid();
  const auto ask = best_ask();
  if (!bid || !ask) return std::nullopt;
  return *ask - *bid;
}

BookSnapshot OrderBook::snapshot(std::size_t depth) const {
  BookSnapshot result;
  const auto append = [depth](const auto& side, auto& output) {
    for (const auto& [price, level] : side) {
      if (depth != 0 && output.size() >= depth) break;
      output.push_back(PriceLevelSnapshot{price, level.total_quantity,
                                          level.orders.size()});
    }
  };
  append(bids_, result.bids);
  append(asks_, result.asks);
  return result;
}

}  // namespace orderbook
