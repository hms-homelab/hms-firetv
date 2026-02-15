# HMS FireTV - Testing & Troubleshooting Guide

**Last Updated**: February 3, 2026
**Version**: 1.0.0

## Table of Contents

1. [Quick Health Check](#quick-health-check)
2. [Component Testing](#component-testing)
3. [Integration Testing](#integration-testing)
4. [Common Issues](#common-issues)
5. [Diagnostic Commands](#diagnostic-commands)
6. [Performance Baselines](#performance-baselines)
7. [Error Messages Reference](#error-messages-reference)

---

## Quick Health Check

### 1-Minute System Check

```bash
cd /home/aamat/maestro_hub/hms-firetv/build

# Test 1: Database Connection
PGPASSWORD=firetv_postgres_2026_secure psql -h 192.168.2.15 \
  -U firetv_user -d firetv -c "SELECT COUNT(*) FROM fire_tv_devices;"
# Expected: Number of devices (e.g., "4")

# Test 2: Device Status
PGPASSWORD=firetv_postgres_2026_secure psql -h 192.168.2.15 \
  -U firetv_user -d firetv -c "SELECT device_id, name, ip_address, status FROM fire_tv_devices;"
# Expected: List of devices with status

# Test 3: Quick Volume Test (Living Room)
./test_volume
# Expected: Both commands return SUCCESS with status=200
```

**All green?** System is healthy ✅
**Any failures?** See [Common Issues](#common-issues)

---

## Component Testing

### Component 1: DatabaseService

**Purpose**: PostgreSQL connection with retry logic

**Test Program**: `test_device_repo.cpp`

**Compile & Run**:
```bash
cd /home/aamat/maestro_hub/hms-firetv/build
g++ -std=c++17 -I../include -I/usr/include/jsoncpp \
  -o test_device_repo ../test_device_repo.cpp \
  ../src/services/DatabaseService.cpp \
  ../src/repositories/DeviceRepository.cpp \
  -lpqxx -lpq -ljsoncpp -pthread

./test_device_repo
```

**Expected Output**:
```
Testing DeviceRepository...
==========================================
[DatabaseService] ✅ Connected to PostgreSQL: firetv@192.168.2.15:5432
✓ Database connected

1. Testing getAllDevices()...
   Found 4 devices in database
   - livingroom_colada (Living Room Fire TV (Colada)) - online
   - albin_colada (Albin Fire TV (Colada)) - online
   - bedroom_colada (Bedroom Fire TV (Colada)) - online
   - test_cpp_device (C++ Test Device) - online

2. Checking for test device 'test_cpp_device'...
   Device exists: yes

3. Test device already exists

4. Testing getDeviceById()...
   ✓ Device retrieved: C++ Test Device
   Status: online
   IP: 192.168.2.99

5. Testing updateLastSeen()...
   Update result: success

6. Testing getDevicesByStatus('online')...
   Found 4 online devices

==========================================
All tests completed successfully!
```

**What It Tests**:
- Database connection establishment
- CRUD operations (Create, Read, Update, Delete)
- Query execution with retry logic
- Device existence checks
- Status filtering

**Common Failures**:

| Error | Cause | Fix |
|-------|-------|-----|
| `connection failed` | PostgreSQL not running | `sudo systemctl start postgresql` |
| `password authentication failed` | Wrong password | Check .secrets file or docker-compose.yml |
| `database "firetv" does not exist` | Database not created | See [Database Setup](#database-setup) |
| `relation "fire_tv_devices" does not exist` | Table not created | See [Database Setup](#database-setup) |

---

### Component 2: LightningClient

**Purpose**: Fire TV Lightning Protocol HTTP/HTTPS client

**Test Program**: `test_lightning_client.cpp`

**Compile & Run**:
```bash
cd /home/aamat/maestro_hub/hms-firetv/build
g++ -std=c++17 -I../include -I/usr/include/jsoncpp \
  -o test_lightning_client ../test_lightning_client.cpp \
  ../src/clients/LightningClient.cpp \
  -ljsoncpp -lcurl -pthread

./test_lightning_client
```

**Expected Output**:
```
Testing LightningClient...
==========================================

1. Creating LightningClient for 192.168.2.99...
[LightningClient] Initialized for device at 192.168.2.99
   ✓ LightningClient created

2. Testing token management...
   Set token: test_token_12345
   Retrieved token: test_token_12345
   ✓ Token management working

3. Testing health check...
   Health check result: unreachable
   (Expected: unreachable for dummy IP)

4. Testing Lightning API availability...
   API available: no
   (Expected: no for dummy IP)

==========================================
Structure tests completed!
```

**What It Tests**:
- LightningClient instantiation
- Token storage and retrieval
- Health check methods (returns false for dummy IP)
- CURL initialization

**Common Failures**:

| Error | Cause | Fix |
|-------|-------|-----|
| `Failed to initialize CURL` | libcurl not installed | `sudo apt-get install libcurl4-openssl-dev` |
| Segmentation fault | Memory corruption | Check CURL handle cleanup in destructor |
| JSON parse errors | jsoncpp not linked | Add `-ljsoncpp` to compile command |

---

### Component 3: Volume Control (Real Device)

**Purpose**: End-to-end test with real Fire TV device

**Test Program**: `test_volume.cpp`

**Compile & Run**:
```bash
cd /home/aamat/maestro_hub/hms-firetv/build
g++ -std=c++17 -I../include -I/usr/include/jsoncpp \
  -o test_volume ../test_volume.cpp \
  ../src/services/DatabaseService.cpp \
  ../src/repositories/DeviceRepository.cpp \
  ../src/clients/LightningClient.cpp \
  -lpqxx -lpq -ljsoncpp -lcurl -pthread

./test_volume
```

**Expected Output** (Success):
```
Volume Control Test - Living Room Fire TV
==========================================
[DatabaseService] ✅ Connected to PostgreSQL: firetv@192.168.2.15:5432
Device: Living Room Fire TV (Colada) (192.168.2.63)
[LightningClient] Initialized for device at 192.168.2.63

[1] Volume DOWN...
[LightningClient] Navigation command 'volume_down' sent (97ms)
    Result: SUCCESS (status=200, 97ms)

[2] Volume UP...
[LightningClient] Navigation command 'volume_up' sent (12ms)
    Result: SUCCESS (status=200, 12ms)

==========================================
Done!
```

**What It Tests**:
- Full stack integration (Database → Repository → Lightning Client → Fire TV)
- Real network communication over HTTPS
- Command execution with timing
- SSL certificate handling (self-signed)

**Common Failures**:

| Error | Status Code | Response Time | Cause | Fix |
|-------|-------------|---------------|-------|-----|
| `Device not found` | N/A | N/A | Device not in database | Check device_id, add device to database |
| `FAILED (status=0)` | 0 | 2000-10000ms | Timeout, device unreachable | Check IP, ensure device powered on |
| `FAILED (status=401)` | 401 | 50-200ms | Unauthorized, no client_token | Pair device first (see [Pairing](#pairing)) |
| `FAILED (status=404)` | 404 | 50-200ms | Endpoint not found | Check Lightning API version, URL format |
| `FAILED (status=500)` | 500 | 50-200ms | Fire TV internal error | Restart Fire TV, check logs |
| `Timeout was reached` | 0 | 10000ms | Network timeout | Check firewall, device in standby |

---

## Integration Testing

### Full System Integration Test

**Test Program**: `test_full_integration.cpp`

**What It Tests**:
1. Database connection and initialization
2. Device retrieval from database
3. LightningClient creation with device data
4. Device health check (2-port strategy)
5. Device wake-up from standby
6. Command execution (home, dpad, back)
7. Database status updates

**Run**:
```bash
cd /home/aamat/maestro_hub/hms-firetv/build
./test_full_integration
```

**Expected Flow**:
```
STEP 1: Initialize Database Connection
✓ Database connected

STEP 2: Get Available Devices from Database
Found 4 devices

STEP 3: Testing with First Device
Testing device: livingroom_colada

STEP 4: Initialize Lightning Client
✓ Lightning client initialized

STEP 5: Device Health Check
Wake endpoint: ✓ responding
Lightning API: ✓ responding
✓ Device is ONLINE and ready

STEP 6: Wake Device - SKIPPED (already awake)

STEP 7: Testing Lightning Commands
✓ Commands executed successfully!
Database update: ✓ success

STEP 8: Test Summary
✓ Full Integration Test Complete!
```

---

## Common Issues

### Issue 1: Device Not Paired (401 Unauthorized)

**Symptoms**:
```
Result: FAILED (status=401, 50ms)
```

**Cause**: Device has no `client_token` in database

**Diagnosis**:
```sql
SELECT device_id, name, client_token FROM fire_tv_devices WHERE device_id = 'livingroom_colada';
```
If `client_token` is NULL, device needs pairing.

**Fix**:
```cpp
// 1. Display PIN on TV
LightningClient client(device_ip, api_key);
std::string pin = client.displayPin("HMS FireTV");
// User enters PIN on Fire TV screen

// 2. Verify PIN and get token
std::string token = client.verifyPin(pin);

// 3. Store token in database
DeviceRepository::getInstance().getDeviceById(device_id);
device.client_token = token;
DeviceRepository::getInstance().updateDevice(device);
```

**Manual Fix** (SQL):
```sql
-- Get token from Python colada-lightning instance
SELECT client_token FROM fire_tv_devices WHERE device_id = 'livingroom_colada';

-- Copy token to another device
UPDATE fire_tv_devices
SET client_token = 'TOKEN_VALUE_HERE'
WHERE device_id = 'new_device';
```

---

### Issue 2: Device Offline (Timeout)

**Symptoms**:
```
Result: FAILED (status=0, 10000ms)
Error: Timeout was reached
```

**Cause**: Device powered off, in deep sleep, or network unreachable

**Diagnosis**:
```bash
# Test 1: Can you ping it?
ping -c 3 192.168.2.63

# Test 2: Is port 8009 open?
curl -X POST http://192.168.2.63:8009/apps/FireTVRemote -v

# Test 3: Is port 8080 open?
curl -k https://192.168.2.63:8080/v1/FireTV -v
```

**Expected Responses**:
- **Device ON**: Ping succeeds, both ports respond
- **Device STANDBY**: Ping succeeds, port 8009 responds, port 8080 timeout
- **Device OFF**: Ping fails or timeout

**Fix**:
1. Power on Fire TV device
2. Ensure device on same network (192.168.2.x)
3. Check IP hasn't changed (Fire TV may get new DHCP lease)
4. Verify firewall not blocking ports 8009/8080

**Update IP in Database**:
```sql
UPDATE fire_tv_devices
SET ip_address = '192.168.2.NEW_IP'
WHERE device_id = 'livingroom_colada';
```

---

### Issue 3: Device in Standby

**Symptoms**:
```
Wake endpoint: ✓ responding
Lightning API: ✗ not responding
```

**Cause**: Fire TV in low-power standby mode

**Fix** (Automatic):
```cpp
if (!client.isLightningApiAvailable() && client.healthCheck()) {
    // Device in standby
    client.wakeDevice();
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Verify awake
    if (client.isLightningApiAvailable()) {
        // Ready for commands
    }
}
```

**Fix** (Manual):
```bash
# Wake device via HTTP
curl -X POST http://192.168.2.63:8009/apps/FireTVRemote

# Wait 3 seconds, then test
sleep 3
curl -k https://192.168.2.63:8080/v1/FireTV \
  -H "X-Api-Key: 0987654321"
```

---

### Issue 4: SSL Certificate Errors

**Symptoms**:
```
Error: SSL certificate problem: self signed certificate
```

**Cause**: Fire TV uses self-signed SSL certificate

**Fix**: Already handled in code
```cpp
curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);  // Disable verification
curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
```

If error still occurs, check libcurl version:
```bash
curl --version
# Should show: OpenSSL support
```

---

### Issue 5: Database Connection Failed

**Symptoms**:
```
[DatabaseService] ❌ Failed to connect to 192.168.2.15:5432/firetv
Error: could not connect to server
```

**Diagnosis**:
```bash
# Test 1: Is PostgreSQL running?
sudo systemctl status postgresql

# Test 2: Can you connect manually?
PGPASSWORD=firetv_postgres_2026_secure psql -h 192.168.2.15 \
  -U firetv_user -d firetv

# Test 3: Check pg_hba.conf
sudo cat /etc/postgresql/*/main/pg_hba.conf | grep "192.168"
```

**Fix**:
```bash
# Start PostgreSQL
sudo systemctl start postgresql

# Allow network connections
echo "host all all 192.168.2.0/24 md5" | \
  sudo tee -a /etc/postgresql/*/main/pg_hba.conf

# Restart PostgreSQL
sudo systemctl restart postgresql
```

---

## Diagnostic Commands

### Database Diagnostics

**Check all devices**:
```sql
SELECT device_id, name, ip_address, status,
       client_token IS NOT NULL as paired,
       last_seen_at
FROM fire_tv_devices
ORDER BY last_seen_at DESC NULLS LAST;
```

**Check device history** (if command_history table exists):
```sql
SELECT device_id, command_type, command, success, response_time_ms, timestamp
FROM command_history
WHERE device_id = 'livingroom_colada'
ORDER BY timestamp DESC
LIMIT 10;
```

**Find stale devices** (not seen in 24 hours):
```sql
SELECT device_id, name, status, last_seen_at
FROM fire_tv_devices
WHERE last_seen_at < NOW() - INTERVAL '24 hours'
   OR last_seen_at IS NULL;
```

---

### Network Diagnostics

**Test Fire TV Reachability**:
```bash
#!/bin/bash
DEVICE_IP="192.168.2.63"

echo "=== Network Diagnostics for $DEVICE_IP ==="

# Ping test
echo -n "Ping: "
if ping -c 1 -W 2 $DEVICE_IP > /dev/null 2>&1; then
    echo "✓ reachable"
else
    echo "✗ unreachable"
fi

# Port 8009 (Wake)
echo -n "Port 8009 (wake): "
if timeout 2 bash -c "echo > /dev/tcp/$DEVICE_IP/8009" 2>/dev/null; then
    echo "✓ open"
else
    echo "✗ closed/filtered"
fi

# Port 8080 (Lightning API)
echo -n "Port 8080 (API): "
if timeout 2 bash -c "echo > /dev/tcp/$DEVICE_IP/8080" 2>/dev/null; then
    echo "✓ open"
else
    echo "✗ closed/filtered"
fi

# HTTP wake test
echo -n "HTTP wake: "
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" \
  -X POST http://$DEVICE_IP:8009/apps/FireTVRemote \
  --connect-timeout 3)
if [ "$HTTP_CODE" = "200" ] || [ "$HTTP_CODE" = "201" ]; then
    echo "✓ responding ($HTTP_CODE)"
else
    echo "✗ not responding ($HTTP_CODE)"
fi

# HTTPS API test
echo -n "HTTPS API: "
HTTPS_CODE=$(curl -k -s -o /dev/null -w "%{http_code}" \
  https://$DEVICE_IP:8080/v1/FireTV \
  -H "X-Api-Key: 0987654321" \
  --connect-timeout 3)
if [ "$HTTPS_CODE" = "200" ] || [ "$HTTPS_CODE" = "401" ]; then
    echo "✓ responding ($HTTPS_CODE)"
else
    echo "✗ not responding ($HTTPS_CODE)"
fi
```

Save as `diagnose_device.sh`, make executable:
```bash
chmod +x diagnose_device.sh
./diagnose_device.sh
```

---

### Build Diagnostics

**Check compilation**:
```bash
cd /home/aamat/maestro_hub/hms-firetv/build
make clean
cmake ..
make -j$(nproc) 2>&1 | tee build.log

# Check for errors
grep -i "error" build.log
# Expected: 0 errors

# Check binary size
ls -lh hms_firetv
# Expected: ~3.0MB

# Check dependencies
ldd hms_firetv | grep -E "curl|pqxx|json"
# Expected: All found (not "not found")
```

**Missing dependencies fix**:
```bash
sudo apt-get update
sudo apt-get install -y \
  libcurl4-openssl-dev \
  libpqxx-dev \
  libpq-dev \
  libjsoncpp-dev \
  build-essential \
  cmake
```

---

## Performance Baselines

### Expected Response Times

| Operation | Min | Typical | Max | Notes |
|-----------|-----|---------|-----|-------|
| Database query | 1ms | 5ms | 50ms | Local PostgreSQL |
| Wake endpoint | 50ms | 150ms | 500ms | HTTP, port 8009 |
| Lightning API health | 50ms | 100ms | 300ms | HTTPS, port 8080 |
| Navigation command | 30ms | 80ms | 200ms | D-pad, home, back |
| Media command | 30ms | 80ms | 200ms | Play, pause |
| App launch | 100ms | 300ms | 1000ms | Depends on app |
| Volume control | 10ms | 50ms | 150ms | Fastest command |
| Display PIN | 100ms | 250ms | 500ms | Pairing step 1 |
| Verify PIN | 100ms | 250ms | 500ms | Pairing step 2 |
| Wake from standby | 2000ms | 3000ms | 5000ms | Full device boot |

### Performance Red Flags

| Metric | Threshold | Action |
|--------|-----------|--------|
| Command response > 500ms | Slow network | Check WiFi signal, router |
| Database query > 100ms | Database overload | Check PostgreSQL logs, restart |
| Wake time > 5s | Device issue | Restart Fire TV |
| Timeout (status=0) | Network failure | Check connectivity |

---

## Error Messages Reference

### DatabaseService Errors

| Message | Meaning | Fix |
|---------|---------|-----|
| `Failed to connect to server` | PostgreSQL not reachable | Check service, firewall |
| `password authentication failed` | Wrong credentials | Check password in code |
| `database "firetv" does not exist` | Database not created | Create database (see below) |
| `relation "fire_tv_devices" does not exist` | Table missing | Run schema SQL (see below) |
| `Query failed after 3 attempts` | Database connection unstable | Check network, restart PostgreSQL |
| `No connection available` | Connection lost | Auto-reconnect will trigger |

### LightningClient Errors

| Message | Meaning | Fix |
|---------|---------|-----|
| `Failed to initialize CURL` | libcurl missing | Install libcurl4-openssl-dev |
| `Timeout was reached` | Device unreachable | Check device power, IP |
| `SSL certificate problem` | SSL verification issue | Already disabled in code |
| `Couldn't connect to server` | Network failure | Check device IP, firewall |
| `Empty reply from server` | Device crashed | Restart Fire TV |
| `HTTP 401 Unauthorized` | Missing/invalid token | Pair device |
| `HTTP 404 Not Found` | Wrong endpoint | Check API version, URL |

### DeviceRepository Errors

| Message | Meaning | Fix |
|---------|---------|-----|
| `Failed to create device` | Database constraint violation | Check unique device_id |
| `Device not found` | Query returned no rows | Verify device_id exists |
| `PIN mismatch` | Wrong PIN entered | Try pairing again |
| `No PIN set` | PIN expired or cleared | Call displayPin() again |

---

## Database Setup

### Create Database and User

```sql
-- As postgres superuser
CREATE DATABASE firetv;
CREATE USER firetv_user WITH PASSWORD 'firetv_postgres_2026_secure';
GRANT ALL PRIVILEGES ON DATABASE firetv TO firetv_user;
```

### Create Table Schema

```sql
-- Connect to firetv database
\c firetv

CREATE TABLE fire_tv_devices (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(100) UNIQUE NOT NULL,
    name VARCHAR(200) NOT NULL,
    ip_address VARCHAR(50) NOT NULL,
    api_key VARCHAR(100) NOT NULL DEFAULT '0987654321',
    client_token VARCHAR(500),
    pin_code VARCHAR(10),
    pin_expires_at TIMESTAMP,
    status VARCHAR(20) DEFAULT 'offline',
    adb_enabled BOOLEAN DEFAULT false,
    last_seen_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Grant permissions
GRANT ALL ON fire_tv_devices TO firetv_user;
GRANT USAGE, SELECT ON SEQUENCE fire_tv_devices_id_seq TO firetv_user;

-- Insert test device
INSERT INTO fire_tv_devices (device_id, name, ip_address, api_key, status)
VALUES ('livingroom_colada', 'Living Room Fire TV', '192.168.2.63', '0987654321', 'online');
```

---

## Pairing Process

### Step-by-Step Pairing

**1. Display PIN on Fire TV**:
```cpp
LightningClient client("192.168.2.63", "0987654321");

// Optional: Wake device first
client.wakeDevice();
std::this_thread::sleep_for(std::chrono::seconds(3));

// Display PIN (will appear on TV screen)
std::string pin = client.displayPin("HMS FireTV");
std::cout << "PIN displayed on TV: " << pin << std::endl;
// Example output: "123456"
```

**2. User Action**:
- User sees PIN on Fire TV screen
- User enters PIN using Fire TV remote
- User confirms PIN

**3. Verify PIN and Get Token**:
```cpp
// Verify the PIN (must match what user entered on TV)
std::string token = client.verifyPin(pin);
if (!token.empty()) {
    std::cout << "Token: " << token << std::endl;

    // Store token in database
    auto device = DeviceRepository::getInstance().getDeviceById("livingroom_colada");
    if (device.has_value()) {
        device->client_token = token;
        device->status = "online";
        DeviceRepository::getInstance().updateDevice(*device);
        std::cout << "Device paired successfully!" << std::endl;
    }
}
```

**4. Verification**:
```bash
# Check token in database
PGPASSWORD=firetv_postgres_2026_secure psql -h 192.168.2.15 \
  -U firetv_user -d firetv \
  -c "SELECT device_id, client_token FROM fire_tv_devices WHERE device_id = 'livingroom_colada';"

# Expected: client_token has value
```

---

## Test Files Reference

### Available Test Programs

| File | Purpose | Compile Command |
|------|---------|----------------|
| `test_device_repo.cpp` | Database + Repository | See [Component 1](#component-1-databaseservice) |
| `test_lightning_client.cpp` | Lightning client structure | See [Component 2](#component-2-lightningclient) |
| `test_volume.cpp` | Volume control (real device) | See [Component 3](#component-3-volume-control-real-device) |
| `test_full_integration.cpp` | Full system integration | See [Integration Testing](#integration-testing) |
| `test_livingroom.cpp` | Living room specific tests | Similar to test_volume |

### Compilation Template

```bash
g++ -std=c++17 \
  -I../include \
  -I/usr/include/jsoncpp \
  -o TEST_NAME \
  ../TEST_NAME.cpp \
  ../src/services/DatabaseService.cpp \
  ../src/repositories/DeviceRepository.cpp \
  ../src/clients/LightningClient.cpp \
  -lpqxx -lpq -ljsoncpp -lcurl -pthread
```

---

## Quick Reference Card

### ✅ System Healthy Checklist

- [ ] PostgreSQL running: `systemctl status postgresql`
- [ ] Database accessible: `psql -h 192.168.2.15 -U firetv_user -d firetv`
- [ ] Devices in database: `SELECT COUNT(*) FROM fire_tv_devices;`
- [ ] Fire TV reachable: `ping 192.168.2.63`
- [ ] Port 8009 open: `curl -X POST http://192.168.2.63:8009/apps/FireTVRemote`
- [ ] Port 8080 open: `curl -k https://192.168.2.63:8080/v1/FireTV`
- [ ] Volume test passes: `./test_volume` returns SUCCESS

### 🔍 First Troubleshooting Steps

1. **Check logs**: Look for error messages in output
2. **Test connectivity**: `ping <device_ip>`
3. **Check database**: Query `fire_tv_devices` table
4. **Verify pairing**: Check `client_token` not NULL
5. **Run diagnostics**: `./diagnose_device.sh`
6. **Check device state**: Run health check test

### 📞 When All Else Fails

1. Restart Fire TV device (hold power button 10s)
2. Restart PostgreSQL: `sudo systemctl restart postgresql`
3. Recompile everything: `make clean && cmake .. && make`
4. Check device IP hasn't changed
5. Re-pair device if 401 errors
6. Review this document section by section

---

**Document Version**: 1.0.0
**Last Verified**: February 3, 2026
**Next Review**: When adding Phase 4 (REST API Endpoints)
