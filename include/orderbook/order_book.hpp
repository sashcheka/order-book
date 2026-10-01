#pragma once

#include "orderbook/types.hpp"

#include <functional>
#include <list>
#include <map>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace orderbook {

class OrderBook {
 public:
  // IDs remain reserved after an order is canceled or fully filled.
  [[nodiscard]] std::vector<Trade> add_limit_order(OrderId id, Side side,
                                                   Price price, Quantity quantity);
  // Unfilled market quantity is canceled and never rests in the book.
  [[nodiscard]] std::vector<Trade> add_market_order(OrderId id, Side side,
                                                    Quantity quantity);
  [[nodiscard]] bool cancel_order(OrderId id);

  [[nodiscard]] std::optional<Price> best_bid() const;
  [[nodiscard]] std::optional<Price> best_ask() const;
  [[nodiscard]] std::optional<Price> spread() const;
  // depth == 0 returns every level; otherwise returns up to depth levels per side.
  [[nodiscard]] BookSnapshot snapshot(std::size_t depth = 0) const;
  [[nodiscard]] std::size_t order_count() const noexcept { return locations_.size(); }

 private:
  using Orders = std::list<Order>;
  struct PriceLevel {
    Orders orders;
    Quantity total_quantity{};
  };
  using Bids = std::map<Price, PriceLevel, std::greater<Price>>;
  using Asks = std::map<Price, PriceLevel, std::less<Price>>;
  struct Location {
    Side side{};
    Price price{};
    Orders::iterator order;
  };

  Bids bids_;
  Asks asks_;
  std::unordered_map<OrderId, Location> locations_;
  std::unordered_set<OrderId> used_ids_;
  std::uint64_t next_sequence_{1};
  TradeId next_trade_id_{1};

  void validate_new_order(OrderId id, Side side, Quantity quantity);
  [[nodiscard]] std::vector<Trade> match(Order& incoming);
  void rest(Order&& order);
  void erase_order(Location& location);
};

}  // namespace orderbook
