#pragma once
#include <iostream>
#include <memory>
#include <functional>
#include "rtc/rtc.hpp"

class Server {
private:
    int SERVER_PORT;
    std::unique_ptr<rtc::WebSocketServer> wss;

public:
    Server();
    Server(int puerto);
    ~Server();

    void configServer();
    
    void onClient(std::function<void(std::shared_ptr<rtc::WebSocket>)> handler);

    rtc::WebSocketServer* getWSServer() const { return wss.get(); }
    void closeServer();
};