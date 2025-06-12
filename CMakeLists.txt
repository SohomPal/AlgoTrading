find_package(OpenSSL REQUIRED)
cmake_minimum_required(VERSION 3.15)
project(sfox_data_collector)

set(CMAKE_CXX_STANDARD 17)

# Enable standalone Asio (NO Boost)
add_definitions(-DASIO_STANDALONE -D_WEBSOCKETPP_CPP11_STL_)

# Ignore deprecation warnings from WebSocket++ and Asio
add_compile_options(-Wno-deprecated-declarations)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/websocketpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio/include  # << the new standalone Asio
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json
)

add_executable(sfox_data_collector
    src/main.cpp
    src/WebSocketClient.cpp
    src/trading/Order.cpp
    src/trading/OrderBook.cpp
)

target_link_libraries(sfox_data_collector
    pthread
    OpenSSL::SSL
    OpenSSL::Crypto
)

