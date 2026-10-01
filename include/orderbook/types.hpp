#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace orderbook {

using OrderId = std::uint64_t;
using TradeId = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::uint64_t;

enum class Side { buy, sell };
enum class OrderType { limit, market };

struct Order {
  OrderId id{};
  Side side{};
  OrderType type{};
  Price price{};
  Quantity original_quantity{};
  Quantity remaining_quantity{};
  std::uint64_t sequence{};
};

struct Trade {
  TradeId id{};
  OrderId buy_order_id{};
  OrderId sell_order_id{};
  Price price{};
  Quantity quantity{};
};

struct PriceLevelSnapshot {
  Price price{};
  Quantity quantity{};
  std::size_t order_count{};
};

struct BookSnapshot {
  std::vector<PriceLevelSnapshot> bids;
  std::vector<PriceLevelSnapshot> asks;
};

}  // namespace orderbook
