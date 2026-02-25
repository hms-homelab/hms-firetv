# Device Detection Strategy

**Fire TV Device State Detection**

## Device States

Fire TV devices can be in multiple states:
1. **Powered Off** - Completely off, not reachable
2. **Standby** - Low power mode, wake endpoint responds, Lightning API offline
3. **Active** - Fully awake, both endpoints respond, ready for commands

## Detection Methods

### Method 1: Wake Endpoint (Port 8009)
```
http://{ip}:8009/apps/FireTVRemote
```
- **Always responds** when device is powered on (even in standby)
- Uses HTTP (no SSL)
- Fast response (~50-200ms)
- **Use case**: Basic reachability check

### Method 2: Lightning API (Port 8080)
```
https://{ip}:8080/v1/FireTV
```
- **Stops responding** when device enters standby mode
- Uses HTTPS with self-signed certificate
- Requires authentication headers
- **Use case**: Check if device is ready for commands

## Recommended Detection Flow

### Simple Reachability Check
```cpp
bool is_reachable = client.healthCheck();
// Returns: true if powered on (standby or active)
//          false if powered off or network unreachable
```

### Full State Detection
```cpp
bool wake_responds = client.healthCheck();
bool api_responds = client.isLightningApiAvailable();

if (!wake_responds) {
    // Device is OFF or network unreachable
    status = "offline";
} else if (!api_responds) {
    // Device is in STANDBY mode
    status = "standby";
    // Optional: Wake it up
    client.wakeDevice();
} else {
    // Device is ACTIVE and ready
    status = "online";
}
```

## Implementation in C++

### LightningClient Methods

**healthCheck()** - Wake endpoint check
```cpp
bool LightningClient::healthCheck() {
    auto result = executeGet(wake_url_, HEALTH_TIMEOUT);

    // 200, 204, 404 are all good (device is reachable)
    return result.status_code == 200 ||
           result.status_code == 204 ||
           result.status_code == 404;
}
```

**isLightningApiAvailable()** - Lightning API check
```cpp
bool LightningClient::isLightningApiAvailable() {
    std::string url = base_url_ + "/v1/FireTV";
    auto result = executeGet(url, HEALTH_TIMEOUT);

    // Any response (even errors) means API is up
    return result.status_code > 0;
}
```

## Command Execution Strategy

### Before Sending Commands

**Option 1: Optimistic (Fast)**
```cpp
// Just send command, handle errors
auto result = client.home();
if (!result.success) {
    if (result.error && result.error.find("timeout") != std::string::npos) {
        // Device might be in standby, try waking
        client.wakeDevice();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        result = client.home();  // Retry
    }
}
```

**Option 2: Safe (Slower but Robust)**
```cpp
// Check device state first
if (!client.isLightningApiAvailable()) {
    // Device in standby or off
    if (!client.healthCheck()) {
        return {false, "Device offline"};
    }

    // Device in standby, wake it up
    client.wakeDevice();
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Verify it woke up
    if (!client.isLightningApiAvailable()) {
        return {false, "Failed to wake device"};
    }
}

// Now safe to send command
auto result = client.home();
```

## Timeouts

### Recommended Timeout Values
- **Wake Endpoint**: 3-5 seconds (usually responds in <200ms)
- **Lightning API**: 2-3 seconds (can be slower on first request after wake)
- **Commands**: 5-10 seconds (some commands take time to execute)

### Current Implementation
```cpp
class LightningClient {
    static constexpr long WAKE_TIMEOUT = 5L;      // 5 seconds
    static constexpr long HEALTH_TIMEOUT = 2L;    // 2 seconds
    static constexpr long COMMAND_TIMEOUT = 10L;  // 10 seconds
};
```

## Automatic Device Monitoring

### Polling Strategy (for DeviceRepository updates)

```cpp
void monitorDeviceStatus() {
    auto devices = DeviceRepository::getInstance().getAllDevices();

    for (const auto& device : devices) {
        LightningClient client(device.ip_address, device.api_key,
                                device.client_token.value_or(""));

        // Quick check
        bool wake_responds = client.healthCheck();
        bool api_responds = wake_responds ? client.isLightningApiAvailable() : false;

        // Determine status
        std::string new_status;
        if (!wake_responds) {
            new_status = "offline";
        } else if (!api_responds) {
            new_status = "standby";
        } else {
            new_status = "online";
        }

        // Update database if status changed
        if (device.status != new_status) {
            DeviceRepository::getInstance().updateLastSeen(device.device_id, new_status);
        }
    }
}
```

### Polling Interval
- **Recommended**: 30-60 seconds
- **Fast**: 10-15 seconds (more network traffic)
- **Slow**: 2-5 minutes (delayed status updates)

## Error Scenarios

### Network Unreachable
```
HTTP request timeout
Status code: 0
Error: "Timeout was reached"
```
**Action**: Mark device as offline

### Device in Standby
```
Wake endpoint: 200 OK
Lightning API: Timeout/Connection refused
```
**Action**: Mark device as standby, optionally wake

### Authentication Failure
```
Status code: 401 Unauthorized
```
**Action**: Pairing required, clear client_token

### Device Rebooting
```
Wake endpoint: Intermittent timeouts
Lightning API: Connection refused
```
**Action**: Wait 30-60 seconds for device to fully boot

## Best Practices

1. **Don't poll too frequently** - Fire TV devices don't like aggressive polling
2. **Cache status** - Only check when needed, not before every command
3. **Handle wake-up time** - After calling `wakeDevice()`, wait 2-3 seconds
4. **Retry once** - If command fails, try waking and retrying once
5. **Update last_seen** - Always update database after successful command
6. **Exponential backoff** - If device consistently unreachable, reduce check frequency

## Example: Complete Command Execution

```cpp
CommandResult sendCommandWithRetry(LightningClient& client,
                                     const std::string& device_id,
                                     std::function<CommandResult()> command) {
    // Attempt 1: Optimistic send
    auto result = command();

    if (result.success) {
        // Update last seen
        DeviceRepository::getInstance().updateLastSeen(device_id, "online");
        return result;
    }

    // Command failed, check if device is reachable
    if (!client.healthCheck()) {
        // Device completely offline
        DeviceRepository::getInstance().updateLastSeen(device_id, "offline");
        return result;  // Return original error
    }

    // Device reachable but maybe in standby
    if (!client.isLightningApiAvailable()) {
        std::cout << "[Command] Device in standby, waking..." << std::endl;
        client.wakeDevice();
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    // Attempt 2: Retry after wake
    result = command();

    if (result.success) {
        DeviceRepository::getInstance().updateLastSeen(device_id, "online");
    } else {
        DeviceRepository::getInstance().updateLastSeen(device_id, "error");
    }

    return result;
}

// Usage:
LightningClient client("192.168.2.50", "0987654321", token);
auto result = sendCommandWithRetry(client, "living_room", [&]() {
    return client.home();
});
```

## Testing Device Detection

### Manual Test Script
```bash
# Test wake endpoint
curl -X POST http://192.168.2.50:8009/apps/FireTVRemote -v

# Test Lightning API
curl -k -X GET https://192.168.2.50:8080/v1/FireTV \
  -H "X-Api-Key: 0987654321" -v
```

### Expected Responses

**Device Active:**
```
Wake: HTTP 200 OK
Lightning: HTTP 200 OK (or 401 if not paired)
```

**Device Standby:**
```
Wake: HTTP 200 OK
Lightning: Timeout / Connection refused
```

**Device Offline:**
```
Wake: Timeout / No route to host
Lightning: Timeout / No route to host
```

---

**Summary**: Use `healthCheck()` for basic reachability, use `isLightningApiAvailable()` to check if ready for commands. Always handle wake-up scenarios gracefully.
