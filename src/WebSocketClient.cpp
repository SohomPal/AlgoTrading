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

// Timeout tracking
std::unordered_map<std::string, std::atomic<std::chrono::steady_clock::time_point>> last_data_time;
std::mutex timeout_mutex;

void connectToInstrument(const std::string& instrument) {
    const std::string uri = "wss://ws.sfox.com/ws";
    const std::chrono::seconds DATA_TIMEOUT(5);
    
    // Initialize last data time
    {
        std::lock_guard<std::mutex> lock(timeout_mutex);
        last_data_time[instrument].store(std::chrono::steady_clock::now());
    }

    while (true) {
        try {
            client c;
            c.init_asio();
            
            // Set access and error log levels to reduce noise
            c.set_access_channels(websocketpp::log::alevel::none);
            c.set_error_channels(websocketpp::log::elevel::none);

            c.set_tls_init_handler([&instrument](websocketpp::connection_hdl) {
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

            // Flag to track if we're connected and authenticated
            auto connected = std::make_shared<std::atomic<bool>>(false);
            auto authenticated = std::make_shared<std::atomic<bool>>(false);

            c.set_open_handler([&c, &instrument, connected, authenticated](websocketpp::connection_hdl hdl) {
                try {
                    {
                        std::lock_guard<std::mutex> lock(output_mutex);
                        std::cout << "🔗 Connected to sFOX WebSocket for instrument: " << instrument << "\n";
                    }
                    
                    connected->store(true);
                    
                    // Update last data time on connection
                    {
                        std::lock_guard<std::mutex> lock(timeout_mutex);
                        last_data_time[instrument].store(std::chrono::steady_clock::now());
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
                    
                    authenticated->store(true);
                    
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    std::cerr << "❌ [" << instrument << "] Error in open handler: " << e.what() << "\n";
                    connected->store(false);
                }
            });

            const size_t DEPTH_LIMIT = 10;

            c.set_message_handler([&, instrument, connected, authenticated](websocketpp::connection_hdl hdl, message_ptr msg) {
                try {
                    // Update last data time
                    {
                        std::lock_guard<std::mutex> lock(timeout_mutex);
                        last_data_time[instrument].store(std::chrono::steady_clock::now());
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
                            c.close(hdl, websocketpp::close::status::protocol_error, "Auth failed");
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

            c.set_fail_handler([&instrument, &connected](websocketpp::connection_hdl) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "❌ [" << instrument << "] WebSocket connection failed. Will retry.\n";
                connected->store(false);
            });

            c.set_close_handler([&instrument, &connected](websocketpp::connection_hdl) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "🔌 [" << instrument << "] WebSocket closed. Will reconnect.\n";
                connected->store(false);
            });

            // Remove the interrupt handler as it's not compatible with this websocketpp version
            // The connection will be monitored by the timeout thread instead

            websocketpp::lib::error_code ec;
            client::connection_ptr con = c.get_connection(uri, ec);
            if (ec) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "❌ [" << instrument << "] Connection setup failed: " << ec.message() << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            c.connect(con);

            // Start timeout monitoring thread
            std::thread timeout_thread([&instrument, &c, con, connected, authenticated, DATA_TIMEOUT]() {
                while (connected->load()) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    
                    if (!connected->load()) break;
                    
                    auto now = std::chrono::steady_clock::now();
                    std::chrono::steady_clock::time_point last_time;
                    
                    {
                        std::lock_guard<std::mutex> lock(timeout_mutex);
                        last_time = last_data_time[instrument].load();
                    }
                    
                    if (now - last_time > DATA_TIMEOUT) {
                        std::lock_guard<std::mutex> lock(output_mutex);
                        std::cerr << "⏰ [" << instrument << "] No data received for " 
                                  << std::chrono::duration_cast<std::chrono::seconds>(now - last_time).count() 
                                  << " seconds. Forcing reconnection.\n";
                        
                        try {
                            c.close(con, websocketpp::close::status::going_away, "Data timeout");
                        } catch (const std::exception& e) {
                            std::lock_guard<std::mutex> lock2(output_mutex);
                            std::cerr << "❌ [" << instrument << "] Error closing connection on timeout: " 
                                      << e.what() << "\n";
                        }
                        connected->store(false);
                        break;
                    }
                }
            });

            try {
                c.run(); // blocks until connection closes
            } catch (const websocketpp::exception& e) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "❌ [" << instrument << "] WebSocket++ exception: " << e.what() << "\n";
            } catch (const asio::system_error& e) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "❌ [" << instrument << "] ASIO system error: " << e.what() << " (code: " << e.code() << ")\n";
            } catch (const std::runtime_error& e) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "❌ [" << instrument << "] Runtime error: " << e.what() << "\n";
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "❌ [" << instrument << "] Unexpected exception: " << e.what() << "\n";
            }

            connected->store(false);
            
            // Wait for timeout thread to finish
            if (timeout_thread.joinable()) {
                timeout_thread.join();
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

        // Exponential backoff for reconnection
        static thread_local int retry_count = 0;
        int delay = std::min(2 * (1 << retry_count), 30); // Max 30 seconds
        retry_count = (retry_count + 1) % 5; // Reset after 5 attempts to avoid overflow
        
        std::this_thread::sleep_for(std::chrono::seconds(delay));
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "🔁 [" << instrument << "] Attempting reconnect in " << delay << " seconds...\n";
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
    
    // Initialize orderbooks and timeout tracking for each instrument
    {
        std::lock_guard<std::mutex> lock(orderbook_mutex);
        std::lock_guard<std::mutex> timeout_lock(timeout_mutex);
        for (size_t i = 0; i < max_connections; ++i) {
            global_orderbooks[instruments[i]] = OrderBook();
            last_data_time[instruments[i]].store(std::chrono::steady_clock::now());
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