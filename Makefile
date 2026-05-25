.PHONY: all clean

.SUFFIXES:

CC      = gcc
CFLAGS  = -Wall -Werror
LIBS   += -lpthread
INCLDIR = -Iinclude

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=src/%.o)

PROGS = ftpserveri ftpclient ftpmaster ftpslave

COMMON_OBJS = $(filter-out src/ftpserveri.o src/ftpclient.o src/ftpmaster.o src/ftpslave.o,$(OBJS))

all: $(PROGS)

ftpserveri: src/ftpserveri.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LIBS)

ftpclient: src/ftpclient.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LIBS)

ftpmaster: src/ftpmaster.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LIBS)

ftpslave: src/ftpslave.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLDIR) -c -o $@ $<

clean:
	rm -f $(PROGS) src/*.o
