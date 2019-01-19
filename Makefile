CC=gcc
CFILES=htmlify.c
OFILES=htmlify.o
LIBS=-lmisclib
WFLAGS=-Wall -Werror
GFLAGS=-g
ALLFLAGS=$(GFLAGS) $(WFLAGS)

htmlify: $(OFILES)
	$(CC) $(ALLFLAGS) -o htmlify $(OFILES) $(LIBS)

$(OFILES): $(CFILES)
	$(CC) $(ALLFLAGS) -c $(CFILES)

clean:
	rm *.o
