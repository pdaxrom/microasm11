TARGET = microasm11

all: $(TARGET) $(MODULES)

CFLAGS = -Wall -Wpedantic -g

LDFLAGS = -g

OBJS = microasm11.o

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

.SUFFIXES: .bin .asm

tests:
	./tests11/run_golden_tests.sh
clean:
	rm -f $(OBJS) $(TARGET) $(MODULES)

codestyle:
	astyle --style=kr --indent=spaces=4 --add-braces *.c

.PHONY: tests
