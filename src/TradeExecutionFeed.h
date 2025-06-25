#pragma once
#include <string>
#include <nlohmann/json.hpp>

struct TradeReport {
    std::string symbol;
    std::string tradeId;
    double price;
    double quantity;
    double makerFee;
    double takerFee;
    std::string aggressor;      // "BUY" or "SELL" - side of taker order
    std::string makerOrderId;
    std::string takerOrderId;
    long long timestamp;
    
    TradeReport() = default;
    
    TradeReport(const std::string& sym, const std::string& tid, 
                double p, double q, double mf, double tf,
                const std::string& agg, const std::string& maker_id,
                const std::string& taker_id, long long ts)
        : symbol(sym), tradeId(tid), price(p), quantity(q),
          makerFee(mf), takerFee(tf), aggressor(agg),
          makerOrderId(maker_id), takerOrderId(taker_id), timestamp(ts) {}
    
    // Convert to JSON for API responses
    nlohmann::json toJson() const {
        return nlohmann::json{
            {"timestamp", std::to_string(timestamp)},
            {"symbol", symbol},
            {"trade_id", tradeId},
            {"price", std::to_string(price)},
            {"quantity", std::to_string(quantity)},
            {"aggressor_side", aggressor},
            {"maker_order_id", makerOrderId},
            {"taker_order_id", takerOrderId},
            {"maker_fee", std::to_string(makerFee)},
            {"taker_fee", std::to_string(takerFee)}
        };
    }
};