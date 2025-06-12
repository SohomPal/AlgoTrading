#include "WebSocketClient.h"
#include "trading/OrderBook.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>

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
    
    // Start connections in a separate thread so we can print orderbook states
    std::thread connection_thread([&client, &instruments]() {
        client.connect(instruments);
    });
    
    // Allow some time for connections to establish
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    std::cout << "📈 Starting orderbook monitoring (press Ctrl+C to exit)...\n";
    
    // Print orderbook state every second
    while (true) {
        // Clear screen (optional - comment out if you don't want clearing)
        // system("clear"); // Linux/Mac
        // system("cls");   // Windows
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "⏰ " << std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() << "\n";
        
        // Print state for each instrument
        for (const auto& instrument : instruments) {
            OrderBook orderbook = WebSocketClient::getOrderBook(instrument);
            printOrderBookState(instrument, orderbook);
        }
        
        std::cout << std::string(60, '=') << "\n";
        
        // Wait 1 second before next update
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // This will never be reached due to the infinite loop above
    if (connection_thread.joinable()) {
        connection_thread.join();
    }
    
    return 0;
}