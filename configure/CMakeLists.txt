cmake_minimum_required(VERSION 3.15)
project(AlgoTrader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(absl REQUIRED)

# Add protobuf and grpc generated sources
set(GRPC_SOURCES
    grpc/orderbook.pb.cc
    grpc/orderbook.grpc.pb.cc
)

# Define all absll dependencies
set(ABSL_DEPS
    # Comprehensive Abseil linking:
    absl::base
    absl::strings
    absl::synchronization
    absl::time
    absl::status
    absl::hash
    absl::int128
    absl::symbolize
    absl::stacktrace
    absl::leak_check

    # Add these manually — replace with correct full paths if necessary
    /usr/local/lib/libabsl_log_internal_message.a
    /usr/local/lib/libabsl_log_internal_check_op.a
    /usr/local/lib/libabsl_log_internal_nullguard.a
    /usr/local/lib/libabsl_cord.a
    /usr/local/lib/libabsl_cord_internal.a
    /usr/local/lib/libabsl_malloc_internal.a
    /usr/local/lib/libabsl_synchronization.a
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
    /usr/local/include
)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${gRPC_INCLUDE_DIRS})

link_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_executable(AlgoTrader
    src/main.cpp
    src/WebSocketClient.cpp
    src/trading/Order.cpp
    src/trading/OrderBook.cpp
    src/OrderBookServer.cpp
    ${GRPC_SOURCES}
)

target_link_libraries(AlgoTrader
    pthread
    grpc++        # gRPC C++ library
    grpc
    gpr
    protobuf      # Protobuf library
    OpenSSL::SSL
    OpenSSL::Crypto
    ${ABSL_DEPS}  # Abseil dependencies
)
