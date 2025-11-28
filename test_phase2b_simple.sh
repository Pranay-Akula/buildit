#!/bin/bash
# Phase 2B Simple Test Script

echo "=========================================="
echo "PHASE 2B: Key Loading & Card Secrets"
echo "=========================================="
echo ""

PASS=0
FAIL=0

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

pass() { echo -e "${GREEN}✓${NC} $1"; ((PASS++)); }
fail() { echo -e "${RED}✗${NC} $1"; ((FAIL++)); }

# Cleanup
rm -f /tmp/phase2b.atm /tmp/phase2b.bank Alice.card Bob.card 2>/dev/null

echo "TEST 1: Init creates key files"
./bin/init /tmp/phase2b >/dev/null 2>&1
if [ -f /tmp/phase2b.atm ] && [ -f /tmp/phase2b.bank ] && [ $(wc -c < /tmp/phase2b.atm) -eq 32 ]; then
    pass "Init creates 32-byte key files"
else
    fail "Init failed"
fi

echo "TEST 2: Bank loads key and starts"
echo "" | head -1 | ./bin/bank /tmp/phase2b.bank 2>&1 | grep -q "BANK:"
if [ $? -eq 0 ]; then
    pass "Bank loads key successfully"
else
    fail "Bank failed to load key"
fi

echo "TEST 3: ATM loads key and starts"
echo "" | head -1 | ./bin/atm /tmp/phase2b.atm 2>&1 | grep -q "ATM:"
if [ $? -eq 0 ]; then
    pass "ATM loads key successfully"
else
    fail "ATM failed to load key"
fi

echo "TEST 4: Bank rejects invalid key file"
echo "bad" > /tmp/bad.bank
./bin/bank /tmp/bad.bank 2>&1 | grep -q "Error opening bank initialization file"
if [ $? -eq 0 ]; then
    pass "Bank rejects invalid key file"
else
    fail "Bank should reject invalid key"
fi
rm -f /tmp/bad.bank

echo "TEST 5: ATM rejects invalid key file"
echo "bad" > /tmp/bad.atm
./bin/atm /tmp/bad.atm 2>&1 | grep -q "Error opening ATM initialization file"
if [ $? -eq 0 ]; then
    pass "ATM rejects invalid key file"
else
    fail "ATM should reject invalid key"
fi
rm -f /tmp/bad.atm

echo "TEST 6: Bank creates card with 32-byte secret"
./bin/router >/dev/null 2>&1 & ROUTER_PID=$!
sleep 0.2
echo "create-user Alice 1234 100" | ./bin/bank /tmp/phase2b.bank 2>&1 | grep -q "Created user Alice"
CREATE_STATUS=$?
sleep 0.2
kill $ROUTER_PID 2>/dev/null

if [ $CREATE_STATUS -eq 0 ] && [ -f Alice.card ] && [ $(wc -c < Alice.card) -eq 32 ]; then
    pass "Bank creates 32-byte card secret"
else
    fail "Card creation failed"
fi

echo "TEST 7: Card secrets are random"
./bin/router >/dev/null 2>&1 & ROUTER_PID=$!
sleep 0.2
echo "create-user Bob 5678 200" | ./bin/bank /tmp/phase2b.bank >/dev/null 2>&1
sleep 0.2
kill $ROUTER_PID 2>/dev/null

if [ -f Alice.card ] && [ -f Bob.card ]; then
    if ! cmp -s Alice.card Bob.card; then
        pass "Card secrets are different (random)"
    else
        fail "Card secrets are identical!"
    fi
else
    fail "Could not create both cards"
fi

echo "TEST 8: Keys match in .atm and .bank"
if cmp -s /tmp/phase2b.atm /tmp/phase2b.bank; then
    pass "Keys are identical in .atm and .bank"
else
    fail "Keys differ between .atm and .bank"
fi

# Cleanup
rm -f /tmp/phase2b.atm /tmp/phase2b.bank Alice.card Bob.card 2>/dev/null

echo ""
echo "=========================================="
echo "Results: ${GREEN}${PASS} passed${NC}, ${RED}${FAIL} failed${NC}"
echo "=========================================="

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}✓ Phase 2B Complete!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    exit 1
fi
