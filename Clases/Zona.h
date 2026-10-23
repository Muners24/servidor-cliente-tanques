#pragma once
#include "..\t.h"
#include "Tanque.h"

typedef struct _zona
{
    Rectangle pos;
    Color color;
    int timer[9];
    bool anime_b;
} _Zona;

class Zona
{
private:
    Rectangle pos;
    Color color;
    bool anime_b;
    int timer[9];

    void colisionsTanques(list<Tanque*> tanques);
    void transform();
    void animacion();
    void drawPrc(Color c,Color txt_c);
public:
    Zona(){};
    Zona(_Zona &z);
    Zona(int x,int y);
    bool colisionTanque(Tanque t);
    void update(list<Tanque*> tanques);
    void getPrc(Tanque t);
    Rectangle getRec(){return pos;};
    Color getColor(){return color;};
    _Zona toStruct();
    
};

