CC=gcc
WFLAGS=-Wall -Werror
CFILES=snake.c
OFILES=snake.o
LIBS=-lscreenlib -lpthread

snake: snake.o
	$(CC) -O2 -o snake $(WFLAGS) $(OFILES) $(LIBS)
snake.o: snake.c
	$(CC) -O2 -c $(WFLAGS) $(CFILES)

clean:
	rm *.o
