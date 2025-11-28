#!/bin/bash
# Test Phase 2D: ATM Login Implementation

echo "=== Phase 2D Test: ATM-Bank Login ==="
echo

# Setup
TEST_DIR="/tmp/phase2d_test"
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"

cd "$(dirname "$0")"

# Kill any existing processes
pkill -9 router 2>/dev/null || true
pkill -9 bank 2>/dev/null || true
pkill -9 atm 2>/dev/null || true
sleep 0.5

# Initialize
echo "1. Initializing bank state..."
./bin/init "$TEST_DIR/test"

# Start router
echo "2. Starting router..."
./bin/router >/dev/null 2>&1 &
ROUTER_PID=$!
sleep 0.5

# Start bank in daemon mode with stdin from named pipe so we can send commands to it
echo "3. Starting bank in daemon mode..."
FIFO="/tmp/bank_input_$$"
mkfifo "$FIFO"
./bin/bank "$TEST_DIR/test.bank" <"$FIFO" >/dev/null 2>&1 &
BANK_PID=$!
exec 3>"$FIFO"  # Keep pipe open
sleep 0.5

# Create user in the running bank
echo "4. Creating user Alice with PIN 1234..."
echo "create-user Alice 1234 100" >&3
sleep 0.5

# Verify card was created
if [ ! -f Alice.card ]; then
    echo "   ✗ FAIL: Alice.card not created"
    kill $BANK_PID $ROUTER_PID 2>/dev/null
    rm -f "$FIFO"
    exit 1
fi
echo "   ✓ Alice.card created"

# Test 1: Valid login
echo "5. Test 1: Valid login with correct PIN..."
OUTPUT=$(echo -e "begin-session Alice\n1234\nend-session" | ./bin/atm "$TEST_DIR/test.atm" 2>&1)
echo "$OUTPUT" | grep -q "Authorized" && echo "   ✓ PASS: Login succeeded" || echo "   ✗ FAIL: Login failed"

# Test 2: Invalid PIN  
echo "6. Test 2: Invalid login with wrong PIN..."
OUTPUT=$(echo -e "begin-session Alice\n9999\nend-session" | ./bin/atm "$TEST_DIR/test.atm" 2>&1)
echo "$OUTPUT" | grep -q "Not authorized" && echo "   ✓ PASS: Login rejected" || echo "   ✗ FAIL: Should have rejected"

# Test 3: Non-existent user
echo "7. Test 3: Non-existent user..."
OUTPUT=$(echo -e "begin-session Bob\n1234" | ./bin/atm "$TEST_DIR/test.atm" 2>&1)
echo "$OUTPUT" | grep -q "Unable to access" && echo "   ✓ PASS: User not found" || echo "   ✗ FAIL: Should have failed"

# Cleanup
echo
echo "8. Cleaning up..."
exec 3>&-  # Close pipe
kill $BANK_PID $ROUTER_PID 2>/dev/null || true
sleep 0.5
pkill -9 bank router 2>/dev/null || true
rm -f "$FIFO"

echo
echo "=== Phase 2D Test Complete ==="
