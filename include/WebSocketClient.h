#pragma once

#include <string>
#include <vector>
#include "trading/OrderBook.h"

class WebSocketClient {
public:
    void connect(const std::vector<std::string>& instruments);
    
    // Static methods to access orderbooks from anywhere
    static OrderBook getOrderBook(const std::string& instrument);
    static std::vector<std::string> getAvailableInstruments();
};