EXT_LIBS = `pkg-config --cflags --libs glib-2.0` `pkg-config --cflags --libs libxml-2.0`
LIBS = lib/semver.c

all:
	clang -std=gnu11 ${EXT_LIBS} ${LIBS} -Ilib -Iinclude *.c -o eq -g
