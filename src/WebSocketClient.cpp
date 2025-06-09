#include "WebSocketClient.h"

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_STL_

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

        // Subscribe after authentication
        json subscribe = {
            {"type", "subscribe"},
            {"feeds", {"orderbook.sfox.ethbtc"}}
        };
        c.send(hdl, subscribe.dump(), websocketpp::frame::opcode::text);
    });

    c.set_message_handler([](websocketpp::connection_hdl, message_ptr msg) {
        try {
            const std::string& payload_str = msg->get_payload();
            auto payload = json::parse(payload_str);

            std::string type = payload.value("type", "unknown");

            if (type == "snapshot") {
                std::cout << "Received snapshot for: " << payload["product_id"] << "\n";
                std::cout << "Top bid: " << payload["bids"][0][0] << " x " << payload["bids"][0][1] << "\n";
                std::cout << "Top ask: " << payload["asks"][0][0] << " x " << payload["asks"][0][1] << "\n";
            }
            else if (type == "update") {
                std::cout << "Update received for: " << payload["product_id"] << "\n";
                if (payload.contains("bids")) {
                    for (const auto& level : payload["bids"]) {
                        std::cout << "  Bid: " << level[0] << " x " << level[1] << "\n";
                    }
                }
                if (payload.contains("asks")) {
                    for (const auto& level : payload["asks"]) {
                        std::cout << "  Ask: " << level[0] << " x " << level[1] << "\n";
                    }
                }
            }
            else {
                std::cout << "Other message: " << type << "\n";
                std::cout << payload.dump(2) << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "JSON parse error: " << e.what() << "\n";
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
