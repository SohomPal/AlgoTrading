find_package(OpenSSL REQUIRED)
cmake_minimum_required(VERSION 3.15)
project(sfox_data_collector)

set(CMAKE_CXX_STANDARD 17)

# Add protobuf and grpc generated sources
set(GRPC_SOURCES
    grpc/orderbook.pb.cc
    grpc/orderbook.grpc.pb.cc
)

# Define all absll dependencies
set(ABSL_DEPS
    # Comprehensive Abseil linking:
    absl_log_internal_message
    absl_log_internal_check_op  
    absl_log_internal_nullguard
    absl_cord
    absl_cordz_info
    absl_strings
    absl_synchronization
    absl_time
    absl_base
)

# Enable standalone Asio (NO Boost)
add_definitions(-DASIO_STANDALONE -D_WEBSOCKETPP_CPP11_STL_)

# Ignore deprecation warnings from WebSocket++ and Asio
add_compile_options(-Wno-deprecated-declarations)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/websocketpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio/include  # << the new standalone Asio
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json
    ${CMAKE_CURRENT_SOURCE_DIR}/grpc
    /opt/homebrew/include
)

link_directories(/opt/homebrew/lib)

add_executable(sfox_data_collector
    src/main.cpp
    src/WebSocketClient.cpp
    src/trading/Order.cpp
    src/trading/OrderBook.cpp
    src/OrderBookServer.cpp
    ${GRPC_SOURCES}
)

target_link_libraries(sfox_data_collector
    pthread
    grpc++        # gRPC C++ library
    grpc
    gpr
    protobuf      # Protobuf library
    OpenSSL::SSL
    OpenSSL::Crypto
    ${ABSL_DEPS}  # Abseil dependencies
)
