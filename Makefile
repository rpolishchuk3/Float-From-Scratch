CC     = cc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2
SRC    = src/nfloat.c

all: nfloat

nfloat: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: nfloat
	@mkdir -p build
	@fail=0; \
	for input in Testcases/*-input.txt; do \
		name=$$(basename $$input -input.txt); \
		./nfloat < $$input > build/$$name-output.txt; \
		if cmp -s build/$$name-output.txt Testcases/$$name-output.txt; then \
			echo "PASS $$name"; \
		else \
			echo "FAIL $$name"; fail=1; \
		fi; \
	done; \
	exit $$fail

clean:
	rm -rf nfloat build

.PHONY: all test clean
