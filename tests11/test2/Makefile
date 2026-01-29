MICROASM11 = ../../microasm11

all: hello.bin test.bin test1.bin test2.bin test3.bin test4.bin test5.bin life2.bin graph.bin

.SUFFIXES: .asm .bin

.asm.bin:
	$(MICROASM11) -binary --list $@.lst $< $@

clean:
	rm -f *.bin *.lst
