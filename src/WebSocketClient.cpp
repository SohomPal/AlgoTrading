#include "WebSocketClient.h"
#include "trading/OrderBook.h"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include <json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <algorithm>

using json = nlohmann::json;
typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

std::unordered_map<std::string, std::string> loadConfig(const std::string& path);
std::mutex output_mutex; // For thread-safe console output

// Global map of instrument -> OrderBook
std::unordered_map<std::string, OrderBook> global_orderbooks;
std::mutex orderbook_mutex; // For thread-safe orderbook access

void connectToInstrument(const std::string& instrument) {
    const std::string uri = "wss://ws.sfox.com/ws";
    
    client c;
    c.init_asio();

    c.set_tls_init_handler([](websocketpp::connection_hdl) {
        return websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::tlsv12_client);
    });

    c.set_open_handler([&c, instrument](websocketpp::connection_hdl hdl) {
        {
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cout << "🔗 Connected to sFOX WebSocket for instrument: " << instrument << "\n";
        }

        // Load API key from config file
        auto config = loadConfig("config.cfg");
        std::string token = "ee40b218493863711d0f6044d3a8f46e4ee5bce50e99eae5eabd8434182590f6";

        // Send auth message
        json auth = {
            {"type", "authenticate"},
            {"apiKey", token}
        };
        c.send(hdl, auth.dump(), websocketpp::frame::opcode::text);

        sleep(1); // Wait for authentication to complete

        // Subscribe to specific instrument
        json subscribe = {
            {"type", "subscribe"},
            {"feeds", {"orderbook.sfox." + instrument}}
        };
        c.send(hdl, subscribe.dump(), websocketpp::frame::opcode::text);
    });

    const size_t DEPTH_LIMIT = 10;

    c.set_message_handler([&, instrument](websocketpp::connection_hdl, message_ptr msg) {
        try {
            const std::string& payload_str = msg->get_payload();
            auto payload = json::parse(payload_str);

            std::string type = payload.value("type", "unknown");

            if (payload.contains("payload") &&
                (payload["payload"].contains("bids") || payload["payload"].contains("asks"))) {

                const auto& data = payload["payload"];
                
                // Create JSON object for the orderbook with top 10 bids and asks
                json orderbook_data;
                
                if (data.contains("bids")) {
                    std::vector<json> sorted_bids = data["bids"];
                    std::sort(sorted_bids.begin(), sorted_bids.end(), [](const json& a, const json& b) {
                        return a[0].get<double>() > b[0].get<double>(); // Sort bids descending
                    });
                    
                    // Take top 10 bids
                    json top_bids = json::array();
                    for (size_t i = 0; i < std::min(sorted_bids.size(), static_cast<size_t>(10)); ++i) {
                        top_bids.push_back(sorted_bids[i]);
                    }
                    orderbook_data["bids"] = top_bids;
                }
                
                if (data.contains("asks")) {
                    std::vector<json> sorted_asks = data["asks"];
                    std::sort(sorted_asks.begin(), sorted_asks.end(), [](const json& a, const json& b) {
                        return a[0].get<double>() < b[0].get<double>(); // Sort asks ascending
                    });
                    
                    // Take top 10 asks
                    json top_asks = json::array();
                    for (size_t i = 0; i < std::min(sorted_asks.size(), static_cast<size_t>(10)); ++i) {
                        top_asks.push_back(sorted_asks[i]);
                    }
                    orderbook_data["asks"] = top_asks;
                }
                
                // Update the global orderbook
                {
                    std::lock_guard<std::mutex> lock(orderbook_mutex);
                    global_orderbooks[instrument].setOrderBook(orderbook_data);
                }
                
                {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    std::cout << "📈 [" << instrument << "] Orderbook updated\n";
                }
            } else {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cout << "🔍 [" << instrument << "] Non-orderbook message received:\n";
                std::cout << payload.dump(2) << "\n";
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cerr << "❌ [" << instrument << "] JSON parse error: " << e.what() << "\n";
        }
    });

    c.set_fail_handler([instrument](websocketpp::connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cerr << "❌ [" << instrument << "] Connection failed\n";
    });

    c.set_close_handler([instrument](websocketpp::connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "🔌 [" << instrument << "] Connection closed\n";
    });

    websocketpp::lib::error_code ec;
    client::connection_ptr con = c.get_connection(uri, ec);
    if (ec) {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cerr << "❌ [" << instrument << "] Connection error: " << ec.message() << "\n";
        return;
    }

    c.connect(con);
    c.run();
}

void WebSocketClient::connect(const std::vector<std::string>& instruments) {
    if (instruments.empty()) {
        std::cerr << "❌ No instruments provided\n";
        return;
    }

    // Limit to maximum 10 connections
    size_t max_connections = std::min(instruments.size(), static_cast<size_t>(10));
    
    std::cout << "🚀 Starting " << max_connections << " WebSocket connections...\n";
    
    // Initialize orderbooks for each instrument
    {
        std::lock_guard<std::mutex> lock(orderbook_mutex);
        for (size_t i = 0; i < max_connections; ++i) {
            global_orderbooks[instruments[i]] = OrderBook();
        }
    }
    
    std::vector<std::thread> threads;
    threads.reserve(max_connections);

    // Create a thread for each instrument (up to 10)
    for (size_t i = 0; i < max_connections; ++i) {
        threads.emplace_back(connectToInstrument, instruments[i]);
        
        // Small delay between connection attempts to avoid overwhelming the server
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

// Function to get orderbook for a specific instrument (thread-safe)
OrderBook WebSocketClient::getOrderBook(const std::string& instrument) {
    extern std::unordered_map<std::string, OrderBook> global_orderbooks;
    extern std::mutex orderbook_mutex;
    
    std::lock_guard<std::mutex> lock(orderbook_mutex);
    auto it = global_orderbooks.find(instrument);
    if (it != global_orderbooks.end()) {
        return it->second;
    }
    return OrderBook(); // Return empty orderbook if not found
}

// Function to get all available instruments
std::vector<std::string> WebSocketClient::getAvailableInstruments() {
    extern std::unordered_map<std::string, OrderBook> global_orderbooks;
    extern std::mutex orderbook_mutex;
    
    std::lock_guard<std::mutex> lock(orderbook_mutex);
    std::vector<std::string> instruments;
    for (const auto& pair : global_orderbooks) {
        instruments.push_back(pair.first);
    }
    return instruments;
}

std::unordered_map<std::string, std::string> loadConfig(const std::string& path) {
    std::unordered_map<std::string, std::string> config;
    std::ifstream file(path);
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream is_line(line);
        std::string key;

        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (std::getline(is_line, value)) {
                config[key] = value;
            }
        }
    }
    return config;
}

bool WebSocketClient::hasOrderBook(const std::string& symbol) {
    return global_orderbooks.find(symbol) != global_orderbooks.end();
}