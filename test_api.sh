#!/bin/bash
# ==============================================================================
# HMS FireTV API Test Script
# ==============================================================================
#
# Tests all REST API endpoints to verify functionality
#

set -e

BASE_URL="http://localhost:8888"
DEVICE_ID="test_device_$(date +%s)"

echo "================================================================================================="
echo "HMS FireTV API Test Suite"
echo "================================================================================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
}

fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    exit 1
}

info() {
    echo -e "${YELLOW}➜${NC} $1"
}

# ==============================================================================
# Test 1: Health Check
# ==============================================================================
info "Test 1: Health check endpoint"
RESPONSE=$(curl -s $BASE_URL/health)
if echo "$RESPONSE" | grep -q '"service":"HMS FireTV"'; then
    pass "Health check returned valid response"
else
    fail "Health check failed: $RESPONSE"
fi

# ==============================================================================
# Test 2: Status Check
# ==============================================================================
info "Test 2: Status endpoint"
RESPONSE=$(curl -s $BASE_URL/status)
if echo "$RESPONSE" | grep -q '"status":"running"'; then
    pass "Status check returned valid response"
else
    fail "Status check failed: $RESPONSE"
fi

# ==============================================================================
# Test 3: List Devices (empty)
# ==============================================================================
info "Test 3: List devices (should be empty initially)"
RESPONSE=$(curl -s $BASE_URL/api/devices)
if echo "$RESPONSE" | grep -q '"success":true'; then
    pass "List devices endpoint works"
else
    fail "List devices failed: $RESPONSE"
fi

# ==============================================================================
# Test 4: Create Device
# ==============================================================================
info "Test 4: Create new device"
RESPONSE=$(curl -s -X POST $BASE_URL/api/devices \
    -H "Content-Type: application/json" \
    -d "{
        \"device_id\": \"$DEVICE_ID\",
        \"name\": \"Test Device\",
        \"ip_address\": \"192.168.1.100\",
        \"api_key\": \"0987654321\"
    }")

if echo "$RESPONSE" | grep -q '"success":true'; then
    pass "Device created successfully"
else
    fail "Device creation failed: $RESPONSE"
fi

# ==============================================================================
# Test 5: Get Device by ID
# ==============================================================================
info "Test 5: Get device by ID"
RESPONSE=$(curl -s $BASE_URL/api/devices/$DEVICE_ID)
if echo "$RESPONSE" | grep -q '"device_id":"'$DEVICE_ID'"'; then
    pass "Get device by ID works"
else
    fail "Get device failed: $RESPONSE"
fi

# ==============================================================================
# Test 6: Update Device
# ==============================================================================
info "Test 6: Update device"
RESPONSE=$(curl -s -X PUT $BASE_URL/api/devices/$DEVICE_ID \
    -H "Content-Type: application/json" \
    -d "{
        \"name\": \"Updated Test Device\",
        \"status\": \"online\"
    }")

if echo "$RESPONSE" | grep -q '"success":true'; then
    pass "Device updated successfully"
else
    fail "Device update failed: $RESPONSE"
fi

# ==============================================================================
# Test 7: Get Device Status
# ==============================================================================
info "Test 7: Get device status"
RESPONSE=$(curl -s $BASE_URL/api/devices/$DEVICE_ID/status)
if echo "$RESPONSE" | grep -q '"status":"online"'; then
    pass "Get device status works"
else
    fail "Get device status failed: $RESPONSE"
fi

# ==============================================================================
# Test 8: Get Pairing Status
# ==============================================================================
info "Test 8: Get pairing status (should be unpaired)"
RESPONSE=$(curl -s $BASE_URL/api/devices/$DEVICE_ID/pair/status)
if echo "$RESPONSE" | grep -q '"is_paired":false'; then
    pass "Get pairing status works"
else
    fail "Get pairing status failed: $RESPONSE"
fi

# ==============================================================================
# Test 9: List Apps (empty)
# ==============================================================================
info "Test 9: List apps for device (should be empty)"
RESPONSE=$(curl -s $BASE_URL/api/devices/$DEVICE_ID/apps)
if echo "$RESPONSE" | grep -q '"success":true'; then
    pass "List apps endpoint works"
else
    fail "List apps failed: $RESPONSE"
fi

# ==============================================================================
# Test 10: Add App to Device
# ==============================================================================
info "Test 10: Add app to device"
RESPONSE=$(curl -s -X POST $BASE_URL/api/devices/$DEVICE_ID/apps \
    -H "Content-Type: application/json" \
    -d "{
        \"package\": \"com.netflix.ninja\",
        \"name\": \"Netflix\"
    }")

if echo "$RESPONSE" | grep -q '"success":true'; then
    pass "App added successfully"
else
    fail "App addition failed: $RESPONSE"
fi

# ==============================================================================
# Test 11: Get Popular Apps
# ==============================================================================
info "Test 11: Get popular apps catalog"
RESPONSE=$(curl -s "$BASE_URL/api/apps/popular?category=streaming")
if echo "$RESPONSE" | grep -q '"success":true'; then
    pass "Get popular apps works"
else
    fail "Get popular apps failed: $RESPONSE"
fi

# ==============================================================================
# Test 12: Bulk Add Popular Apps
# ==============================================================================
info "Test 12: Bulk add popular apps to device"
RESPONSE=$(curl -s -X POST $BASE_URL/api/devices/$DEVICE_ID/apps/bulk \
    -H "Content-Type: application/json" \
    -d "{
        \"category\": \"streaming\"
    }")

if echo "$RESPONSE" | grep -q '"success":true'; then
    pass "Bulk add apps successful"
else
    fail "Bulk add apps failed: $RESPONSE"
fi

# ==============================================================================
# Test 13: List Apps Again (should have apps now)
# ==============================================================================
info "Test 13: List apps again (should have multiple apps)"
RESPONSE=$(curl -s $BASE_URL/api/devices/$DEVICE_ID/apps)
if echo "$RESPONSE" | grep -q '"count":[1-9]'; then
    pass "Apps were added successfully"
else
    fail "No apps found after bulk add: $RESPONSE"
fi

# ==============================================================================
# Test 14: Delete App
# ==============================================================================
info "Test 14: Delete an app"
RESPONSE=$(curl -s -X DELETE "$BASE_URL/api/devices/$DEVICE_ID/apps/com.netflix.ninja")
if echo "$RESPONSE" | grep -q '"success":true'; then
    pass "App deleted successfully"
else
    fail "App deletion failed: $RESPONSE"
fi

# ==============================================================================
# Test 15: Delete Device (Cleanup)
# ==============================================================================
info "Test 15: Delete test device (cleanup)"
RESPONSE=$(curl -s -X DELETE $BASE_URL/api/devices/$DEVICE_ID)
if echo "$RESPONSE" | grep -q '"success":true'; then
    pass "Device deleted successfully"
else
    fail "Device deletion failed: $RESPONSE"
fi

# ==============================================================================
# Summary
# ==============================================================================
echo ""
echo "================================================================================================="
echo -e "${GREEN}ALL TESTS PASSED!${NC}"
echo "================================================================================================="
echo ""
echo "API Endpoints Tested:"
echo "  ✓ Health check"
echo "  ✓ Status check"
echo "  ✓ Device CRUD (Create, Read, Update, Delete)"
echo "  ✓ Device status"
echo "  ✓ Pairing status check"
echo "  ✓ Apps CRUD"
echo "  ✓ Popular apps catalog"
echo "  ✓ Bulk add apps"
echo ""
echo "All REST API endpoints are working correctly!"
echo ""
