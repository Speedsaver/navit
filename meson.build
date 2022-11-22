project('navit', 'c', 'cpp', default_options: ['warning_level=3'])

# Enable some sanity
add_project_arguments(
	'-fno-strict-aliasing',
	'-fwrapv',
	language: ['c', 'cpp'])

# Enable large files
add_project_arguments(
	'-D_LARGEFILE_SOURCE=1',
	'-D_FILE_OFFSET_BITS=64',
	language: ['c', 'cpp'])

# Set directories for data
sharedir = get_option('datadir') + '/navit'
add_project_arguments(
	'-DPREFIX="' + get_option('prefix') + '"',
	'-DLIBDIR="' + get_option('libdir') + '"',
	'-DLIB_DIR="' + get_option('libdir') + '"',
	'-DSHARE_DIR="' + sharedir + '"',
	'-DLOCALE_DIR="' + get_option('localedir') + '"',
	language: ['c', 'cpp'])

# Navit source
includedir = include_directories('navit')
sources = [
	'navit/announcement.c',
	'navit/atom.c',
	'navit/attr.c',
	'navit/bookmarks.c',
	'navit/builtin.c',
	'navit/cache.c',
	'navit/callback.c',
	'navit/command.c',
	'navit/config_.c',
	'navit/coord.c',
	'navit/country.c',
	'navit/data_window.c',
	'navit/debug.c',
	'navit/event.c',
	'navit/event_glib.c',
	'navit/fib-1.1/fib.c',
	'navit/file.c',
	'navit/geom.c',
	'navit/graphics.c',
	'navit/graphics/ssd1306/graphics_init_animation.cpp',
	'navit/graphics/ssd1306/graphics_ssd1306.cpp',
	'navit/gui.c',
	'navit/gui/speedsaver/gui_speedsaver.c',
	'navit/item.c',
	'navit/layout.c',
	'navit/linguistics.c',
	'navit/log.c',
	'navit/main.c',
	'navit/map/binfile/binfile.c',
	'navit/map.c',
	'navit/maps.c',
	'navit/mapset.c',
	'navit/maptype.c',
	'navit/menu.c',
	'navit/messages.c',
	'navit/navigation.c',
	'navit/navit.c',
	'navit/navit_nls.c',
	'navit/osd.c',
	'navit/param.c',
	'navit/phrase.c',
	'navit/plugin.c',
	'navit/popup.c',
	'navit/profile.c',
	'navit/profile_option.c',
	'navit/projection.c',
	'navit/roadprofile.c',
	'navit/route.c',
	'navit/search.c',
	'navit/search_houseno_interpol.c',
	'navit/speech.c',
	'navit/start.c',
	'navit/start_real.c',
	'navit/sunriset.c',
	'navit/track.c',
	'navit/transform.c',
	'navit/util.c',
	'navit/vehicle.c',
	'navit/vehicle/gpsd/vehicle_gpsd.c',
	'navit/vehicleprofile.c',
	'navit/xmlconfig.c']

# Runtime files
install_data('navit/navit_shipped.xml', rename: 'navit.xml', install_dir: sharedir)
install_data('navit/graphics/ssd1306/tone7.wav', install_dir: sharedir)

# Dependencies
cc = meson.get_compiler('cpp')
zlib = dependency('zlib')
libgps = dependency('libgps')
glib = dependency('glib-2.0')
arduipi = cc.find_library('ArduiPi_OLED')
libm = cc.find_library('m')
libpthread = cc.find_library('pthread')

# Binary
executable('navit',
	sources,
	dependencies: [zlib, libgps, glib, arduipi, libm, libpthread],
	include_directories: includedir,
	install: true)
