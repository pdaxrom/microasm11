TARGET = microasm11 microlink11 microdis11

all: $(TARGET) $(MODULES)

CFLAGS = -Wall -Wpedantic -g

LDFLAGS = -g

MICROASM11_OBJS = microasm11.o
MICROLINK11_OBJS = microlink11.o
MICRODIS11_OBJS = microdis11.o

microasm11: $(MICROASM11_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

microlink11: $(MICROLINK11_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

microdis11: $(MICRODIS11_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

.SUFFIXES: .bin .asm

tests: $(TARGET) gen-tests
	./tests11/run_golden_tests.sh
	./tests11/run_tests.sh
	bash ./tests11/run_object_tests.sh
	$(MAKE) -C tests11/test2

gen-tests:
	python3 tests11/gen_tests.py tests11/fp11_golden.yaml

clean-tests:
	python3 tests11/gen_tests.py tests11/fp11_golden.yaml --clean

clean: clean-tests
	rm -rf $(MICROASM11_OBJS) $(MICROLINK11_OBJS) $(MICRODIS11_OBJS) $(TARGET) $(MODULES) *.dSYM
	$(MAKE) -C tests11/test2 clean

codestyle:
	astyle --style=kr --indent=spaces=4 --add-braces *.c

.PHONY: tests
