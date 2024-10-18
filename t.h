#pragma once
#include <ws2tcpip.h>
#include <winsock2.h>
#include <string.h>
#include <stdio.h>
#include <cstring>
#include <stdlib.h>
#include <raylib.h>
#include <iostream>
#include <unordered_map>
#include <sstream>
#include <future>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <array>
#include <cmath>
#include <mutex>
#include <queue>
#include <list>
#include <map>

using std::unordered_map;
using std::to_string;
using std::vector;
using std::string;
using std::thread;
using std::queue;
using std::mutex;
using std::array;
using std::list;
using std::cout;
using std::map;

#define IP "192.168.100.3"
#define PUERTO 12345

#define RANCHO 1600.f
#define RALTO 960.f

#define CASILLA 80.f
#define BORDE_LEFT 0.0f 
#define BORDE_UP 0.0f
#define BORDE_RIGHT 50*CASILLA
#define BORDE_DOWN 50*CASILLA

#define COLUMNAS BORDE_RIGHT/CASILLA
#define FILAS BORDE_DOWN/CASILLA
#define TANKV 3.3f
#define TANKH 60.f
#define TANKW 80.f
#define TANKDIAG sqrt((TANKH / 2.f) * (TANKH / 2.f) + (TANKW / 2.f) * (TANKW / 2.f))
#define TANK_DANIO 24

#define TANK_ESC_RAD 65
#define TANK_ESC_CD 300
#define TANK_ESC_DUR 90

#define VEL_PYTL 4.8f
#define RAD_PYTL 9.f
#define DELTA_DIR 2.6
#define VEL_ATK 45

#define ZONA_TTT 600
#define ZONA_PRP 50

#define MAX_SOUNDS 20
#define TTL_SOUNDS 120

#define DISP 0
#define DANIO 1
#define SHIELD 2

#define SEND_PROYECTILES '0'
#define SEND_OBSTACULOS '1'
#define SEND_TANQUES '2'
#define SEND_SONIDOS '3'
#define SEND_ZONAS '4'
#define SEND_INPUT '5'
#define SEND_COLOR '6'
#define SEND_ID '7'
#define SEND_DISCONNECT '8'

typedef struct _peticiontanque
{
    int id;
    Color color;
} _PeticionTanque;