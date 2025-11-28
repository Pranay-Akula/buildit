#!/bin/bash
# Phase 2B Testing Script - Key Loading & Card Secret Generation

echo "=========================================="
echo "PHASE 2B TESTING: Key Loading & Card Secrets"
echo "=========================================="
echo ""

PASS=0
FAIL=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    ((PASS++))
}

fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    ((FAIL++))
}

# Cleanup function
cleanup() {
    rm -f /tmp/phase2b_test.atm /tmp/phase2b_test.bank
    rm -f /tmp/phase2b_user.card
    rm -f /tmp/test_output.txt
}

cleanup

echo "=== TEST 1: Init Creates Key Files ==="
./bin/init /tmp/phase2b_test >/dev/null 2>&1
if [ -f /tmp/phase2b_test.atm ] && [ -f /tmp/phase2b_test.bank ]; then
    pass "Init created .atm and .bank files"
else
    fail "Init failed to create files"
fi
echo ""

echo "=== TEST 2: ATM Loads Key from File ==="
timeout 1 ./bin/atm /tmp/phase2b_test.atm >/dev/null 2>&1 &
ATM_PID=$!
sleep 0.2
if kill -0 $ATM_PID 2>/dev/null; then
    kill $ATM_PID 2>/dev/null
    pass "ATM successfully loaded key and started"
else
    fail "ATM failed to start with key file"
fi
echo ""

echo "=== TEST 3: Bank Loads Key from File ==="
timeout 1 ./bin/bank /tmp/phase2b_test.bank >/dev/null 2>&1 &
BANK_PID=$!
sleep 0.2
if kill -0 $BANK_PID 2>/dev/null; then
    kill $BANK_PID 2>/dev/null
    pass "Bank successfully loaded key and started"
else
    fail "Bank failed to start with key file"
fi
echo ""

echo "=== TEST 4: ATM Rejects Invalid Key File ==="
echo "not32bytes" > /tmp/bad_key.atm
./bin/atm /tmp/bad_key.atm >/tmp/test_output.txt 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 64 ] && grep -q "Error opening ATM initialization file" /tmp/test_output.txt; then
    pass "ATM rejects invalid key file"
else
    fail "ATM should reject invalid key file (exit code: $EXIT_CODE)"
fi
rm -f /tmp/bad_key.atm
echo ""

echo "=== TEST 5: Bank Rejects Invalid Key File ==="
echo "not32bytes" > /tmp/bad_key.bank
./bin/bank /tmp/bad_key.bank >/tmp/test_output.txt 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 64 ] && grep -q "Error opening bank initialization file" /tmp/test_output.txt; then
    pass "Bank rejects invalid key file"
else
    fail "Bank should reject invalid key file (exit code: $EXIT_CODE)"
fi
rm -f /tmp/bad_key.bank
echo ""

echo "=== TEST 6: Bank Creates Card with Secret ==="
./bin/router >/dev/null 2>&1 &
ROUTER_PID=$!
sleep 0.2

./bin/bank /tmp/phase2b_test.bank >/tmp/test_output.txt 2>&1 &
BANK_PID=$!
sleep 0.3

# Send create-user command
echo "create-user TestUser 1234 100" | timeout 1 nc localhost 32001 2>/dev/null &
sleep 0.3
echo "create-user TestUser 1234 100" > /tmp/bank_input.txt
timeout 1 bash -c "cat /tmp/bank_input.txt > /proc/$BANK_PID/fd/0" 2>/dev/null || true

# Alternative: use expect or just check if card file is created
kill $BANK_PID 2>/dev/null
kill $ROUTER_PID 2>/dev/null

# Try a simpler approach - run bank with input
./bin/router >/dev/null 2>&1 &
ROUTER_PID=$!
sleep 0.2

echo "create-user Alice 1234 100" | timeout 2 ./bin/bank /tmp/phase2b_test.bank > /tmp/bank_output.txt 2>&1 &
BANK_PID=$!
sleep 0.5

if [ -f Alice.card ]; then
    CARD_SIZE=$(wc -c < Alice.card)
    if [ "$CARD_SIZE" -eq 32 ]; then
        pass "Bank creates card file with 32-byte secret"
    else
        fail "Card file size incorrect (got $CARD_SIZE, expected 32)"
    fi
else
    fail "Bank did not create card file"
fi

kill $BANK_PID 2>/dev/null
kill $ROUTER_PID 2>/dev/null
echo ""

echo "=== TEST 7: Card Secrets Are Random ==="
./bin/router >/dev/null 2>&1 &
ROUTER_PID=$!
sleep 0.2

echo -e "create-user Bob 5678 200\nquit" | timeout 2 ./bin/bank /tmp/phase2b_test.bank >/dev/null 2>&1 &
BANK_PID=$!
sleep 0.5

if [ -f Alice.card ] && [ -f Bob.card ]; then
    if ! cmp -s Alice.card Bob.card; then
        pass "Card secrets are different (random)"
    else
        fail "Card secrets are identical (not random!)"
    fi
else
    echo -e "${YELLOW}ℹ SKIP${NC}: Could not create both card files for comparison"
fi

kill $BANK_PID 2>/dev/null
kill $ROUTER_PID 2>/dev/null
echo ""

echo "=== TEST 8: ATM Rejects Non-existent Key File ==="
./bin/atm /tmp/does_not_exist.atm >/tmp/test_output.txt 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 64 ]; then
    pass "ATM returns 64 for non-existent key file"
else
    fail "ATM should return 64 (got $EXIT_CODE)"
fi
echo ""

echo "=== TEST 9: Bank Rejects Non-existent Key File ==="
./bin/bank /tmp/does_not_exist.bank >/tmp/test_output.txt 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 64 ]; then
    pass "Bank returns 64 for non-existent key file"
else
    fail "Bank should return 64 (got $EXIT_CODE)"
fi
echo ""

echo "=== TEST 10: Keys Match Between ATM and Bank Files ==="
if [ -f /tmp/phase2b_test.atm ] && [ -f /tmp/phase2b_test.bank ]; then
    if cmp -s /tmp/phase2b_test.atm /tmp/phase2b_test.bank; then
        pass "ATM and Bank files contain identical keys"
    else
        fail "ATM and Bank keys differ"
    fi
else
    fail "Key files missing"
fi
echo ""

cleanup
rm -f Alice.card Bob.card /tmp/bank_input.txt /tmp/bank_output.txt

echo "=========================================="
echo "PHASE 2B TEST RESULTS"
echo "=========================================="
echo -e "${GREEN}PASSED: $PASS${NC}"
echo -e "${RED}FAILED: $FAIL${NC}"
echo "TOTAL:  $((PASS + FAIL))"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}🎉 ALL PHASE 2B TESTS PASSED!${NC}"
    echo ""
    echo "Phase 2B is complete. Keys are loaded and card secrets are generated."
    exit 0
else
    echo -e "${RED}❌ SOME TESTS FAILED${NC}"
    exit 1
fi
