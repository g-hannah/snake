CC=gcc
CFILES=htmlify.c
OFILES=htmlify.o
LIBS=-lmisclib

htmlify: $(OFILES)
	$(CC) -o htmlify $(OFILES) $(LIBS)

$(OFILES): $(CFILES)
	$(CC) -c $(CFILES)

clean:
	rm *.o
