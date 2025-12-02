#!/bin/bash

# Debug test for authentication issue

cd "$(dirname "$0")"
make > /dev/null 2>&1

rm -f test.atm test.bank alice.card bob.card

./bin/init test > /dev/null
./bin/router &
ROUTER_PID=$!
sleep 0.5

mkfifo bank_fifo
(tail -f bank_fifo) | ./bin/bank test.bank > bank_debug.txt 2>&1 &
BANK_PID=$!
sleep 0.5

echo "create-user alice 1234 100" > bank_fifo
sleep 0.3

# Check alice.card was created
if [ -f "alice.card" ]; then
    echo "✓ alice.card created"
    hexdump -C alice.card | head -3
else
    echo "✗ alice.card not found"
fi

echo ""
echo "Testing login with correct PIN (1234):"
(echo "begin-session alice"; sleep 0.3; echo "1234") | ./bin/atm test.atm 2>&1 &
ATM_PID=$!
sleep 1
kill $ATM_PID 2>/dev/null

echo ""
echo "Creating bob..."
echo "create-user bob 6789 200" > bank_fifo
sleep 0.3

if [ -f "bob.card" ]; then
    echo "✓ bob.card created"
    hexdump -C bob.card | head -3
else
    echo "✗ bob.card not found"
fi

echo ""
echo "Testing login with wrong PIN (6789 instead of 1234):"
(echo "begin-session alice"; sleep 0.3; echo "6789") | ./bin/atm test.atm 2>&1

sleep 1

kill $BANK_PID $ROUTER_PID 2>/dev/null
rm bank_fifo
sleep 0.2

echo ""
echo "Bank output:"
cat bank_debug.txt

# Cleanup
rm -f test.atm test.bank alice.card bob.card bank_debug.txt
