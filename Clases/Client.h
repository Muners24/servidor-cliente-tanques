#pragma once

#include "t.h"

class Client
{
private:
    std::string ip;
    int puerto;

    std::shared_ptr<rtc::WebSocket> ws;
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel> dc;

    std::function<void(const std::vector<std::byte>&)> messageHandler;
    bool connected;

public:
    int id;

    Client();
    Client(std::string ip, int puerto);
    ~Client();

    void configConexion();
    void close();

    void sendData(const void* data, size_t size);
    void onBinaryMessage(std::function<void(const std::vector<std::byte>&)> handler);

    int getId() const { return id; }
    bool isConnected() const { return connected && dc && dc->isOpen(); }
};