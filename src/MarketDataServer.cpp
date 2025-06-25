#include "MarketDataServer.h"
#include <iostream>
#include <algorithm>
using namespace std;

MarketDataServer::MarketDataServer(MatchingEngine& engine, int port)
    : engine_(engine), app_() {
    cout <<"=== CONSTRUCTING MarketDataServer ===" << endl;
    
    setupRestRoutes();
    setupWebSocketEndpoints();
    
    // Subscribe to engine events
    engine_.tradeFeed().subscribe([this](const TradeReport& trade) {
        broadcastTrade(trade);
    });
    
    engine_.l2Feed().subscribe([this](const L2Update& update) {
        broadcastL2Update(update);
    });
    
    app_.port(port).multithreaded();
}

void MarketDataServer::stop() {
    app_.stop();
}

void MarketDataServer::setupRestRoutes() {
    using json = nlohmann::json;

    // Order submission endpoint
    CROW_ROUTE(app_, "/orders").methods("POST"_method)
    ([this](const crow::request& req) {
        cout << "POST /orders - Body: " << req.body << endl;
        
        try {
            auto body = json::parse(req.body);
            
            Order order;
            order.orderId = body.at("order_id").get<string>();
            order.symbol = body.at("symbol").get<string>();
            order.side = (body.at("side").get<string>() == "buy" ? Side::BUY : Side::SELL);
            
            // Parse order type
            string typeStr = body.at("order_type").get<string>();
            if (typeStr == "market") order.type = OrderType::MARKET;
            else if (typeStr == "limit") order.type = OrderType::LIMIT;
            else if (typeStr == "ioc") order.type = OrderType::IOC;
            else if (typeStr == "fok") order.type = OrderType::FOK;
            else throw invalid_argument("Invalid order_type: " + typeStr);
            
            order.quantity = body.at("quantity").get<double>();
            order.price = body.value("price", 0.0);
            order.timestamp = Order::now();

            // Submit to matching engine
            auto response = engine_.submitOrder(order);
            
            // Create response JSON
            json responseJson = {
                {"order_id", order.orderId},
                {"status", [&]() {
                    switch (response.result) {
                        case OrderResult::ACCEPTED: return "accepted";
                        case OrderResult::COMPLETELY_FILLED: return "filled";
                        case OrderResult::PARTIALLY_FILLED: return "partially_filled";
                        case OrderResult::REJECTED_INVALID_PARAMS: return "rejected_invalid";
                        case OrderResult::REJECTED_TRADE_THROUGH: return "rejected_trade_through";
                        case OrderResult::REJECTED_FOK_UNFILLABLE: return "rejected_fok";
                        default: return "unknown";
                    }
                }()},
                {"message", response.message},
                {"filled_quantity", to_string(response.filledQuantity)},
                {"trades", json::array()}
            };
            
            // Add trade details
            for (const auto& trade : response.trades) {
                responseJson["trades"].push_back(trade.toJson());
            }
            
            int httpStatus = (response.result == OrderResult::ACCEPTED || 
                            response.result == OrderResult::COMPLETELY_FILLED ||
                            response.result == OrderResult::PARTIALLY_FILLED) ? 201 : 400;
            
            return crow::response(httpStatus, responseJson.dump());
        }
        catch (const exception& e) {
            cerr << "Order submission error: " << e.what() << endl;
            json errorJson = {{"error", e.what()}};
            return crow::response(400, errorJson.dump());
        }
    });

    // BBO endpoint
    CROW_ROUTE(app_, "/bbo/<string>").methods("GET"_method)
    ([this](const string& symbol) {
        auto [bid, ask] = engine_.getBBO(symbol);
        json response = {
            {"symbol", symbol},
            {"timestamp", to_string(Order::now())},
            {"best_bid", bid > 0 ? to_string(bid) : ""},
            {"best_ask", ask > 0 ? to_string(ask) : ""}
        };
        return crow::response(200, response.dump());
    });

    // L2 Order Book endpoint
    CROW_ROUTE(app_, "/orderbook/<string>").methods("GET"_method)
    ([this](const crow::request& req, const string& symbol) {
        int depth = 10;
        auto depthParam = req.url_params.get("depth");
        if (depthParam) {
            try {
                depth = stoi(depthParam);
                depth = max(1, min(depth, 100)); // Clamp between 1-100
            } catch (...) {
                depth = 10;
            }
        }
        
        auto l2Update = engine_.getL2Update(symbol, depth);
        
        json response = {
            {"timestamp", to_string(l2Update.timestamp)},
            {"symbol", l2Update.symbol},
            {"bids", json::array()},
            {"asks", json::array()}
        };
        
        for (const auto& [price, qty] : l2Update.bids) {
            response["bids"].push_back({to_string(price), to_string(qty)});
        }
        
        for (const auto& [price, qty] : l2Update.asks) {
            response["asks"].push_back({to_string(price), to_string(qty)});
        }
        
        return crow::response(200, response.dump());
    });

    // Health check endpoint
    CROW_ROUTE(app_, "/health").methods("GET"_method)
    ([]() {
        json response = {
            {"status", "healthy"},
            {"timestamp", to_string(Order::now())}
        };
        return crow::response(200, response.dump());
    });
}

void MarketDataServer::setupWebSocketEndpoints() {
    // Trade feed WebSocket
    CROW_ROUTE(app_, "/ws/trades")
        .websocket(&app_)
        .onopen([this](crow::websocket::connection& conn) {
            addTradeClient(&conn);
            cout << "[WS] Trade client connected, total=" << tradeClients_.size() << endl;
        })
        .onclose([this](crow::websocket::connection& conn, const string&, bool) {
            removeTradeClient(&conn);
            cout << "[WS] Trade client disconnected, remaining=" << tradeClients_.size() << endl;
        })
        .onmessage([](crow::websocket::connection&, const string& message, bool) {
            // Echo or handle client messages if needed
            cout << "[WS] Trade client message: " << message << endl;
        });

    // L2 order book feed WebSocket
    CROW_ROUTE(app_, "/ws/orderbook")
        .websocket(&app_)
        .onopen([this](crow::websocket::connection& conn) {
            addL2Client(&conn);
            cout << "[WS] L2 client connected, total=" << l2Clients_.size() << endl;
        })
        .onclose([this](crow::websocket::connection& conn, const string&, bool) {
            removeL2Client(&conn);
            cout << "[WS] L2 client disconnected, remaining=" << l2Clients_.size() << endl;
        })
        .onmessage([](crow::websocket::connection&, const string& message, bool) {
            // Handle client messages (e.g., symbol subscriptions)
            cout << "[WS] L2 client message: " << message << endl;
        });
}

void MarketDataServer::addTradeClient(crow::websocket::connection* conn) {
    lock_guard<mutex> lock(clientsMutex_);
    tradeClients_.push_back(conn);
}

void MarketDataServer::removeTradeClient(crow::websocket::connection* conn) {
    lock_guard<mutex> lock(clientsMutex_);
    tradeClients_.erase(
        remove(tradeClients_.begin(), tradeClients_.end(), conn),
        tradeClients_.end()
    );
}

void MarketDataServer::addL2Client(crow::websocket::connection* conn) {
    lock_guard<mutex> lock(clientsMutex_);
    l2Clients_.push_back(conn);
}

void MarketDataServer::removeL2Client(crow::websocket::connection* conn) {
    lock_guard<mutex> lock(clientsMutex_);
    l2Clients_.erase(
        remove(l2Clients_.begin(), l2Clients_.end(), conn),
        l2Clients_.end()
    );
}

void MarketDataServer::broadcastTrade(const TradeReport& trade) {
    lock_guard<mutex> lock(clientsMutex_);
    string message = trade.toJson().dump();
    
    for (auto* client : tradeClients_) {
        try {
            client->send_text(message);
        } catch (const exception& e) {
            cerr << "Failed to send trade to client: " << e.what() << endl;
        }
    }
}

void MarketDataServer::broadcastL2Update(const L2Update& update) {
    lock_guard<mutex> lock(clientsMutex_);
    
    nlohmann::json json = {
        {"timestamp", to_string(update.timestamp)},
        {"symbol", update.symbol},
        {"bids", nlohmann::json::array()},
        {"asks", nlohmann::json::array()}
    };
    
    for (const auto& [price, qty] : update.bids) {
        json["bids"].push_back({to_string(price), to_string(qty)});
    }
    
    for (const auto& [price, qty] : update.asks) {
        json["asks"].push_back({to_string(price), to_string(qty)});
    }
    
    string message = json.dump();
    
    for (auto* client : l2Clients_) {
        try {
            client->send_text(message);
        } catch (const exception& e) {
            cerr << "Failed to send L2 update to client: " << e.what() << endl;
        }
    }
}

void MarketDataServer::run() {
    cout << "=== STARTING ENHANCED MARKET DATA SERVER ===" << endl;
    cout << "Endpoints available:" << endl;
    cout << "  POST /orders - Submit orders" << endl;
    cout << "  GET /bbo/<symbol> - Best bid/offer" << endl;
    cout << "  GET /orderbook/<symbol>?depth=N - L2 order book" << endl;
    cout << "  GET /health - Health check" << endl;
    cout << "  WS /ws/trades - Trade feed" << endl;
    cout << "  WS /ws/orderbook - L2 order book feed" << endl;
    app_.run();
}