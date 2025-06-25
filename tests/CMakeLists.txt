enable_testing()

add_executable(MatchingTests MatchingTests.cpp)

target_include_directories(MatchingTests
  PRIVATE ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(MatchingTests
  PRIVATE
    matching_engine
    GTest::gtest_main
)

add_test(NAME MatchingTests COMMAND MatchingTests)
