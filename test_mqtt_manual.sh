#!/bin/bash

# HMS FireTV MQTT Manual Test Script

BROKER="192.168.2.15"
USER="aamat"
PASS="exploracion"
DEVICE_ID="livingroom_colada"

echo "============================================================================"
echo "HMS FireTV - MQTT Manual Test Script"
echo "============================================================================"
echo ""
echo "This script provides test commands for MQTT integration"
echo ""
echo "Prerequisites:"
echo "  1. HMS FireTV service must be running: ./build/hms_firetv"
echo "  2. EMQX broker must be running at $BROKER:1883"
echo "  3. Device '$DEVICE_ID' must exist in database"
echo ""
echo "============================================================================"
echo ""

# Function to publish command
publish_command() {
    local topic=$1
    local payload=$2
    echo "📤 Publishing to: $topic"
    echo "   Payload: $payload"
    mosquitto_pub -h "$BROKER" -u "$USER" -P "$PASS" -t "$topic" -m "$payload"
    echo "   ✅ Published"
    echo ""
}

# Check if service argument provided
if [ "$1" == "monitor" ]; then
    echo "🔍 Monitoring all MQTT traffic..."
    echo "   Press Ctrl+C to stop"
    echo ""
    mosquitto_sub -h "$BROKER" -u "$USER" -P "$PASS" -t "maestro_hub/firetv/#" -v
    exit 0
fi

if [ "$1" == "volume_up" ]; then
    publish_command "maestro_hub/firetv/$DEVICE_ID/set" '{"command": "volume_up"}'
    exit 0
fi

if [ "$1" == "volume_down" ]; then
    publish_command "maestro_hub/firetv/$DEVICE_ID/set" '{"command": "volume_down"}'
    exit 0
fi

if [ "$1" == "home" ]; then
    publish_command "maestro_hub/firetv/$DEVICE_ID/set" '{"command": "navigate", "action": "home"}'
    exit 0
fi

if [ "$1" == "play" ]; then
    publish_command "maestro_hub/firetv/$DEVICE_ID/set" '{"command": "media_play_pause"}'
    exit 0
fi

if [ "$1" == "netflix" ]; then
    publish_command "maestro_hub/firetv/$DEVICE_ID/set" '{"command": "launch_app", "package": "com.netflix.ninja"}'
    exit 0
fi

# Show usage
echo "Usage: $0 [command]"
echo ""
echo "Available commands:"
echo "  monitor       - Monitor all MQTT traffic"
echo "  volume_up     - Increase volume"
echo "  volume_down   - Decrease volume"
echo "  home          - Go to home screen"
echo "  play          - Play/pause media"
echo "  netflix       - Launch Netflix"
echo ""
echo "Examples:"
echo "  # Monitor MQTT traffic in one terminal:"
echo "  $0 monitor"
echo ""
echo "  # Test volume control in another terminal:"
echo "  $0 volume_up"
echo "  $0 volume_down"
echo ""
echo "Manual test commands:"
echo ""
echo "# Volume up:"
echo "mosquitto_pub -h $BROKER -u $USER -P $PASS \\"
echo "  -t 'maestro_hub/firetv/$DEVICE_ID/set' \\"
echo "  -m '{\"command\": \"volume_up\"}'"
echo ""
echo "# Volume down:"
echo "mosquitto_pub -h $BROKER -u $USER -P $PASS \\"
echo "  -t 'maestro_hub/firetv/$DEVICE_ID/set' \\"
echo "  -m '{\"command\": \"volume_down\"}'"
echo ""
echo "# Home button:"
echo "mosquitto_pub -h $BROKER -u $USER -P $PASS \\"
echo "  -t 'maestro_hub/firetv/$DEVICE_ID/set' \\"
echo "  -m '{\"command\": \"navigate\", \"action\": \"home\"}'"
echo ""
echo "# Play/Pause:"
echo "mosquitto_pub -h $BROKER -u $USER -P $PASS \\"
echo "  -t 'maestro_hub/firetv/$DEVICE_ID/set' \\"
echo "  -m '{\"command\": \"media_play_pause\"}'"
echo ""
echo "# Launch Netflix:"
echo "mosquitto_pub -h $BROKER -u $USER -P $PASS \\"
echo "  -t 'maestro_hub/firetv/$DEVICE_ID/set' \\"
echo "  -m '{\"command\": \"launch_app\", \"package\": \"com.netflix.ninja\"}'"
echo ""
