CC := gcc
LD := $(CC)

# Glib/GObject/Gio includes and libraries
GLIB_CFLAGS  := $(shell pkg-config --cflags glib-2.0 gobject-2.0 gio-2.0)
GLIB_LDFLAGS := $(shell pkg-config --libs glib-2.0 gobject-2.0 gio-2.0)

CFLAGS  := -fvisibility=hidden -fPIC -std=c99 -Wall -g $(GLIB_CFLAGS) -DG_LOG_DOMAIN=\"sooshi\" -DSOOSHI_DLL -DSOOSHI_DLL_EXPORTS
LDFLAGS := $(GLIB_LDFLAGS)

TARGET  := libsooshi.so
SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) -shared -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

run: $(TARGET)
	G_MESSAGES_DEBUG=all ./$(TARGET)

clean:
	rm $(TARGET) $(OBJECTS)

test:
	make -C tests/
	./tests/test
