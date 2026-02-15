#!/bin/bash

# HMS FireTV Service Uninstallation Script

set -e

SERVICE_NAME="hms-firetv"

echo "============================================================================"
echo "HMS FireTV - Service Uninstallation"
echo "============================================================================"
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo "❌ Do not run this script as root!"
    echo "   Run without sudo, it will prompt for password when needed"
    exit 1
fi

echo "Step 1: Stop service..."
sudo systemctl stop $SERVICE_NAME || echo "Service not running"
echo "✅ Stopped"
echo ""

echo "Step 2: Disable service..."
sudo systemctl disable $SERVICE_NAME || echo "Service not enabled"
echo "✅ Disabled"
echo ""

echo "Step 3: Remove service file..."
sudo rm -f /etc/systemd/system/$SERVICE_NAME.service
echo "✅ Removed"
echo ""

echo "Step 4: Reload systemd daemon..."
sudo systemctl daemon-reload
echo "✅ Reloaded"
echo ""

echo "============================================================================"
echo "Uninstallation Complete!"
echo "============================================================================"
echo ""
echo "The hms-firetv service has been removed."
echo "The binary and source code remain in: /home/aamat/maestro_hub/hms-firetv"
echo ""
