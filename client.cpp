#include "t.h"
#include "Clases/SoundMsg.h"
#include "Clases/Proyectil.h"
#include "Clases/Tanque.h"
#include "Clases/Client.h"
#include "Clases/Obstaculo.h"
#include "Clases/Zona.h"
#include <fstream>
#include <cstddef>
#include <cstring>
#include <iostream>

void playEffect(Sound s, Vector2 destino, Vector2 origen);
array<Texture2D, 3> obtenerTanque(Color color);
void drawBoton(char str[], int x, int y, bool select);
bool colorCmpS(Color c1, Color c2);
void initCamara(Camera2D &camara);
void initTexture();
void initSonido();
void drawMiniMapa(Tanque temp);
void drawEscudoCd(Tanque temp);
void setupNetworkCallbacks();
void gameLoopNetwork();
void drawSuelo();
void drawBorde();
void initColors();
void initZona();
void initInput();
void drawVida(Tanque temp);
void inputT();
void input();
void view();
void menu();
void centerCamera(Camera2D &camara, Vector2 point);

// Peticiones al servidor
void sendRequestHeader(uint8_t header);
void sendColor();
void sendInput();

// Procesadores de paquetes entrantes
void handleIncomingPacket(const std::vector<std::byte>& data);

int sel_color;
bool on_menu;
int op;

list<Proyectil> proyectiles;
list<Obstaculo> obstaculos;
queue<_SoundMsg> sonidos;
list<Tanque> tanques;
vector<Zona> zonas;
mutex mtx;

array<Texture2D, 3> t_red;
array<Texture2D, 3> t_blue;
array<Texture2D, 3> t_purple;
array<Texture2D, 3> t_cian;
array<Texture2D, 3> t_black_r;
array<Texture2D, 3> t_black_d;
array<Texture2D, 3> t_green;
array<Texture2D, 3> t_yellow;
Texture2D losa;
Texture2D borde;
Texture2D box;
Sound mov;
Sound danio;
Sound disp;
Sound shield;
vector<Color> colors;
string ms;

Camera2D camara;
int bits[7];

Client client;

int main()
{
    std::string targetIP = IP; 
    std::cout << "[CONFIG] Conectando directamente a la IP definida en t.h: " << targetIP << std::endl;

    client = Client(targetIP, PUERTO);

    initInput();
    try
    {
        client.configConexion();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    setupNetworkCallbacks();

    on_menu = true;
    thread threadJuego(gameLoopNetwork);

    view();

    if (threadJuego.joinable()) threadJuego.join();
    client.close();
    return 0;
}

void setupNetworkCallbacks()
{
    client.onBinaryMessage([](const std::vector<std::byte>& data) {
        handleIncomingPacket(data);
    });
}

void gameLoopNetwork()
{
    std::cout << "[CLIENTE] Hilo de red iniciado. Esperando conexión DataChannel..." << std::endl;
    while (!client.isConnected())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::cout << "[CLIENTE] DataChannel detectado como abierto. Solicitando ID..." << std::endl;

    while (client.id == -1 && client.isConnected())
    {
        std::cout << "[CLIENTE] Enviando petición SEND_ID..." << std::endl;
        sendRequestHeader(SEND_ID);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    std::cout << "[CLIENTE] ID asignado exitosamente: " << client.id << ". Solicitando obstáculos..." << std::endl;

    while (obstaculos.empty() && client.isConnected())
    {
        std::cout << "[CLIENTE] Enviando petición SEND_OBSTACULOS..." << std::endl;
        sendRequestHeader(SEND_OBSTACULOS);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    std::cout << "[CLIENTE] Obstáculos cargados (" << obstaculos.size() << "). Esperando que el usuario presione 'Jugar'..." << std::endl;

    while (on_menu && client.isConnected())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "[CLIENTE] 'Jugar' seleccionado. Registrando tanque en servidor con SEND_COLOR..." << std::endl;
    sendColor();

    while (client.isConnected())
    {
        auto inicio = std::chrono::high_resolution_clock::now();

        sendInput();
        sendRequestHeader(SEND_TANQUES);
        sendRequestHeader(SEND_ZONAS);
        sendRequestHeader(SEND_PROYECTILES);

        uint8_t soundReq[1 + sizeof(int)];
        soundReq[0] = SEND_SONIDOS;
        std::memcpy(soundReq + 1, &client.id, sizeof(int));
        client.sendData(soundReq, sizeof(soundReq));

        auto fin = std::chrono::high_resolution_clock::now();
        auto duracion = std::chrono::duration_cast<std::chrono::milliseconds>(fin - inicio);
        ms = "MS:" + to_string(duracion.count());

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void sendRequestHeader(uint8_t header)
{
    client.sendData(&header, sizeof(uint8_t));
}

void sendColor()
{
    _PeticionTanque peticion;
    peticion.color = colors[sel_color];
    peticion.id = client.id;

    std::vector<uint8_t> buffer(1 + sizeof(_PeticionTanque));
    buffer[0] = SEND_COLOR;
    std::memcpy(buffer.data() + 1, &peticion, sizeof(_PeticionTanque));

    client.sendData(buffer.data(), buffer.size());
}

void sendInput()
{
    inputT();
    std::vector<uint8_t> buffer(1 + sizeof(bits));
    buffer[0] = SEND_INPUT;
    std::memcpy(buffer.data() + 1, bits, sizeof(bits));

    client.sendData(buffer.data(), buffer.size());
}

void handleIncomingPacket(const std::vector<std::byte>& data)
{
    if (data.empty()) return;

    uint8_t header = std::to_integer<uint8_t>(data[0]);
    const std::byte* payload = data.data() + 1;
    size_t payloadSize = data.size() - 1;

    switch (header)
    {
    case SEND_ID:
    {
        if (payloadSize >= sizeof(int)) {
            std::memcpy(&client.id, payload, sizeof(int));
            bits[6] = client.id;
            std::cout << "[RECIBIDO] ID confirmado por el servidor: " << client.id << std::endl;
        }
        break;
    }

    case SEND_OBSTACULOS:
    {
        if (payloadSize > 0) {
            size_t cantidad = payloadSize / sizeof(_Obstaculo);
            std::vector<_Obstaculo> buffer(cantidad);
            std::memcpy(buffer.data(), payload, payloadSize);

            lock_guard<mutex> lock(mtx);
            obstaculos.clear();
            for (size_t i = 0; i < cantidad; i++) {
                obstaculos.push_back(Obstaculo(buffer[i]));
            }
            std::cout << "[RECIBIDO] Obstáculos procesados: " << cantidad << std::endl;
        }
        break;
    }

    case SEND_TANQUES:
    {
        if (payloadSize > 0) {
            size_t cantidad = payloadSize / sizeof(_Tanque);
            std::vector<_Tanque> buffer(cantidad);
            std::memcpy(buffer.data(), payload, payloadSize);

            lock_guard<mutex> lock(mtx);
            tanques.clear();
            for (size_t i = 0; i < cantidad; i++) {
                tanques.push_back(Tanque(buffer[i]));
            }
        }
        break;
    }

    case SEND_PROYECTILES:
    {
        lock_guard<mutex> lock(mtx);
        if (payloadSize == sizeof(int)) {
            proyectiles.clear();
        } else if (payloadSize > 0) {
            size_t cantidad = payloadSize / sizeof(_Proyectil);
            std::vector<_Proyectil> buffer(cantidad);
            std::memcpy(buffer.data(), payload, payloadSize);

            proyectiles.clear();
            for (size_t i = 0; i < cantidad; i++) {
                proyectiles.push_back(Proyectil(buffer[i]));
            }
        }
        break;
    }

    case SEND_ZONAS:
    {
        if (payloadSize > 0) {
            size_t cantidad = payloadSize / sizeof(_Zona);
            std::vector<_Zona> buffer(cantidad);
            std::memcpy(buffer.data(), payload, payloadSize);

            lock_guard<mutex> lock(mtx);
            zonas.clear();
            for (size_t i = 0; i < cantidad; i++) {
                zonas.push_back(Zona(buffer[i]));
            }
        }
        break;
    }

    case SEND_SONIDOS:
    {
        if (payloadSize > sizeof(int)) {
            size_t cantidad = payloadSize / sizeof(_SoundMsg);
            std::vector<_SoundMsg> buffer(cantidad);
            std::memcpy(buffer.data(), payload, payloadSize);

            lock_guard<mutex> lock(mtx);
            for (size_t i = 0; i < cantidad; i++) {
                sonidos.push(buffer[i]);
            }
        }
        break;
    }

    default:
        break;
    }
}

void view()
{
    SetTargetFPS(60);
    InitWindow(GetScreenWidth(), GetScreenHeight(), "TANKS");
    SetWindowPosition(0, 10);
    InitAudioDevice();

    initColors();

    while (client.id == -1 && !WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Conectando con el Servidor WebRTC...", GetScreenWidth() / 2 - 250, GetScreenHeight() / 2 - 20, 28, RAYWHITE);
        EndDrawing();
    }

    if (WindowShouldClose())
    {
        CloseAudioDevice();
        CloseWindow();
        return;
    }

    menu();

    if (WindowShouldClose())
    {
        CloseAudioDevice();
        CloseWindow();
        return;
    }

    Camera2D camara;
    float actual_pitch = 1.0f;
    char buffer1[10];
    char buffer2[10];
    int listo = 0;
    Tanque temp = Tanque(colors[sel_color], -1);

    initCamara(camara);
    initTexture();
    initZona();
    initSonido();

    while (!WindowShouldClose())
    {
        {
            lock_guard<mutex> lock(mtx);
            for (auto &tanque : tanques)
            {
                if (tanque.id == client.id)
                {
                    temp = tanque;
                }
            }
        }

        BeginDrawing();
        BeginMode2D(camara);
        ClearBackground(DARKBLUE);
        drawSuelo();
        drawBorde();

        for (auto &obs : obstaculos)
        {
            obs.draw(box);
        }

        if (!IsSoundPlaying(mov))
        {
            PlaySound(mov);
        }

        if (bits[0] || bits[1] || bits[2] || bits[3])
        {
            if (actual_pitch != 1.1f)
            {
                actual_pitch = 1.1f;
                SetSoundPitch(mov, actual_pitch);
            }
        }
        else
        {
            if (actual_pitch != 1.0f)
            {
                actual_pitch = 1.0f;
                SetSoundPitch(mov, actual_pitch);
            }
        }

        {
            lock_guard<mutex> lock(mtx);
            while (!sonidos.empty())
            {
                _SoundMsg s = sonidos.front();
                sonidos.pop();

                switch (s.id)
                {
                case DISP:
                    playEffect(disp, temp.getCentro(), s.origen);
                    break;
                case DANIO:
                    playEffect(danio, temp.getCentro(), s.origen);
                    break;
                case SHIELD:
                    playEffect(shield, temp.getCentro(), s.origen);
                    break;
                }
            }

            for (auto &tanque : tanques)
            {
                tanque.draw(obtenerTanque(tanque.getColor()));
                if (tanque.id != client.id)
                {
                    tanque.drawVida();
                }
            }

            for (auto &disp_obj : proyectiles)
            {
                disp_obj.draw();
            }
        }

        EndMode2D();

        for (auto &zona : zonas)
        {
            zona.getPrc(temp);
        }
        centerCamera(camara, temp.getCentro());

        drawEscudoCd(temp);
        drawVida(temp);
        drawMiniMapa(temp);

        strcpy(buffer2, "FPS:");
        itoa(GetFPS(), buffer1, 10);
        strcat(buffer2, buffer1);
        strcat(buffer2, " ");
        strcat(buffer2, ms.c_str());
        DrawText(buffer2, 20, 30, 35, WHITE);

        EndDrawing();

        if (listo < 180)
        {
            camara.target.x = temp.getCentro().x;
            camara.target.y = temp.getCentro().y;
            camara.offset.x = GetScreenWidth() / 2.0f;
            camara.offset.y = GetScreenHeight() / 2.0f;
            camara.rotation = 0;
            camara.zoom = 0.8f;
        }
        listo++;
    }

    uint8_t disconPacket[1 + sizeof(int)];
    disconPacket[0] = SEND_DISCONNECT;
    std::memcpy(disconPacket + 1, &client.id, sizeof(int));
    client.sendData(disconPacket, sizeof(disconPacket));

    CloseAudioDevice();
    CloseWindow();
}

void inputT()
{
    bits[0] = IsKeyDown(KEY_A) ? 1 : 0;
    bits[1] = IsKeyDown(KEY_W) ? 1 : 0;
    bits[2] = IsKeyDown(KEY_S) ? 1 : 0;
    bits[3] = IsKeyDown(KEY_D) ? 1 : 0;
    bits[4] = IsKeyDown(KEY_O) ? 1 : 0;
    bits[5] = IsKeyDown(KEY_P) ? 1 : 0;
    bits[6] = client.id;
}

bool colorCmpS(Color c1, Color c2)
{
    return (c1.a == c2.a && c1.r == c2.r && c1.g == c2.g && c1.b == c2.b);
}

array<Texture2D, 3> obtenerTanque(Color color)
{
    if (colorCmpS(color, RED)) return t_red;
    if (colorCmpS(color, YELLOW)) return t_yellow;
    if (colorCmpS(color, BLUE)) return t_blue;
    if (colorCmpS(color, PURPLE)) return t_purple;
    if (colorCmpS(color, BLACK)) return t_black_d;
    if (colorCmpS(color, DARKGRAY)) return t_black_r;
    if (colorCmpS(color, GREEN)) return t_green;
    if (colorCmpS(color, SKYBLUE)) return t_cian;
    return t_red;
}

void initCamara(Camera2D &camara)
{
    camara.target.x = 800;
    camara.target.y = 800;
    camara.offset.x = GetScreenWidth() / 2.0f;
    camara.offset.y = GetScreenHeight() / 2.0f;
    camara.rotation = 0;
    camara.zoom = 0.8f;
}

void initTexture()
{
    t_red[0] = LoadTexture("img\\T_RED0.png");
    t_red[1] = LoadTexture("img\\T_RED1.png");
    t_red[2] = LoadTexture("img\\T_RED2.png");

    t_blue[0] = LoadTexture("img\\T_BLUE0.png");
    t_blue[1] = LoadTexture("img\\T_BLUE1.png");
    t_blue[2] = LoadTexture("img\\T_BLUE2.png");

    t_purple[0] = LoadTexture("img\\T_PURPLE0.png");
    t_purple[1] = LoadTexture("img\\T_PURPLE1.png");
    t_purple[2] = LoadTexture("img\\T_PURPLE2.png");

    t_cian[0] = LoadTexture("img\\T_CIAN0.png");
    t_cian[1] = LoadTexture("img\\T_CIAN1.png");
    t_cian[2] = LoadTexture("img\\T_CIAN2.png");

    t_black_d[0] = LoadTexture("img\\T_BLACKD0.png");
    t_black_d[1] = LoadTexture("img\\T_BLACKD1.png");
    t_black_d[2] = LoadTexture("img\\T_BLACKD2.png");

    t_black_r[0] = LoadTexture("img\\T_BLACKR0.png");
    t_black_r[1] = LoadTexture("img\\T_BLACKR1.png");
    t_black_r[2] = LoadTexture("img\\T_BLACKR2.png");

    t_green[0] = LoadTexture("img\\T_GREEN0.png");
    t_green[1] = LoadTexture("img\\T_GREEN1.png");
    t_green[2] = LoadTexture("img\\T_GREEN2.png");

    t_yellow[0] = LoadTexture("img\\T_YELLOW0.png");
    t_yellow[1] = LoadTexture("img\\T_YELLOW1.png");
    t_yellow[2] = LoadTexture("img\\T_YELLOW2.png");

    losa = LoadTexture("img\\losa.png");
    borde = LoadTexture("img\\borde.png");
    box = LoadTexture("img\\box.png");
}

void initSonido()
{
    danio = LoadSound("audio\\danio.wav");
    disp = LoadSound("audio\\exp.wav");
    shield = LoadSound("audio\\shield.wav");

    mov = LoadSound("audio\\mov.wav");
    SetSoundVolume(mov, 0.15f);
}

void initColors()
{
    colors.push_back(RED);
    colors.push_back(YELLOW);
    colors.push_back(BLUE);
    colors.push_back(PURPLE);
    colors.push_back(GREEN);
    colors.push_back(SKYBLUE);
    colors.push_back(BLACK);
    colors.push_back(DARKGRAY);
}

void drawSuelo()
{
    for (int y = -RALTO; y < BORDE_DOWN + RALTO; y += 160)
    {
        for (int x = -RANCHO; x < BORDE_RIGHT + RANCHO; x += 160)
        {
            DrawTexture(losa, x, y, WHITE);
        }
    }
}

void drawBorde()
{
    for (int x = BORDE_LEFT - CASILLA; x <= BORDE_RIGHT; x += CASILLA)
    {
        DrawTexture(borde, x, BORDE_UP - CASILLA, WHITE);
        DrawTexture(borde, x, BORDE_DOWN, WHITE);
    }
    for (int y = BORDE_UP - CASILLA; y <= BORDE_DOWN; y += CASILLA)
    {
        DrawTexture(borde, BORDE_LEFT - CASILLA, y, WHITE);
        DrawTexture(borde, BORDE_RIGHT, y, WHITE);
    }
}

void menu()
{
    Image img_fondo = LoadImage("img\\menu.png");
    int width = GetScreenWidth();
    ImageResize(&img_fondo, GetScreenWidth(), GetScreenHeight());

    Texture2D fondo = LoadTextureFromImage(img_fondo);
    UnloadImage(img_fondo);

    Texture2D mouse = LoadTexture("img\\op.png");
    Texture2D wasd = LoadTexture("img\\wasd.png");
    op = 0;
    sel_color = 0;

    while (on_menu && !WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTextureEx(fondo, {0, 0}, 0, 1, Color{220, 220, 220, 255});
        int actual = 100;
        drawBoton((char *)"TNKRD", width / 2.0f, actual, false);
        actual += 150;

        drawBoton((char *)"Jugar", width / 2.0f, actual, (op == 0));
        actual += 100;

        if (op == 1)
        {
            drawBoton((char *)"Controles", width / 2.0f, actual, true);
            actual += 100;
            DrawRectangle(width / 2.0f - CASILLA * 3, actual, CASILLA * 6, CASILLA * 2.5, Color{220, 220, 220, 255});
            DrawTexture(wasd, width / 2.0f - CASILLA * 3, actual - 50, Color{220, 220, 220, 255});
            DrawTexture(mouse, width / 2.0f + CASILLA, actual + 70, Color{220, 220, 220, 255});
            actual += 150;
        }
        else
        {
            drawBoton((char *)"Controles", width / 2.0f, actual, false);
        }
        actual += 100;

        if (op == 2)
        {
            drawBoton((char *)" <- Color ->", width / 2.0f, actual, true);
            actual += 100;
            DrawRectangle(width / 2.0f - CASILLA - 4, actual - 4, CASILLA * 2 + 8, CASILLA * 2 + 8, BLACK);
            DrawRectangle(width / 2.0f - CASILLA, actual, CASILLA * 2, CASILLA * 2, colors[sel_color]);
            actual += 100;
        }
        else
        {
            drawBoton((char *)"Color", width / 2.0f, actual, false);
        }

        actual += 100;
        drawBoton((char *)"Salir", width / 2.0f, actual, (op == 3));

        EndDrawing();

        input();
    }

    UnloadTexture(fondo);
    UnloadTexture(mouse);
    UnloadTexture(wasd);
}

void drawBoton(char str[], int x, int y, bool select)
{
    int ancho = MeasureText(str, 70);
    Vector2 pos = {(float)x - ancho / 2.0f + 12.0f, (float)y};
    if (select)
    {
        DrawRectangle(pos.x - 20, pos.y - 8, ancho + 10, 70, BLACK);
        DrawText(str, pos.x, pos.y, 60, Color{220, 220, 220, 255});
    }
    else
    {
        DrawRectangle(pos.x - 20, pos.y - 8, ancho + 10, 70, Color{220, 220, 220, 255});
        DrawText(str, pos.x, pos.y, 60, BLACK);
    }
}

void input()
{
    if (IsKeyPressed(KEY_W))
    {
        op = op > 0 ? op - 1 : 0;
    }
    if (IsKeyPressed(KEY_S))
    {
        op = op < 3 ? op + 1 : 3;
    }

    if (op == 2)
    {
        if (IsKeyPressed(KEY_A))
        {
            sel_color = (sel_color > 0) ? sel_color - 1 : colors.size() - 1;
        }
        if (IsKeyPressed(KEY_D))
        {
            sel_color = (sel_color < colors.size() - 1) ? sel_color + 1 : 0;
        }
    }

    if (op == 0 && IsKeyPressed(KEY_ENTER))
    {
        on_menu = false;
    }
    if (op == 3 && IsKeyPressed(KEY_ENTER))
    {
        CloseWindow();
        client.close();
        exit(0);
    }
}

void drawEscudoCd(Tanque temp)
{
    int ancho = MeasureText("FPS:60", 35);
    Color transparent = temp.getColor();
    string timer = to_string((int)(temp.getEscudoTimer() / 60.0f) + 1);
    if (temp.getEscudoTimer() != 0)
    {
        transparent = Color{25, 25, 25, 90};
    }
    else
    {
        timer = '0';
    }
    transparent.a = 90;

    DrawCircle(20 + ancho / 2, 120, TANK_ESC_RAD / 1.5f, transparent);
    DrawText(timer.c_str(), 20 + ancho / 2 - MeasureText(timer.c_str(), 40) / 2.0f, 102, 40, WHITE);
}

void drawVida(Tanque temp)
{
    int ancho = MeasureText("FPS:60", 35);
    float iniciox = 20 + ancho / 2.0f - 20;
    float inicioy = 180;
    Rectangle vida;
    vida.x = iniciox;
    vida.y = inicioy;
    vida.height = 300;
    vida.width = 40;
    DrawRectangle(vida.x, vida.y, vida.width, vida.height * temp.getVida() / 100.0f, Color{6, 174, 5, 200});
    DrawRectangle(iniciox, inicioy, 2, 300, BLACK);
    DrawRectangle(iniciox, inicioy, 40, 2, BLACK);
    DrawRectangle(iniciox, inicioy + 298, 40, 2, BLACK);
    DrawRectangle(iniciox + 38, inicioy, 2, 300, BLACK);
}

void playEffect(Sound s, Vector2 destino, Vector2 origen)
{
    Sound ss = LoadSoundAlias(s);
    float max_dist = 1500;
    float max_vol = 0.8f;
    float distance = sqrtf(powf(origen.x - destino.x, 2) + powf(origen.y - destino.y, 2));
    float volume = max_vol - (distance / max_dist);
    volume = fmaxf(volume, 0.0f);
    float balance = (((origen.x - destino.x) / max_dist) / 2.0f * -1.0f + 0.5f);
    SetSoundVolume(ss, volume);
    SetSoundPan(ss, balance);

    PlaySound(ss);
}

void drawMiniMapa(Tanque temp)
{
    static bool open = true;
    float ancho = (float)GetScreenWidth();
    float iniciox = ancho - 5 - COLUMNAS * 5;
    float inicioy = 15;
    DrawRectangle(iniciox - 45, inicioy, 45, 20, Color{255, 255, 255, 150});
    DrawText("tab", iniciox - 38, inicioy, 20, Color{0, 0, 0, 150});
    if (IsKeyPressed(KEY_TAB))
    {
        open = !open;
    }

    if (open)
    {
        Rectangle rec;
        Color c;
        for (auto &zona : zonas)
        {
            rec = zona.getRec();
            c = zona.getColor();
            DrawRectangle(iniciox + rec.x / CASILLA * 5, inicioy + rec.y / CASILLA * 5, rec.width / CASILLA * 5, rec.height / CASILLA * 5, c);
        }

        for (auto &obs : obstaculos)
        {
            rec = obs.getRec();
            DrawRectangle(iniciox + rec.x / CASILLA * 5, inicioy + rec.y / CASILLA * 5, rec.width / CASILLA * 5, rec.height / CASILLA * 5, Color{167, 85, 68, 150});
        }

        lock_guard<mutex> lock(mtx);
        for (auto &tanque : tanques)
        {
            c = tanque.getColor();
            if (colorCmpS(c, temp.getColor()))
            {
                rec = tanque.getRec();
                c.a = 220;
                DrawRectangle(iniciox + (rec.x - rec.width / 2.0f) / CASILLA * 5, inicioy + (rec.y - rec.height / 2.0f) / CASILLA * 5, 5, 5, c);
            }
            else
            {
                for (auto &t : tanques)
                {
                    if (colorCmpS(t.getColor(), temp.getColor()))
                    {
                        Vector2 t1 = tanque.getCentro();
                        Vector2 t2 = t.getCentro();
                        float dist = sqrtf((t1.x - t2.x) * (t1.x - t2.x) + (t1.y - t2.y) * (t1.y - t2.y));
                        if (dist < 1000)
                        {
                            rec = tanque.getRec();
                            c.a = 220;
                            DrawRectangle(iniciox + (rec.x - rec.width / 2.0f) / CASILLA * 5, inicioy + (rec.y - rec.height / 2.0f) / CASILLA * 5, 5, 5, c);
                            break;
                        }
                    }
                }
            }
        }
    }
}

void initZona()
{
    zonas.clear();
    for (int i = 0; i < 25; i++)
    {
        zonas.push_back(Zona());
    }
}

void initInput()
{
    for (int i = 0; i < 6; i++)
    {
        bits[i] = 0;
    }
    bits[6] = client.id;
}

void centerCamera(Camera2D &camara, Vector2 point)
{
    if (camara.target.x < point.x) camara.target.x += 2;
    if (camara.target.y < point.y) camara.target.y += 2;
    if (camara.target.x > point.x) camara.target.x -= 2;
    if (camara.target.y > point.y) camara.target.y -= 2;

    if (IsKeyPressed(KEY_SPACE))
    {
        camara.target = point;
    }
}