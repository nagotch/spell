CFLAGS=-std=c11 -g

SRCS=$(filter-out funcs.c,$(wildcard *.c))
OBJS=$(SRCS:.c=.o)
COMMON_OBJS=$(filter-out main.o lsp.o,$(OBJS))

all: spell spell-lsp

spell: $(COMMON_OBJS) main.o
	$(CC) -o spell $(COMMON_OBJS) main.o $(LDFLAGS)

spell-lsp: $(COMMON_OBJS) lsp.o
	$(CC) -o spell-lsp $(COMMON_OBJS) lsp.o $(LDFLAGS)

$(OBJS): spell.h

test: spell
	./test.sh

clean:
	rm -f spell spell-lsp *.o *~ tmp*

.PHONY: test clean
