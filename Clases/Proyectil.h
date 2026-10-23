#pragma once
#include "../t.h"
#include "Obstaculo.h"

typedef struct _proyectil
{
    int rebotes_cont;
    Vector2 centro;
    Vector2 vel;
    Color color;
    float radio;
    bool should_del;

} _Proyectil;

class Proyectil
{
private:
    int rebotes_cont;
    Vector2 centro;
    Vector2 vel;
    Color color;
    float radio;

public:
    bool should_del;

    Proyectil(Vector2 fuente,float direccion,Color color);
    Proyectil(_Proyectil p);
    bool operator==(const Proyectil& p) const;
    void update(list<Obstaculo> obstaculos);
    Vector2 getCentro(){return centro;};
    Color getColor(){return color;};
    float getRadio(){return radio;};
    _Proyectil toStruct();
    Proyectil(){};
    void draw();

private:
    Vector2 right(){return {centro.x+radio,centro.y};};
    Vector2 left(){return {centro.x-radio,centro.y};};
    Vector2 down(){return {centro.x,centro.y+radio};};
    Vector2 up(){return {centro.x,centro.y-radio};};
    void colisionObs(list<Obstaculo> obstaculos);
    void colisionBorde();
    void movimiento();
};

