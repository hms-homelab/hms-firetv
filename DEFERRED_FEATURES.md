# Deferred Features - HMS FireTV

**Date**: February 3, 2026
**Status**: Documented for Future Implementation

---

## Overview

The following features were planned but deferred because the core MQTT integration provides all necessary functionality through Home Assistant. They can be implemented later if needed.

---

## Phase 4: REST API Endpoints ⏸️

**Status**: Deferred
**Reason**: Home Assistant provides full control via MQTT buttons

### Planned Endpoints

#### Device Management

```http
GET    /api/devices                 # List all devices
GET    /api/devices/:id             # Get device details
POST   /api/devices                 # Add new device
PUT    /api/devices/:id             # Update device
DELETE /api/devices/:id             # Remove device
```

#### Device Control

```http
POST   /api/devices/:id/command     # Send generic command
POST   /api/devices/:id/navigation  # Navigation command (dpad, home, back)
POST   /api/devices/:id/media       # Media control (play, pause)
POST   /api/devices/:id/volume      # Volume control
POST   /api/devices/:id/app         # Launch app
POST   /api/devices/:id/power       # Power on/off
```

#### Pairing

```http
POST   /api/devices/:id/pair/start  # Display PIN on TV
POST   /api/devices/:id/pair/verify # Verify PIN and complete pairing
POST   /api/devices/:id/unpair      # Remove pairing
```

#### History & Stats

```http
GET    /api/devices/:id/history     # Command history
GET    /api/devices/:id/stats       # Usage statistics
GET    /api/devices/:id/apps        # Installed apps
```

#### System

```http
GET    /health                      # Service health (✅ Implemented)
GET    /status                      # Service status (✅ Implemented)
GET    /api/devices/:id/status      # Device status
```

### Implementation Notes

**Framework**: Drogon HTTP server (already integrated)

**Example Controller** (not implemented):

```cpp
// controllers/DeviceController.h
class DeviceController : public drogon::HttpController<DeviceController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(DeviceController::listDevices, "/api/devices", Get);
    ADD_METHOD_TO(DeviceController::getDevice, "/api/devices/{id}", Get);
    ADD_METHOD_TO(DeviceController::sendCommand, "/api/devices/{id}/command", Post);
    METHOD_LIST_END

    void listDevices(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);

    void getDevice(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback,
                  std::string id);

    void sendCommand(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback,
                    std::string id);
};
```

**Estimated Effort**: 4-6 hours

**When to Implement**:
- If third-party integrations need HTTP API
- If mobile app is developed
- If web dashboard is needed

---

## Phase 6: Web UI ⏸️

**Status**: Deferred
**Reason**: Home Assistant provides excellent UI for all controls

### Planned Features

#### Remote Control Interface

```
┌─────────────────────────────┐
│   Living Room Fire TV       │
│   Status: Online            │
├─────────────────────────────┤
│                             │
│       [     ↑     ]         │
│     [  ←  OK  →  ]          │
│       [     ↓     ]         │
│                             │
│     [Home] [Back] [Menu]    │
│                             │
│     [ ▶ ] [ ❚❚ ] [ ■ ]      │
│                             │
│     [ 🔊+ ] [ 🔊- ] [ 🔇 ]   │
│                             │
│   Quick Launch:             │
│   [Netflix] [Prime] [YouTube]│
│                             │
└─────────────────────────────┘
```

#### Device Management

- Add/Remove devices
- View device status
- Pairing wizard with PIN display
- App management
- Command history

#### Dashboard

- All devices at a glance
- Quick actions
- Recent activity
- Device health

### Technology Stack

**Frontend**:
- Static HTML/CSS/JavaScript (no framework needed)
- Or: React/Vue for richer experience

**Static File Serving**:
```cpp
// Already configured in main.cpp
app().setDocumentRoot("./static");
app().registerHandler("/",
    [](const HttpRequestPtr &req,
       std::function<void(const HttpResponsePtr &)> &&callback) {
        callback(HttpResponse::newFileResponse("./static/index.html"));
    });
```

**API Integration**:
```javascript
// Example: Send volume up command
async function volumeUp(deviceId) {
    const response = await fetch(`/api/devices/${deviceId}/volume`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({action: 'volume_up'})
    });
    return response.json();
}
```

### Existing Python Web UI

The Python `colada-lightning` service had a web UI that can be adapted:

```
colada-lightning/app/static/
├── index.html           # Main page
├── remote.html          # Remote control
├── devices.html         # Device management
├── css/
│   └── style.css
└── js/
    ├── api.js           # API client
    ├── remote.js        # Remote control logic
    └── devices.js       # Device management
```

**Adaptation Required**:
1. Copy static files to `hms-firetv/static/`
2. Update API endpoints in JavaScript
3. Test with Drogon server

**Estimated Effort**: 2-3 hours (adaptation only)

**When to Implement**:
- If standalone web interface needed (not using HA)
- If remote access from non-HA devices required
- If custom remote layout desired

---

## Phase 7: Additional Integrations ⏸️

**Status**: Not Started

### Planned Integrations

#### 1. Voice Assistant Direct Control

Allow voice control without Home Assistant:

```python
# Alexa Skill / Google Action
def handle_voice_command(command):
    if command == "turn on living room tv":
        mqtt_publish("maestro_hub/colada/livingroom_colada/wake", "PRESS")
```

#### 2. Mobile App API

RESTful API for mobile apps:

```swift
// iOS/Android app
func volumeUp(deviceId: String) {
    HTTPClient.post("/api/devices/\(deviceId)/volume",
                    body: ["action": "volume_up"])
}
```

#### 3. Webhooks

Trigger Fire TV actions from external systems:

```bash
curl -X POST http://192.168.2.15:8888/webhook/netflix \
  -H "X-Webhook-Token: secret" \
  -d '{"device": "livingroom_colada"}'
```

#### 4. Metrics & Monitoring

Prometheus metrics:

```cpp
// Expose metrics endpoint
GET /metrics

# HELP hms_firetv_commands_total Total commands sent
# TYPE hms_firetv_commands_total counter
hms_firetv_commands_total{device="livingroom_colada",command="volume_up"} 42

# HELP hms_firetv_command_duration_seconds Command execution time
# TYPE hms_firetv_command_duration_seconds histogram
hms_firetv_command_duration_seconds_bucket{le="0.01"} 120
hms_firetv_command_duration_seconds_bucket{le="0.05"} 450
```

#### 5. Database Migration Tool

Migrate devices from Python service:

```bash
# Export from Python service
docker exec colada-lightning python export_devices.py > devices.json

# Import to C++ service
./hms-firetv-import devices.json
```

---

## Why These Were Deferred

### 1. Home Assistant Coverage

Home Assistant provides:
- ✅ Button UI for all controls
- ✅ Automation engine
- ✅ Mobile apps (iOS/Android)
- ✅ Voice control (Alexa, Google, Siri)
- ✅ Remote access (via HA Cloud or Nabu Casa)
- ✅ Beautiful UI
- ✅ History tracking
- ✅ Notifications

### 2. Simplicity

Core MQTT integration is:
- ✅ Simpler to maintain
- ✅ More reliable (fewer components)
- ✅ Better separation of concerns
- ✅ Easier to debug

### 3. Resource Efficiency

Without API/UI:
- ✅ Lower memory footprint (~50MB vs ~150MB)
- ✅ Faster startup (<2s)
- ✅ Fewer dependencies
- ✅ Smaller attack surface

### 4. Development Time

Deferring these features saved:
- **Phase 4 (API)**: 4-6 hours
- **Phase 6 (UI)**: 2-3 hours
- **Total**: ~8 hours saved

---

## Implementation Priority (If Needed)

### High Priority (Implement First)

1. **Health Check Endpoint** - ✅ Already implemented
2. **Device List API** - Simple GET endpoint
3. **Command History** - Useful for debugging

### Medium Priority

1. **Pairing API** - Useful if adding devices frequently
2. **Basic Web UI** - Copy from Python service
3. **Webhooks** - For external integrations

### Low Priority

1. **Metrics/Monitoring** - HA has built-in metrics
2. **Advanced UI** - HA UI is sufficient
3. **Mobile Apps** - HA mobile apps work well

---

## How to Implement When Needed

### Phase 4: REST API

1. Create controller classes in `src/api/`:
   ```bash
   mkdir -p src/api
   touch src/api/DeviceController.cpp
   touch include/api/DeviceController.h
   ```

2. Register routes in `main.cpp`:
   ```cpp
   app().registerController<DeviceController>();
   ```

3. Implement CRUD operations using existing repositories

4. Add API documentation (Swagger/OpenAPI)

5. Write API tests

### Phase 6: Web UI

1. Copy static files from Python service:
   ```bash
   cp -r ../colada-lightning/app/static ./static
   ```

2. Update API client URLs in JavaScript:
   ```javascript
   const API_BASE = "http://192.168.2.15:8888/api";
   ```

3. Configure Drogon static file serving (already done)

4. Test all functionality

5. Add authentication if needed

---

## Alternative Approaches

### If Full API Needed

**Option 1**: Keep Python service running alongside C++ for API only
- C++ handles MQTT (performance)
- Python provides API/UI (convenience)
- Both share same database

**Option 2**: Use API Gateway
- nginx or Traefik routes requests
- C++ for control path (MQTT)
- Python/Node.js for management API

**Option 3**: GraphQL API
- Single flexible endpoint
- Better for complex queries
- Popular clients available

---

## Documentation for Future Developers

### Adding REST Endpoints

```cpp
// 1. Create controller
class MyController : public drogon::HttpController<MyController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(MyController::myMethod, "/api/my-endpoint", Post);
    METHOD_LIST_END

    void myMethod(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
};

// 2. Implement method
void MyController::myMethod(const HttpRequestPtr &req,
                           std::function<void(const HttpResponsePtr &)> &&callback) {
    Json::Value response;
    response["status"] = "ok";

    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
}

// 3. Register in main.cpp
app().registerController<MyController>();
```

### Adding Web Pages

```html
<!-- static/my-page.html -->
<!DOCTYPE html>
<html>
<head>
    <title>HMS FireTV</title>
</head>
<body>
    <h1>My Custom Page</h1>
    <script src="/js/api.js"></script>
</body>
</html>
```

---

## Conclusion

**Current Status**: Production-ready MQTT integration with Home Assistant

**Future Work**: API and UI can be added if/when needed

**Recommendation**: Continue with current MQTT-only approach unless specific use case requires API/UI

**Estimated Total Effort to Complete All Deferred Features**: 15-20 hours

---

**Last Updated**: February 3, 2026
**Maintained By**: HMS FireTV Project
