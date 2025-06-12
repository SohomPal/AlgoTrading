#include "WebSocketClient.h"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include <json.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>


using json = nlohmann::json;
typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

std::unordered_map<std::string, std::string> loadConfig(const std::string& path);

void WebSocketClient::connect(const std::string& uri) {
    client c;

    c.init_asio();

    c.set_tls_init_handler([](websocketpp::connection_hdl) {
        return websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::tlsv12_client);
    });

    c.set_open_handler([&c](websocketpp::connection_hdl hdl) {
        std::cout << "Connected to sFOX WebSocket.\n";

        // Load API key from config file
        auto config = loadConfig("config.cfg");
        //std::string token = config["API_KEY"];
        std::string token = "ee40b218493863711d0f6044d3a8f46e4ee5bce50e99eae5eabd8434182590f6";

        // Send auth message
        json auth = {
            {"type", "authenticate"},
            {"apiKey", token}
        };
        c.send(hdl, auth.dump(), websocketpp::frame::opcode::text);

        sleep(1); // Wait for authentication to complete

        // Subscribe after authentication
        json subscribe = {
            {"type", "subscribe"},
            {"feeds", {"orderbook.sfox.ethbtc"}}
        };
        c.send(hdl, subscribe.dump(), websocketpp::frame::opcode::text);
    });

    const size_t DEPTH_LIMIT = 10;

    c.set_message_handler([&](websocketpp::connection_hdl, message_ptr msg) {
        try {
            const std::string& payload_str = msg->get_payload();
            auto payload = json::parse(payload_str);

            std::string type = payload.value("type", "unknown");

            auto print_levels = [DEPTH_LIMIT](const json& levels, bool is_bid) {
                std::vector<json> sorted_levels = levels;

                std::sort(sorted_levels.begin(), sorted_levels.end(), [is_bid](const json& a, const json& b) {
                    return is_bid ? (a[0].get<double>() > b[0].get<double>())
                                : (a[0].get<double>() < b[0].get<double>());
                });

                for (size_t i = 0; i < std::min(sorted_levels.size(), DEPTH_LIMIT); ++i) {
                    const auto& level = sorted_levels[i];
                    std::cout << (is_bid ? "  Bid" : "  Ask")
                            << ": " << level[0] << " x " << level[1] << "\n";
                }
            };

            if (payload.contains("payload") &&
                (payload["payload"].contains("bids") || payload["payload"].contains("asks"))) {

                const auto& data = payload["payload"];
                std::cout << "\n📈 Orderbook Update\n";

                if (data.contains("bids")) {
                    std::cout << "Top Bids:\n";
                    print_levels(data["bids"], true);
                }
                if (data.contains("asks")) {
                    std::cout << "Top Asks:\n";
                    print_levels(data["asks"], false);
                }
            } else {
                std::cout << "\n🔍 Non-orderbook message received:\n";
                std::cout << payload.dump(2) << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ JSON parse error: " << e.what() << "\n";
        }
    });




    websocketpp::lib::error_code ec;
    client::connection_ptr con = c.get_connection(uri, ec);
    if (ec) {
        std::cerr << "Connection error: " << ec.message() << "\n";
        return;
    }

    c.connect(con);
    c.run();
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
