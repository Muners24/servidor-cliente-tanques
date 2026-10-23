#include "t.h"
#include "Clases/Proyectil.h"
#include "Clases/Tanque.h"
#include "Clases/Server.h"
#include "Clases/Obstaculo.h"
#include "Clases/SoundMsg.h"
#include "Clases/Zona.h"
#include "init.h"

#include <unordered_map>
#include <memory>
#include <vector>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <iostream>

struct ClientSession {
    int id = -1;
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel> dc;
    std::shared_ptr<rtc::WebSocket> ws;
};

void setupServerHandlers();
void sendDataWithHeader(ClientSession& session, uint8_t header, const void* data, size_t size);

void sendProyectiles(ClientSession& session);
void sendObstaculos(ClientSession& session);
void sendTanques(ClientSession& session);
void sendSonidos(ClientSession& session, const std::vector<std::byte>& rawData);
void sendZonas(ClientSession& session);
void sendId(ClientSession& session);

void recvDiscon(ClientSession& session, const std::vector<std::byte>& rawData);
void recvColor(ClientSession& session, const std::vector<std::byte>& rawData);
void recvInput(ClientSession& session, const std::vector<std::byte>& rawData);

void updateJuego();

queue<SoundMsg> cola_sonidos;
list<Proyectil> proyectiles;
list<Obstaculo> obstaculos;
list<Tanque *> tanques;
list<SoundMsg> sonidos;
list<int> clients_id;
vector<Zona> zonas;
std::mutex mtx;

std::unordered_map<int, std::shared_ptr<ClientSession>> active_sessions;
std::mutex mtx_sessions;

int cont_tanque = 0;
Server server(PUERTO);

int main()
{
    cont_tanque = 0;

    try
    {
        server.configServer();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    setupServerHandlers();

    thread threadJuego(updateJuego);

    std::cout << "[SERVIDOR] Iniciado y escuchando peticiones WebRTC UDP DataChannel." << std::endl;

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.closeServer();
    if (threadJuego.joinable()) threadJuego.join();

    return 0;
}

void sendDataWithHeader(ClientSession& session, uint8_t header, const void* data, size_t size) {
    if (session.dc && session.dc->isOpen()) {
        std::vector<std::byte> packet(1 + size);
        packet[0] = static_cast<std::byte>(header);
        if (size > 0 && data != nullptr) {
            std::memcpy(packet.data() + 1, data, size);
        }
        rtc::binary binData(packet.begin(), packet.end());
        session.dc->send(binData);
    }
}

void setupServerHandlers()
{
    server.onClient([](std::shared_ptr<rtc::WebSocket> ws) {
        // 1. Manejadores de errores y cierre directo del socket entrante
        ws->onError([](std::string error) {
            // Se ignoran los errores de peticiones HTTP no-WebSocket (pings de Render)
        });

        ws->onClosed([]() {
            // Cierre silencioso si el handshake no se completó
        });

        // 2. ÚNICAMENTE cuando el WebSocket complete el Handshake HTTP exitosamente
        ws->onOpen([ws]() {
            auto session = std::make_shared<ClientSession>();
            session->ws = ws;

            {
                std::lock_guard<std::mutex> lock(mtx);
                session->id = cont_tanque++;
                clients_id.push_back(session->id);
            }

            {
                std::lock_guard<std::mutex> lock(mtx_sessions);
                active_sessions[session->id] = session;
            }

            std::cout << "[LOG-WS] ===== NUEVA CONEXIÓN WEBSOCKET ESTABLECIDA =====" << std::endl;
            std::cout << "[LOG-WS] Sesión creada exitosamente y asignada a ID: " << session->id << std::endl;

            // Configuración del PeerConnection tras validar la conexión WS
            rtc::Configuration rtcConfig;
            rtcConfig.enableIceTcp = true;
            rtcConfig.iceServers.emplace_back("stun:stun.l.google.com:19302");

            session->pc = std::make_shared<rtc::PeerConnection>(rtcConfig);

            // Callback de candidatos ICE locales
            session->pc->onLocalCandidate([ws, id = session->id](rtc::Candidate candidate) {
                if (ws && ws->isOpen()) {
                    ws->send("CAND " + candidate.candidate() + "|" + candidate.mid());
                }
            });

            // Callback de SDP local (Answer)
            session->pc->onLocalDescription([ws, id = session->id](rtc::Description description) {
                if (ws && ws->isOpen()) {
                    ws->send("SDP " + std::string(description));
                }
            });

            // Callback cuando el cliente abre un DataChannel
            session->pc->onDataChannel([session](std::shared_ptr<rtc::DataChannel> incomingDc) {
                session->dc = incomingDc;

                session->dc->onOpen([session]() {
                    std::cout << "[LOG-DC] DataChannel ABIERTO para Cliente ID: " << session->id << std::endl;
                    sendId(*session);
                });

                session->dc->onMessage([session](std::variant<rtc::binary, rtc::string> message) {
                    if (std::holds_alternative<rtc::binary>(message)) {
                        const auto& rawData = std::get<rtc::binary>(message);
                        if (rawData.empty()) return;

                        uint8_t header = std::to_integer<uint8_t>(rawData[0]);
                        switch (header)
                        {
                        case SEND_OBSTACULOS:  sendObstaculos(*session); break;
                        case SEND_PROYECTILES: sendProyectiles(*session); break;
                        case SEND_SONIDOS:    sendSonidos(*session, rawData); break;
                        case SEND_TANQUES:    sendTanques(*session); break;
                        case SEND_ZONAS:      sendZonas(*session); break;
                        case SEND_ID:         sendId(*session); break;
                        case SEND_INPUT:      recvInput(*session, rawData); break;
                        case SEND_COLOR:      recvColor(*session, rawData); break;
                        case SEND_DISCONNECT: recvDiscon(*session, rawData); break;
                        default: break;
                        }
                    }
                });

                session->dc->onClosed([session]() {
                    std::cout << "[LOG-DC] DataChannel CERRADO para Cliente ID: " << session->id << std::endl;
                    std::vector<std::byte> dummy;
                    recvDiscon(*session, dummy);
                });
            });

            // Procesamiento de mensajes de Signaling entrantes
            ws->onMessage([session](std::variant<rtc::binary, rtc::string> message) {
                if (std::holds_alternative<rtc::string>(message)) {
                    std::string msg = std::get<rtc::string>(message);
                    
                    if (msg.rfind("SDP ", 0) == 0) {
                        std::cout << "[LOG-MSG] Oferta SDP recibida del Cliente ID: " << session->id << std::endl;
                        session->pc->setRemoteDescription(rtc::Description(msg.substr(4), "offer"));
                        session->pc->setLocalDescription();
                    } else if (msg.rfind("CAND ", 0) == 0) {
                        std::cout << "[LOG-MSG] Candidato ICE recibido del Cliente ID: " << session->id << std::endl;
                        std::string data = msg.substr(5);
                        auto pos = data.find('|');
                        if (pos != std::string::npos) {
                            session->pc->addRemoteCandidate(rtc::Candidate(data.substr(0, pos), data.substr(pos + 1)));
                        }
                    }
                }
            });

            // Evento de cierre para sesiones activas confirmadas
            ws->onClosed([id = session->id]() {
                std::cout << "[LOG-WS] Conexión WebSocket CERRADA para Cliente ID: " << id << std::endl;
            });
        });
    });
}

void sendTanques(ClientSession& session)
{
    std::lock_guard<std::mutex> lock(mtx);
    size_t len = tanques.size();
    if (len == 0) return;

    std::vector<_Tanque> pack_tanque(len);
    size_t i = 0;
    for (auto &tanque : tanques)
    {
        pack_tanque[i++] = tanque->toStruct();
    }
    sendDataWithHeader(session, SEND_TANQUES, pack_tanque.data(), len * sizeof(_Tanque));
}

void sendProyectiles(ClientSession& session)
{
    std::lock_guard<std::mutex> lock(mtx);
    size_t len = proyectiles.size();
    if (len > 0)
    {
        std::vector<_Proyectil> pack_proyectil(len);
        size_t i = 0;
        for (auto &proyectil : proyectiles)
        {
            pack_proyectil[i++] = proyectil.toStruct();
        }
        sendDataWithHeader(session, SEND_PROYECTILES, pack_proyectil.data(), len * sizeof(_Proyectil));
    }
    else
    {
        int no_send = 0;
        sendDataWithHeader(session, SEND_PROYECTILES, &no_send, sizeof(int));
    }
}

void sendZonas(ClientSession& session)
{
    std::lock_guard<std::mutex> lock(mtx);
    size_t len = zonas.size();
    if (len == 0) return;

    std::vector<_Zona> pack_zona(len);
    size_t i = 0;
    for (auto &zona : zonas)
    {
        pack_zona[i++] = zona.toStruct();
    }
    sendDataWithHeader(session, SEND_ZONAS, pack_zona.data(), len * sizeof(_Zona));
}

void sendObstaculos(ClientSession& session)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (obstaculos.empty()) {
        initObstaculo(obstaculos);
    }
    
    size_t len = obstaculos.size();
    std::vector<_Obstaculo> pack_obstaculo(len);
    size_t i = 0;
    for (auto &obstaculo : obstaculos)
    {
        pack_obstaculo[i++] = obstaculo.toStruct();
    }
    std::cout << "[SERVIDOR] Enviando " << len << " obstáculos a Cliente ID: " << session.id << std::endl;
    sendDataWithHeader(session, SEND_OBSTACULOS, pack_obstaculo.data(), len * sizeof(_Obstaculo));
}

void sendSonidos(ClientSession& session, const std::vector<std::byte>& rawData)
{
    std::lock_guard<std::mutex> lock(mtx);
    int clientId = session.id;

    if (rawData.size() >= 1 + sizeof(int)) {
        std::memcpy(&clientId, rawData.data() + 1, sizeof(int));
    }

    std::vector<_SoundMsg> temp_sonidos;
    for (auto &sonido : sonidos)
    {
        if (sonido.shouldPlay(clientId))
        {
            temp_sonidos.push_back(sonido.toStruct());
        }
    }

    if (!temp_sonidos.empty())
    {
        sendDataWithHeader(session, SEND_SONIDOS, temp_sonidos.data(), temp_sonidos.size() * sizeof(_SoundMsg));
    }
    else
    {
        int len = 0;
        sendDataWithHeader(session, SEND_SONIDOS, &len, sizeof(int));
    }
}

void sendId(ClientSession& session)
{
    std::cout << "[SERVIDOR] Asignando e informando ID " << session.id << " al cliente." << std::endl;
    sendDataWithHeader(session, SEND_ID, &session.id, sizeof(int));
}

void recvColor(ClientSession& session, const std::vector<std::byte>& rawData)
{
    if (rawData.size() < 1 + sizeof(_PeticionTanque)) return;

    _PeticionTanque pt;
    std::memcpy(&pt, rawData.data() + 1, sizeof(_PeticionTanque));

    std::lock_guard<std::mutex> lock(mtx);
    tanques.push_back(new Tanque(pt.color, pt.id));
    std::cout << "[SERVIDOR] Tanque creado e instanciado en el mundo para el Cliente ID: " << pt.id << std::endl;
}

void recvInput(ClientSession& session, const std::vector<std::byte>& rawData)
{
    if (rawData.size() < 1 + (7 * sizeof(int))) return;

    int bits[7];
    std::memcpy(bits, rawData.data() + 1, 7 * sizeof(int));

    std::lock_guard<std::mutex> lock(mtx);
    for (auto &tanque : tanques)
    {
        if (tanque->id == bits[6])
        {
            tanque->wasd.left  = (bits[0] == 1);
            tanque->wasd.up    = (bits[1] == 1);
            tanque->wasd.down  = (bits[2] == 1);
            tanque->wasd.right = (bits[3] == 1);
            tanque->click_b    = (bits[4] == 1);
            tanque->clickr_b   = (bits[5] == 1);
        }
    }
}

void recvDiscon(ClientSession& session, const std::vector<std::byte>& rawData)
{
    int targetId = session.id;
    if (rawData.size() >= 1 + sizeof(int)) {
        std::memcpy(&targetId, rawData.data() + 1, sizeof(int));
    }

    std::lock_guard<std::mutex> lock(mtx);
    auto it = tanques.begin();
    while (it != tanques.end())
    {
        if ((*it)->id == targetId)
        {
            delete *it;
            it = tanques.erase(it);
        }
        else
        {
            ++it;
        }
    }

    {
        std::lock_guard<std::mutex> lock_sess(mtx_sessions);
        active_sessions.erase(targetId);
    }
}

void updateJuego()
{
    initObstaculo(obstaculos);
    initZona(zonas);

    auto disp_it = proyectiles.begin();
    auto it = tanques.begin();
    auto s = sonidos.begin();

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));

        mtx.lock();
        s = sonidos.begin();
        while (s != sonidos.end())
        {
            if (s->readyToRemove())
            {
                s = sonidos.erase(s);
            }
            else
            {
                s++;
            }
        }
        mtx.unlock();

        mtx.lock();
        it = tanques.begin();
        while (it != tanques.end())
        {
            if (!(*it)->should_del)
            {
                (*it)->update(obstaculos);

                try
                {
                    Proyectil nuevo = (*it)->disparar();
                    proyectiles.push_front(nuevo);
                    sonidos.push_back(SoundMsg(DISP, clients_id, (*it)->getCentro()));
                }
                catch (const std::exception &e)
                {
                }

                if ((*it)->colisionProyectiles(proyectiles))
                {
                    (*it)->danio();
                    sonidos.push_back(SoundMsg(DANIO, clients_id, (*it)->getCentro()));
                }

                if ((*it)->escudo())
                {
                    sonidos.push_back(SoundMsg(SHIELD, clients_id, (*it)->getCentro()));
                }

                it++;
            }
            else
            {
                delete *it;
                it = tanques.erase(it);
            }
        }

        disp_it = proyectiles.begin();
        while (disp_it != proyectiles.end())
        {
            if (!(*disp_it).should_del)
            {
                (*disp_it).update(obstaculos);
                disp_it++;
            }
            else
            {
                disp_it = proyectiles.erase(disp_it);
            }
        }
        mtx.unlock();

        for (auto &zona : zonas)
        {
            zona.update(tanques);
        }
    }
}