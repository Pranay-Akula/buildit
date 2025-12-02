#!/bin/bash

# Test the two fixes

cd "$(dirname "$0")"
make > /dev/null 2>&1

# Clean up
rm -f test.atm test.bank alice.card bob.card

# Test 1: Balance with extra arguments
echo "Test 1: Balance command with extra arguments"
./bin/init test > /dev/null
mkfifo bank_fifo
(tail -f bank_fifo) | ./bin/bank test.bank > bank_output.txt &
BANK_PID=$!
sleep 0.5

echo "create-user alice 1234 100" > bank_fifo
sleep 0.2
echo "balance alice 1000" > bank_fifo
sleep 0.2

kill $BANK_PID 2>/dev/null
rm bank_fifo
sleep 0.2

if grep -q "Usage:  balance <user-name>" bank_output.txt; then
    echo "✓ Test 1 PASSED: Balance rejects extra arguments"
else
    echo "✗ Test 1 FAILED: Balance should reject extra arguments"
    cat bank_output.txt
fi

rm bank_output.txt

# Test 2: Wrong PIN should fail
echo ""
echo "Test 2: Wrong PIN authentication"
./bin/init test > /dev/null
./bin/router &
ROUTER_PID=$!
sleep 0.5

mkfifo bank_fifo
(tail -f bank_fifo) | ./bin/bank test.bank > /dev/null &
BANK_PID=$!
sleep 0.5

echo "create-user alice 1234 100" > bank_fifo
sleep 0.3
echo "create-user bob 6789 200" > bank_fifo
sleep 0.3

(echo "begin-session alice"; sleep 0.3; echo "6789") | ./bin/atm test.atm > atm_output.txt &
ATM_PID=$!
sleep 1

kill $ATM_PID $BANK_PID $ROUTER_PID 2>/dev/null
rm bank_fifo
sleep 0.2

if grep -q "Not authorized" atm_output.txt; then
    echo "✓ Test 2 PASSED: Wrong PIN rejected"
else
    echo "✗ Test 2 FAILED: Wrong PIN should be rejected"
    cat atm_output.txt
fi

# Cleanup
rm -f test.atm test.bank alice.card bob.card bank_output.txt atm_output.txt
