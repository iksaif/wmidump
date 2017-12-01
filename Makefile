NAME=	wmidump

SRCS=	wmidump.c
OBJS=	$(SRCS:.c=.o)

CFLAGS+=	-Wall -Wextra

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $(NAME) $(OBJS) $(LDFLAGS)

test: all
	$(MAKE) -Ctests

clean:
	rm -f $(NAME) $(OBJS)
	$(MAKE) -Ctests clean

.PHONY: all clean test
