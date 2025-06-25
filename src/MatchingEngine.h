#pragma once
#include "Order.h"
#include "OrderBook.h"
#include "TradeExecutionFeed.h"
#include "EventFeed.h"
#include "FeeCalculator.h"  
#include "PersistenceManager.h"
#include <unordered_map>
#include <shared_mutex>

enum class OrderResult {
    ACCEPTED,
    REJECTED_INVALID_PARAMS,
    REJECTED_TRADE_THROUGH,
    REJECTED_FOK_UNFILLABLE,
    PARTIALLY_FILLED,
    COMPLETELY_FILLED
};

struct OrderResponse {
    OrderResult result;
    std::string message;
    double filledQuantity = 0.0;
    std::vector<TradeReport> trades;
};

class MatchingEngine {
public:
    MatchingEngine();
    
    // Core order submission API
    OrderResponse submitOrder(const Order& order);
    
    // Market data access
    std::pair<double, double> getBBO(const std::string& symbol);
    L2Update getL2Update(const std::string& symbol, int depth = 10);
    
    // Event feeds for market data dissemination
    EventFeed<TradeReport>& tradeFeed() { return tradeFeed_; }
    EventFeed<L2Update>& l2Feed() { return l2Feed_; }
    
    // Order management
    bool cancelOrder(const std::string& orderId);
    Order* getOrder(const std::string& orderId);
    
    // Statistics
    size_t getTotalOrders() const;
    size_t getActiveOrders() const;
    
private:
    // Core matching logic with REG NMS compliance
    OrderResponse processMarketOrder(Order& order);
    OrderResponse processLimitOrder(Order& order);
    OrderResponse processIOCOrder(Order& order);
    OrderResponse processFOKOrder(Order& order);
    
    // Matching engine core
    void matchAgainstBook(Order& taker, std::vector<TradeReport>& trades);
    bool canFillCompletely(const Order& order, double& avgPrice);
    
    // Order validation
    bool validateOrder(const Order& order, std::string& errorMsg);
    
    // Thread-safe order storage
    mutable std::shared_mutex ordersMu_;
    std::unordered_map<std::string, Order> allOrders_;
    
    // Per-symbol order books
    mutable std::shared_mutex booksMu_;
    std::unordered_map<std::string, OrderBook> books_;
    
    // Event feeds
    EventFeed<TradeReport> tradeFeed_;
    EventFeed<L2Update> l2Feed_;
    
    // Supporting components
    FeeModel fees_;
    Persistence persist_;
    
    // Trade ID generation
    static std::string generateTradeId();
    
    // Helper methods
    OrderBook& getOrCreateBook(const std::string& symbol);
    void publishL2Update(const std::string& symbol);
    void logOrderEvent(const Order& order, const std::string& event);
};