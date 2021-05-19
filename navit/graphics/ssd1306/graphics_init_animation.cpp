#include <iostream>
#include <sstream>
#include <string>
#include <cstring>

#include "graphics_init_animation.h"

// Parse an in-memory XPM image.
// TODO: Would using gdk_pixbuf_new_from_xpm_data() be simpler overall?
// Parsing would be done for us, but we'd still need to turn a GdkPixbuf into
// something the OLED driver could use.
simple_bm::simple_bm(const char *pixmap[], size_t lines) :
	m_width(0),
	m_height(0),
	m_bytes_per_row(0),
	m_bm(nullptr) {
	std::istringstream  is(pixmap[0]);
	int width, height, colours, chars_per_pixel;
	if ( is >> width >> height >> colours >> chars_per_pixel ) {
		if ( height + colours + 1 != (int)lines ) {
			std::cerr << "Bad XPM - inconsistent height, colours and lines\n";
			return;
		}
		// Only interested in simple monochrome images
		if ( colours == 2 && chars_per_pixel == 1 ) {
			char set_char, ident_char;
			std::string colour_name;
			for ( int i = 0 ; i < colours ; i++ ) {
				is.clear();
				is.str(pixmap[1+i]);
				if ( is >> set_char >> ident_char >> colour_name ) {
					if ( colour_name == "#000000" ) {
						m_width = width;
						m_height = height;
						m_bytes_per_row = ((width+7)/8);
						m_bm = new unsigned char[m_bytes_per_row * height];
						memset(m_bm, 0, m_bytes_per_row * height);
						load_bm(pixmap,(colours+1),set_char);
						break;
					}
				}
			}
			if ( m_bm == nullptr ) {
				std::cerr << "Bad XPM - failed to find colour_name #000000\n";
				return;
			}
		} else {
			std::cerr << "Bad XPM - unexpected number of colours\n";
			return;
		}
	} else {
		std::cerr << "Bad XPM - failed to comprehend line 1\n";
		return;
	}
}

void simple_bm::set_pixel(int r, int c, int value) {
	int row_byte = c / 8;
	int row_bit = c % 8;
	int index = r * m_bytes_per_row + row_byte;
	// LHS of XPM is RHS on the OLED
	m_bm[index] |= (value<<(7-row_bit));
}

void simple_bm::load_bm(const char *pixmap[], int first_row, char set_char) {
	for ( int r = 0 ; r < m_height ; r++ ) {
		const char *p = pixmap[r+first_row];
		int len = strlen(p);
		if ( len != m_width ) {
			std::cerr << "Bad width(" << len << "), expected (" << m_width << ")\n";
			return;
		}
		for( int c = 0 ; c < m_width ; c++ ) {
			if ( p[c] == set_char ) {
				set_pixel(r,c,1);
			} else {
				set_pixel(r,c,0);
			}
		}
	}
}

#ifdef SIMPLE_BM_DEBUG
/* XPM */
static const char * sample_xpm[] = {
	// Sampler to check for byte/bit/row order weirdess.
	"32 32 2 1",
	".	c #FFFFFF",
	"#	c #000000",
	"........""........""........""........",
	".######.""...#...."".######."".######.",
	".#....#.""..##....""......#.""......#.",
	".#....#.""...#....""......#."".######.",
	".#....#.""...#...."".######.""......#.",
	".#....#.""...#...."".#......""......#.",
	".######."".######."".######."".######.",
	"........""........""........""........",

	"........""........""........""........",
	".#....#."".######."".#......"".######.",
	".#....#."".#......"".#......""......#.",
	".#....#."".#......"".######.""......#.",
	".######."".######."".#....#.""......#.",
	"......#.""......#."".#....#.""......#.",
	"......#."".######."".######.""......#.",
	"........""........""........""........",

	"........""........""........""........",
	".######."".######."".######."".######.",
	".#....#."".#....#."".#....#."".#....#.",
	".#....#."".#....#."".#....#."".#####..",
	".######."".######."".######."".#....#.",
	".#....#.""......#."".#....#."".#....#.",
	".######.""......#."".#....#."".######.",
	"........""........""........""........",

	"........""........""........""........",
	".######."".####..."".######."".######.",
	".#......"".#...#.."".#......"".#......",
	".#......"".#....#."".######."".######.",
	".#......"".#....#."".#......"".#......",
	".#......"".#...#.."".#......"".#......",
	".######."".####..."".######."".#......",
	"........""........""........""........",
};
#endif

/* XPM */
static const char * satellite_xpm[] = {
	"40 32 2 1",
	" 	c #FFFFFF",
	".	c #000000",
	"                                        ",
	"               ......                   ",
	"               .    .                   ",
	"               .    .                   ",
	"               .    .                   ",
	"               .    .                   ",
	"               .    .                   ",
	"               ......                   ",
	"               .    .                   ",
	"               .    .                   ",
	"               ......                   ",
	"                 ..                     ",
	"       ..    ...........                ",
	"       . .   .       . ...              ",
	"       .  ....       . . .              ",
	"       .   . .       . . .              ",
	"       .   . .       . . .              ",
	"       .   . .       . . .              ",
	"       .  ....       . ...              ",
	"       . .   ...........                ",
	"       ..        ..                     ",
	"                 ..                     ",
	"               ......                   ",
	"               .    .                   ",
	"               .    .                   ",
	"               ......                   ",
	"               .    .                   ",
	"               .    .                   ",
	"               .    .                   ",
	"               .    .                   ",
	"               ......                   ",
	"                                        "
};

/* XPM */
static const char * small_car_1_xpm[] = {
	"40 32 2 1",
	" 	c #FFFFFF",
	".	c #000000",
	"                                        ",
	"                                        ",
	"                                        ",
	"              .....                     ",
	"           ....    ...                  ",
	"         ..           ..                ",
	"        ..              ..              ",
	"       .    ....  ...    ..             ",
	"      .    ..  .  . ...   .             ",
	"     ..   .    .  .   ..   .            ",
	"     .   .     .  .    ..  ..           ",
	"    .   .      .  .     ..  .           ",
	"    .  ..      .  .      .   .          ",
	"    .  .       .  .      .    ..        ",
	"   .   ..      .  .     ..     ...      ",
	"   .    .......   ......          ..    ",
	"   .                               ..   ",
	"   .                                .   ",
	"   .    .....            .....      .   ",
	"   .  ... . ...        ... . ...    .   ",
	"   .  .   .   .        .   .   .    .   ",
	"    ...   .   ..      ..   .   ..   .   ",
	"    ..    .    .      .    .    .   .   ",
	"     .    .    .      .    .    .  .    ",
	"     .   . .   ........   . .   ...     ",
	"     .. .   . ..      .. .   . ..       ",
	"      ..     ..        ..     ..        ",
	"      ...   ...        ...   ...        ",
	"        .....            .....          ",
	"                                        ",
	"                                        ",
	"                                        "
};

/* XPM */
static const char * small_car_2_xpm[] = {
	"40 32 2 1",
	" 	c #FFFFFF",
	".	c #000000",
	"                                        ",
	"                                        ",
	"                                        ",
	"              .....                     ",
	"           ....    ...                  ",
	"         ..           ..                ",
	"        ..              ..              ",
	"       .    ....  ...    ..             ",
	"      .    ..  .  . ...   .             ",
	"     ..   .    .  .   ..   .            ",
	"     .   .     .  .    ..  ..           ",
	"    .   .      .  .     ..  .           ",
	"    .  ..      .  .      .   .          ",
	"    .  .       .  .      .    ..        ",
	"   .   ..      .  .     ..     ...      ",
	"   .    .......   ......          ..    ",
	"   .                               ..   ",
	"   .                                .   ",
	"   .    .....            .....      .   ",
	"   .  ...   ...        ...   ...    .   ",
	"   .  .      ..        .      ..    .   ",
	"    ...     . ..      ..     . ..   .   ",
	"    ..     .   .      .     .   .   .   ",
	"     ......    .      ......    .  .    ",
	"     .    .    ........    .    ...     ",
	"     ..   .   ..      ..   .   ..       ",
	"      .   .   .        .   .   .        ",
	"      ... . ...        ... . ...        ",
	"        .....            .....          ",
	"                                        ",
	"                                        ",
	"                                        "
};

/* XPM */
static const char * small_car_3_xpm[] = {
	"40 32 2 1",
	" 	c #FFFFFF",
	".	c #000000",
	"                                        ",
	"                                        ",
	"                                        ",
	"              .....                     ",
	"           ....    ...                  ",
	"         ..           ..                ",
	"        ..              ..              ",
	"       .    ....  ...    ..             ",
	"      .    ..  .  . ...   .             ",
	"     ..   .    .  .   ..   .            ",
	"     .   .     .  .    ..  ..           ",
	"    .   .      .  .     ..  .           ",
	"    .  ..      .  .      .   .          ",
	"    .  .       .  .      .    ..        ",
	"   .   ..      .  .     ..     ...      ",
	"   .    .......   ......          ..    ",
	"   .                               ..   ",
	"   .                                .   ",
	"   .    .....            .....      .   ",
	"   .  ...   ...        ...   ...    .   ",
	"   .  ..      .        ..      .    .   ",
	"    ... .     ..      .. .     ..   .   ",
	"    ..   .     .      .   .     .   .   ",
	"     .    ......      .    ......  .    ",
	"     .   .     ........   .     ...     ",
	"     .. .     ..      .. .     ..       ",
	"      ..      .        ..      .        ",
	"      ...   ...        ...   ...        ",
	"        .....            .....          ",
	"                                        ",
	"                                        ",
	"                                        "
};

/* XPM */
static const char * signal_bar1_xpm[] = {
	"32 32 2 1",
	" 	c #FFFFFF",
	".	c #000000",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                   ..           ",
	"                   ...          ",
	"                  ....          ",
	"                  ...           ",
	"                  ...  ..       ",
	"                 ....  ...      ",
	"                 ....  ...      ",
	"                  ...  ..       ",
	"                  ...           ",
	"                  ....          ",
	"                   ...          ",
	"                   ..           ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                "
};

/* XPM */
static const char * signal_bar2_xpm[] = {
	"32 32 2 1",
	" 	c #FFFFFF",
	".	c #000000",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"               ..               ",
	"               ...              ",
	"              ....              ",
	"             ....               ",
	"             ....               ",
	"             ...                ",
	"            ....                ",
	"            ....                ",
	"            ....                ",
	"            ....                ",
	"            ....                ",
	"            ....                ",
	"            ....                ",
	"            ....                ",
	"             ...                ",
	"             ....               ",
	"             ....               ",
	"              ....              ",
	"               ...              ",
	"               ..               ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                ",
	"                                "
};

/* XPM */
static const char * signal_bar3_xpm[] = {
	"32 32 2 1",
	" 	c #FFFFFF",
	".	c #000000",
	"                                ",
	"                                ",
	"           ..                   ",
	"          ....                  ",
	"         .....                  ",
	"         ....                   ",
	"        .....                   ",
	"        ....                    ",
	"       .....                    ",
	"       ....                     ",
	"       ....                     ",
	"       ....                     ",
	"      ....                      ",
	"      ....                      ",
	"      ....                      ",
	"      ....                      ",
	"      ....                      ",
	"      ....                      ",
	"      ....                      ",
	"      ....                      ",
	"       ....                     ",
	"       ....                     ",
	"       ....                     ",
	"       .....                    ",
	"        ....                    ",
	"        .....                   ",
	"         .....                  ",
	"         .....                  ",
	"          ....                  ",
	"           ..                   ",
	"                                ",
	"                                "
};

#define NLINES(x)	(sizeof(x)/sizeof(*x))
#define BMX(x)		x, NLINES(x)
void generate_init_animations(simple_bm *bms[], size_t num_bms) {
	size_t i = 0;
#ifdef SIMPLE_BM_DEBUG
	bms[i++] = new simple_bm(BMX(sample_xpm));
#else
	bms[i++] = new simple_bm(BMX(small_car_1_xpm));
	bms[i++] = new simple_bm(BMX(signal_bar1_xpm));
	bms[i++] = new simple_bm(BMX(satellite_xpm));

	bms[i++] = new simple_bm(BMX(small_car_2_xpm));
	bms[i++] = new simple_bm(BMX(signal_bar2_xpm));
	bms[i++] = new simple_bm(BMX(satellite_xpm));

	bms[i++] = new simple_bm(BMX(small_car_3_xpm));
	bms[i++] = new simple_bm(BMX(signal_bar3_xpm));
	bms[i++] = new simple_bm(BMX(satellite_xpm));
#endif
}
