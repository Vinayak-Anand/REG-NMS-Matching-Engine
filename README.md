# 🚀 High-Performance Cryptocurrency Matching Engine

This project implements a real-time, high-performance cryptocurrency matching engine inspired by **REG NMS** principles. It supports a complete REST API and WebSocket interface for submitting orders and observing market data. The system is capable of handling multiple advanced order types, includes a persistence layer, and features a maker-taker fee model.

## 📦 Features

- ✅ Matching engine with **price-time priority**  
- ✅ Real-time **Best Bid and Offer (BBO)** calculation  
- ✅ Support for advanced order types: `Market`, `Limit`, `IOC`, `FOK`, `Stop-Loss`, `Stop-Limit`, `Take-Profit`
- ✅ Real-time market data via **WebSocket feeds**
- ✅ REST API for order submission and L2 book retrieval
- ✅ **Persistence layer** using append-only journaling and snapshot recovery
- ✅ **Maker-Taker Fee Model**
- ✅ Unit testing with Google Test (GTest)

## 🛠️ Technologies & Tools

- **Language:** C++17  
- **Build System:** CMake  
- **Web Framework:** Crow  
- **JSON:** nlohmann/json  
- **Package Manager:** vcpkg  
- **WebSocket Client Testing:** wscat, Thunder Client  
- **Testing:** Google Test

## 🚀 Getting Started

### 🔧 Dependencies

Ensure you have the following installed:
- C++ compiler (Visual Studio 2022 on Windows / g++ >= 10)
- CMake (>= 3.20)
- vcpkg

### 📥 Install Dependencies

```bash
# Clone vcpkg if not already
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat # On Windows
```

Then install the required packages:

```bash
./vcpkg install crow nlohmann-json
```

## 🔨 Build Instructions

```bash
git clone <your-repo-url>
cd your-repo
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --parallel
```

## ▶️ Running the Engine

```bash
cd build/src/Release
./engine_app
```

## 🧪 REST API Example

### 📌 Submit Order (Buy)

```bash
curl -X POST http://localhost:18080/orders \
  -H "Content-Type: application/json" \
  -d '{
        "order_id": "o100",
        "symbol": "BTC-USDT",
        "side": "buy",
        "order_type": "limit",
        "quantity": 0.1,
        "price": 46000.0
      }'
```

### 📌 Best Bid/Offer

```bash
curl http://localhost:18080/bbo/BTC-USDT
```

## 🌐 WebSocket Endpoints

| Endpoint           | Description          |
|--------------------|----------------------|
| `/ws/trades`       | Live trade feed      |
| `/ws/orderbook`    | Live L2 book updates |

Connect using:

```bash
wscat -c ws://localhost:18080/ws/trades
```

## 📂 Project Structure

```
.
├── src/
│   ├── main.cpp
│   ├── Order.cpp / .h
│   ├── OrderBook.cpp / .h
│   ├── MatchingEngine.cpp / .h
│   ├── MarketDataServer.cpp / .h
│   ├── FeeCalculator.cpp / .h
│   ├── PersistenceManager.cpp / .h
├── tests/
│   ├── MatchingTests.cpp
├── journal.log
├── snapshot.json
├── README.md
├── .gitignore
```

## 📘 Bonus Features Implemented

- **Advanced Order Types:** IOC, FOK, Stop-Loss, Stop-Limit, Take-Profit
- **Persistence:** Journaling + Snapshot-based recovery
- **Maker-Taker Fee Model:** Tracks and logs maker/taker fees in trade reports

## 📄 License

This project is for academic and demonstration purposes.
