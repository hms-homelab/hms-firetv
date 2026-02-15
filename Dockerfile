# ==============================================================================
# HMS FireTV - Multi-stage Dockerfile
# ==============================================================================
#
# Stage 1: Build environment with all dependencies
# Stage 2: Runtime environment with minimal footprint
#
# Usage:
#   docker build -t hms-firetv:latest .
#   docker run -d --env-file .env -p 8888:8888 hms-firetv:latest
#

# ==============================================================================
# Stage 1: Builder
# ==============================================================================
FROM debian:bookworm-slim AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libpqxx-dev \
    libcurl4-openssl-dev \
    libjsoncpp-dev \
    libssl-dev \
    uuid-dev \
    zlib1g-dev \
    wget \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install Paho MQTT C library
WORKDIR /tmp
RUN git clone https://github.com/eclipse/paho.mqtt.c.git && \
    cd paho.mqtt.c && \
    git checkout v1.3.13 && \
    cmake -Bbuild -H. -DPAHO_WITH_SSL=ON -DPAHO_ENABLE_TESTING=OFF && \
    cmake --build build/ --target install && \
    ldconfig

# Install Paho MQTT C++ library
RUN git clone https://github.com/eclipse/paho.mqtt.cpp.git && \
    cd paho.mqtt.cpp && \
    git checkout v1.3.2 && \
    cmake -Bbuild -H. -DPAHO_BUILD_DOCUMENTATION=OFF -DPAHO_BUILD_SAMPLES=OFF && \
    cmake --build build/ --target install && \
    ldconfig

# Install Drogon framework
RUN git clone https://github.com/drogonframework/drogon && \
    cd drogon && \
    git checkout v1.9.3 && \
    git submodule update --init && \
    mkdir build && cd build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_CTL=OFF \
        -DBUILD_ORM=OFF && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Copy source code
WORKDIR /app
COPY include/ include/
COPY src/ src/
COPY static/ static/
COPY CMakeLists.txt .

# Build HMS FireTV
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    strip hms_firetv

# ==============================================================================
# Stage 2: Runtime
# ==============================================================================
FROM debian:bookworm-slim

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libpqxx-6.4 \
    libcurl4 \
    libjsoncpp25 \
    libssl3 \
    libuuid1 \
    zlib1g \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy runtime libraries from builder
COPY --from=builder /usr/local/lib/libpaho* /usr/local/lib/
COPY --from=builder /usr/local/lib/libdrogon* /usr/local/lib/
COPY --from=builder /usr/local/lib/libtrantor* /usr/local/lib/
RUN ldconfig

# Create app directory for static files
WORKDIR /app

# Copy binary and static files
COPY --from=builder /app/build/hms_firetv /usr/local/bin/hms_firetv
COPY --from=builder /app/static ./static

# Create non-root user and set permissions
RUN useradd -r -s /bin/false hmsfiretv && \
    chown -R hmsfiretv:hmsfiretv /app

# Switch to non-root user
USER hmsfiretv

# Expose API port
EXPOSE 8888

# Health check
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8888/health || exit 1

# Run the service
CMD ["/usr/local/bin/hms_firetv"]
