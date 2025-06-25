#pragma once
#include <functional>
#include <vector>
#include <mutex>

template<typename T>
class EventFeed {
public:
    using Callback = std::function<void(const T&)>;
    
    void subscribe(Callback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.push_back(callback);
    }
    
    void publish(const T& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& callback : subscribers_) {
            try {
                callback(event);
            } catch (const std::exception& e) {
                // Log error but continue with other subscribers
                // In production, you might want better error handling
            }
        }
    }
    
    size_t subscriberCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return subscribers_.size();
    }
    
private:
    mutable std::mutex mutex_;
    std::vector<Callback> subscribers_;
};