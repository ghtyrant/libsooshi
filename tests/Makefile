CC := gcc
LD := $(CC)

# Glib/GObject/Gio includes and libraries
GLIB_CFLAGS  := $(shell pkg-config --cflags glib-2.0 gobject-2.0 gio-2.0)
GLIB_LDFLAGS := $(shell pkg-config --libs glib-2.0 gobject-2.0 gio-2.0)

CFLAGS  := -Wall -g $(GLIB_CFLAGS) -I../src/
LDFLAGS := $(GLIB_LDFLAGS) -L../ -lsooshi

TARGET  := test
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

run: $(TARGET)
	LD_LIBRARY_PATH=../ G_MESSAGES_DEBUG=all ./$(TARGET)

gdb: $(TARGET)
	LD_LIBRARY_PATH=../ G_MESSAGES_DEBUG=all gdb $(TARGET)

clean:
	rm $(TARGET) $(OBJECTS)
