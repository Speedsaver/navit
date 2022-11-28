# Speedsaver software #

This is the core Speedsaver code. Derived from Navit, the open source satnav software. Thanks to the Navit team for their great work over the years.

# Building #

Install meson, ninja-build and libglib2.0-dev using your favorite package manager. Then run:

```
git clone https://github.com/Speedsaver/navit
cd navit
meson setup builddir
cd builddir
ninja
```

This will try to build the following software:

- navit: Requires libgps and [ArduiPi_OLED](https://github.com/Speedsaver/ArduiPi_OLED)
- maptool: Requires an x86_64 target processor

# Importing maps #

To import maps, follow the https://github.com/speedsaver/ogr2osm-translations tool. Make sure to copy the 'maptool' binary to the required directory.
