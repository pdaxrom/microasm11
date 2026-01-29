TARGET = microasm11

all: $(TARGET) $(MODULES)

CFLAGS = -Wall -Wpedantic -g

LDFLAGS = -g

OBJS = microasm11.o

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

.SUFFIXES: .bin .asm

tests: $(TARGET)
	./tests11/run_golden_tests.sh
	make -C tests11/test2

clean:
	rm -rf $(OBJS) $(TARGET) $(MODULES) *.dSYM
	make -C tests11/test2 clean

codestyle:
	astyle --style=kr --indent=spaces=4 --add-braces *.c

.PHONY: tests
