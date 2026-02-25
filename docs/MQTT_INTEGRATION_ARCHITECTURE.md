# HMS FireTV - MQTT Integration Architecture

**Last Updated**: February 3, 2026

## Overview

Home Assistant communicates with HMS FireTV via **MQTT**, not direct API calls. The service uses **MQTT Discovery** protocol to auto-register devices as media players in Home Assistant.

## Architecture Flow

```
Home Assistant (192.168.2.7)
    ↓ MQTT Publish
EMQX Broker (192.168.2.15:1883)
    ↓ MQTT Subscribe
HMS FireTV (C++ Service)
    ↓ Lightning Protocol (HTTPS)
Fire TV Device (192.168.2.63)
```

## MQTT Topics

### Discovery Topics (Service → Home Assistant)

**Configuration Topic** (published once on startup):
```
homeassistant/media_player/{device_id}/config
```

**Payload Example**:
```json
{
  "name": "Living Room Fire TV",
  "unique_id": "firetv_livingroom_colada",
  "device": {
    "identifiers": ["firetv_livingroom_colada"],
    "name": "Living Room Fire TV",
    "manufacturer": "Amazon",
    "model": "Fire TV Stick",
    "sw_version": "1.0.0"
  },
  "state_topic": "maestro_hub/firetv/livingroom_colada/state",
  "availability_topic": "maestro_hub/firetv/livingroom_colada/availability",
  "command_topic": "maestro_hub/firetv/livingroom_colada/set",
  "supported_features": [
    "volume_up",
    "volume_down",
    "volume_mute",
    "turn_on",
    "turn_off",
    "select_source"
  ]
}
```

**State Topic** (published on state changes):
```
maestro_hub/firetv/{device_id}/state
```

**Payload Example**:
```json
{
  "state": "on",
  "volume_level": 0.5,
  "is_volume_muted": false,
  "source": "Netflix",
  "media_title": "Watching content"
}
```

**Availability Topic** (published periodically):
```
maestro_hub/firetv/{device_id}/availability
```

**Payload**: `online` or `offline`

### Command Topics (Home Assistant → Service)

**Main Command Topic** (subscribed by service):
```
maestro_hub/firetv/{device_id}/set
```

**Command Payloads**:
```json
// Media controls
{"command": "media_play"}
{"command": "media_pause"}
{"command": "media_play_pause"}
{"command": "media_stop"}
{"command": "media_next_track"}
{"command": "media_previous_track"}

// Volume controls
{"command": "volume_up"}
{"command": "volume_down"}
{"command": "volume_mute"}
{"command": "volume_set", "volume_level": 0.7}

// Power controls
{"command": "turn_on"}
{"command": "turn_off"}

// Navigation
{"command": "navigate", "direction": "up"}
{"command": "navigate", "direction": "down"}
{"command": "navigate", "direction": "left"}
{"command": "navigate", "direction": "right"}
{"command": "navigate", "action": "select"}
{"command": "navigate", "action": "home"}
{"command": "navigate", "action": "back"}
{"command": "navigate", "action": "menu"}

// App launch
{"command": "select_source", "source": "Netflix"}
{"command": "select_source", "source": "Prime Video"}
{"command": "launch_app", "package": "com.netflix.ninja"}
```

## Python Implementation (Current - colada-lightning)

### MQTT Handler (`app/mqtt/handlers.py`)

```python
class MQTTCommandHandler:
    def __init__(self, mqtt_client, device_repository, lightning_client_factory):
        self.mqtt_client = mqtt_client
        self.device_repo = device_repository
        self.lightning_factory = lightning_client_factory

    async def handle_command(self, topic: str, payload: dict):
        # Extract device_id from topic
        device_id = self.extract_device_id(topic)

        # Get device from database
        device = await self.device_repo.get_device(device_id)
        if not device:
            return

        # Create Lightning client
        client = self.lightning_factory.create(device)

        # Route command
        command = payload.get("command")

        if command == "volume_up":
            await client.sendNavigationCommand("volume_up")
        elif command == "volume_down":
            await client.sendNavigationCommand("volume_down")
        elif command == "media_play_pause":
            await client.sendMediaCommand("play")
        elif command == "turn_on":
            await client.wakeDevice()
        # ... etc
```

### MQTT Discovery (`app/mqtt/discovery.py`)

```python
async def publish_discovery(device: Device):
    topic = f"homeassistant/media_player/{device.device_id}/config"
    payload = {
        "name": device.name,
        "unique_id": f"firetv_{device.device_id}",
        "state_topic": f"maestro_hub/firetv/{device.device_id}/state",
        "command_topic": f"maestro_hub/firetv/{device.device_id}/set",
        "availability_topic": f"maestro_hub/firetv/{device.device_id}/availability",
        # ... full config
    }

    await mqtt_client.publish(topic, json.dumps(payload), retain=True)
```

## C++ Implementation (Phase 5 - To Be Implemented)

### Component Structure

```
include/mqtt/
├── MQTTClient.h           # MQTT client wrapper (paho-mqtt-cpp)
├── DiscoveryPublisher.h   # Home Assistant MQTT Discovery
└── CommandHandler.h       # Route MQTT commands to Lightning client

src/mqtt/
├── MQTTClient.cpp
├── DiscoveryPublisher.cpp
└── CommandHandler.cpp
```

### Key Classes

**MQTTClient**:
```cpp
class MQTTClient {
public:
    void connect(const std::string& broker, int port,
                 const std::string& user, const std::string& pass);
    void subscribe(const std::string& topic, CommandCallback callback);
    void publish(const std::string& topic, const std::string& payload, bool retain);
    void publishState(const std::string& device_id, const DeviceState& state);
    void publishAvailability(const std::string& device_id, bool online);
};
```

**DiscoveryPublisher**:
```cpp
class DiscoveryPublisher {
public:
    void publishDeviceConfig(const Device& device);
    void removeDevice(const std::string& device_id);
private:
    Json::Value buildDiscoveryPayload(const Device& device);
};
```

**CommandHandler**:
```cpp
class CommandHandler {
public:
    void handleCommand(const std::string& device_id, const Json::Value& payload);
private:
    void handleMediaCommand(const std::string& device_id, const std::string& command);
    void handleVolumeCommand(const std::string& device_id, const std::string& command);
    void handleNavigationCommand(const std::string& device_id, const Json::Value& payload);
    void handleAppLaunch(const std::string& device_id, const std::string& source);
};
```

## MQTT Broker Configuration

**Current Setup**:
- **Broker**: Native EMQX at 192.168.2.15:1883
- **Username**: aamat
- **Password**: exploracion
- **QoS**: 1 (at least once delivery)
- **Retain**: Yes for discovery/availability, No for commands

**Connection String**:
```cpp
mqtt::connect_options connOpts;
connOpts.set_keep_alive_interval(20);
connOpts.set_clean_session(true);
connOpts.set_user_name("aamat");
connOpts.set_password("exploracion");
```

## Command Routing Matrix

| HA Command | MQTT Payload | Lightning Method | Lightning Action |
|------------|--------------|------------------|------------------|
| volume_up | `{"command": "volume_up"}` | `sendNavigationCommand("volume_up")` | Volume up |
| volume_down | `{"command": "volume_down"}` | `sendNavigationCommand("volume_down")` | Volume down |
| media_play | `{"command": "media_play"}` | `sendMediaCommand("play")` | Play/pause toggle |
| media_pause | `{"command": "media_pause"}` | `sendMediaCommand("pause")` | Pause |
| turn_on | `{"command": "turn_on"}` | `wakeDevice()` | Wake from standby |
| turn_off | `{"command": "turn_off"}` | `sendNavigationCommand("sleep")` | Enter standby |
| navigate up | `{"command": "navigate", "direction": "up"}` | `dpadUp()` | D-pad up |
| navigate down | `{"command": "navigate", "direction": "down"}` | `dpadDown()` | D-pad down |
| navigate left | `{"command": "navigate", "direction": "left"}` | `dpadLeft()` | D-pad left |
| navigate right | `{"command": "navigate", "direction": "right"}` | `dpadRight()` | D-pad right |
| select | `{"command": "navigate", "action": "select"}` | `select()` | OK/Enter |
| home | `{"command": "navigate", "action": "home"}` | `home()` | Home button |
| back | `{"command": "navigate", "action": "back"}` | `back()` | Back button |
| menu | `{"command": "navigate", "action": "menu"}` | `menu()` | Menu button |
| launch Netflix | `{"command": "select_source", "source": "Netflix"}` | `launchApp("com.netflix.ninja")` | Launch Netflix |
| launch Prime | `{"command": "select_source", "source": "Prime Video"}` | `launchApp("com.amazon.avod.thirdpartyclient")` | Launch Prime |

## State Management

### Device State Enum
```cpp
enum class DeviceState {
    UNKNOWN,
    OFF,
    STANDBY,
    IDLE,      // Awake but not playing
    PLAYING,
    PAUSED,
    BUFFERING
};
```

### State Updates
```cpp
void updateDeviceState(const std::string& device_id, DeviceState state) {
    Json::Value payload;

    switch (state) {
        case DeviceState::PLAYING:
            payload["state"] = "playing";
            break;
        case DeviceState::PAUSED:
            payload["state"] = "paused";
            break;
        case DeviceState::IDLE:
            payload["state"] = "idle";
            break;
        case DeviceState::STANDBY:
        case DeviceState::OFF:
            payload["state"] = "off";
            break;
        default:
            payload["state"] = "unknown";
    }

    std::string topic = "maestro_hub/firetv/" + device_id + "/state";
    mqtt_client.publish(topic, payload.toStyledString(), false);
}
```

## Integration Testing

### Test MQTT Commands from Command Line

**1. Subscribe to state updates**:
```bash
mosquitto_sub -h 192.168.2.15 -u aamat -P exploracion \
  -t "maestro_hub/firetv/livingroom_colada/state" -v
```

**2. Send volume up command**:
```bash
mosquitto_pub -h 192.168.2.15 -u aamat -P exploracion \
  -t "maestro_hub/firetv/livingroom_colada/set" \
  -m '{"command": "volume_up"}'
```

**3. Check Home Assistant**:
- Go to Developer Tools → MQTT
- Listen to: `maestro_hub/firetv/#`
- Publish to: `maestro_hub/firetv/livingroom_colada/set`

### Test Discovery

**1. Publish discovery message**:
```bash
mosquitto_pub -h 192.168.2.15 -u aamat -P exploracion \
  -t "homeassistant/media_player/livingroom_colada/config" \
  -m '{
    "name": "Living Room Fire TV",
    "unique_id": "firetv_livingroom_colada",
    "state_topic": "maestro_hub/firetv/livingroom_colada/state",
    "command_topic": "maestro_hub/firetv/livingroom_colada/set"
  }' \
  -r  # Retain flag
```

**2. Check Home Assistant**:
- Settings → Devices & Services → MQTT
- Should see "Living Room Fire TV" device

**3. Test control**:
- Click on device in HA
- Press volume up/down
- Check MQTT traffic in terminal

## Migration from Python to C++

### Current State (Python)
- ✅ MQTT Discovery working
- ✅ Command handling via MQTT
- ✅ State publishing
- ✅ Availability tracking
- ✅ App source mapping

### Phase 5 (C++ Implementation)
- [ ] Implement MQTTClient wrapper (paho-mqtt-cpp)
- [ ] Implement DiscoveryPublisher
- [ ] Implement CommandHandler
- [ ] State tracking and publishing
- [ ] Availability heartbeat (every 30s)
- [ ] App source → package mapping
- [ ] Error handling and retries

### Compatibility
Both implementations can run simultaneously:
- Python: Handles existing devices
- C++: Handles new devices with different device_id prefix
- Eventually migrate all devices to C++

## Troubleshooting MQTT

### Issue: Commands not reaching service

**Check 1: MQTT broker running**:
```bash
docker logs emqx | tail -20
# Or for native:
sudo systemctl status emqx
```

**Check 2: Topic subscription**:
```bash
# Subscribe to all topics
mosquitto_sub -h 192.168.2.15 -u aamat -P exploracion -t '#' -v
```

**Check 3: Service connected**:
```bash
# Check MQTT connections in EMQX dashboard
# http://192.168.2.15:18083
# Username: admin / Password: maestro_emqx_2026
```

### Issue: Device not appearing in HA

**Check 1: Discovery message published**:
```bash
mosquitto_sub -h 192.168.2.15 -u aamat -P exploracion \
  -t "homeassistant/media_player/+/config" -v
```

**Check 2: Reload MQTT integration**:
- Home Assistant → Settings → Devices & Services
- MQTT integration → ... → Reload

**Check 3: Check HA MQTT logs**:
- Home Assistant → Settings → System → Logs
- Filter: "mqtt"

## Dependencies

**C++ Libraries**:
- `paho-mqtt-cpp` - MQTT client (Eclipse Paho)
- `jsoncpp` - JSON parsing

**Install**:
```bash
sudo apt-get install -y libpaho-mqtt-dev libpaho-mqttpp-dev
```

**CMakeLists.txt**:
```cmake
find_package(PahoMqttCpp REQUIRED)
target_link_libraries(hms_firetv PahoMqttCpp::paho-mqttpp3)
```

## Summary

**Current Architecture**:
- Home Assistant → MQTT (commands) → Python colada-lightning → Lightning Protocol → Fire TV
- Python colada-lightning → MQTT (state/availability) → Home Assistant

**Future Architecture (Phase 5)**:
- Home Assistant → MQTT (commands) → C++ HMS FireTV → Lightning Protocol → Fire TV
- C++ HMS FireTV → MQTT (state/availability) → Home Assistant

**Key Point**: Home Assistant does NOT make direct HTTP API calls to the service. All communication is via MQTT for decoupling and reliability.

---

**Next Phase**: Phase 5 - MQTT Integration (after Phase 4 REST API)
