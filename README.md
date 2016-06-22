# libsooshi [![Build Status](https://travis-ci.org/ghtyrant/libsooshi.svg?branch=master)](https://travis-ci.org/ghtyrant/libsooshi)

This is a library to interface with Mooshim Engineering's [Mooshimeter](https://moosh.im/), a wireless multimeter and data logger.

**This still is highly experimental, use at your own risk!**

The API is not stable and is most likely going to change. The library supports everything you need to fully use your Mooshimeter but lacks a lot of convenience functions. When running examples, and nothing happens after the library detected input/output characteristics of the meter, you most likely have to reset your device.

There is no way of updating the firmware (yet), you still need the Android app to do that.

Feel free to open pull requests/issues.

## Building
Simply run _make_ and you are good to go.

## Installing
Currently, there is no _install_ target. If you want to install the library, copy libsooshi.so to /usr/lib and src/sooshi.h to /usr/include.

## Examples
For an example, see [example/main.c](example/main.c). For a more sophisticated example, see [ghtyrant/sooshichef](http://github.com/ghtyrant/sooshichef)

## Dependencies
This library links against:

 * glib
 * gio
 * gobject

For Bluetooth communication, BlueZ is required. Make sure it's a recent version which supports BLE/GATT. You most likely will have to enable experimental support (run bluetoothd with --experimental).