#include "trading/OrderBook.h"
#include <algorithm>
#include <json.hpp>

void OrderBook::addBid(double price, long volume) {
    bids[price].emplace_back(price, volume);
}

void OrderBook::addAsk(double price, long volume) {
    asks[price].emplace_back(price, volume);
}

double OrderBook::bestBid() const {
    return bids.empty() ? -1.0 : bids.rbegin()->first;
}

double OrderBook::bestAsk() const {
    return asks.empty() ? -1.0 : asks.begin()->first;
}

bool OrderBook::matchOrders() {
    while (!bids.empty() && !asks.empty()) {
        auto bestBidIt = std::prev(bids.end());
        auto bestAskIt = asks.begin();

        if (bestBidIt->first < bestAskIt->first) break;

        auto& bidQueue = bestBidIt->second;
        auto& askQueue = bestAskIt->second;

        Order& bid = bidQueue.front();
        Order& ask = askQueue.front();

        long tradeVolume = std::min(bid.volume, ask.volume);
        bid.volume -= tradeVolume;
        ask.volume -= tradeVolume;

        if (bid.volume == 0) bidQueue.pop_front();
        if (ask.volume == 0) askQueue.pop_front();

        if (bidQueue.empty()) bids.erase(bestBidIt);
        if (askQueue.empty()) asks.erase(bestAskIt);

        return true; // one match done
    }
    return false;
}

void OrderBook::setOrderBook(const nlohmann::json& json) {
    bids.clear();
    asks.clear();

    if (json.contains("bids") && json["bids"].is_array()) {
        for (const auto& entry : json["bids"]) {
            if (entry.size() >= 2) {
                double price = entry[0].get<double>();
                long volume = entry[1].get<long>();
                bids[price].emplace_back(price, volume);
            }
        }
    }

    if (json.contains("asks") && json["asks"].is_array()) {
        for (const auto& entry : json["asks"]) {
            if (entry.size() >= 2) {
                double price = entry[0].get<double>();
                long volume = entry[1].get<long>();
                asks[price].emplace_back(price, volume);
            }
        }
    }
}

std::vector<Order> OrderBook::getBids() const {
    std::vector<Order> allBids;
    for (auto it = bids.rbegin(); it != bids.rend(); ++it) { // Highest price first
        for (const auto& order : it->second) {
            allBids.push_back(order);
        }
    }
    return allBids;
}

std::vector<Order> OrderBook::getAsks() const {
    std::vector<Order> allAsks;
    for (const auto& [price, queue] : asks) { // Lowest price first
        for (const auto& order : queue) {
            allAsks.push_back(order);
        }
    }
    return allAsks;
}

