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
#include <chrono>
#include <atomic>

using json = nlohmann::json;
typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

std::unordered_map<std::string, std::string> loadConfig(const std::string& path);
std::mutex output_mutex; // For thread-safe console output

// Global map of instrument -> OrderBook
std::unordered_map<std::string, OrderBook> global_orderbooks;
std::mutex orderbook_mutex; // For thread-safe orderbook access

// Timeout tracking - using regular variables instead of atomics in map
struct ConnectionState {
    std::chrono::steady_clock::time_point last_data_time;
    bool should_stop;
    std::mutex state_mutex;
    
    ConnectionState() : last_data_time(std::chrono::steady_clock::now()), should_stop(false) {}
    
    void updateDataTime() {
        std::lock_guard<std::mutex> lock(state_mutex);
        last_data_time = std::chrono::steady_clock::now();
    }
    
    std::chrono::steady_clock::time_point getLastDataTime() {
        std::lock_guard<std::mutex> lock(state_mutex);
        return last_data_time;
    }
    
    void setShouldStop(bool stop) {
        std::lock_guard<std::mutex> lock(state_mutex);
        should_stop = stop;
    }
    
    bool getShouldStop() {
        std::lock_guard<std::mutex> lock(state_mutex);
        return should_stop;
    }
};

std::unordered_map<std::string, std::unique_ptr<ConnectionState>> connection_states;
std::mutex connection_states_mutex;

void connectToInstrument(const std::string& instrument) {
    const std::string uri = "wss://ws.sfox.com/ws";
    
    // Initialize connection state
    {
        std::lock_guard<std::mutex> lock(connection_states_mutex);
        connection_states[instrument] = std::make_unique<ConnectionState>();
    }

    while (true) {
        try {
            // Check if we should stop
            {
                std::lock_guard<std::mutex> lock(connection_states_mutex);
                if (connection_states[instrument]->getShouldStop()) {
                    break;
                }
            }

            client c;
            c.init_asio();
            
            // Set access and error log levels to reduce noise
            c.set_access_channels(websocketpp::log::alevel::none);
            c.set_error_channels(websocketpp::log::elevel::none);

            c.set_tls_init_handler([&instrument](websocketpp::connection_hdl) -> websocketpp::lib::shared_ptr<asio::ssl::context> {
                try {
                    auto ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::tlsv12_client);
                    ctx->set_options(asio::ssl::context::default_workarounds |
                                   asio::ssl::context::no_sslv2 |
                                   asio::ssl::context::no_sslv3 |
                                   asio::ssl::context::single_dh_use);
                    return ctx;
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    std::cerr << "❌ [" << instrument << "] TLS initialization error: " << e.what() << "\n";
                    throw;
                }
            });

            // Simple connection tracking
            bool is_connected = false;
            bool is_authenticated = false;

            c.set_open_handler([&c, &instrument, &is_connected, &is_authenticated](websocketpp::connection_hdl hdl) {
                try {
                    {
                        std::lock_guard<std::mutex> lock(output_mutex);
                        std::cout << "🔗 Connected to sFOX WebSocket for instrument: " << instrument << "\n";
                    }
                    
                    is_connected = true;
                    
                    // Update last data time on connection
                    {
                        std::lock_guard<std::mutex> lock(connection_states_mutex);
                        if (connection_states.find(instrument) != connection_states.end()) {
                            connection_states[instrument]->updateDataTime();
                        }
                    }

                    auto config = loadConfig("config.cfg");
                    std::string token = config.count("API_KEY") ? config["API_KEY"] : "";

                    json auth = {
                        {"type", "authenticate"},
                        {"apiKey", token}
                    };
                    
                    websocketpp::lib::error_code ec;
                    c.send(hdl, auth.dump(), websocketpp::frame::opcode::text, ec);
                    if (ec) {
                        std::lock_guard<std::mutex> lock(output_mutex);
                        std::cerr << "❌ [" << instrument << "] Failed to send authentication: " << ec.message() << "\n";
                        c.close(hdl, websocketpp::close::status::protocol_error, "Auth send failed");
                        return;
                    }

                    std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for authentication

                    json subscribe = {
                        {"type", "subscribe"},
                        {"feeds", {"orderbook.sfox." + instrument}}
                    };
                    
                    c.send(hdl, subscribe.dump(), websocketpp::frame::opcode::text, ec);
                    if (ec) {
                        std::lock_guard<std::mutex> lock(output_mutex);
                        std::cerr << "❌ [" << instrument << "] Failed to send subscription: " << ec.message() << "\n";
                        c.close(hdl, websocketpp::close::status::protocol_error, "Subscribe send failed");
                        return;
                    }
                    
                    is_authenticated = true;
                    
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    std::cerr << "❌ [" << instrument << "] Error in open handler: " << e.what() << "\n";
                    is_connected = false;
                }
            });

            const size_t DEPTH_LIMIT = 10;

            c.set_message_handler([&instrument, &is_connected, &is_authenticated, DEPTH_LIMIT](websocketpp::connection_hdl hdl, message_ptr msg) {
                try {
                    // Update last data time
                    {
                        std::lock_guard<std::mutex> lock(connection_states_mutex);
                        if (connection_states.find(instrument) != connection_states.end()) {
                            connection_states[instrument]->updateDataTime();
                        }
                    }

                    auto payload = json::parse(msg->get_payload());
                    
                    // Check for authentication response
                    if (payload.contains("type") && payload["type"] == "authenticate") {
                        if (payload.contains("success") && payload["success"] == true) {
                            std::lock_guard<std::mutex> lock(output_mutex);
                            std::cout << "✅ [" << instrument << "] Authentication successful\n";
                        } else {
                            std::lock_guard<std::mutex> lock(output_mutex);
                            std::cerr << "❌ [" << instrument << "] Authentication failed\n";
                            return;
                        }
                    }

                    if (payload.contains("payload") &&
                        (payload["payload"].contains("bids") || payload["payload"].contains("asks"))) {

                        const auto& data = payload["payload"];
                        json orderbook_data;

                        if (data.contains("bids")) {
                            std::vector<json> sorted_bids = data["bids"];
                            std::sort(sorted_bids.begin(), sorted_bids.end(), [](const json& a, const json& b) {
                                return a[0].get<double>() > b[0].get<double>();
                            });

                            json top_bids = json::array();
                            for (size_t i = 0; i < std::min(sorted_bids.size(), DEPTH_LIMIT); ++i) {
                                top_bids.push_back(sorted_bids[i]);
                            }
                            orderbook_data["bids"] = top_bids;
                        }

                        if (data.contains("asks")) {
                            std::vector<json> sorted_asks = data["asks"];
                            std::sort(sorted_asks.begin(), sorted_asks.end(), [](const json& a, const json& b) {
                                return a[0].get<double>() < b[0].get<double>();
                            });

                            json top_asks = json::array();
                            for (size_t i = 0; i < std::min(sorted_asks.size(), DEPTH_LIMIT); ++i) {
                                top_asks.push_back(sorted_asks[i]);
                            }
                            orderbook_data["asks"] = top_asks;
                        }

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
                        std::cout << "🔍 [" << instrument << "] Non-orderbook message:\n";
                        std::cout << payload.dump(2) << "\n";
                    }
                } catch (const json::parse_error& e) {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    std::cerr << "❌ [" << instrument << "] JSON parse error: " << e.what() << "\n";
                    std::cerr << "Raw message: " << msg->get_payload() << "\n";
                } catch (const json::type_error& e) {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    std::cerr << "❌ [" << instrument << "] JSON type error: " << e.what() << "\n";
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    std::cerr << "❌ [" << instrument << "] Message handler error: " << e.what() << "\n";
                }
            });

            c.set_fail_handler([&instrument, &is_connected](websocketpp::connection_hdl) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "❌ [" << instrument << "] WebSocket connection failed. Will retry.\n";
                is_connected = false;
            });

            c.set_close_handler([&instrument, &is_connected](websocketpp::connection_hdl) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "🔌 [" << instrument << "] WebSocket closed. Will reconnect.\n";
                is_connected = false;
            });

            websocketpp::lib::error_code ec;
            client::connection_ptr con = c.get_connection(uri, ec);
            if (ec) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "❌ [" << instrument << "] Connection setup failed: " << ec.message() << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            c.connect(con);

            // Start reconnection timer thread (30 minutes)
            bool timer_thread_running = true;
            auto connection_start_time = std::chrono::steady_clock::now();
            const std::chrono::minutes RECONNECT_INTERVAL(30);
            
            std::thread timer_thread([&instrument, &c, con, &is_connected, &timer_thread_running, RECONNECT_INTERVAL, connection_start_time]() {
                while (timer_thread_running && is_connected) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    
                    if (!timer_thread_running || !is_connected) break;
                    
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = now - connection_start_time;
                    
                    if (elapsed >= RECONNECT_INTERVAL) {
                        std::lock_guard<std::mutex> lock(output_mutex);
                        std::cout << "🔄 [" << instrument << "] 30 minutes elapsed. Forcing reconnection for connection refresh.\n";
                        
                        try {
                            c.close(con, websocketpp::close::status::going_away, "Scheduled reconnection");
                        } catch (const std::exception& e) {
                            std::lock_guard<std::mutex> lock2(output_mutex);
                            std::cerr << "❌ [" << instrument << "] Error closing connection on scheduled reconnection: " 
                                      << e.what() << "\n";
                        }
                        is_connected = false;
                        break;
                    }
                }
            });

            try {
                c.run(); // blocks until connection closes
            } catch (const websocketpp::exception& e) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "❌ [" << instrument << "] WebSocket++ exception: " << e.what() << "\n";
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "❌ [" << instrument << "] Exception in run(): " << e.what() << "\n";
            }

            is_connected = false;
            timer_thread_running = false;
            
            // Wait for timer thread to finish
            if (timer_thread.joinable()) {
                timer_thread.join();
            }

        } catch (const std::bad_alloc& e) {
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cerr << "💾 [" << instrument << "] Memory allocation error: " << e.what() << "\n";
        } catch (const std::system_error& e) {
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cerr << "🔧 [" << instrument << "] System error: " << e.what() << " (code: " << e.code() << ")\n";
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cerr << "🚨 [" << instrument << "] Exception in WebSocket loop: " << e.what() << "\n";
        } catch (...) {
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cerr << "⚠️ [" << instrument << "] Unknown exception in WebSocket loop\n";
        }

        // Check if we should stop before reconnecting
        {
            std::lock_guard<std::mutex> lock(connection_states_mutex);
            if (connection_states.find(instrument) != connection_states.end() && 
                connection_states[instrument]->getShouldStop()) {
                break;
            }
        }

        // Exponential backoff for reconnection
        static thread_local int retry_count = 0;
        int delay = std::min(2 * (1 << retry_count), 30); // Max 30 seconds
        retry_count = (retry_count + 1) % 5; // Reset after 5 attempts to avoid overflow
        
        std::this_thread::sleep_for(std::chrono::seconds(delay));
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "🔁 [" << instrument << "] Attempting reconnect in " << delay << " seconds...\n";
    }
    
    // Clean up connection state
    {
        std::lock_guard<std::mutex> lock(connection_states_mutex);
        connection_states.erase(instrument);
    }
}

void WebSocketClient::connect(const std::vector<std::string>& instruments) {
    if (instruments.empty()) {
        std::cerr << "❌ No instruments provided\n";
        return;
    }

    // Limit to maximum 10 connections
    size_t max_connections = std::min(instruments.size(), static_cast<size_t>(10));
    
    std::cout << "🚀 Starting " << max_connections << " WebSocket connections...\n";
    
    // Initialize orderbooks and connection states for each instrument
    {
        std::lock_guard<std::mutex> lock(orderbook_mutex);
        std::lock_guard<std::mutex> state_lock(connection_states_mutex);
        for (size_t i = 0; i < max_connections; ++i) {
            global_orderbooks[instruments[i]] = OrderBook();
            connection_states[instruments[i]] = std::make_unique<ConnectionState>();
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