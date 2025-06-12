#include "WebSocketClient.h"
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

            auto print_levels = [DEPTH_LIMIT, instrument](const json& levels, bool is_bid) {
                std::vector<json> sorted_levels = levels;

                std::sort(sorted_levels.begin(), sorted_levels.end(), [is_bid](const json& a, const json& b) {
                    return is_bid ? (a[0].get<double>() > b[0].get<double>())
                                : (a[0].get<double>() < b[0].get<double>());
                });

                for (size_t i = 0; i < std::min(sorted_levels.size(), DEPTH_LIMIT); ++i) {
                    const auto& level = sorted_levels[i];
                    std::cout << "  [" << instrument << "] " 
                            << (is_bid ? "Bid" : "Ask")
                            << ": " << level[0] << " x " << level[1] << "\n";
                }
            };

            if (payload.contains("payload") &&
                (payload["payload"].contains("bids") || payload["payload"].contains("asks"))) {

                const auto& data = payload["payload"];
                
                {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    std::cout << "\n📈 [" << instrument << "] Orderbook Update\n";

                    if (data.contains("bids")) {
                        std::cout << "Top Bids:\n";
                        print_levels(data["bids"], true);
                    }
                    if (data.contains("asks")) {
                        std::cout << "Top Asks:\n";
                        print_levels(data["asks"], false);
                    }
                }
            } else {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cout << "\n🔍 [" << instrument << "] Non-orderbook message received:\n";
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