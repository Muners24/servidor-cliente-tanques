#include "Client.h"

Client::Client() : ip(IP), puerto(PUERTO), id(-1), connected(false) {}

Client::Client(std::string ip, int puerto) : ip(ip), puerto(puerto), id(-1), connected(false) {}

Client::~Client()
{
    close();
}

void Client::configConexion()
{
    rtc::Configuration rtcConfig;
    rtcConfig.enableIceTcp = false;

    pc = std::make_shared<rtc::PeerConnection>(rtcConfig);
    ws = std::make_shared<rtc::WebSocket>();

    pc->onLocalCandidate([this](rtc::Candidate candidate) {
        if (ws && ws->isOpen()) {
            ws->send("CAND " + candidate.candidate() + "|" + candidate.mid());
        }
    });

    pc->onLocalDescription([this](rtc::Description description) {
        if (ws && ws->isOpen()) {
            ws->send("SDP " + std::string(description));
        }
    });

    ws->onMessage([this](std::variant<rtc::binary, rtc::string> message) {
        if (std::holds_alternative<rtc::string>(message)) {
            std::string msg = std::get<rtc::string>(message);
            if (msg.rfind("SDP ", 0) == 0) {
                pc->setRemoteDescription(rtc::Description(msg.substr(4), "answer"));
            } else if (msg.rfind("CAND ", 0) == 0) {
                std::string data = msg.substr(5);
                auto pos = data.find('|');
                if (pos != std::string::npos) {
                    pc->addRemoteCandidate(rtc::Candidate(data.substr(0, pos), data.substr(pos + 1)));
                }
            }
        }
    });

    ws->onOpen([this]() {
        rtc::DataChannelInit dcInit;
        dcInit.reliability.unordered = true;
        dc = pc->createDataChannel("game-data", dcInit);

        dc->onOpen([this]() {
            this->connected = true;
            std::cout << "[CLIENTE] Conectado exitosamente via WebRTC DataChannel UDP." << std::endl;
        });

        dc->onClosed([this]() {
            this->connected = false;
        });

        dc->onMessage([this](std::variant<rtc::binary, rtc::string> message) {
            if (std::holds_alternative<rtc::binary>(message) && this->messageHandler) {
                this->messageHandler(std::get<rtc::binary>(message));
            }
        });
    });

    std::string wsUrl = "ws://" + ip + ":" + std::to_string(puerto);
    ws->open(wsUrl);
}

void Client::onBinaryMessage(std::function<void(const std::vector<std::byte>&)> handler)
{
    this->messageHandler = handler;
}

void Client::sendData(const void* data, size_t size)
{
    // Reintentar brevemente si el canal está conectando
    int retries = 0;
    while ((!dc || !dc->isOpen()) && retries < 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        retries++;
    }

    if (dc && dc->isOpen()) {
        const auto* bytePtr = reinterpret_cast<const std::byte*>(data);
        rtc::binary binData(bytePtr, bytePtr + size);
        dc->send(binData);
    }
}

void Client::close()
{
    connected = false;
    if (dc) dc->close();
    if (pc) pc->close();
    if (ws) ws->close();
}