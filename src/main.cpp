#include "WebSocketClient.h"

int main() {
    WebSocketClient client;
    client.connect("wss://ws.sfox.com/ws");
    return 0;
}
