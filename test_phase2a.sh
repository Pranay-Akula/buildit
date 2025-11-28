#!/bin/bash
# Phase 2A Testing Script - Data Structure Updates

echo "=========================================="
echo "PHASE 2A TESTING: Data Structure Updates"
echo "=========================================="
echo ""

PASS=0
FAIL=0

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    ((PASS++))
}

fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    ((FAIL++))
}

echo "=== TEST 1: ATM Header Updates ==="
if grep -q "unsigned char key_K\[KEY_SIZE\]" atm/atm.h && \
   grep -q "unsigned long long seq" atm/atm.h && \
   grep -q "unsigned char card_secret\[CARD_SECRET_SIZE\]" atm/atm.h && \
   grep -q "int key_loaded" atm/atm.h; then
    pass "ATM struct has all required crypto fields"
else
    fail "ATM struct missing crypto fields"
fi
echo ""

echo "=== TEST 2: Bank Header Updates ==="
if grep -q "unsigned char key_K\[KEY_SIZE\]" bank/bank.h && \
   grep -q "int key_loaded" bank/bank.h; then
    pass "Bank struct has all required crypto fields"
else
    fail "Bank struct missing crypto fields"
fi
echo ""

echo "=== TEST 3: User Struct Updates ==="
if grep -q "unsigned char card_secret\[CARD_SECRET_SIZE\]" bank/bank.h && \
   grep -q "unsigned long long last_seq" bank/bank.h; then
    pass "User struct has all required crypto fields"
else
    fail "User struct missing crypto fields"
fi
echo ""

echo "=== TEST 4: ATM Function Signature ==="
if grep -q "ATM\* atm_create(const char \*atm_init_file)" atm/atm.h; then
    pass "atm_create() now takes init file parameter"
else
    fail "atm_create() signature not updated"
fi
echo ""

echo "=== TEST 5: Bank Function Signature ==="
if grep -q "Bank\* bank_create(const char \*bank_init_file)" bank/bank.h; then
    pass "bank_create() now takes init file parameter"
else
    fail "bank_create() signature not updated"
fi
echo ""

echo "=== TEST 6: ATM Main Command Line Args ==="
if grep -q "argc != 2" atm/atm-main.c; then
    pass "ATM main checks for command line argument"
else
    fail "ATM main doesn't check command line args"
fi
echo ""

echo "=== TEST 7: Bank Main Command Line Args ==="
if grep -q "argc != 2" bank/bank-main.c; then
    pass "Bank main checks for command line argument"
else
    fail "Bank main doesn't check command line args"
fi
echo ""

echo "=== TEST 8: Compilation Success ==="
if [ -f bin/atm ] && [ -f bin/bank ]; then
    pass "All binaries compiled successfully"
else
    fail "Binaries not compiled"
fi
echo ""

echo "=== TEST 9: ATM Requires Argument ==="
./bin/atm >/dev/null 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 64 ]; then
    pass "ATM returns 64 when no argument provided"
else
    fail "ATM exit code incorrect (got $EXIT_CODE, expected 64)"
fi
echo ""

echo "=== TEST 10: Bank Requires Argument ==="
./bin/bank >/dev/null 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 64 ]; then
    pass "Bank returns 64 when no argument provided"
else
    fail "Bank exit code incorrect (got $EXIT_CODE, expected 64)"
fi
echo ""

echo "=== TEST 11: ATM Struct Field Sizes ==="
cat > /tmp/test_atm_sizes.c << 'EOF'
#include "atm.h"
int main() {
    _Static_assert(sizeof(((ATM*)0)->key_K) == 32, "key_K should be 32 bytes");
    _Static_assert(sizeof(((ATM*)0)->seq) == 8, "seq should be 8 bytes");
    _Static_assert(sizeof(((ATM*)0)->card_secret) == 32, "card_secret should be 32 bytes");
    return 0;
}
EOF
if gcc -I. -Iatm -Ibank -Iutil /tmp/test_atm_sizes.c -o /tmp/test_atm_sizes 2>/dev/null; then
    pass "ATM crypto fields have correct sizes"
else
    fail "ATM crypto field sizes incorrect"
fi
echo ""

echo "=== TEST 12: User Struct Field Sizes ==="
cat > /tmp/test_user_sizes.c << 'EOF'
#include "bank.h"
int main() {
    _Static_assert(sizeof(((User*)0)->card_secret) == 32, "card_secret should be 32 bytes");
    _Static_assert(sizeof(((User*)0)->last_seq) == 8, "last_seq should be 8 bytes");
    return 0;
}
EOF
if gcc -I. -Iatm -Ibank -Iutil /tmp/test_user_sizes.c -o /tmp/test_user_sizes 2>/dev/null; then
    pass "User crypto fields have correct sizes"
else
    fail "User crypto field sizes incorrect"
fi
echo ""

echo "=========================================="
echo "PHASE 2A TEST RESULTS"
echo "=========================================="
echo -e "${GREEN}PASSED: $PASS${NC}"
echo -e "${RED}FAILED: $FAIL${NC}"
echo "TOTAL:  $((PASS + FAIL))"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}🎉 ALL PHASE 2A TESTS PASSED!${NC}"
    echo ""
    echo "Phase 2A is complete. Data structures are ready for Phase 2B."
    exit 0
else
    echo -e "${RED}❌ SOME TESTS FAILED${NC}"
    exit 1
fi
