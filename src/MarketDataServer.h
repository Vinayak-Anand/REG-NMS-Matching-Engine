#pragma once
#include "MatchingEngine.h"
#include <crow.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <mutex>

class MarketDataServer {
public:
    MarketDataServer(MatchingEngine& engine, int port = 8080);
    ~MarketDataServer() = default;
    
    void run();
    void stop();
    
private:
    void setupRestRoutes();
    void setupWebSocketEndpoints();
    
    // WebSocket client management
    void addTradeClient(crow::websocket::connection* conn);
    void removeTradeClient(crow::websocket::connection* conn);
    void addL2Client(crow::websocket::connection* conn);
    void removeL2Client(crow::websocket::connection* conn);
    
    // Broadcast to clients
    void broadcastTrade(const TradeReport& trade);
    void broadcastL2Update(const L2Update& update);
    
    MatchingEngine& engine_;
    crow::SimpleApp app_;
    
    // WebSocket client connections
    std::mutex clientsMutex_;
    std::vector<crow::websocket::connection*> tradeClients_;
    std::vector<crow::websocket::connection*> l2Clients_;
};