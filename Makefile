CC=gcc
WFLAGS=-Wall -Werror
CFILES=snake.c
OFILES=snake.o
LIBS=-lscreenlib -lmisclib -lpthread

snake: snake.o
	$(CC) -g -o snake $(WFLAGS) $(OFILES) $(LIBS)
snake.o: snake.c
	$(CC) -g -c $(WFLAGS) $(CFILES) $(LIBS)

clean:
	rm *.o
