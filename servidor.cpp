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

// Estructura de sesión por cliente WebRTC
struct ClientSession {
    int id = -1;
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel> dc;
    std::shared_ptr<rtc::WebSocket> ws;
};

// Firmas adaptadas para WebRTC
void setupServerHandlers();
void sendData(ClientSession& session, const void* data, size_t size);

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

// Contenedores del juego
queue<SoundMsg> cola_sonidos;
list<Proyectil> proyectiles;
list<Obstaculo> obstaculos;
list<Tanque *> tanques;
list<SoundMsg> sonidos;
list<int> clients_id;
vector<Zona> zonas;
std::mutex mtx;

// Contenedores de sesiones de red WebRTC
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

    std::cout << "Servidor iniciado en modo WebRTC UDP DataChannel." << std::endl;

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.closeServer();
    if (threadJuego.joinable()) threadJuego.join();

    return 0;
}

// Función auxiliar para emitir buffers binarios sobre el DataChannel de la sesión
void sendData(ClientSession& session, const void* data, size_t size) {
    if (session.dc && session.dc->isOpen()) {
        const auto* bytePtr = reinterpret_cast<const std::byte*>(data);
        rtc::binary binData(bytePtr, bytePtr + size);
        session.dc->send(binData);
    }
}

void setupServerHandlers()
{
    server.onClient([](std::shared_ptr<rtc::WebSocket> ws) {
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

        // 1. Configurar STUN e IceTcp
        rtc::Configuration rtcConfig;
        rtcConfig.enableIceTcp = true; // Permite TCP si UDP está bloqueado
        rtcConfig.iceServers.emplace_back("stun:stun.l.google.com:19302"); // Servidor STUN de Google

        session->pc = std::make_shared<rtc::PeerConnection>(rtcConfig);

        // 2. Mapeo de candidatos ICE (los callbacks se mantienen igual)
        session->pc->onLocalCandidate([ws](rtc::Candidate candidate) {
            if (ws && ws->isOpen()) {
                ws->send("CAND " + candidate.candidate() + "|" + candidate.mid());
            }
        });

        session->pc->onLocalDescription([ws](rtc::Description description) {
            if (ws && ws->isOpen()) {
                ws->send("SDP " + std::string(description));
            }
        });

        // Escuchar la creación del DataChannel proveniente del cliente
        session->pc->onDataChannel([session](std::shared_ptr<rtc::DataChannel> incomingDc) {
            session->dc = incomingDc;

            session->dc->onOpen([session]() {
                std::cout << "[SERVER] Cliente " << session->id << " conectado vía DataChannel UDP." << std::endl;
                sendId(*session);
            });

            session->dc->onMessage([session](std::variant<rtc::binary, rtc::string> message) {
                if (std::holds_alternative<rtc::binary>(message)) {
                    const auto& rawData = std::get<rtc::binary>(message);
                    if (rawData.empty()) return;

                    uint8_t header = std::to_integer<uint8_t>(rawData[0]);
                    switch (header)
                    {
                    case SEND_OBSTACULOS:
                        sendObstaculos(*session);
                        break;
                    case SEND_PROYECTILES:
                        sendProyectiles(*session);
                        break;
                    case SEND_SONIDOS:
                        sendSonidos(*session, rawData);
                        break;
                    case SEND_TANQUES:
                        sendTanques(*session);
                        break;
                    case SEND_ZONAS:
                        sendZonas(*session);
                        break;
                    case SEND_ID:
                        sendId(*session);
                        break;
                    case SEND_INPUT:
                        recvInput(*session, rawData);
                        break;
                    case SEND_COLOR:
                        recvColor(*session, rawData);
                        break;
                    case SEND_DISCONNECT:
                        recvDiscon(*session, rawData);
                        break;
                    default:
                        break;
                    }
                }
            });

            session->dc->onClosed([session]() {
                std::vector<std::byte> dummy;
                recvDiscon(*session, dummy);
            });
        });

        // Recibir la Oferta (Offer) del cliente y enviar Respuesta (Answer)
        ws->onMessage([session](std::variant<rtc::binary, rtc::string> message) {
            if (std::holds_alternative<rtc::string>(message)) {
                std::string msg = std::get<rtc::string>(message);
                if (msg.rfind("SDP ", 0) == 0) {
                    session->pc->setRemoteDescription(rtc::Description(msg.substr(4), "offer"));
                } else if (msg.rfind("CAND ", 0) == 0) {
                    std::string data = msg.substr(5);
                    auto pos = data.find('|');
                    if (pos != std::string::npos) {
                        session->pc->addRemoteCandidate(rtc::Candidate(data.substr(0, pos), data.substr(pos + 1)));
                    }
                }
            }
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
    sendData(session, pack_tanque.data(), len * sizeof(_Tanque));
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
        sendData(session, pack_proyectil.data(), len * sizeof(_Proyectil));
    }
    else
    {
        int no_send = 0;
        sendData(session, &no_send, sizeof(int));
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
    sendData(session, pack_zona.data(), len * sizeof(_Zona));
}

void sendObstaculos(ClientSession& session)
{
    std::lock_guard<std::mutex> lock(mtx);
    size_t len = obstaculos.size();
    if (len == 0) return;

    std::vector<_Obstaculo> pack_obstaculo(len);
    size_t i = 0;
    for (auto &obstaculo : obstaculos)
    {
        pack_obstaculo[i++] = obstaculo.toStruct();
    }
    sendData(session, pack_obstaculo.data(), len * sizeof(_Obstaculo));
}

void sendSonidos(ClientSession& session, const std::vector<std::byte>& rawData)
{
    std::lock_guard<std::mutex> lock(mtx);
    int clientId = session.id;

    // Si el payload recibido adjunta un ID específico
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
        sendData(session, temp_sonidos.data(), temp_sonidos.size() * sizeof(_SoundMsg));
    }
    else
    {
        int len = 0;
        sendData(session, &len, sizeof(int));
    }
}

void sendId(ClientSession& session)
{
    sendData(session, &session.id, sizeof(int));
}

void recvColor(ClientSession& session, const std::vector<std::byte>& rawData)
{
    if (rawData.size() < 1 + sizeof(_PeticionTanque)) return;

    _PeticionTanque pt;
    std::memcpy(&pt, rawData.data() + 1, sizeof(_PeticionTanque));

    std::lock_guard<std::mutex> lock(mtx);
    tanques.push_back(new Tanque(pt.color, pt.id));
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
        std::this_thread::sleep_for(std::chrono::milliseconds(8));

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