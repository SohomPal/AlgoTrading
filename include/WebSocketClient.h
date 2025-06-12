#pragma once

#include <string>
#include <vector>

class WebSocketClient {
public:
    void connect(const std::vector<std::string>& instruments);
};