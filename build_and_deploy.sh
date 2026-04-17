#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
FRONTEND_DIR="$SCRIPT_DIR/frontend"
STATIC_DIR="$SCRIPT_DIR/static"
BINARY="hms_firetv"
INSTALL_PATH="${HMS_FIRETV_INSTALL_PATH:-/usr/local/bin/$BINARY}"
SERVICE_NAME="${HMS_FIRETV_SERVICE:-hms-firetv}"

echo "=== Building Angular frontend ==="
cd "$FRONTEND_DIR"
npm ci --no-audit --no-fund 2>/dev/null || true
npx ng build --configuration=production

echo "=== Copying frontend to static/ ==="
rm -rf "$STATIC_DIR"/*
cp -r "$FRONTEND_DIR/dist/frontend/browser/"* "$STATIC_DIR/"

echo "=== Building C++ ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF ..
make -j"$(nproc)"

echo "=== Deploying ==="
sudo systemctl stop "$SERVICE_NAME"
sudo cp "$BUILD_DIR/$BINARY" "$INSTALL_PATH"
sudo systemctl start "$SERVICE_NAME"

echo "=== Done ==="
sleep 2
systemctl is-active --quiet "$SERVICE_NAME" && echo "$SERVICE_NAME is running" || echo "WARNING: $SERVICE_NAME failed to start"
