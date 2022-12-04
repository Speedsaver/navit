## Files overview ###

Documentation:

- AUTHORS - Navit authors
- ChangeLog - Navit changelog
- COPYING - Navit copyright information
- COPYRIGHT - Navit copyright notice
- GPL-2 - GPLv2 license
- LGPL-2 - LGPLv2 license
- README.md - README file
- ARCHITECTURE.md - This file!

Development:

- meson.build - Meson build configuration
- .gitignore - List of files to ignore in Git
- navit/Doxyfile - Doxygen configuration

Third party dependencies:

- navit/fib-1.1/* - Fibonacci heap

Utility code:

- navit/navit_nls.c - Stub code, formerly translated strings
- navit/endianess.h - Defines byteswapping macros
- navit/atom.c - Maps duplicate strings to a single string pointer
- navit/cache.c - Memory-based cache for arbitrary data
- navit/zipfile.h - ZIP file data structures
- navit/coord.c - Coordinate handling
- navit/transform.c - Coordinate transformation
- navit/projection.c - Coordinate projections
- navit/country.c - Country lookup utilities
- navit/point.h - 2D point definition
- navit/util.c - Timestamp parsing, process spawning, min/max, case changing
- navit/geom.c - Geometry utilities
- navit/file.c - Filesystem access
- navit/param.c - String-based parameter lists
- navit/debug.c - Debug logging
- navit/linguistics.c - Handles string operations on non-English characters

Core code:

- navit/attr_def.h - List of all attribute types and flags
- navit/attr.c - Attributes
- navit/callback.c - Callbacks
- navit/xmlconfig.c - Object configuration and creation
- navit/navit.dtd - Navit XML doctype
- navit/navit_shipped.xml - Navit XML configuration
- navit/event.c - Event loop interface
- navit/event_glib.c - Event loop Glib implementation
- navit/track.c - Vehicle tracking information
- navit/roadprofile.c - Road profiles
- navit/vehicleprofile.c - Vehicle profiles
- navit/profile_option.c - Options for vehicle profiles
- navit/vehicle.c - Vehicles
- navit/item_def.h - List of all map item types and flags
- navit/item.c - Map items
- navit/map.c - Map file handling
- navit/maps.c - Configuration element to load maps using a wildcard filename
- navit/mapset.c - Sets of maps
- navit/plugin_def.h - All plugin categories and functions
- navit/plugin.h - Macros for using or defining plugins
- navit/plugin.c - Finds and loads plugins by category and name

Application code:

- navit/start_real.c - Application entry point
- navit/start.c - main stub, calls code in start_real
- navit/navit.service.in - systemd unit template
- navit/main.c - Sets up the NAVIT_* environment variables
- navit/config_.c - <config> object, sets up signals and LANG
- navit/graphics/ssd1306/graphics_ssd1306.cpp - SSD1306 GUI for Speedsaver
- navit/graphics/ssd1306/graphics_init_animation.cpp - Animations for SSD1306
- navit/graphics/ssd1306/tone7.wav - Tone that plays when over speed
- navit/map/binfile/binfile.c - Binfile map format
- navit/vehicle/gpsd/vehicle_gpsd.c - GPS vehicle
- navit/navit.c - Core object handling user commands and global state

MapTool:

- navit/maptool/maptool.c - Application entry and core logic
- navit/maptool/misc.c - Random utility functions
- navit/maptool/buffer.c - Saves and loads buffers from files
- navit/maptool/zip.c - ZIP file handling
- navit/maptool/tempfile.c - Creates and deletes temporary files
- navit/maptool/boundaries.c - Handles administrative boundaries
- navit/maptool/coastline.c - Handles coastline data
- navit/maptool/osm.c - OpenStreetMap to Navit attribute mapping
- navit/maptool/osm_xml.c - OpenStreetMap XML parser
- navit/maptool/osm_relations.c - Relations collections
- navit/maptool/tile.c - Tile management
- navit/maptool/itembin.c - Item handling, items are attributes and co-ords
- navit/maptool/itembin_buffer.c - Buffer for temporary items
- navit/maptool/sourcesink.c - Reads and writes groups of items to files
