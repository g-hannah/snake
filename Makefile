CC=gcc
WFLAGS=-Wall -Werror
LIBS=-lscreen -lpthread
DEBUG := 0

SOURCE_FILES := \
	snake.c

OBJECT_FILES := ${SOURCE_FILES:.c=.o}

snake: $(OBJECT_FILES)
	$(CC) $(WFLAGS) -o snake $(OBJECT_FILES) $(LIBS)

$(OBJECT_FILES): $(SOURCE_FILES)
ifeq ($(DEBUG), 1)
	$(CC) $(WFLAGS) -DDEBUG -g -c $^
else
	$(CC) $(WFLAGS) -c $^
endif

clean:
	rm *.o
