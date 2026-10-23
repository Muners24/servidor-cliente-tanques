#include <iostream>
#include <thread>
#include <chrono>
#include <variant>
#include <memory>
#include "rtc/rtc.hpp"

using std::cout;
using std::endl;
using std::shared_ptr;
using std::variant;
using std::get;
using std::holds_alternative;
namespace chrono = std::chrono;
namespace this_thread = std::this_thread;

int main() {
    rtc::InitLogger(rtc::LogLevel::Warning);

    rtc::Configuration config;

    // Crear cliente y servidor
    auto server = std::make_shared<rtc::PeerConnection>(config);
    auto client = std::make_shared<rtc::PeerConnection>(config);

    // Intercambio de candidatos ICE
    server->onLocalCandidate([client](rtc::Candidate candidate) {
        client->addRemoteCandidate(candidate);
    });

    client->onLocalCandidate([server](rtc::Candidate candidate) {
        server->addRemoteCandidate(candidate);
    });

    // Intercambio de descripciones SDP (Oferta / Respuesta)
    server->onLocalDescription([client](rtc::Description description) {
        client->setRemoteDescription(description);
    });

    client->onLocalDescription([server](rtc::Description description) {
        server->setRemoteDescription(description);
    });

    // Configurar canal de datos en el servidor
    server->onDataChannel([](shared_ptr<rtc::DataChannel> dc) {
        cout << "[SERVER] ¡Canal de datos entrante detectado!: " << dc->label() << endl;

        dc->onOpen([dc]() {
            cout << "[SERVER] Canal abierto. Enviando bienvenida al cliente..." << endl;
            dc->send("Hola Cliente, la conexión WebRTC funciona perfectamente!");
        });

        dc->onMessage([dc](variant<rtc::binary, rtc::string> message) {
            if (holds_alternative<rtc::string>(message)) {
                cout << "[SERVER] Respuesta recibida del cliente: " << get<rtc::string>(message) << endl;
            }
        });
    });

    // El cliente crea el canal de datos para iniciar la oferta
    auto dc = client->createDataChannel("chat");

    dc->onOpen([dc]() {
        cout << "[CLIENT] ¡Canal de datos abierto!" << endl;
        dc->send("Hola Servidor, confirmando recepción.");
    });

    dc->onMessage([](variant<rtc::binary, rtc::string> message) {
        if (holds_alternative<rtc::string>(message)) {
            cout << "[CLIENT] Mensaje recibido del servidor: " << get<rtc::string>(message) << endl;
        }
    });

    // Iniciar negociación WebRTC
    client->setLocalDescription();

    cout << "Estableciendo conexión WebRTC local..." << endl;
    this_thread::sleep_for(chrono::seconds(5));

    return 0;
}