#include <glib.h>
#include "plugin.h"

// #include "ArduiPi_OLED_lib.h"
// #include "Adafruit_GFX.h"
// #include "ArduiPi_OLED.h"
// 
// ArduiPi_OLED display;
// 
extern "C" {

#include "item.h"
#include "callback.h"
#include "debug.h"
#include "event.h"
#include "point.h"
#include "graphics.h"
}

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

extern char *version;


struct graphics_priv {
    int width;
    int height;
    enum draw_mode_num mode;
    struct callback_list *cbl;
};

static gboolean graphics_ssd1306_idle(void *data)
{
    dbg(lvl_debug, "idle\n");
    g_timeout_add(10, graphics_ssd1306_idle, data);
    return TRUE;
}

static void
draw_mode(struct graphics_priv *gr, enum draw_mode_num mode)
{
    if (mode == draw_mode_begin){
	dbg(lvl_debug,"draw_mode_begin\n");
    } else if (mode == draw_mode_end) {
	dbg(lvl_debug,"draw_mode_end\n"); 
    } else {
    	dbg(lvl_error,"draw mode unknown\n");
    }
}

static struct graphics_methods graphics_methods = {
    NULL, //graphics_destroy,
    draw_mode,
    NULL, //draw_lines,
    NULL, //draw_polygon,
    NULL, //draw_rectangle,
    NULL,
    NULL, //draw_text,
    NULL, //draw_image,
    NULL,
    NULL, //draw_drag,
    NULL,
    NULL, //gc_new,
    NULL, //background_gc,
    NULL, //overlay_new,
    NULL, //image_new,
    NULL, //get_data,
    NULL, //image_free,
    NULL,
    NULL, //overlay_disable,
    NULL, //overlay_resize,
    NULL, /* set_attr, */
    NULL, /* show_native_keyboard */
    NULL, /* hide_native_keyboard */
};

static struct graphics_priv *
graphics_ssd1306_new(struct navit *nav, struct graphics_methods *meth,
            struct attr **attrs, struct callback_list *cbl)
{
    struct attr *attr;
    if (!event_request_system("glib", "graphics_opengl_new"))
        return NULL;
    struct graphics_priv *this_ = g_new0(struct graphics_priv, 1);
    *meth = graphics_methods;

    this_->cbl = cbl;

    this_->width = SCREEN_WIDTH;
    if ((attr = attr_search(attrs, NULL, attr_w)))
        this_->width = attr->u.num;
    this_->height = SCREEN_HEIGHT;
    if ((attr = attr_search(attrs, NULL, attr_h)))
        this_->height = attr->u.num;

    g_timeout_add(10, graphics_ssd1306_idle, this_);

//    if (!display.init (OLED_I2C_RESET, 2))
//        exit (-1);
//    
//    display.begin ();
//    display.clearDisplay ();
//    
//    display.setTextSize (1);
//    display.setTextColor (WHITE);
//    display.setCursor (0, 0);
//    display.print ("Navit\n");
//    // display.setTextColor(BLACK, WHITE); // 'inverted' text
//    display.printf (version);
//    display.setTextSize (2);
//    display.setTextColor (WHITE);
//    display.display ();

    dbg(lvl_debug, "initialized\n");
    return this_;
}

void
plugin_init(void)
{
    plugin_register_category_graphics("ssd1306", graphics_ssd1306_new);
}

