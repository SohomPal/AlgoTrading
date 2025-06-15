#pragma once

#include <grpcpp/grpcpp.h>
#include <unordered_map>
#include <string>

// Include your generated protobuf files
#include "orderbook.grpc.pb.h"

class OrderBookServer final : public orderbook::OrderBookService::Service {
public:
    OrderBookServer();

    grpc::Status GetOrderBook(grpc::ServerContext* context,
                             const orderbook::OrderBookRequest* request,
                             orderbook::OrderBookResponse* response) override;
    
    grpc::Status GetAvailableSymbols(grpc::ServerContext* context,
                                   const orderbook::Empty* request,
                                   orderbook::SymbolsResponse* response) override;
};