#pragma once
#include "Order.h"
#include <string>
#include <fstream>
#include <mutex>

class Persistence {
public:
    Persistence(const std::string& journalFile, const std::string& snapshotFile)
        : journalFile_(journalFile), snapshotFile_(snapshotFile) {
        // Open journal file in append mode
        journalStream_.open(journalFile_, std::ios::app);
    }
    
    ~Persistence() {
        if (journalStream_.is_open()) {
            journalStream_.close();
        }
    }
    
    void logOrderEvent(const Order& order, const std::string& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (journalStream_.is_open()) {
            journalStream_ << Order::now() << "|"
                          << event << "|"
                          << order.orderId << "|"
                          << order.symbol << "|"
                          << (order.side == Side::BUY ? "BUY" : "SELL") << "|"
                          << static_cast<int>(order.type) << "|"
                          << order.price << "|"
                          << order.quantity << "|"
                          << order.filledQty << "|"
                          << order.timestamp << std::endl;
            journalStream_.flush();
        }
    }
    
    void createSnapshot() {
        std::lock_guard<std::mutex> lock(mutex_);
        // Implementation would save current state to snapshot file
        // This is a placeholder for the bonus feature
    }
    
    bool loadFromSnapshot() {
        std::lock_guard<std::mutex> lock(mutex_);
        // Implementation would restore state from snapshot file
        // This is a placeholder for the bonus feature
        return false;
    }
    
private:
    std::string journalFile_;
    std::string snapshotFile_;
    std::ofstream journalStream_;
    mutable std::mutex mutex_;
};