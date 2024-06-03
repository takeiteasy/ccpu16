default: all

ccpu:
	clang -shared -fpic \
		-Isrc src/cpu.c src/disassemble.c src/assembler.c \
		-o build/libccpu.dylib

test: ccpu
	clang -Isrc tests/test.c -Lbuild -lccpu -o build/test

all: test ccpu
