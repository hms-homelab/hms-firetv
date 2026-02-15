#!/bin/bash

# HMS FireTV Service Startup Script

export DB_HOST=192.168.2.15
export DB_PORT=5432
export DB_NAME=firetv
export DB_USER=firetv_user
export DB_PASSWORD=firetv_postgres_2026_secure

export MQTT_BROKER_HOST=192.168.2.15
export MQTT_BROKER_PORT=1883
export MQTT_USER=aamat
export MQTT_PASS=exploracion

export API_HOST=0.0.0.0
export API_PORT=8888
export THREAD_NUM=4
export LOG_LEVEL=info

echo "Starting HMS FireTV service..."
echo "Database: $DB_HOST:$DB_PORT/$DB_NAME"
echo "MQTT: tcp://$MQTT_BROKER_HOST:$MQTT_BROKER_PORT"
echo "API: $API_HOST:$API_PORT"
echo ""

./build/hms_firetv
