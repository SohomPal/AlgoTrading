cmake_minimum_required(VERSION 3.16)
project(AlgoTrader C CXX)

find_package(OpenSSL REQUIRED)

# === Define Root Paths ===
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(GRPC_DIR "${PROJECT_ROOT}/grpc")
set(PROTO_FILE "${GRPC_DIR}/orderbook.proto")

# === Load gRPC's common settings (from official gRPC repo) ===
include("${PROJECT_ROOT}/cmake/common.cmake")

# === Proto Compilation Output Files ===
set(PROTO_SRCS "${GRPC_DIR}/orderbook.pb.cc")
set(PROTO_HDRS "${GRPC_DIR}/orderbook.pb.h")
set(GRPC_SRCS "${GRPC_DIR}/orderbook.grpc.pb.cc")
set(GRPC_HDRS "${GRPC_DIR}/orderbook.grpc.pb.h")

add_custom_command(
  OUTPUT "${PROTO_SRCS}" "${PROTO_HDRS}" "${GRPC_SRCS}" "${GRPC_HDRS}"
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS --grpc_out="${GRPC_DIR}"
       --cpp_out="${GRPC_DIR}"
       -I "${GRPC_DIR}"
       --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
       "${PROTO_FILE}"
  DEPENDS "${PROTO_FILE}"
)

# === Include Directories ===
include_directories(
  "${GRPC_DIR}"
  "${PROJECT_ROOT}/include"
  "${PROJECT_ROOT}/third_party/websocketpp"
  "${PROJECT_ROOT}/third_party/asio/include"
  "${PROJECT_ROOT}/third_party/json"
)

# === Source Files ===
set(SOURCES
  "${PROJECT_ROOT}/src/main.cpp"
  "${PROJECT_ROOT}/src/WebSocketClient.cpp"
  "${PROJECT_ROOT}/src/OrderBookServer.cpp"
  "${PROJECT_ROOT}/src/trading/Order.cpp"
  "${PROJECT_ROOT}/src/trading/OrderBook.cpp"
  ${PROTO_SRCS}
  ${GRPC_SRCS}
)

# === Executable Target ===
add_executable(AlgoTrader ${SOURCES})

# === Link Libraries ===
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
