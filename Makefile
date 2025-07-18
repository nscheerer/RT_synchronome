INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS)
LIBS= -lrt

HFILES= 
CFILES= synchro.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	synchro

clean:
	-rm -f *.o *.d synchro

synchro: ${OBJS}
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o $(LIBS)

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
