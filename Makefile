CC = gcc

# Detect OS for stack flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  # macOS - just disable stack protector (executable stack not available)
  STACK_FLAGS = -fno-stack-protector
else
  # Linux
  STACK_FLAGS = -fno-stack-protector -z execstack
endif

# OpenSSL paths for macOS (Homebrew)
OPENSSL_INCLUDE = -I/opt/homebrew/opt/openssl@3/include
OPENSSL_LIB = -L/opt/homebrew/opt/openssl@3/lib

CFLAGS = ${STACK_FLAGS} -Wall -Iutil -Iatm -Ibank -Irouter -I. ${OPENSSL_INCLUDE}
LDFLAGS = ${OPENSSL_LIB} -lcrypto

all: bin bin/init bin/atm bin/bank bin/router

bin:
	mkdir -p bin

bin/init : init.c
	${CC} ${CFLAGS} init.c -o bin/init ${LDFLAGS}

bin/atm : atm/atm-main.c atm/atm.c util/crypto.c
	${CC} ${CFLAGS} util/crypto.c atm/atm.c atm/atm-main.c -o bin/atm ${LDFLAGS}

bin/bank : bank/bank-main.c bank/bank.c util/crypto.c
	${CC} ${CFLAGS} util/crypto.c bank/bank.c bank/bank-main.c -o bin/bank ${LDFLAGS}

bin/router : router/router-main.c router/router.c
	${CC} ${CFLAGS} router/router.c router/router-main.c -o bin/router

test : util/list.c util/list_example.c util/hash_table.c util/hash_table_example.c
	${CC} ${CFLAGS} util/list.c util/list_example.c -o bin/list-test
	${CC} ${CFLAGS} util/list.c util/hash_table.c util/hash_table_example.c -o bin/hash-table-test

clean:
	cd bin && rm -f *
