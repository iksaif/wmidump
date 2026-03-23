NAME     = wmidump
CC      ?= gcc
SRCS     = wmidump.c
OBJS     = $(SRCS:.c=.o)

CFLAGS  ?= -O2 -g
CFLAGS  += -Wall -Wextra -std=c17 -pedantic
CFLAGS  += -D_GNU_SOURCE

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin

.PHONY: all clean install uninstall test

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(NAME)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(NAME) $(DESTDIR)$(BINDIR)/$(NAME)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(NAME)

test: $(NAME)
	@./test.sh

clean:
	rm -f $(OBJS) $(NAME)
