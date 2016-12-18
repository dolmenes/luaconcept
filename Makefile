CC:= gcc
CFLAGS:= -g -ggdb -std=gnu11 -Wall -Wextra

CPPJIT:= -I/usr/include/luajit-2.0
LDJIT:= -lluajit-5.1

.PHONY: all
all: test1

test1: CPPFLAGS+= $(CPPJIT)
test1: LDLIBS+= $(LDJIT)

test1: test1.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDLIBS)

.PHONY: clean
clean:
	rm -f *.o
	rm -f test1
