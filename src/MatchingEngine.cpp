#include "MatchingEngine.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <sstream>

MatchingEngine::MatchingEngine()
    : persist_("journal.log", "snapshot.json"),
      fees_(0.001, 0.002) {
    std::cout << "=== MatchingEngine initialized ===" << std::endl;
}

OrderResponse MatchingEngine::submitOrder(const Order& order) {
    std::cout << "=== SUBMIT ORDER ===" << std::endl;
    std::cout << "Order: " << order.orderId << " " << order.symbol 
              << " " << (order.side == Side::BUY ? "BUY" : "SELL")
              << " " << order.quantity << "@" << order.price << std::endl;
    
    // Validate order
    std::string errorMsg;
    if (!validateOrder(order, errorMsg)) {
        return {OrderResult::REJECTED_INVALID_PARAMS, errorMsg};
    }
    
    // Store order
    {
        std::unique_lock lk(ordersMu_);
        allOrders_.emplace(order.orderId, order);
    }
    
    // Log order creation
    logOrderEvent(order, "NEW");
    
    // Process based on order type
    OrderResponse response;
    {
        std::unique_lock lk(ordersMu_);
        Order& storedOrder = allOrders_.at(order.orderId);
        
        switch (order.type) {
            case OrderType::MARKET:
                response = processMarketOrder(storedOrder);
                break;
            case OrderType::LIMIT:
                response = processLimitOrder(storedOrder);
                break;
            case OrderType::IOC:
                response = processIOCOrder(storedOrder);
                break;
            case OrderType::FOK:
                response = processFOKOrder(storedOrder);
                break;
        }
    }
    
    return response;
}

OrderResponse MatchingEngine::processMarketOrder(Order& order) {
    std::vector<TradeReport> trades;
    matchAgainstBook(order, trades);
    
    OrderResponse response;
    response.trades = trades;
    response.filledQuantity = order.filledQty;
    
    if (order.isFilled()) {
        response.result = OrderResult::COMPLETELY_FILLED;
        response.message = "Market order completely filled";
        logOrderEvent(order, "FILLED");
    } else {
        // Market orders that can't be completely filled are canceled
        response.result = OrderResult::PARTIALLY_FILLED;
        response.message = "Market order partially filled, remainder canceled";
        logOrderEvent(order, "CANCELED");
    }
    
    return response;
}

OrderResponse MatchingEngine::processLimitOrder(Order& order) {
    std::vector<TradeReport> trades;
    
    // Check for trade-through violations (REG NMS requirement)
    OrderBook& book = getOrCreateBook(order.symbol);
    if (book.wouldTradeThrough(order)) {
        return {OrderResult::REJECTED_TRADE_THROUGH, "Order would trade through BBO"};
    }
    
    matchAgainstBook(order, trades);
    
    OrderResponse response;
    response.trades = trades;
    response.filledQuantity = order.filledQty;
    
    if (order.isFilled()) {
        response.result = OrderResult::COMPLETELY_FILLED;
        response.message = "Limit order completely filled";
        logOrderEvent(order, "FILLED");
    } else {
        // Rest on book
        book.addOrder(&order);
        response.result = OrderResult::ACCEPTED;
        response.message = "Limit order rested on book";
        logOrderEvent(order, "RESTED");
        publishL2Update(order.symbol);
    }
    
    return response;
}

OrderResponse MatchingEngine::processIOCOrder(Order& order) {
    std::vector<TradeReport> trades;
    matchAgainstBook(order, trades);
    
    OrderResponse response;
    response.trades = trades;
    response.filledQuantity = order.filledQty;
    
    if (order.isFilled()) {
        response.result = OrderResult::COMPLETELY_FILLED;
        response.message = "IOC order completely filled";
        logOrderEvent(order, "FILLED");
    } else {
        response.result = OrderResult::PARTIALLY_FILLED;
        response.message = "IOC order partially filled, remainder canceled";
        logOrderEvent(order, "CANCELED");
    }
    
    return response;
}

OrderResponse MatchingEngine::processFOKOrder(Order& order) {
    double avgPrice;
    if (!canFillCompletely(order, avgPrice)) {
        return {OrderResult::REJECTED_FOK_UNFILLABLE, "FOK order cannot be completely filled"};
    }
    
    std::vector<TradeReport> trades;
    matchAgainstBook(order, trades);
    
    OrderResponse response;
    response.trades = trades;
    response.filledQuantity = order.filledQty;
    response.result = OrderResult::COMPLETELY_FILLED;
    response.message = "FOK order completely filled";
    logOrderEvent(order, "FILLED");
    
    return response;
}

void MatchingEngine::matchAgainstBook(Order& taker, std::vector<TradeReport>& trades) {
    OrderBook& book = getOrCreateBook(taker.symbol);
    auto bookLock = book.acquireLock();
    
    if (taker.side == Side::BUY) {
        auto& asks = book.getAsks();
        for (auto it = asks.begin(); it != asks.end() && taker.remaining() > 0;) {
            double level = it->first;
            
            // Price validation for limit orders
            if (taker.type == OrderType::LIMIT && level > taker.price) break;
            
            auto& orderQueue = it->second;
            for (auto orderIt = orderQueue.begin(); orderIt != orderQueue.end() && taker.remaining() > 0;) {
                Order* maker = *orderIt;
                double tradeQty = std::min(maker->remaining(), taker.remaining());
                double tradePrice = maker->price; // Price-time priority: maker's price
                
                // Calculate fees
                auto feeResult = fees_.computeFees(tradePrice, tradeQty, true);
                
                // Create trade report
                TradeReport trade(
                    taker.symbol, generateTradeId(), tradePrice, tradeQty,
                    feeResult.makerFee, feeResult.takerFee, "BUY",
                    maker->orderId, taker.orderId, Order::now()
                );
                
                // Execute trade
                maker->filledQty += tradeQty;
                taker.filledQty += tradeQty;
                trades.push_back(trade);
                
                // Publish trade
                tradeFeed_.publish(trade);
                
                // Log fills
                logOrderEvent(*maker, maker->isFilled() ? "FILLED" : "PARTIAL_FILL");
                logOrderEvent(taker, taker.isFilled() ? "FILLED" : "PARTIAL_FILL");
                
                // Remove filled orders
                if (maker->isFilled()) {
                    orderIt = orderQueue.erase(orderIt);
                } else {
                    ++orderIt;
                }
            }
            
            // Remove empty price levels
            if (orderQueue.empty()) {
                it = asks.erase(it);
            } else {
                ++it;
            }
        }
    } else { // SELL
        auto& bids = book.getBids();
        for (auto it = bids.begin(); it != bids.end() && taker.remaining() > 0;) {
            double level = it->first;
            
            // Price validation for limit orders
            if (taker.type == OrderType::LIMIT && level < taker.price) break;
            
            auto& orderQueue = it->second;
            for (auto orderIt = orderQueue.begin(); orderIt != orderQueue.end() && taker.remaining() > 0;) {
                Order* maker = *orderIt;
                double tradeQty = std::min(maker->remaining(), taker.remaining());
                double tradePrice = maker->price; // Price-time priority: maker's price
                
                // Calculate fees
                auto feeResult = fees_.computeFees(tradePrice, tradeQty, true);
                
                // Create trade report
                TradeReport trade(
                    taker.symbol, generateTradeId(), tradePrice, tradeQty,
                    feeResult.makerFee, feeResult.takerFee, "SELL",
                    maker->orderId, taker.orderId, Order::now()
                );
                
                // Execute trade
                maker->filledQty += tradeQty;
                taker.filledQty += tradeQty;
                trades.push_back(trade);
                
                // Publish trade
                tradeFeed_.publish(trade);
                
                // Log fills
                logOrderEvent(*maker, maker->isFilled() ? "FILLED" : "PARTIAL_FILL");
                logOrderEvent(taker, taker.isFilled() ? "FILLED" : "PARTIAL_FILL");
                
                // Remove filled orders
                if (maker->isFilled()) {
                    orderIt = orderQueue.erase(orderIt);
                } else {
                    ++orderIt;
                }
            }
            
            // Remove empty price levels
            if (orderQueue.empty()) {
                it = bids.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Publish L2 update after matching
    if (!trades.empty()) {
        publishL2Update(taker.symbol);
    }
}

bool MatchingEngine::canFillCompletely(const Order& order, double& avgPrice) {
    OrderBook& book = getOrCreateBook(order.symbol);
    auto bookLock = book.acquireLock();
    
    double remainingQty = order.quantity;
    double totalCost = 0.0;
    
    if (order.side == Side::BUY) {
        const auto& asks = book.getAsks();
        for (const auto& [price, orderQueue] : asks) {
            if (order.type == OrderType::LIMIT && price > order.price) break;
            
            for (Order* maker : orderQueue) {
                double availableQty = maker->remaining();
                double tradeQty = std::min(availableQty, remainingQty);
                totalCost += tradeQty * price;
                remainingQty -= tradeQty;
                
                if (remainingQty <= 0) {
                    avgPrice = totalCost / order.quantity;
                    return true;
                }
            }
        }
    } else {
        const auto& bids = book.getBids();
        for (const auto& [price, orderQueue] : bids) {
            if (order.type == OrderType::LIMIT && price < order.price) break;
            
            for (Order* maker : orderQueue) {
                double availableQty = maker->remaining();
                double tradeQty = std::min(availableQty, remainingQty);
                totalCost += tradeQty * price;
                remainingQty -= tradeQty;
                
                if (remainingQty <= 0) {
                    avgPrice = totalCost / order.quantity;
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool MatchingEngine::validateOrder(const Order& order, std::string& errorMsg) {
    if (order.orderId.empty()) {
        errorMsg = "Order ID cannot be empty";
        return false;
    }
    
    if (order.symbol.empty()) {
        errorMsg = "Symbol cannot be empty";
        return false;
    }
    
    if (order.quantity <= 0) {
        errorMsg = "Quantity must be positive";
        return false;
    }
    
    if ((order.type == OrderType::LIMIT || order.type == OrderType::IOC || order.type == OrderType::FOK) 
        && order.price <= 0) {
        errorMsg = "Price must be positive for limit orders";
        return false;
    }
    
    // Check for duplicate order ID
    {
        std::shared_lock lk(ordersMu_);
        if (allOrders_.find(order.orderId) != allOrders_.end()) {
            errorMsg = "Duplicate order ID";
            return false;
        }
    }
    
    return true;
}

std::string MatchingEngine::generateTradeId() {
    static std::atomic<uint64_t> counter{0};
    return "T" + std::to_string(++counter);
}

OrderBook& MatchingEngine::getOrCreateBook(const std::string& symbol) {
    std::unique_lock lk(booksMu_);
    return books_[symbol]; // Creates if doesn't exist
}

void MatchingEngine::publishL2Update(const std::string& symbol) {
    OrderBook& book = getOrCreateBook(symbol);
    L2Update update = book.generateL2Update(symbol);
    l2Feed_.publish(update);
}

void MatchingEngine::logOrderEvent(const Order& order, const std::string& event) {
    persist_.logOrderEvent(order, event);
}

std::pair<double, double> MatchingEngine::getBBO(const std::string& symbol) {
    std::shared_lock lk(booksMu_);
    auto it = books_.find(symbol);
    if (it != books_.end()) {
        return it->second.bestBidOffer();
    }
    return {0.0, 0.0};
}

L2Update MatchingEngine::getL2Update(const std::string& symbol, int depth) {
    std::shared_lock lk(booksMu_);
    auto it = books_.find(symbol);
    if (it != books_.end()) {
        return it->second.generateL2Update(symbol, depth);
    }
    return L2Update{symbol, Order::now(), {}, {}};
}