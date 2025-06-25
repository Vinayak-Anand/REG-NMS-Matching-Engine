/* main.cpp */
#include "MatchingEngine.h"
#include "MarketDataServer.h"
#include <iostream>
#include <csignal>

static MarketDataServer* g_server = nullptr;

// Handle SIGINT/SIGTERM for graceful shutdown
void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received. Stopping server...\n";
    if (g_server) g_server->stop();
}

int main() {
    std::cout << "=== MAIN FUNCTION STARTED ===\n";

    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Construct the engine
    MatchingEngine engine;
    std::cout << "=== MatchingEngine initialized ===\n";

    // Subscribe console logger to the TRADE feed
    engine.tradeFeed().subscribe([](const TradeReport& rpt) {
        std::cout << "[TRADE] " << rpt.tradeId
                  << " " << rpt.quantity << "@" << rpt.price
                  << " (makerFee=" << rpt.makerFee
                  << ", takerFee=" << rpt.takerFee << ")\n";
    });

    // (Optional) Subscribe console logger to the L2 book feed
    engine.l2Feed().subscribe([](const L2Update& upd) {
        std::cout << "[L2] " << upd.symbol
                  << " bids=" << upd.bids.size()
                  << " asks=" << upd.asks.size()
                  << " ts=" << upd.timestamp << "\n";
    });

    std::cout << "Creating MarketDataServer...\n";
    MarketDataServer server(engine, 18080);
    g_server = &server;

    std::cout << "Server listening on http://0.0.0.0:18080\n";
    server.run();
    std::cout << "Server has shut down.\n";
    return 0;
}
