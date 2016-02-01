CC=gcc
CFLAGS=-I. -Wall -lssl -pthread
DEPS = globals.h 
OBJ = rip.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

rip: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)