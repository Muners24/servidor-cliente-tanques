#pragma once
#include "../t.h"

typedef Rectangle _Obstaculo;

class Obstaculo
{
private:
    Rectangle pos;

public:
    Obstaculo(){};
    Obstaculo(_Obstaculo &o);
    Obstaculo(Vector2 inicio);
    Rectangle getRec(){return pos;};
    void draw(Texture2D box);
    _Obstaculo toStruct();
};

