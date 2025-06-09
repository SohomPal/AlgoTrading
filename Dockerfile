FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    cmake build-essential git \
    libprotobuf-dev protobuf-compiler \
    libgrpc++-dev libgrpc-dev \
    libquickfix-dev

WORKDIR /app
COPY . .

RUN cmake -Bbuild -H. && cmake --build build

CMD ["./build/sfox_data_collector"]
