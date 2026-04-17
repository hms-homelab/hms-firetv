# HMS-FireTV - Multi-stage Docker build
# C++ Fire TV Lightning service with Angular web UI

# =============================================================================
# Stage 1: Angular UI Builder
# =============================================================================
FROM node:22-slim AS ui-builder

WORKDIR /ui
COPY frontend/package*.json ./
RUN npm ci --no-audit --no-fund
COPY frontend/ ./
RUN npx ng build --configuration production

# =============================================================================
# Stage 2: C++ Builder
# =============================================================================
FROM debian:trixie-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ca-certificates \
    pkg-config \
    libssl-dev \
    libjsoncpp-dev \
    libdrogon-dev \
    libpqxx-dev \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev \
    libcurl4-openssl-dev \
    uuid-dev libhiredis-dev libbrotli-dev zlib1g-dev \
    libmariadb-dev libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY CMakeLists.txt VERSION ./
COPY src/ ./src/
COPY include/ ./include/

RUN mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF .. && \
    make -j$(nproc) && \
    strip hms_firetv

# =============================================================================
# Stage 3: Runtime
# =============================================================================
FROM debian:trixie-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    libssl3 \
    libjsoncpp26 \
    libdrogon1t64 \
    libtrantor1 \
    libpqxx-7.10 \
    libpaho-mqtt1.3 \
    libpaho-mqttpp3-1 \
    libcurl4t64 \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -r -u 1000 -m -s /bin/bash firetv

COPY --from=builder /build/build/hms_firetv /usr/local/bin/hms_firetv
RUN chmod +x /usr/local/bin/hms_firetv

COPY --from=ui-builder /ui/dist/frontend/browser/ /home/firetv/static/
RUN chown -R firetv:firetv /home/firetv/static

USER firetv
WORKDIR /home/firetv

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8888/health || exit 1

EXPOSE 8888

CMD ["/usr/local/bin/hms_firetv"]
