#pragma once
#include "t.h"
#include "Clases/Obstaculo.h"
#include "Clases/Zona.h"

bool CheckCollisionRecsss(Rectangle rect1, Rectangle rect2)
{
    // Verificar colisión en el eje X
    bool collisionX = rect1.x < rect2.x + rect2.width && rect1.x + rect1.width > rect2.x;

    // Verificar colisión en el eje Y
    bool collisionY = rect1.y < rect2.y + rect2.height && rect1.y + rect1.height > rect2.y;

    // Si hay colisión en ambos ejes, los rectángulos se solapan
    return collisionX && collisionY;
}

void initObstaculo(list<Obstaculo> &obstaculos)
{
    srand(time(NULL));
    int x, y;
    for (y = BORDE_UP + CASILLA * 2; y < BORDE_DOWN - CASILLA * 4; y += CASILLA)
    {
        for (x = BORDE_LEFT + CASILLA * 2; x < BORDE_RIGHT - CASILLA * 4; x += CASILLA)
        {
            if ((rand() % 12) == 1)
            {
                bool colision = false;
                Obstaculo obs({(float)x, (float)y});
                for (auto obstaculo : obstaculos)
                {
                    if (CheckCollisionRecsss(obstaculo.getRec(), obs.getRec()))
                    {
                        colision = true;
                    }
                }
                if (!colision)
                {
                    obstaculos.emplace_back(obs);
                }
            }
        }
    }
}

void initZona(vector<Zona> &zonas)
{
    int x, y;
    for (y = 0; y < BORDE_RIGHT; y += CASILLA * 10)
    {
        for (x = 0; x < BORDE_DOWN; x += CASILLA * 10)
        {
            zonas.push_back(Zona(x, y));
        }
    }
}