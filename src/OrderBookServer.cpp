#include "OrderBookServer.h"
#include "WebSocketClient.h"  // For access to global orderBooks
#include "trading/OrderBook.h"
#include "trading/Order.h"

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <stdexcept>

using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;

using orderbook::OrderBookRequest;
using orderbook::OrderBookResponse;
using orderbook::SymbolsResponse;
using orderbook::Empty;
//using orderbook::Order;

// Constructor
OrderBookServer::OrderBookServer() = default;

// GetOrderBook implementation
Status OrderBookServer::GetOrderBook(ServerContext* context,
                                     const OrderBookRequest* request,
                                     OrderBookResponse* response) {
    std::string symbol = request->symbol();

    // Get order book from global map
    if (!WebSocketClient::hasOrderBook(symbol)) {
        return Status(StatusCode::NOT_FOUND, "Symbol not found in order books");
    }

    const OrderBook& book = WebSocketClient::getOrderBook(symbol);
    const auto& bids = book.getBids();
    const auto& asks = book.getAsks();

    if (bids.empty() && asks.empty()) {
        return Status(StatusCode::NOT_FOUND, "No order book data available for symbol: " + symbol);
    }

    for (const auto& b : bids) {
        orderbook::Order* o = response->add_bids();
        o->set_price(b.price);
        o->set_volume(b.volume);
    }

    for (const auto& a : asks) {
        orderbook::Order* o = response->add_asks();
        o->set_price(a.price);
        o->set_volume(a.volume);
    }

    response->set_symbol(symbol);
    response->set_best_bid(book.bestBid());
    response->set_best_ask(book.bestAsk());
    response->set_timestamp(static_cast<int64_t>(std::time(nullptr)));  // current UNIX time

    std::cout << "📡 Served order book for " << symbol
              << " | " << bids.size() << " bids, " << asks.size() << " asks\n";

    return Status::OK;
}

// GetAvailableSymbols implementation
Status OrderBookServer::GetAvailableSymbols(ServerContext* context,
                                            const Empty* request,
                                            SymbolsResponse* response) {
    const auto& instruments = WebSocketClient::getAvailableInstruments();

    for (const std::string& sym : instruments) {
        response->add_symbols(sym);
    }

    std::cout << "📡 Served symbol list: " << instruments.size() << " instruments\n";
    return Status::OK;
}
