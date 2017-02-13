CFLAGS = `xml2-config --cflags --libs` `pkg-config --cflags --libs glib-2.0`
FILES = main.c utils.c use.c

all:
	clang $(CFLAGS) $(FILES) -Iinclude -o equ
