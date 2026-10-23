# 1. Imagen base con herramientas de compilación
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Instalar dependencias del sistema y de compilación
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    pkg-config \
    libasound2-dev \
    libx11-dev \
    libxrandr-dev \
    libxi-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libxcursor-dev \
    libxinerama-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copiar el código fuente del proyecto
COPY . .

# Compilar el servidor de C++ para Linux
RUN g++ -std=c++17 server.cpp Clases/Server.cpp Clases/Tanque.cpp Clases/Obstaculo.cpp Clases/Zona.cpp Clases/Proyectil.cpp Clases/SoundMsg.cpp \
    -o server \
    -I. -IClases \
    -ldatachannel -lraylib -lpthread -ldl

# 2. Imagen de ejecución ligera
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copiar el ejecutable compilado
COPY --from=builder /app/server /app/server

# Exponer el puerto de señalización WebSocket
EXPOSE 8080

# Ejecutar el servidor
CMD ["./server"]