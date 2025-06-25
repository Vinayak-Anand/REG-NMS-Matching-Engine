#pragma once
#include "Order.h"
#include <map>
#include <vector>
#include <shared_mutex>
#include <deque>

// L2 Market Data Structure
struct L2Update {
    std::string symbol;
    long long timestamp;
    std::vector<std::pair<double, double>> bids;  // [price, quantity]
    std::vector<std::pair<double, double>> asks;  // [price, quantity]
};

class OrderBook {
public:
    OrderBook() = default;
    
    void addOrder(Order* order);
    void removeOrder(Order* order);
    
    // BBO calculation - core REG NMS requirement
    std::pair<double, double> bestBidOffer() const;
    
    // Market data for L2 feed
    std::vector<std::pair<double, double>> topBids(int N = 10) const;
    std::vector<std::pair<double, double>> topAsks(int N = 10) const;
    
    // Generate L2 market data update
    L2Update generateL2Update(const std::string& symbol, int depth = 10) const;
    
    // Thread-safe access for matching engine
    std::unique_lock<std::shared_mutex> acquireLock();
    
    // Direct access to order book sides (for matching engine)
    auto& getBids() { return bids_; }
    auto& getAsks() { return asks_; }
    const auto& getBids() const { return bids_; }
    const auto& getAsks() const { return asks_; }
    
    // Check if order would trade through (violate price-time priority)
    bool wouldTradeThrough(const Order& order) const;
    
private:
    mutable std::shared_mutex mu_;
    
    // Price-time priority: map<price, deque<Order*>>
    // Bids: higher prices first (reverse order)
    std::map<double, std::deque<Order*>, std::greater<double>> bids_;
    // Asks: lower prices first (normal order)
    std::map<double, std::deque<Order*>, std::less<double>> asks_;
};