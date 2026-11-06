cmake_minimum_required(VERSION 3.20)

project(QuantEdge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(quantedge INTERFACE)
target_include_directories(quantedge INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(quantedge_cli apps/cli/main.cpp)
target_link_libraries(quantedge_cli PRIVATE quantedge)

enable_testing()

function(quantedge_add_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE quantedge)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

quantedge_add_test(quantedge_pricing_tests tests/unit/pricing/pricing_tests.cpp)
quantedge_add_test(quantedge_smile_calibration_tests tests/unit/pricing/smile_calibration_tests.cpp)

quantedge_add_test(quantedge_market_replay_tests tests/unit/market/market_replay_tests.cpp)

quantedge_add_test(quantedge_risk_export_tests tests/unit/risk/risk_export_tests.cpp)
quantedge_add_test(quantedge_portfolio_risk_tests tests/unit/risk/portfolio_risk_tests.cpp)
quantedge_add_test(quantedge_correlation_risk_tests tests/unit/risk/correlation_risk_tests.cpp)
quantedge_add_test(quantedge_pnl_explain_tests tests/unit/risk/pnl_explain_tests.cpp)

quantedge_add_test(quantedge_runtime_tests tests/unit/runtime/runtime_tests.cpp)
quantedge_add_test(quantedge_adaptive_controller_tests tests/unit/runtime/adaptive_controller_tests.cpp)
quantedge_add_test(quantedge_benchmark_tests tests/unit/runtime/benchmark_tests.cpp)

quantedge_add_test(quantedge_system_monitor_tests tests/unit/system/system_monitor_tests.cpp)
quantedge_add_test(quantedge_config_tests tests/unit/config/config_tests.cpp)
quantedge_add_test(quantedge_microstructure_backtest_tests tests/unit/research/microstructure_backtest_tests.cpp)
