# MQTT Extensibility - Multi-Service Integration

## The Power of MQTT Decoupling

MQTT acts as a **message bus** that decouples services. Any application can:
- **Subscribe** to state changes → React to Fire TV events
- **Publish** commands → Control Fire TV devices
- **Intercept** messages → Add middleware logic

## Use Cases

### 1. N8N Workflow Automation

**Scenario**: Turn off Fire TV when leaving home

```javascript
// N8N workflow listening to location changes
{
  trigger: "geofence_exit",
  action: "mqtt_publish",
  topic: "maestro_hub/firetv/livingroom_colada/set",
  payload: {
    "command": "turn_off"
  }
}
```

### 2. Voice Assistant Integration

**Scenario**: "Alexa, play Netflix on living room TV"

```
Alexa/Google Home
    ↓
Custom Skill/Action
    ↓
MQTT Publish: maestro_hub/firetv/livingroom_colada/set
    ↓
HMS FireTV receives command
    ↓
Launches Netflix
```

### 3. Automation Based on Other Devices

**Scenario**: Dim lights when Fire TV starts playing

```python
# Python script monitoring Fire TV state
import paho.mqtt.client as mqtt

def on_message(client, userdata, msg):
    payload = json.loads(msg.payload)

    if payload.get("state") == "playing":
        # Publish to lights
        client.publish(
            "zigbee2mqtt/living_room_lights/set",
            json.dumps({"brightness": 50})
        )

client = mqtt.Client()
client.on_message = on_message
client.connect("192.168.2.15", 1883)
client.subscribe("maestro_hub/firetv/livingroom_colada/state")
client.loop_forever()
```

### 4. Analytics and Logging

**Scenario**: Track Fire TV usage patterns

```python
# Service logging all Fire TV commands
def on_message(client, userdata, msg):
    if "/set" in msg.topic:
        device_id = extract_device_id(msg.topic)
        command = json.loads(msg.payload)

        # Log to InfluxDB/Grafana
        log_command(device_id, command, timestamp=now())
```

### 5. Command Interception/Middleware

**Scenario**: Block certain apps during work hours

```python
# Middleware service
def on_message(client, userdata, msg):
    if "/set" in msg.topic:
        command = json.loads(msg.payload)

        # Intercept app launch
        if command.get("command") == "launch_app":
            if is_work_hours() and is_entertainment_app(command["package"]):
                print("Blocked during work hours")
                return  # Don't forward command

        # Forward to HMS FireTV (different topic)
        client.publish(msg.topic + "_forwarded", msg.payload)
```

### 6. Multi-Device Coordination

**Scenario**: Pause all Fire TVs when doorbell rings

```python
def on_doorbell_ring():
    devices = ["livingroom_colada", "bedroom_colada", "albin_colada"]

    for device in devices:
        mqtt_client.publish(
            f"maestro_hub/firetv/{device}/set",
            json.dumps({"command": "media_pause"})
        )
```

### 7. Testing and Debugging

**Scenario**: Monitor all Fire TV activity

```bash
# Subscribe to all topics
mosquitto_sub -h 192.168.2.15 -u aamat -P exploracion \
  -t "maestro_hub/firetv/#" -v

# Output:
maestro_hub/firetv/livingroom_colada/set {"command": "volume_up"}
maestro_hub/firetv/livingroom_colada/state {"state": "playing"}
maestro_hub/firetv/livingroom_colada/availability online
```

### 8. Conditional Commands

**Scenario**: Only turn on Fire TV if someone is home

```javascript
// N8N workflow
if (home_assistant.person.state === "home") {
    mqtt.publish(
        "maestro_hub/firetv/livingroom_colada/set",
        JSON.stringify({"command": "turn_on"})
    );
}
```

### 9. Command Queuing

**Scenario**: Queue multiple commands to execute sequentially

```python
# Command queue service
commands = [
    {"command": "turn_on"},
    {"command": "launch_app", "package": "com.netflix.ninja"},
    {"command": "navigate", "action": "select"}
]

for cmd in commands:
    mqtt_client.publish("maestro_hub/firetv/livingroom_colada/set", json.dumps(cmd))
    time.sleep(1)  # Wait between commands
```

### 10. Cross-Platform Integration

**Scenario**: iOS Shortcut to control Fire TV

```
iOS Shortcut
    ↓ HTTP Request to MQTT bridge
MQTT Bridge (Node-RED/n8n)
    ↓ MQTT Publish
HMS FireTV
    ↓
Fire TV Device
```

## Architecture Benefits

### Decoupling
- Services don't need to know about each other
- HMS FireTV doesn't care who sends commands
- Home Assistant doesn't care about HMS FireTV implementation

### Scalability
- Add new services without modifying existing ones
- Multiple consumers can subscribe to same topics
- Easy to add caching, logging, analytics layers

### Reliability
- MQTT broker handles message persistence (QoS 1)
- Services can restart without losing messages
- Retained messages ensure last state is always available

### Flexibility
- Change HMS FireTV implementation (Python → C++) without affecting HA
- Add middleware services transparently
- A/B test new logic by subscribing to same topics

## Topic Patterns

### Command Topics (Publish to control)
```
maestro_hub/firetv/{device_id}/set
maestro_hub/firetv/{device_id}/launch_app
maestro_hub/firetv/{device_id}/navigation
```

### State Topics (Subscribe to monitor)
```
maestro_hub/firetv/{device_id}/state
maestro_hub/firetv/{device_id}/availability
```

### Discovery Topics (Subscribe for device detection)
```
homeassistant/media_player/{device_id}/config
```

## Security Considerations

### Current Setup
- MQTT broker requires authentication (aamat/exploracion)
- No encryption (local network only)
- No ACLs (all users can publish/subscribe to all topics)

### Production Recommendations
1. **Enable TLS**: Encrypt MQTT traffic
2. **Per-service credentials**: Different username/password per service
3. **ACLs**: Restrict topics per user
   ```
   user hms_firetv can publish maestro_hub/firetv/#
   user hms_firetv can subscribe homeassistant/#
   user home_assistant can publish maestro_hub/firetv/+/set
   user home_assistant can subscribe maestro_hub/firetv/#
   ```
4. **Command validation**: HMS FireTV validates all incoming commands
5. **Rate limiting**: Prevent command flooding

## Example: Multi-Service Setup

```
┌─────────────────┐
│ Home Assistant  │─┐
└─────────────────┘ │
                    │
┌─────────────────┐ │
│   N8N Workflow  │─┤
└─────────────────┘ │
                    │         ┌──────────────┐
┌─────────────────┐ ├────────▶│  EMQX Broker │
│ Python Monitor  │─┤         │  (MQTT Bus)  │
└─────────────────┘ │         └──────┬───────┘
                    │                │
┌─────────────────┐ │                │
│ iOS Shortcut    │─┤                │
└─────────────────┘ │                │
                    │         ┌──────▼───────┐
┌─────────────────┐ │         │ HMS FireTV   │
│ Voice Assistant │─┘         │ (C++ Service)│
└─────────────────┘           └──────┬───────┘
                                     │
                              ┌──────▼──────┐
                              │  Fire TV    │
                              │  Devices    │
                              └─────────────┘
```

All services communicate via MQTT - no direct coupling!

## Testing Multi-Service Integration

### Setup
```bash
# Terminal 1: Monitor all traffic
mosquitto_sub -h 192.168.2.15 -u aamat -P exploracion \
  -t "maestro_hub/firetv/#" -v

# Terminal 2: HMS FireTV service
cd /home/aamat/maestro_hub/hms-firetv/build
./hms_firetv

# Terminal 3: Simulate Home Assistant
mosquitto_pub -h 192.168.2.15 -u aamat -P exploracion \
  -t "maestro_hub/firetv/livingroom_colada/set" \
  -m '{"command": "volume_up"}'

# Terminal 4: Simulate N8N workflow
mosquitto_pub -h 192.168.2.15 -u aamat -P exploracion \
  -t "maestro_hub/firetv/livingroom_colada/set" \
  -m '{"command": "turn_on"}'
```

## Future Possibilities

### 1. Command Macros
Create complex sequences:
```json
{
  "macro": "movie_night",
  "commands": [
    {"device": "livingroom_colada", "command": "turn_on"},
    {"device": "lights", "command": "dim", "brightness": 30},
    {"device": "speakers", "command": "volume_set", "level": 0.7},
    {"device": "livingroom_colada", "command": "launch_app", "package": "com.netflix.ninja"}
  ]
}
```

### 2. AI/ML Integration
Train models on usage patterns:
```python
# Predict what user wants to watch
if time_of_day() == "evening" and day_of_week() == "friday":
    predicted_app = ml_model.predict(user_history)
    mqtt_publish("launch_app", predicted_app)
```

### 3. Multi-Room Audio
Sync Fire TVs for audio:
```python
# Pause all devices, play one
for device in all_devices:
    mqtt_publish(f"{device}/set", {"command": "pause"})

mqtt_publish("livingroom_colada/set", {"command": "play"})
```

## Summary

**Key Insight**: MQTT transforms HMS FireTV from a **standalone service** into a **platform component** that can be integrated with any other service in the smart home ecosystem.

**Benefits**:
- ✅ Any app can control Fire TVs
- ✅ Any app can react to Fire TV state
- ✅ Middleware can intercept/modify commands
- ✅ Easy testing and debugging
- ✅ No code changes to add integrations

**This is why MQTT was chosen over direct HTTP API calls!**

---

**Next**: Implement MQTT client in Phase 5 to unlock these capabilities
