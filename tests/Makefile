CC := gcc
LD := $(CC)

# Glib/GObject/Gio includes and libraries
GLIB_CFLAGS  := $(shell pkg-config --cflags glib-2.0 gobject-2.0 gio-2.0)
GLIB_LDFLAGS := $(shell pkg-config --libs glib-2.0 gobject-2.0 gio-2.0)

CFLAGS  := -Wall -std=c99 -g $(GLIB_CFLAGS) -I../src/
LDFLAGS := $(GLIB_LDFLAGS)

TARGET  := test
SOURCES := $(wildcard ../src/*.c tests.c)
OBJECTS := $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

run: all
	LD_LIBRARY_PATH=../ G_MESSAGES_DEBUG=all ./$(TARGET)

gdb: all
	LD_LIBRARY_PATH=../ G_MESSAGES_DEBUG=all gdb $(TARGET)

valgrind: all
	LD_LIBRARY_PATH=../ valgrind --tool=memcheck --leak-check=full --show-leak-kinds=definite,indirect,possible --show-reachable=no --leak-resolution=high --num-callers=20 --trace-children=no --child-silent-after-fork=yes --track-fds=yes --track-origins=yes ./$(TARGET) 2>&1 | tee valgrind.log

callgrind: all
	LD_LIBRARY_PATH=../ valgrind --tool=callgrind ./$(TARGET)

clean:
	rm $(TARGET) $(OBJECTS)
