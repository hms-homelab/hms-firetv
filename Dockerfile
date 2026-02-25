# HMS-FireTV - Multi-stage Dockerfile
# Uses trixie (Debian 13) for Drogon + Paho MQTT from repos (multi-arch safe)

# =============================================================================
# Stage 1: Builder
# =============================================================================
FROM debian:trixie-slim AS builder

# Install build dependencies (all from Debian repos, multi-arch safe)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ca-certificates \
    libdrogon-dev \
    libjsoncpp-dev \
    libpqxx-dev \
    libpaho-mqttpp-dev \
    libpaho-mqtt-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    libsqlite3-dev \
    default-libmysqlclient-dev \
    libhiredis-dev \
    libyaml-cpp-dev \
    uuid-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source code
WORKDIR /app
COPY CMakeLists.txt VERSION ./
COPY src/ src/
COPY include/ include/
COPY static/ static/

# Build HMS-FireTV (disable tests for production image)
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    strip hms_firetv

# =============================================================================
# Stage 2: Runtime
# =============================================================================
FROM debian:trixie-slim

# Install runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    libdrogon1t64 \
    libpqxx-7.10 \
    libpaho-mqtt1.3 \
    libpaho-mqttpp3-1 \
    && rm -rf /var/lib/apt/lists/*

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
