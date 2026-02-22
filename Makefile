TARGET = microasm11

all: $(TARGET) $(MODULES)

CFLAGS = -Wall -Wpedantic -g

LDFLAGS = -g

OBJS = microasm11.o

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

.SUFFIXES: .bin .asm

tests: $(TARGET) gen-tests
	./tests11/run_golden_tests.sh
	./tests11/run_tests.sh
	$(MAKE) -C tests11/test2

gen-tests:
	python3 tests11/gen_tests.py tests11/fp11_golden.yaml

clean-tests:
	python3 tests11/gen_tests.py tests11/fp11_golden.yaml --clean

clean: clean-tests
	rm -rf $(OBJS) $(TARGET) $(MODULES) *.dSYM
	$(MAKE) -C tests11/test2 clean

codestyle:
	astyle --style=kr --indent=spaces=4 --add-braces *.c

.PHONY: tests
