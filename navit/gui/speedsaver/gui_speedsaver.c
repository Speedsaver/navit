#define MODULE gui_speedsaver

#include <glib.h>
#include "item.h" /* needs to be first, as attr.h depends on it */
#include "attr.h"
#include "plugin.h"

#include "debug.h"

#include "gui.h"

struct gui_priv {
    /* navit internal handle */
    struct navit* nav;
    /* gui handle */
    struct gui* gui;

    /* attributes given to us */
    struct attr attributes;

    /* list of callbacks to navit */
    struct callback_list* callbacks;
    /* current graphics */
    struct graphics* gra;
};

static int
gui_speedsaver_set_graphics(struct gui_priv* gui_priv, struct graphics* gra)
{
    struct transformation* trans;
    dbg(lvl_debug, "enter\n");

    navit_draw(gui_priv->nav);
    return 0;
}

struct gui_methods gui_speedsaver_methods = {
    NULL,
    NULL,
    gui_speedsaver_set_graphics,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL, //gui_qt5_qml_get_attr,
    NULL,
    NULL, // gui_qt5_qml_set_attr,
};


static struct gui_priv*
gui_speedsaver_new(struct navit* nav, struct gui_methods* meth, struct attr** attrs, struct gui* gui)
{
    struct gui_priv* gui_priv;
    dbg(lvl_debug,"initializing gui\n");

    /* tell navit our methods */
    *meth = gui_speedsaver_methods;

    /* allocate gui private structure */
    gui_priv = g_new0(struct gui_priv, 1);

    /* remember navit internal handle */
    gui_priv->nav = nav;
    /* remember our gui handle */
    gui_priv->gui = gui;

    /* remember the attributes given to us */
    gui_priv->attributes.type = attr_gui;
    gui_priv->attributes.u.gui = gui;

    /* create new callbacks */
    gui_priv->callbacks = callback_list_new();

    /* return self */
    return gui_priv;
}


void plugin_init(void)
{
    plugin_register_category_gui("speedsaver", gui_speedsaver_new);
}

