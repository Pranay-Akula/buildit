#!/bin/bash
# Phase 1 Testing Script - Init Program and Crypto Utilities

echo "=========================================="
echo "PHASE 1 TESTING: Init + Crypto Utilities"
echo "=========================================="
echo ""

PASS=0
FAIL=0

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    ((PASS++))
}

fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    ((FAIL++))
}

info() {
    echo -e "${YELLOW}ℹ INFO${NC}: $1"
}

echo "=== TEST 1: Init Program - No Arguments ==="
./bin/init > /tmp/test_out.txt 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 62 ] && grep -q "Usage:" /tmp/test_out.txt; then
    pass "Returns 62 and shows usage with no arguments"
else
    fail "Expected exit code 62, got $EXIT_CODE"
fi
echo ""

echo "=== TEST 2: Init Program - Too Many Arguments ==="
./bin/init foo bar baz > /tmp/test_out.txt 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 62 ] && grep -q "Usage:" /tmp/test_out.txt; then
    pass "Returns 62 and shows usage with too many arguments"
else
    fail "Expected exit code 62, got $EXIT_CODE"
fi
echo ""

echo "=== TEST 3: Init Program - Successful Creation ==="
rm -f /tmp/phase1_test.atm /tmp/phase1_test.bank 2>/dev/null
./bin/init /tmp/phase1_test > /tmp/test_out.txt 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] && grep -q "Successfully initialized bank state" /tmp/test_out.txt; then
    pass "Returns 0 and success message"
else
    fail "Expected exit code 0, got $EXIT_CODE"
fi
echo ""

echo "=== TEST 4: Init Program - Files Created ==="
if [ -f /tmp/phase1_test.atm ] && [ -f /tmp/phase1_test.bank ]; then
    pass "Both .atm and .bank files created"
else
    fail "Files not created"
fi
echo ""

echo "=== TEST 5: Init Program - File Sizes ==="
ATM_SIZE=$(wc -c < /tmp/phase1_test.atm)
BANK_SIZE=$(wc -c < /tmp/phase1_test.bank)
if [ "$ATM_SIZE" -eq 32 ] && [ "$BANK_SIZE" -eq 32 ]; then
    pass "Both files are exactly 32 bytes"
else
    fail "File sizes incorrect (atm: $ATM_SIZE, bank: $BANK_SIZE, expected: 32)"
fi
echo ""

echo "=== TEST 6: Init Program - Keys Are Identical ==="
if diff /tmp/phase1_test.atm /tmp/phase1_test.bank > /dev/null 2>&1; then
    pass "Keys in .atm and .bank are identical"
else
    fail "Keys differ between .atm and .bank"
fi
echo ""

echo "=== TEST 7: Init Program - File Already Exists ==="
./bin/init /tmp/phase1_test > /tmp/test_out.txt 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 63 ] && grep -q "already exists" /tmp/test_out.txt; then
    pass "Returns 63 when files already exist"
else
    fail "Expected exit code 63, got $EXIT_CODE"
fi
echo ""

echo "=== TEST 8: Init Program - Keys Are Random ==="
rm -f /tmp/phase1_test.atm /tmp/phase1_test.bank
./bin/init /tmp/phase1_test > /dev/null 2>&1
KEY1=$(hexdump -ve '1/1 "%.2x"' /tmp/phase1_test.atm)

rm -f /tmp/phase1_test2.atm /tmp/phase1_test2.bank
./bin/init /tmp/phase1_test2 > /dev/null 2>&1
KEY2=$(hexdump -ve '1/1 "%.2x"' /tmp/phase1_test2.atm)

if [ "$KEY1" != "$KEY2" ]; then
    pass "Keys are randomly generated (different each run)"
else
    fail "Keys are identical across runs (not random!)"
fi
echo ""

echo "=== TEST 9: Crypto Library - Compilation Check ==="
if [ -f util/crypto.c ] && [ -f util/crypto.h ]; then
    if grep -q "aes_encrypt" util/crypto.c && \
       grep -q "aes_decrypt" util/crypto.c && \
       grep -q "hmac_sha256" util/crypto.c && \
       grep -q "hmac_verify" util/crypto.c && \
       grep -q "compute_auth_token" util/crypto.c; then
        pass "All required crypto functions present in crypto.c"
    else
        fail "Missing crypto functions in crypto.c"
    fi
else
    fail "crypto.c or crypto.h not found"
fi
echo ""

echo "=== TEST 10: Crypto Library - Linked in Binaries ==="
if nm bin/atm | grep -q "aes_encrypt" && \
   nm bin/bank | grep -q "aes_encrypt"; then
    pass "Crypto library linked into atm and bank binaries"
else
    fail "Crypto library not linked properly"
fi
echo ""

echo "=== TEST 11: Makefile - All Targets Build ==="
if [ -f bin/init ] && [ -f bin/atm ] && [ -f bin/bank ] && [ -f bin/router ]; then
    pass "All 4 executables built successfully"
else
    fail "Missing executables"
fi
echo ""

echo "=== TEST 12: OpenSSL Integration ==="
if ldd bin/init 2>/dev/null | grep -q "crypto" || \
   otool -L bin/init 2>/dev/null | grep -q "crypto"; then
    pass "Init binary links against OpenSSL crypto library"
else
    fail "OpenSSL not linked in init binary"
fi
echo ""

# Cleanup
rm -f /tmp/test_out.txt
rm -f /tmp/phase1_test.atm /tmp/phase1_test.bank
rm -f /tmp/phase1_test2.atm /tmp/phase1_test2.bank

echo ""
echo "=========================================="
echo "PHASE 1 TEST RESULTS"
echo "=========================================="
echo -e "${GREEN}PASSED: $PASS${NC}"
echo -e "${RED}FAILED: $FAIL${NC}"
echo "TOTAL:  $((PASS + FAIL))"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}🎉 ALL PHASE 1 TESTS PASSED!${NC}"
    echo ""
    echo "Phase 1 is complete and working correctly."
    exit 0
else
    echo -e "${RED}❌ SOME TESTS FAILED${NC}"
    echo ""
    echo "Please fix the failing tests before proceeding."
    exit 1
fi
