# Speedsaver software #

This is the core Speedsaver code. Derived from Navit, the open source satnav software. Thanks to the Navit team for their great work over the years.


To create the maptool executable binary file required to use our ogr2osm-translations map conversion repository do the following:

dependencies: meson ninja-build libglib2.0-dev

git clone https://github.com/Speedsaver/navit.git && cd navit && meson setup builddir && cd builddir && ninja

copy the newly created maptool executable binary file to path/to/ogr2osm/translations as described in our ogr2osm-translations repo
