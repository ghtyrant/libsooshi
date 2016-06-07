CC=gcc
LD=$(CC)
CFLAGS=-Wall -g -pedantic $(GLIB_CFLAGS) -DG_LOG_DOMAIN=\"sooshi\"
LDFLAGS=$(GLIB_LDFLAGS)

GLIB_CFLAGS=$(shell pkg-config --cflags glib-2.0 gobject-2.0 gio-2.0)
GLIB_LDFLAGS=$(shell pkg-config --libs glib-2.0 gobject-2.0 gio-2.0)

TARGET=mooshi
SOURCES=$(wildcard src/*.c)
OBJECTS=$(SOURCES:.c=.o)

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

run: $(TARGET)
	G_MESSAGES_DEBUG=all ./$(TARGET)

clean:
	rm $(TARGET) $(OBJECTS)
