#include "Server.h"

Server::Server() : SERVER_PORT(8080) {}

Server::Server(int puerto) : SERVER_PORT(puerto) {}

Server::~Server() {
    closeServer();
}

void Server::configServer() {
    rtc::InitLogger(rtc::LogLevel::Warning);

    rtc::WebSocketServer::Configuration wsConfig;
    wsConfig.port = static_cast<uint16_t>(SERVER_PORT);
    wsConfig.enableTls = false;
    
    // PERMITIR CONEXIONES DESDE PROXIES INVERSOS (RENDER)
    wsConfig.bindAddress = "0.0.0.0";

    wss = std::make_unique<rtc::WebSocketServer>(wsConfig);

    std::cout << "Servidor de Signaling WebRTC escuchando en 0.0.0.0:" << SERVER_PORT << std::endl;
}

void Server::onClient(std::function<void(std::shared_ptr<rtc::WebSocket>)> handler) {
    if (wss) {
        wss->onClient(handler);
    }
}

void Server::closeServer() {
    if (wss) {
        wss.reset();
        std::cout << "Servidor cerrado correctamente." << std::endl;
    }
}