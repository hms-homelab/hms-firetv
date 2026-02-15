#!/bin/bash

# HMS FireTV Service Installation Script
# Installs the systemd service and enables it to start on boot

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICE_FILE="hms-firetv.service"
SERVICE_NAME="hms-firetv"

echo "============================================================================"
echo "HMS FireTV - Service Installation"
echo "============================================================================"
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo "❌ Do not run this script as root!"
    echo "   Run without sudo, it will prompt for password when needed"
    exit 1
fi

# Check if binary exists
if [ ! -f "$SCRIPT_DIR/build/hms_firetv" ]; then
    echo "❌ Binary not found: $SCRIPT_DIR/build/hms_firetv"
    echo "   Please build the project first:"
    echo "   cd build && cmake .. && make -j\$(nproc)"
    exit 1
fi

# Check if service file exists
if [ ! -f "$SCRIPT_DIR/$SERVICE_FILE" ]; then
    echo "❌ Service file not found: $SCRIPT_DIR/$SERVICE_FILE"
    exit 1
fi

echo "Step 1: Stop any running instance..."
pkill -f hms_firetv || true
sleep 2
echo "✅ Stopped"
echo ""

echo "Step 2: Copy service file to systemd..."
sudo cp "$SCRIPT_DIR/$SERVICE_FILE" /etc/systemd/system/
sudo chown root:root /etc/systemd/system/$SERVICE_FILE
sudo chmod 644 /etc/systemd/system/$SERVICE_FILE
echo "✅ Service file installed"
echo ""

echo "Step 3: Reload systemd daemon..."
sudo systemctl daemon-reload
echo "✅ Reloaded"
echo ""

echo "Step 4: Enable service (start on boot)..."
sudo systemctl enable $SERVICE_NAME
echo "✅ Enabled"
echo ""

echo "Step 5: Start service..."
sudo systemctl start $SERVICE_NAME
echo "✅ Started"
echo ""

# Wait a moment for service to start
sleep 2

echo "============================================================================"
echo "Service Status:"
echo "============================================================================"
sudo systemctl status $SERVICE_NAME --no-pager || true
echo ""

echo "============================================================================"
echo "Installation Complete!"
echo "============================================================================"
echo ""
echo "Service management commands:"
echo "  sudo systemctl status hms-firetv     - Check status"
echo "  sudo systemctl start hms-firetv      - Start service"
echo "  sudo systemctl stop hms-firetv       - Stop service"
echo "  sudo systemctl restart hms-firetv    - Restart service"
echo "  sudo systemctl enable hms-firetv     - Enable on boot"
echo "  sudo systemctl disable hms-firetv    - Disable on boot"
echo ""
echo "Logs:"
echo "  sudo journalctl -u hms-firetv -f     - Follow logs"
echo "  sudo journalctl -u hms-firetv -n 50  - Last 50 lines"
echo "  sudo journalctl -u hms-firetv --since today - Today's logs"
echo ""
echo "Health check:"
echo "  curl http://localhost:8888/health"
echo ""
