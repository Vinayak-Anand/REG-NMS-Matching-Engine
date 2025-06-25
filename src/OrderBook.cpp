#include "OrderBook.h"
#include <algorithm>
#include <chrono>

void OrderBook::addOrder(Order* o) {
    std::unique_lock lock(mu_);
    if (o->side == Side::BUY) {
        bids_[o->price].push_back(o);
    } else {
        asks_[o->price].push_back(o);
    }
}

void OrderBook::removeOrder(Order* o) {
    std::unique_lock lock(mu_);
    if (o->side == Side::BUY) {
        auto it = bids_.find(o->price);
        if (it != bids_.end()) {
            auto& dq = it->second;
            dq.erase(std::remove(dq.begin(), dq.end(), o), dq.end());
            if (dq.empty()) bids_.erase(it);
        }
    } else {
        auto it = asks_.find(o->price);
        if (it != asks_.end()) {
            auto& dq = it->second;
            dq.erase(std::remove(dq.begin(), dq.end(), o), dq.end());
            if (dq.empty()) asks_.erase(it);
        }
    }
}

std::pair<double,double> OrderBook::bestBidOffer() const {
    std::shared_lock lock(mu_);
    double bid = bids_.empty() ? 0.0 : bids_.begin()->first;
    double ask = asks_.empty() ? 0.0 : asks_.begin()->first;
    return {bid, ask};
}

std::vector<std::pair<double,double>> OrderBook::topBids(int N) const {
    std::shared_lock lock(mu_);
    std::vector<std::pair<double,double>> res;
    int count = 0;
    for (auto& [p, dq] : bids_) {
        if (count++ >= N) break;
        double qty = 0;
        for (auto* o : dq) qty += o->remaining();
        if (qty > 0) {  // Only include levels with remaining quantity
            res.emplace_back(p, qty);
        }
    }
    return res;
}

std::vector<std::pair<double,double>> OrderBook::topAsks(int N) const {
    std::shared_lock lock(mu_);
    std::vector<std::pair<double,double>> res;
    int count = 0;
    for (auto& [p, dq] : asks_) {
        if (count++ >= N) break;
        double qty = 0;
        for (auto* o : dq) qty += o->remaining();
        if (qty > 0) {  // Only include levels with remaining quantity
            res.emplace_back(p, qty);
        }
    }
    return res;
}

L2Update OrderBook::generateL2Update(const std::string& symbol, int depth) const {
    L2Update update;
    update.symbol = symbol;
    update.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
    update.bids = topBids(depth);
    update.asks = topAsks(depth);
    return update;
}

std::unique_lock<std::shared_mutex> OrderBook::acquireLock() {
    return std::unique_lock<std::shared_mutex>(mu_);
}

bool OrderBook::wouldTradeThrough(const Order& order) const {
    std::shared_lock lock(mu_);
    auto [bestBid, bestAsk] = bestBidOffer();
    
    // Check for trade-through violations
    if (order.side == Side::BUY && order.type == OrderType::LIMIT) {
        // Buy limit order should not be priced above the best ask
        return bestAsk > 0 && order.price > bestAsk;
    } else if (order.side == Side::SELL && order.type == OrderType::LIMIT) {
        // Sell limit order should not be priced below the best bid
        return bestBid > 0 && order.price < bestBid;
    }
    return false;
}