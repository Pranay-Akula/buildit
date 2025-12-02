#!/bin/bash

# Simulate exact test 17 scenario

cd "$(dirname "$0")"
make > /dev/null 2>&1

# Clean everything
rm -f test.atm test.bank alice.card bob.card

echo "=== Simulating Test 17 ==="
echo ""

# Init
./bin/init test > /dev/null
echo "✓ Initialized"

# Start router
./bin/router > /dev/null 2>&1 &
ROUTER_PID=$!
sleep 0.5

# Start bank
mkfifo bank_fifo
(tail -f bank_fifo) | ./bin/bank test.bank > bank.log 2>&1 &
BANK_PID=$!
sleep 0.5

# Create alice with PIN 1234
echo "create-user alice 1234 100" > bank_fifo
sleep 0.3
echo "✓ Created alice (PIN: 1234)"
echo "  alice.card contents:"
hexdump -C alice.card | head -2

# Create bob with PIN 6789
echo "create-user bob 6789 200" > bank_fifo
sleep 0.3
echo "✓ Created bob (PIN: 6789)"
echo "  bob.card contents:"
hexdump -C bob.card | head -2

echo ""
echo "Now trying to login to alice with wrong PIN (6789)..."
echo ""

# Try to login with wrong PIN
(echo "begin-session alice"; sleep 0.3; echo "6789"; sleep 0.3) | ./bin/atm test.atm > atm.log 2>&1 &
ATM_PID=$!
sleep 1.5

echo "ATM output:"
cat atm.log

echo ""
echo "Bank log:"
cat bank.log

# Cleanup
kill $ATM_PID $BANK_PID $ROUTER_PID 2>/dev/null
rm bank_fifo
sleep 0.2

if grep -q "Not authorized" atm.log; then
    echo ""
    echo "✓ TEST PASSED: Wrong PIN rejected"
else
    echo ""
    echo "✗ TEST FAILED: Wrong PIN was accepted!"
fi

# Cleanup
rm -f test.atm test.bank alice.card bob.card bank.log atm.log bank_fifo
