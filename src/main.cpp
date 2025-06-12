#include "WebSocketClient.h"
#include "trading/OrderBook.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <mutex>

// Declare the global orderBooks map (already used in WebSocketClient.cpp)
//extern std::unordered_map<std::string, OrderBook> orderBooks;

int main() {
    WebSocketClient client;

    // Start the WebSocket client in a separate thread
    // std::thread ws_thread([&client]() {
    //     client.connect("wss://ws.sfox.com/ws");
    // });

    std::vector<std::string> instruments = {"ethbtc", "btcusd"};

    client.connect(instruments);

    // Print the order books every second
    // while (true) {
    //     std::this_thread::sleep_for(std::chrono::seconds(1));
    //     std::cout << "\n================= 🔄 Snapshot of All Order Books =================\n";
    //     for (const auto& [symbol, ob] : orderBooks) {
    //         std::cout << "\n📊 Symbol: " << symbol << "\n";

    //         const auto& bids = ob.getBids();
    //         const auto& asks = ob.getAsks();

    //         std::cout << "Top Bids:\n";
    //         for (const auto& bid : bids) {
    //             std::cout << "  " << bid.price << " x " << bid.volume << "\n";
    //         }

    //         std::cout << "Top Asks:\n";
    //         for (const auto& ask : asks) {
    //             std::cout << "  " << ask.price << " x " << ask.volume << "\n";
    //         }
    //     }
    // }

    // ws_thread.join(); // (not reachable, loop is infinite)
    return 0;
}
