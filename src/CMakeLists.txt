add_library(matching_engine
  Order.cpp
  OrderBook.cpp
  FeeCalculator.cpp
  PersistenceManager.cpp
  MatchingEngine.cpp
  MarketDataServer.cpp
)
target_compile_definitions(matching_engine PUBLIC _WIN32_WINNT=0x0601)

target_include_directories(matching_engine
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(matching_engine
  PUBLIC
    asio::asio
    nlohmann_json::nlohmann_json
    Threads::Threads
)

add_executable(engine_app main.cpp)
target_link_libraries(engine_app PRIVATE matching_engine)
