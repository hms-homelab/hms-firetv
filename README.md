# HMS FireTV - Fire TV Lightning Protocol Service

**High-performance C++ service for controlling Amazon Fire TV devices via MQTT**

[![Status](https://img.shields.io/badge/status-production-green)]()
[![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)]()
[![License](https://img.shields.io/badge/license-MIT-blue)]()
[![Docker](https://img.shields.io/badge/docker-supported-blue)]()
[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-support-%23FFDD00.svg?logo=buy-me-a-coffee)](https://www.buymeacoffee.com/aamat09)

---

## Overview

HMS FireTV is a native C++ service that controls Amazon Fire TV devices using the Lightning protocol. It provides seamless integration with Home Assistant through MQTT Discovery, creating 15 button entities per device for full remote control functionality.

### Key Features

- ✅ **Native C++17** - High performance, low resource usage
- ✅ **MQTT Integration** - Full Home Assistant MQTT Discovery support
- ✅ **15 Button Entities** per device (navigation, media, volume, power)
- ✅ **Auto-reconnect** - Exponential backoff for MQTT and database
- ✅ **Docker Support** - Easy deployment with Docker/Compose
- ✅ **Systemd Service** - Production-ready native deployment
- ✅ **3x Faster** than Python implementation (11-40ms vs ~100ms latency)
- ✅ **3x Lower Memory** (~50MB vs ~150MB)

### What It Does

This service replaces traditional Python-based Fire TV control services with a more efficient C++ implementation while maintaining full compatibility with existing Home Assistant configurations.

---

## Quick Start

### Option 1: Docker (Recommended)

```bash
# Clone repository
git clone https://github.com/hms-homelab/hms-firetv.git
cd hms-firetv

# Copy and configure environment
cp .env.example .env
nano .env  # Edit with your credentials

# Start with docker-compose
docker-compose up -d

# Check logs
docker logs -f hms-firetv
```

### Option 2: Native Build

```bash
# Install dependencies (Debian/Ubuntu)
sudo apt-get install -y build-essential cmake libpqxx-dev libcurl4-openssl-dev \
    libjsoncpp-dev libpaho-mqtt-dev libpaho-mqttpp-dev

# Build
mkdir build && cd build
cmake .. && make -j$(nproc)

# Run (with environment variables)
export DB_HOST=localhost
export DB_PASSWORD=your_password
export MQTT_USER=your_mqtt_user
export MQTT_PASS=your_mqtt_password
./hms_firetv
```

---

## Architecture

```
Home Assistant (UI)
    ↓ Button Press
MQTT Broker (Port 1883)
    ↓ maestro_hub/colada/{device_id}/{action}
HMS FireTV Service (C++)
    ├─ MQTT Client (Eclipse Paho)
    ├─ Button Discovery (15 buttons/device)
    ├─ Command Handler (action → Lightning)
    └─ Lightning Client (HTTPS)
        ↓
Fire TV Devices (Lightning Protocol)
```

### Components

**MQTTClient** (`src/mqtt/MQTTClient.cpp` - 345 lines)
- Eclipse Paho C++ wrapper
- Auto-reconnect with exponential backoff (1s → 64s)
- Topic: `maestro_hub/colada/{device_id}/{action}`

**DiscoveryPublisher** (`src/mqtt/DiscoveryPublisher.cpp` - 160 lines)
- Home Assistant MQTT Discovery
- 15 button entities per device
- Topic: `homeassistant/button/colada/{device_id}_{button}/config`

**CommandHandler** (`src/mqtt/CommandHandler.cpp` - 290 lines)
- Button press → Lightning command conversion
- Lightning client caching
- Command execution and error handling

**LightningClient** (`src/clients/LightningClient.cpp` - 780 lines)
- Fire TV Lightning protocol implementation
- HTTPS with libcurl
- Pairing, navigation, media, app control

---

## Button Entities

Each Fire TV device gets **15 button entities** in Home Assistant:

### Navigation (5 buttons)
- `button.{device}_up` - D-pad up
- `button.{device}_down` - D-pad down
- `button.{device}_left` - D-pad left
- `button.{device}_right` - D-pad right
- `button.{device}_select` - Select/OK

### Media Control (2 buttons)
- `button.{device}_play` - Play
- `button.{device}_pause` - Pause

### System (3 buttons)
- `button.{device}_home` - Home screen
- `button.{device}_back` - Back
- `button.{device}_menu` - Menu

### Volume (3 buttons)
- `button.{device}_volume_up` - Volume up
- `button.{device}_volume_down` - Volume down
- `button.{device}_mute` - Mute

### Power (2 buttons)
- `button.{device}_wake` - Power on
- `button.{device}_sleep` - Power off

---

## Performance

| Metric | C++ Service | Python Service | Improvement |
|--------|-------------|----------------|-------------|
| Startup Time | <2s | ~3s | 1.5x faster |
| Memory Usage | ~50MB | ~150MB | 3x lower |
| Command Latency | 11-40ms | ~100ms | 3x faster |
| CPU (idle) | <2% | ~5% | 2.5x lower |
| Binary Size | 3.1MB | N/A (interpreted) | Compact |

---

## Prerequisites

### Required Services

- **PostgreSQL** - Device registry database
- **MQTT Broker** - EMQX, Mosquitto, or any MQTT broker
- **Home Assistant** - Smart home platform with MQTT integration

### Fire TV Devices

- Fire TV devices must be on the same network
- Lightning protocol must be enabled (enabled by default)
- Devices must be paired (see docs for pairing process)

---

## Configuration

### Environment Variables

All configuration is done via environment variables. Copy `.env.example` to `.env` and configure:

```bash
# Database
DB_HOST=localhost
DB_PORT=5432
DB_NAME=firetv
DB_USER=firetv_user
DB_PASSWORD=your_database_password

# MQTT
MQTT_BROKER_HOST=localhost
MQTT_BROKER_PORT=1883
MQTT_USER=your_mqtt_username
MQTT_PASS=your_mqtt_password

# API
API_HOST=0.0.0.0
API_PORT=8888
```

See `.env.example` for full configuration options.

---

## Installation

### Docker Deployment (Recommended)

1. **Clone and configure:**

```bash
git clone https://github.com/hms-homelab/hms-firetv.git
cd hms-firetv
cp .env.example .env
nano .env  # Configure your settings
```

2. **Start services:**

```bash
# With included PostgreSQL and MQTT
docker-compose up -d

# Or with external services
docker run -d \
  --name hms-firetv \
  --env-file .env \
  -p 8888:8888 \
  hms-firetv:latest
```

3. **Verify:**

```bash
docker logs -f hms-firetv
curl http://localhost:8888/health
```

### Native Deployment

1. **Install dependencies:**

```bash
# Debian/Ubuntu
sudo apt-get update
sudo apt-get install -y build-essential cmake g++ git \
    libpqxx-dev libcurl4-openssl-dev libjsoncpp-dev \
    libpaho-mqtt-dev libpaho-mqttpp-dev

# For Drogon framework
git clone https://github.com/drogonframework/drogon
cd drogon && git submodule update --init
mkdir build && cd build
cmake .. && make -j$(nproc) && sudo make install
```

2. **Build:**

```bash
cd hms-firetv
mkdir build && cd build
cmake .. && make -j$(nproc)
```

3. **Install systemd service:**

```bash
# Copy environment variables
sudo cp .env /etc/hms-firetv.env

# Create service file
sudo tee /etc/systemd/system/hms-firetv.service > /dev/null <<EOF
[Unit]
Description=HMS FireTV Service
After=network.target postgresql.service

[Service]
Type=simple
User=hmsfiretv
EnvironmentFile=/etc/hms-firetv.env
ExecStart=/usr/local/bin/hms_firetv
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# Install and start
sudo cp build/hms_firetv /usr/local/bin/
sudo useradd -r -s /bin/false hmsfiretv
sudo systemctl daemon-reload
sudo systemctl enable hms-firetv
sudo systemctl start hms-firetv
```

---

## Usage

### Docker

```bash
# View logs
docker logs -f hms-firetv

# Restart service
docker restart hms-firetv

# Stop service
docker-compose down

# Rebuild after code changes
docker-compose build --no-cache
docker-compose up -d
```

### Native Service

```bash
# Start/stop/restart
sudo systemctl start hms-firetv
sudo systemctl stop hms-firetv
sudo systemctl restart hms-firetv

# View logs
sudo journalctl -u hms-firetv -f

# View status
sudo systemctl status hms-firetv
```

### Health Check

```bash
curl http://localhost:8888/health
```

**Response:**
```json
{
  "service": "HMS FireTV",
  "version": "1.0.0",
  "database": "connected",
  "mqtt": "connected",
  "status": "healthy"
}
```

---

## Home Assistant Integration

### Automatic Discovery

Entities appear automatically in Home Assistant:

1. Service starts → Publishes 15 button entities per device
2. Home Assistant discovers entities via MQTT
3. Buttons appear in: **Settings → Devices & Services → MQTT**

### Using Buttons

**In Automations:**
```yaml
automation:
  - alias: "Bedtime - Turn off TVs"
    trigger:
      - platform: time
        at: "23:00:00"
    action:
      - service: button.press
        target:
          entity_id:
            - button.livingroom_firetv_sleep
            - button.bedroom_firetv_sleep
```

**In Scripts:**
```yaml
script:
  launch_netflix:
    sequence:
      - service: button.press
        target:
          entity_id: button.livingroom_firetv_wake
      - delay: 00:00:03
      - service: button.press
        target:
          entity_id: button.livingroom_firetv_home
```

---

## Device Management

### Adding Devices

Devices are stored in PostgreSQL. Add devices manually:

```sql
INSERT INTO fire_tv_devices (device_id, name, ip_address, api_key)
VALUES ('livingroom_firetv', 'Living Room Fire TV', '192.168.1.100', '0987654321');
```

Or use the REST API (if you implement device management endpoints).

### Device Pairing

Fire TV devices must be paired before use. The pairing process involves:
1. Initiating pairing request
2. Entering PIN shown on TV screen
3. Storing client token

See the Lightning protocol documentation for details.

---

## Troubleshooting

### Service won't start

```bash
# Check logs
docker logs hms-firetv
# or
sudo journalctl -u hms-firetv -n 100

# Check dependencies
docker ps | grep -E "postgres|mqtt"
```

### Buttons not appearing in HA

```bash
# Check MQTT connection
docker exec hms-firetv curl http://localhost:8888/health

# Monitor discovery messages
mosquitto_sub -h localhost -p 1883 -u your_user -P your_pass \
  -t "homeassistant/button/#" -v

# Reload MQTT integration in HA
```

### Commands not working

```bash
# Test direct MQTT
mosquitto_pub -h localhost -p 1883 -u your_user -P your_pass \
  -t "maestro_hub/colada/livingroom_firetv/volume_up" \
  -m "PRESS"

# Watch logs
docker logs -f hms-firetv
```

### Database connection issues

```bash
# Test PostgreSQL connection
psql -h localhost -U firetv_user -d firetv -c "SELECT * FROM fire_tv_devices;"

# Check credentials in .env
cat .env | grep DB_
```

---

## Development

### Building from Source

```bash
git clone https://github.com/hms-homelab/hms-firetv.git
cd hms-firetv
mkdir build && cd build
cmake .. && make -j$(nproc)
```

### Project Structure

```
hms-firetv/
├── include/               # Header files
│   ├── clients/          # Lightning client
│   ├── mqtt/             # MQTT components
│   ├── models/           # Data models
│   ├── repositories/     # Database layer
│   ├── services/         # Business logic
│   └── utils/            # Utilities
├── src/                  # Source files (mirrors include/)
├── build/                # Build output (gitignored)
├── docs/                 # Documentation
├── .env.example          # Configuration template
├── .gitignore            # Git ignore rules
├── CMakeLists.txt        # Build configuration
├── Dockerfile            # Docker image
├── docker-compose.yml    # Docker composition
├── schema.sql            # Database schema
├── mosquitto.conf        # MQTT config example
└── README.md             # This file
```

### Code Statistics

- **Total Lines**: ~2,500
- **C++ Source**: ~2,000 lines
- **Headers**: ~500 lines
- **Dependencies**: 6 libraries
- **Compilation Time**: ~30 seconds

### Contributing

1. Fork the repository
2. Create feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open Pull Request

---

## Docker Hub

Coming soon: Pre-built images on Docker Hub

```bash
docker pull hms-homelab/hms-firetv:latest
```

---
---

## ☕ Support

If this project is useful to you, consider buying me a coffee!

[![Buy Me A Coffee](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/aamat09)

## License

MIT License - see LICENSE file for details

---

## Support

**Issues**: https://github.com/hms-homelab/hms-firetv/issues
**Documentation**: See `docs/` directory
**Reddit Post**: https://www.reddit.com/r/firetvstick/comments/1r534bl/local_script_to_control_multiple_fire_tv_devices/

---

## Acknowledgments

- **Eclipse Paho MQTT** - MQTT client library
- **Drogon** - HTTP framework
- **Home Assistant** - Smart home platform
- **Amazon Fire TV** - Lightning protocol

---

**Version**: 1.0.0
**Status**: Production Ready
**Last Updated**: February 2026
