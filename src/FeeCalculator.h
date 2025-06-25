#pragma once

struct FeeResult {
    double makerFee;
    double takerFee;
    double totalFee;
    
    FeeResult(double maker, double taker) 
        : makerFee(maker), takerFee(taker), totalFee(maker + taker) {}
};

class FeeModel {
public:
    FeeModel(double makerRate = 0.001, double takerRate = 0.002)
        : makerRate_(makerRate), takerRate_(takerRate) {}
    
    FeeResult computeFees(double price, double quantity, bool isTaker = true) const {
        double notional = price * quantity;
        double makerFee = notional * makerRate_;
        double takerFee = notional * takerRate_;
        return FeeResult(makerFee, takerFee);
    }
    
    double getMakerRate() const { return makerRate_; }
    double getTakerRate() const { return takerRate_; }
    
    void setRates(double makerRate, double takerRate) {
        makerRate_ = makerRate;
        takerRate_ = takerRate;
    }
    
private:
    double makerRate_;  // Fee rate for makers (liquidity providers)
    double takerRate_;  // Fee rate for takers (liquidity removers)
};