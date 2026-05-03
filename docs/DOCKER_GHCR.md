# Docker & GHCR Publishing Guide

## Overview

HMS-FireTV publishes multi-arch Docker images (amd64 + arm64) to GitHub Container Registry (GHCR) via GitHub Actions.

**Image:** `ghcr.io/hms-homelab/hms-firetv`

## Quick Start

```bash
# Pull latest
docker pull ghcr.io/hms-homelab/hms-firetv:latest

# Run with environment variables
docker run -d \
  --name hms-firetv \
  -p 8888:8888 \
  -e MQTT_HOST=192.168.2.15 \
  -e MQTT_PORT=1883 \
  -e MQTT_USER=aamat \
  -e MQTT_PASS=secret \
  -e DB_HOST=192.168.2.15 \
  -e DB_PORT=5432 \
  -e DB_NAME=hms_firetv \
  -e DB_USER=maestro \
  -e DB_PASS=secret \
  ghcr.io/hms-homelab/hms-firetv:latest
```

## Image Details

| Property | Value |
|----------|-------|
| Base | `debian:trixie-slim` |
| Size | ~103 MB |
| Architectures | `linux/amd64`, `linux/arm64` |
| Port | 8888 |
| User | `hmsfiretv` (non-root) |
| Health check | `curl http://localhost:8888/health` |

## CI/CD Workflow

**File:** `.github/workflows/docker-build.yml`

### Triggers
- Push to `main` or `master` branch
- Push of version tags (`v*`)
- Pull requests to `main` or `master`

### Tag Strategy
| Trigger | Tags Generated |
|---------|---------------|
| Push to master | `latest`, `master`, `sha-abc1234` |
| Tag `v1.0.1` | `1.0.1`, `1.0`, `sha-abc1234` |
| Pull request | `pr-123` |

### Workflow Steps
1. Checkout code
2. Set up QEMU (for arm64 emulation)
3. Set up Docker Buildx
4. Login to GHCR (skip on PRs)
5. Build multi-arch image
6. Push to GHCR (skip on PRs)

## Build Architecture

### Why Debian Trixie?

HMS-FireTV uses the **Drogon** C++ web framework, which is only available as a Debian package starting in **trixie (Debian 13)**. Bookworm (Debian 12) does not have it.

### Why Debian Packages Instead of Source Builds?

Building Paho MQTT C++ from source fails on arm64 QEMU due to OpenSSL cross-compilation issues. All dependencies come from Debian repos:

**Builder stage:**
- `libdrogon-dev` — Web framework (pulls in jsoncpp, uuid, zlib, etc.)
- `libpqxx-dev` — PostgreSQL C++ client
- `libpaho-mqttpp-dev` + `libpaho-mqtt-dev` — MQTT client
- `libcurl4-openssl-dev` — HTTP client
- `libsqlite3-dev`, `default-libmysqlclient-dev`, `libhiredis-dev`, `libyaml-cpp-dev` — Required by Drogon cmake even if unused

**Runtime stage:**
- `libdrogon1t64` — Drogon runtime (t64 suffix for 64-bit time_t)
- `libpqxx-7.10` — PostgreSQL runtime
- `libpaho-mqtt1.3` + `libpaho-mqttpp3-1` — MQTT runtime

## Local Build

```bash
# Build for current architecture
docker build -t hms-firetv:local .

# Build multi-arch (requires buildx)
docker buildx build --platform linux/amd64,linux/arm64 -t hms-firetv:local .
```

## Versioning

Version is tracked in the `VERSION` file. To release a new version:

```bash
# Update VERSION file
echo "1.0.2" > VERSION

# Commit and tag
git add VERSION CHANGELOG.md
git commit -m "release: v1.0.2"
git tag v1.0.2
git push origin master --tags
```
