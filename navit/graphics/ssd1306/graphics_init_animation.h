#ifndef GRAPHICS_INIT_ANIMATION_H
#define GRAPHICS_INIT_ANIMATION_H

// #define SIMPLE_BM_DEBUG

class simple_bm {
private:
	int             m_width;
	int             m_height;
	int             m_bytes_per_row;
	unsigned char  *m_bm;
	void set_pixel(int r, int c, int value);
	void load_bm(const char *pixmap[], int first_row, char set_char);
public:
	simple_bm() :
		m_width(0),
		m_height(0),
		m_bytes_per_row(0),
		m_bm(nullptr) {
	};
	simple_bm(const char *pixmap[], size_t lines);
	simple_bm(const char *filename) {
		// TODO: Load from a monochrome PNG file
	};
	const unsigned char *get_bm(void) const {
		return m_bm;
	};
	const int get_width(void) const {
		return m_width;
	};
	const int get_height(void) const {
		return m_height;
	};
	const int get_pixel(int r, int c) const {
		int row_byte = c / 8;
		int row_bit = c % 8;
		int index = r * m_bytes_per_row + row_byte;
		return (m_bm[index] & (1<<row_bit)) != 0;
	}
	~simple_bm() {
		delete [] m_bm;
	};
	// TODO: rule of 3, rule of 5 - non-trival memory management in use.
};

void generate_init_animations(simple_bm *bms[], size_t num_bms);

#endif
