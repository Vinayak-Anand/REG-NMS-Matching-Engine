// #pragma once
// #include <string>
// #include <chrono>

// enum class OrderType { MARKET, LIMIT, IOC, FOK,
//                        STOP_LOSS, STOP_LIMIT, TAKE_PROFIT };

// enum class Side { BUY, SELL };

// struct Order {
//   std::string     orderId;
//   std::string     symbol;
//   Side            side;
//   OrderType       type;
//   double          price      = 0.0;  // for LIMIT, STOP_LIMIT
//   double          stopPrice  = 0.0;  // for STOP orders
//   double          quantity   = 0.0;
//   std::chrono::time_point<std::chrono::high_resolution_clock> timestamp;

//   double remaining() const { return quantity; }
//   static auto now() { return std::chrono::high_resolution_clock::now(); }
// };
#pragma once
#include <string>
#include <chrono>

enum class Side { BUY, SELL };

enum class OrderType { 
    MARKET = 0, 
    LIMIT = 1, 
    IOC = 2,    // Immediate-Or-Cancel
    FOK = 3     // Fill-Or-Kill
};

struct Order {
    std::string orderId;
    std::string symbol;
    Side side;
    OrderType type;
    double price;        // Required for LIMIT orders
    double stopPrice;    // For stop orders (bonus feature)
    double quantity;     // Original quantity
    double filledQty = 0.0;  // Quantity filled so far
    long long timestamp;
    
    // Calculate remaining quantity
    double remaining() const { 
        return quantity - filledQty; 
    }
    
    // Check if order is completely filled
    bool isFilled() const { 
        return remaining() <= 0.0; 
    }
    
    // Check if order is marketable (can execute immediately)
    bool isMarketable(double bestBid, double bestAsk) const {
        if (type == OrderType::MARKET) return true;
        if (side == Side::BUY && price >= bestAsk && bestAsk > 0) return true;
        if (side == Side::SELL && price <= bestBid && bestBid > 0) return true;
        return false;
    }
    
    // Utility function to get current timestamp
    static long long now() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }
};