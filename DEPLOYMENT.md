# HMS FireTV - Deployment Guide

**Date**: February 3, 2026
**Version**: 1.0.0
**Status**: Production Ready

---

## Quick Start

```bash
cd /home/aamat/maestro_hub/hms-firetv

# Install service
./install-service.sh

# Check status
sudo systemctl status hms-firetv

# View logs
sudo journalctl -u hms-firetv -f
```

---

## Prerequisites

### 1. System Dependencies

```bash
# C++ Build Tools
sudo apt-get install build-essential cmake g++

# Libraries
sudo apt-get install libpqxx-dev libcurl4-openssl-dev libjsoncpp-dev

# Drogon Framework (if not installed)
# See: https://github.com/drogonframework/drogon

# Eclipse Paho MQTT C++ (if not installed)
sudo apt-get install libpaho-mqtt-dev libpaho-mqttpp-dev
```

### 2. Services

**PostgreSQL** (postgres_maestro container)
- Database: `firetv`
- User: `firetv_user`
- Password: `firetv_postgres_2026_secure`
- Port: 5432

**EMQX MQTT Broker** (emqx container)
- Broker: 192.168.2.15:1883
- User: `aamat`
- Password: `exploracion`

**Home Assistant**
- URL: http://192.168.2.7:8123
- MQTT integration must be configured with discovery enabled

### 3. Fire TV Devices

Devices must be paired and registered in the PostgreSQL database.

---

## Building

```bash
cd /home/aamat/maestro_hub/hms-firetv

# Create build directory
mkdir -p build
cd build

# Configure
cmake ..

# Build (use all CPU cores)
make -j$(nproc)

# Verify binary
ls -lh hms_firetv
./hms_firetv --version  # If version flag implemented
```

**Expected Output:**
- Binary: `build/hms_firetv` (~3.1 MB)
- Compilation time: ~30 seconds

---

## Installation

### Automatic Installation

```bash
cd /home/aamat/maestro_hub/hms-firetv
./install-service.sh
```

**What it does:**
1. Stops any running instances
2. Copies service file to `/etc/systemd/system/`
3. Reloads systemd daemon
4. Enables service (start on boot)
5. Starts service
6. Shows status

### Manual Installation

```bash
# Copy service file
sudo cp hms-firetv.service /etc/systemd/system/
sudo chmod 644 /etc/systemd/system/hms-firetv.service

# Reload systemd
sudo systemctl daemon-reload

# Enable and start
sudo systemctl enable hms-firetv
sudo systemctl start hms-firetv

# Check status
sudo systemctl status hms-firetv
```

---

## Service Management

### Status

```bash
# Check if service is running
sudo systemctl status hms-firetv

# Check if enabled on boot
sudo systemctl is-enabled hms-firetv

# Check if active
sudo systemctl is-active hms-firetv
```

### Start/Stop/Restart

```bash
# Start service
sudo systemctl start hms-firetv

# Stop service
sudo systemctl stop hms-firetv

# Restart service
sudo systemctl restart hms-firetv

# Reload configuration (after editing service file)
sudo systemctl daemon-reload
sudo systemctl restart hms-firetv
```

### Enable/Disable Auto-Start

```bash
# Enable (start on boot)
sudo systemctl enable hms-firetv

# Disable (don't start on boot)
sudo systemctl disable hms-firetv
```

---

## Logging

### View Logs

```bash
# Follow logs in real-time
sudo journalctl -u hms-firetv -f

# Last 50 lines
sudo journalctl -u hms-firetv -n 50

# Today's logs
sudo journalctl -u hms-firetv --since today

# Logs since specific time
sudo journalctl -u hms-firetv --since "2026-02-03 10:00:00"

# Logs with specific priority
sudo journalctl -u hms-firetv -p err  # Errors only
```

### Log Rotation

Systemd journal handles log rotation automatically. Configuration:

```bash
# Check journal size
sudo journalctl --disk-usage

# Rotate logs manually
sudo journalctl --rotate

# Vacuum old logs (keep last 7 days)
sudo journalctl --vacuum-time=7d

# Vacuum by size (keep last 1GB)
sudo journalctl --vacuum-size=1G
```

---

## Monitoring

### Health Check

```bash
# HTTP health endpoint
curl http://localhost:8888/health

# Expected response:
# {
#   "service": "HMS FireTV",
#   "version": "1.0.0",
#   "database": "connected",
#   "mqtt": "connected",
#   "status": "healthy"
# }
```

### MQTT Monitoring

```bash
# Monitor all button command topics
mosquitto_sub -h 192.168.2.15 -u aamat -P exploracion \
  -t "maestro_hub/colada/#" -v

# Monitor discovery messages
mosquitto_sub -h 192.168.2.15 -u aamat -P exploracion \
  -t "homeassistant/button/colada/#" -v

# Monitor availability
mosquitto_sub -h 192.168.2.15 -u aamat -P exploracion \
  -t "maestro_hub/firetv/+/availability" -v
```

### Database Connection

```bash
# Check database connectivity
docker exec -it postgres_maestro psql -U firetv_user -d firetv -c "SELECT COUNT(*) FROM fire_tv_devices;"
```

---

## Updating

### After Code Changes

```bash
# Stop service
sudo systemctl stop hms-firetv

# Rebuild
cd /home/aamat/maestro_hub/hms-firetv/build
make -j$(nproc)

# Start service
sudo systemctl start hms-firetv

# Check logs for errors
sudo journalctl -u hms-firetv -f
```

### After Configuration Changes

```bash
# Edit service file
sudo nano /etc/systemd/system/hms-firetv.service

# Reload and restart
sudo systemctl daemon-reload
sudo systemctl restart hms-firetv
```

---

## Uninstallation

### Automatic

```bash
cd /home/aamat/maestro_hub/hms-firetv
./uninstall-service.sh
```

### Manual

```bash
# Stop and disable
sudo systemctl stop hms-firetv
sudo systemctl disable hms-firetv

# Remove service file
sudo rm /etc/systemd/system/hms-firetv.service

# Reload systemd
sudo systemctl daemon-reload

# Remove binary (optional)
rm -rf /home/aamat/maestro_hub/hms-firetv/build
```

---

## Troubleshooting

### Service Won't Start

**Check logs:**
```bash
sudo journalctl -u hms-firetv -n 100
```

**Common issues:**

1. **Database not available**
   ```bash
   docker ps | grep postgres_maestro
   ```

2. **MQTT broker not available**
   ```bash
   docker ps | grep emqx
   mosquitto_pub -h 192.168.2.15 -u aamat -P exploracion -t test -m test
   ```

3. **Port 8888 already in use**
   ```bash
   sudo lsof -i :8888
   # Or change port in service file
   ```

4. **Binary not found**
   ```bash
   ls -l /home/aamat/maestro_hub/hms-firetv/build/hms_firetv
   ```

### Buttons Not Appearing in Home Assistant

**Check discovery messages:**
```bash
mosquitto_sub -h 192.168.2.15 -u aamat -P exploracion \
  -t "homeassistant/button/colada/+/config" -C 4
```

**Reload MQTT integration in HA:**
1. Go to Settings → Devices & Services
2. Find MQTT integration
3. Click "..." → Reload

**Check HA MQTT discovery is enabled:**
1. Configuration → Integrations → MQTT
2. Configure → Enable discovery

### Commands Not Working

**Test direct MQTT:**
```bash
# Send volume up command
mosquitto_pub -h 192.168.2.15 -u aamat -P exploracion \
  -t "maestro_hub/colada/livingroom_colada/volume_up" \
  -m "PRESS"

# Watch service logs
sudo journalctl -u hms-firetv -f
```

**Check device pairing:**
```bash
docker exec -it postgres_maestro psql -U firetv_user -d firetv \
  -c "SELECT device_id, name, ip_address, status FROM fire_tv_devices;"
```

### High CPU Usage

```bash
# Check process stats
top -p $(pgrep hms_firetv)

# Check for connection issues
sudo journalctl -u hms-firetv | grep -i "error\|failed\|reconnect"
```

### Memory Leaks

```bash
# Monitor memory usage over time
watch -n 1 'ps aux | grep hms_firetv | grep -v grep'

# Check for file descriptor leaks
lsof -p $(pgrep hms_firetv) | wc -l
```

---

## Performance Tuning

### Systemd Service Limits

Edit `/etc/systemd/system/hms-firetv.service`:

```ini
# Increase file descriptors (for many devices)
LimitNOFILE=131072

# Increase process limit
LimitNPROC=8192

# Set CPU affinity (bind to specific cores)
CPUAffinity=0 1

# Memory limit (optional)
MemoryLimit=512M
```

### Application Configuration

Edit service environment variables:

```ini
# More threads for higher throughput
Environment="THREAD_NUM=8"

# Different log level
Environment="LOG_LEVEL=warning"
```

---

## Backup & Restore

### Backup

```bash
# Stop service
sudo systemctl stop hms-firetv

# Backup binary
cp /home/aamat/maestro_hub/hms-firetv/build/hms_firetv \
   /backup/hms_firetv_$(date +%Y%m%d)

# Backup service file
sudo cp /etc/systemd/system/hms-firetv.service \
   /backup/hms-firetv.service_$(date +%Y%m%d)

# Backup database (via pg_dump)
docker exec postgres_maestro pg_dump -U firetv_user firetv \
  > /backup/firetv_db_$(date +%Y%m%d).sql

# Start service
sudo systemctl start hms-firetv
```

### Restore

```bash
# Stop service
sudo systemctl stop hms-firetv

# Restore binary
cp /backup/hms_firetv_20260203 \
   /home/aamat/maestro_hub/hms-firetv/build/hms_firetv
chmod +x /home/aamat/maestro_hub/hms-firetv/build/hms_firetv

# Restore service file
sudo cp /backup/hms-firetv.service_20260203 \
   /etc/systemd/system/hms-firetv.service
sudo systemctl daemon-reload

# Restore database
cat /backup/firetv_db_20260203.sql | \
  docker exec -i postgres_maestro psql -U firetv_user -d firetv

# Start service
sudo systemctl start hms-firetv
```

---

## Migration from Python Service

### Pre-Migration Checklist

- [ ] C++ service tested and working
- [ ] All devices paired and in database
- [ ] Home Assistant MQTT discovery working
- [ ] Backup of Python service configuration

### Migration Steps

1. **Stop Python service:**
   ```bash
   docker compose stop colada-lightning
   docker compose rm -f colada-lightning
   ```

2. **Install C++ service:**
   ```bash
   cd /home/aamat/maestro_hub/hms-firetv
   ./install-service.sh
   ```

3. **Verify in Home Assistant:**
   - Buttons should appear automatically
   - Test volume up/down
   - Test navigation
   - Test app launching

4. **Monitor for issues:**
   ```bash
   sudo journalctl -u hms-firetv -f
   ```

5. **Clean up old discovery messages (if needed):**
   ```bash
   # Python service used different entity names
   # They should auto-expire, but can be manually removed in HA
   ```

---

## Security Considerations

### Service User

Service runs as user `aamat`. Consider creating dedicated user:

```bash
# Create dedicated user
sudo useradd -r -s /bin/false hms-firetv

# Update service file
sudo sed -i 's/User=aamat/User=hms-firetv/' /etc/systemd/system/hms-firetv.service
sudo sed -i 's/Group=aamat/Group=hms-firetv/' /etc/systemd/system/hms-firetv.service

# Update permissions
sudo chown -R hms-firetv:hms-firetv /home/aamat/maestro_hub/hms-firetv/build

# Reload
sudo systemctl daemon-reload
sudo systemctl restart hms-firetv
```

### Network Security

- Service binds to `0.0.0.0:8888` (all interfaces)
- Consider binding to localhost only if not needed externally
- MQTT credentials in service file (mode 600)
- Database credentials in service file (mode 600)

### Firewall

```bash
# Allow port 8888 only from local network
sudo ufw allow from 192.168.2.0/24 to any port 8888

# Or block external access
sudo ufw deny 8888
```

---

## Production Checklist

- [ ] Service installed and enabled
- [ ] Service starts on boot (tested with reboot)
- [ ] All devices appear in Home Assistant
- [ ] All button commands working
- [ ] Health check returns "healthy"
- [ ] Logs show no errors
- [ ] MQTT reconnect tested (restart EMQX broker)
- [ ] Database reconnect tested (restart PostgreSQL)
- [ ] Service restart tested
- [ ] Monitoring configured
- [ ] Backup automated
- [ ] Documentation reviewed

---

## Support

**Logs:** `/var/log/journal/` (via journalctl)
**Config:** `/etc/systemd/system/hms-firetv.service`
**Binary:** `/home/aamat/maestro_hub/hms-firetv/build/hms_firetv`
**Source:** `/home/aamat/maestro_hub/hms-firetv/`

**Health Check:** http://localhost:8888/health

---

**Version**: 1.0.0
**Last Updated**: February 3, 2026
