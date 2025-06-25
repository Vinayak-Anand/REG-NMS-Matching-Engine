#include <gtest/gtest.h>
#include "MatchingEngine.h"

TEST(MatchingEngine, SimpleLimitMatch) {
    MatchingEngine me;
    std::vector<TradeReport> reports;

    // Subscribe to the new tradeFeed(), not feed()
    me.tradeFeed().subscribe([&](const TradeReport& rpt) {
        reports.push_back(rpt);
    });

    // Build and submit a resting SELL then a matching BUY
    Order sell{
        "s1",               // orderId
        "BTC-USDT",         // symbol
        Side::SELL,         // side
        OrderType::LIMIT,   // type
        10000.0,            // price (double)
        0.0,                // stopPrice
        1.0,                // quantity
        Order::now()        // timestamp
    };
    Order buy{
        "b1",
        "BTC-USDT",
        Side::BUY,
        OrderType::LIMIT,
        10000.0,
        0.0,
        1.0,
        Order::now()
    };

    me.submitOrder(sell);
    me.submitOrder(buy);

    // We expect exactly one trade report:
    ASSERT_EQ(reports.size(), 1);
    // Compare doubles with EXPECT_DOUBLE_EQ
    EXPECT_DOUBLE_EQ(reports[0].price,    10000.0);
    EXPECT_DOUBLE_EQ(reports[0].quantity, 1.0);
    EXPECT_EQ       (reports[0].makerOrderId, "s1");
    EXPECT_EQ       (reports[0].takerOrderId, "b1");
}
