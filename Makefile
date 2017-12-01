NAME	= wmidump

CC	= gcc

SRCS	= wmidump.c

OBJS	=	$(SRCS:.c=.o)

CFLAGS	?= -O2 -ggdb3
CFLAGS	+= -Wall -W -std=gnu99
CFLAGS	+= -D_GNU_SOURCE

.PHONY		:	all clean distclean
.SUFFIXES	:	.c .o

all: $(NAME)

$(NAME): $(OBJS) Makefile
	$(CC) $(CFLAGS) $(LDLIBS) -o $(NAME) $(OBJS) $(LFLAGS)

.c.o: $(HEADERS)
	$(CC) $(INCDIRS) $(CFLAGS) -c $*.c

test: all
	${MAKE} -Ctests

clean:
	find . \( -name "*.o" -o -name "*~" -o -name "#*#" \) -exec rm {} \;
	${MAKE} -Ctests clean

distclean:	clean
	rm -f $(NAME)

