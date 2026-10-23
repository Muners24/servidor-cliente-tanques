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
    wsConfig.enableTls = false; // Render gestiona el cifrado TLS externamente

    wss = std::make_unique<rtc::WebSocketServer>(wsConfig);

    std::cout << "Servidor de Signaling WebRTC escuchando en el puerto " << SERVER_PORT << std::endl;
}

void Server::onClient(std::function<void(std::shared_ptr<rtc::WebSocket>)> handler) {
    if (wss) {
        wss->onClient([handler](std::shared_ptr<rtc::WebSocket> ws) {
            // Configuración del WebSocket entrante para capturar la apertura exitosa
            ws->onOpen([ws]() {
                std::cout << "[LOG-WS] SUCCESS: Handshake completado exitosamente." << std::endl;
            });

            // Pasar el socket al manejador principal de la app
            if (handler) {
                handler(ws);
            }
        });
    }
}

void Server::closeServer() {
    if (wss) {
        wss.reset();
        std::cout << "Servidor cerrado correctamente." << std::endl;
    }
}