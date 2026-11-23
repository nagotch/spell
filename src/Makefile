CFLAGS=-std=c11 -g

SRCS=$(filter-out funcs.c,$(wildcard *.c))
OBJS=$(SRCS:.c=.o)

spell: $(OBJS)
	$(CC) -o spell $(OBJS) $(LDFLAGS)

$(OBJS): spell.h

test: spell
	./test.sh

clean:
	rm -f spell *.o *~ tmp*

.PHONY: test clean
