#pragma once

#include <map>
#include <deque>
#include "Order.h"
#include <json.hpp>

class OrderBook {
public:
    void addBid(double price, long volume);

    void addAsk(double price, long volume);

    double bestBid() const;

    double bestAsk() const;

    bool matchOrders();

    void setOrderBook(const nlohmann::json& json);

    std::vector<Order> getBids() const;
    
    std::vector<Order> getAsks() const;

private:
    std::map<double, std::deque<Order>> bids; // price -> list of orders (buy)
    std::map<double, std::deque<Order>> asks; // price -> list of orders (sell)
};
