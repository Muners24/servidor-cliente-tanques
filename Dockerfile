FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Instalar herramientas esenciales y dependencias de WebRTC/Raylib
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

WORKDIR /deps

# Compilar e instalar libdatachannel
RUN git clone --recursive https://github.com/paullouisageneau/libdatachannel.git && \
    cd libdatachannel && \
    mkdir build && cd build && \
    cmake .. -DUSE_SYSTEM_TP=OFF -DNO_EXAMPLES=ON -DNO_TESTS=ON && \
    make -j$(nproc) && \
    make install

# Compilar e instalar raylib
RUN git clone https://github.com/raysan5/raylib.git && \
    cd raylib/src && \
    make PLATFORM=PLATFORM_DESKTOP && \
    make install

WORKDIR /app
COPY . .

# Compilar el servidor de C++ incluyendo las librerias X11 que pide raylib
RUN g++ -std=c++17 server.cpp Clases/Server.cpp Clases/Tanque.cpp Clases/Obstaculo.cpp Clases/Zona.cpp Clases/Proyectil.cpp Clases/SoundMsg.cpp \
    -o server \
    -I. -IClases \
    -ldatachannel -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# Imagen final liviana
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    libstdc++6 \
    libgl1 \
    libx11-6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /usr/local/lib/ /usr/local/lib/
COPY --from=builder /app/server /app/server

ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
EXPOSE 8080

CMD ["./server"]