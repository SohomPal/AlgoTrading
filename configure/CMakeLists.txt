cmake_minimum_required(VERSION 3.16)
find_package(OpenSSL REQUIRED)

project(AlgoTrader C CXX)

# Load gRPC's shared CMake config
include(../cmake/common.cmake)

# === Proto Setup ===
get_filename_component(proto_file "../../orderbook.proto" ABSOLUTE)
get_filename_component(proto_dir "${proto_file}" PATH)

set(proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/orderbook.pb.cc")
set(proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/orderbook.pb.h")
set(grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/orderbook.grpc.pb.cc")
set(grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/orderbook.grpc.pb.h")

add_custom_command(
  OUTPUT "${proto_srcs}" "${proto_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
       --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
       -I "${proto_dir}"
       --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
       "${proto_file}"
  DEPENDS "${proto_file}"
)

# Include generated headers
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# === Project Source Files ===
set(SOURCES
  ../../src/main.cpp
  ../../src/WebSocketClient.cpp
  ../../src/OrderBookServer.cpp
  ../../src/trading/Order.cpp
  ../../src/trading/OrderBook.cpp
  ${proto_srcs}
  ${grpc_srcs}
)

# === Header Paths ===
include_directories(
  ../../include
  ../../third_party/websocketpp
  ../../third_party/asio/include
  ../../third_party/json
)

# === Build Target ===
add_executable(AlgoTrader ${SOURCES})

# === Link with Dependencies ===
target_link_libraries(AlgoTrader
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF}
  absl::log_initialize
  absl::flags
  absl::flags_parse
  pthread
  ssl
  crypto
)
