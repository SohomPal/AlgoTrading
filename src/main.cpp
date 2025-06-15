#include "WebSocketClient.h"
#include "trading/OrderBook.h"
#include "OrderBookServer.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <grpcpp/grpcpp.h>

void printOrderBookState(const std::string& instrument, const OrderBook& orderbook) {
    std::cout << "\n=== " << instrument << " OrderBook State ===\n";
    
    try {
        // Get bids and asks
        auto bids = orderbook.getBids();
        auto asks = orderbook.getAsks();
        
        // Print best bid/ask if available
        if (!bids.empty() && !asks.empty()) {
            std::cout << "📊 Best Bid: $" << std::fixed << std::setprecision(2) << orderbook.bestBid()
                      << " | Best Ask: $" << orderbook.bestAsk() 
                      << " | Spread: $" << (orderbook.bestAsk() - orderbook.bestBid()) << "\n";
        }
        
        // Print top 5 bids
        std::cout << "\n🟢 Top Bids:\n";
        int count = 0;
        for (const auto& bid : bids) {
            if (count >= 5) break;
            std::cout << "  " << std::fixed << std::setprecision(2) 
                      << "$" << bid.price << " x " << bid.volume << "\n";
            count++;
        }
        
        // Print top 5 asks
        std::cout << "\n🔴 Top Asks:\n";
        count = 0;
        for (const auto& ask : asks) {
            if (count >= 5) break;
            std::cout << "  " << std::fixed << std::setprecision(2) 
                      << "$" << ask.price << " x " << ask.volume << "\n";
            count++;
        }
        
        if (bids.empty() && asks.empty()) {
            std::cout << "⚠️  No orderbook data available yet...\n";
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ Error reading orderbook: " << e.what() << "\n";
    }
}

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    OrderBookServer service = OrderBookServer();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "✅ gRPC server listening on " << server_address << std::endl;

    server->Wait();
}

void monitorOrderBooks(const std::vector<std::string>& instruments) {
    std::cout << "📈 Starting orderbook monitoring (press Ctrl+C to exit)...\n";
    
    // Print orderbook state every 5 seconds (less frequent since we have gRPC server)
    while (true) {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "⏰ " << std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() << "\n";
        
        // Print state for each instrument
        for (const auto& instrument : instruments) {
            OrderBook orderbook = WebSocketClient::getOrderBook(instrument);
            printOrderBookState(instrument, orderbook);
        }
        
        std::cout << std::string(60, '=') << "\n";
        
        // Wait 5 seconds before next update
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

int main() {
    // Create WebSocket client
    WebSocketClient client;
    
    // Define instruments to connect to
    std::vector<std::string> instruments = {"ethbtc", "btcusd"};
    
    std::cout << "🚀 Starting WebSocket client with instruments: ";
    for (const auto& instrument : instruments) {
        std::cout << instrument << " ";
    }
    std::cout << "\n\n";
    
    // Start WebSocket connections in a separate thread
    std::thread connection_thread([&client, &instruments]() {
        client.connect(instruments);
    });
    
    // Allow some time for connections to establish
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Start gRPC server in a separate thread
    std::thread grpc_thread(RunServer);
    
    // Start monitoring orderbooks in a separate thread
    std::thread monitor_thread(monitorOrderBooks, instruments);
    
    std::cout << "🎯 All services started successfully!\n";
    std::cout << "   - WebSocket connections: Running\n";
    std::cout << "   - gRPC server: Running on 0.0.0.0:50051\n";
    std::cout << "   - OrderBook monitor: Running\n\n";
    
    // Keep main thread alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Clean up threads (this will never be reached due to infinite loop above)
    if (connection_thread.joinable()) connection_thread.join();
    if (grpc_thread.joinable()) grpc_thread.join();
    if (monitor_thread.joinable()) monitor_thread.join();
    
    return 0;
}