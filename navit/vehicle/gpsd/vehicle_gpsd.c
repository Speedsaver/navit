/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#define MODULE vehicle_gpsd

#include <gps.h>
#include <string.h>
#include <glib.h>
#include <math.h>
#include <time.h>
#include "debug.h"
#include "callback.h"
#include "plugin.h"
#include "coord.h"
#include "item.h"
#include "vehicle.h"
#include "event.h"

static struct vehicle_priv {
	char *source;
	char *gpsd_query;
	struct callback_list *cbl;
	struct callback *cb, *cbt;
	struct event_watch *evwatch;
	guint retry_interval;
	struct gps_data_t *gps;
	struct coord_geo geo;
	double speed;
	double direction;
	double height;
	double hdop;
	int status;
	int fix_type;
	time_t fix_time;
	time_t update_time;
	int sats;
	int sats_signal;
	int sats_used;
	char *nmea_data;
	char *nmea_data_buf;
	struct event_timeout *retry_timer2;
	struct attr ** attrs;
	char fixiso8601[128];
} *vehicle_last;

#define DEFAULT_RETRY_INTERVAL 5 // seconds
#define MIN_RETRY_INTERVAL 1 // seconds

static void vehicle_gpsd_io(struct vehicle_priv *priv);

static void
vehicle_gpsd_callback(struct gps_data_t *data, const char *buf, size_t len)
{
    char *pos,*nmea_data_buf;
    int i=0,sats_signal=0;

    struct vehicle_priv *priv = vehicle_last;
    if( len > 0 && buf[0] == '$' ) {
        char buffer[len+2];
        buffer[len+1]='\0';
        memcpy(buffer, buf, len);
        pos=strchr(buffer,'\n');
        if(!pos) {
            pos=strchr(buffer,'\r');
        }
        if (pos) {
            *pos  ='\n';
            *++pos='\0';
            if (!priv->nmea_data_buf || strlen(priv->nmea_data_buf) < 65536) {
                nmea_data_buf=g_strconcat(priv->nmea_data_buf ? priv->nmea_data_buf : "", buffer, NULL);
                g_free(priv->nmea_data_buf);
                priv->nmea_data_buf=nmea_data_buf;
            } else {
                dbg(lvl_error, "nmea buffer overflow, discarding '%s'\n", buffer);
            }
        }
    }

    dbg(lvl_debug,"data->set=%llx\n", (unsigned long long)data->set);
    if (data->set & SPEED_SET) {
        priv->speed = data->fix.speed * MPS_TO_KPH;
        if(!isnan(data->fix.speed))
            callback_list_call_attr_0(priv->cbl, attr_position_speed);
        data->set &= ~SPEED_SET;
    }

    if (data->set & TRACK_SET) {
        priv->direction = data->fix.track;
        data->set &= ~TRACK_SET;
    }
    if (data->set & ALTITUDE_SET) {
        priv->height = data->fix.altitude;
        data->set &= ~ALTITUDE_SET;
    }
    if (data->set & SATELLITE_SET) {
// We cannot rely on GPSD_API_MAJOR_VERSION here because it was not
// incremented for this change :-(.
        if(data->satellites_visible > 0) {
            sats_signal=0;
            for( i=0; i<data->satellites_visible; i++) {
                if (data->skyview[i].ss > 0)
                    sats_signal++;
            }
        }
        if (priv->sats_used != data->satellites_used || priv->sats != data->satellites_visible
                || priv->sats_signal != sats_signal ) {
            priv->sats_used = data->satellites_used;
            priv->sats = data->satellites_visible;
            priv->sats_signal = sats_signal;
            callback_list_call_attr_0(priv->cbl, attr_position_sats);
        }
        data->set &= ~SATELLITE_SET;
    }
    if (data->set & STATUS_SET) {
#if GPSD_API_MAJOR_VERSION <= 9
        priv->status = data->status;
#else
        priv->status = data->fix.status;
#endif
        data->set &= ~STATUS_SET;
    }
    if (data->set & MODE_SET) {
        priv->fix_type = data->fix.mode - 1;
        data->set &= ~MODE_SET;
    }
    if (data->set & TIME_SET) {
#if GPSD_API_MAJOR_VERSION >= 9
        priv->fix_time = data->fix.time.tv_sec;
#else
        priv->fix_time = data->fix.time;
#endif
        data->set &= ~TIME_SET;
    }
    if (data->set & DOP_SET) {
        dbg(lvl_debug, "pdop : %g\n", data->dop.pdop);
        priv->hdop = data->dop.pdop;
        data->set &= ~DOP_SET;
    }
    if (data->set & LATLON_SET) {
        priv->geo.lat = data->fix.latitude;
        priv->geo.lng = data->fix.longitude;
        dbg(lvl_debug,"lat=%f lng=%f\n", priv->geo.lat, priv->geo.lng);
        g_free(priv->nmea_data);
        priv->nmea_data=priv->nmea_data_buf;
        priv->nmea_data_buf=NULL;
        data->set &= ~LATLON_SET;
    }
    // If data->fix.speed is NAN, then the drawing gets jumpy.
    if (! isnan(data->fix.speed) && priv->fix_type > 0) {
        callback_list_call_attr_0(priv->cbl, attr_position_coord_geo);
    }
    priv->update_time = time(0);
    dbg(lvl_info,"speed ok\n");
}

/**
 * Attempt to open the gps device.
 * Return FALSE if retry not required
 * Return TRUE to try again
 */
static int vehicle_gpsd_try_open(struct vehicle_priv *priv) {
    char *source = g_strdup(priv->source);
    char *colon = index(source + 7, ':');
    char *port=NULL;
    if (colon) {
        *colon = '\0';
        port=colon+1;
    }
    dbg(lvl_debug,"Trying to connect to %s:%s\n",source+7,port?port:"default");

    /* gps_open returns 0 on success */
    if (gps_open(source + 7, port, priv->gps)) {
        dbg(lvl_error,"gps_open failed for '%s'. Retrying in %d seconds. Have you started gpsd?\n", priv->source,
            priv->retry_interval);
        g_free(source);
        return TRUE;
    }
    g_free(source);

    if (strchr(priv->gpsd_query,'r'))
        gps_stream(priv->gps, WATCH_ENABLE|WATCH_NMEA|WATCH_JSON, NULL);
    else
        gps_stream(priv->gps, WATCH_ENABLE|WATCH_JSON, NULL);

    priv->cb = callback_new_1(callback_cast(vehicle_gpsd_io), priv);
    priv->evwatch = event_add_watch(priv->gps->gps_fd, event_watch_cond_read, priv->cb);
    if (!priv->gps->gps_fd) {
        dbg(lvl_error,"Warning: gps_fd is 0, most likely you have used a gps.h incompatible to libgps\n");
    }
    dbg(lvl_debug,"Connected to gpsd fd=%d evwatch=%p\n", priv->gps->gps_fd, priv->evwatch);
    event_remove_timeout(priv->retry_timer2);
    priv->retry_timer2=NULL;
    return FALSE;
}

/**
 * Open a connection to gpsd. Will re-try the connection if it fails
 */
static void vehicle_gpsd_open(struct vehicle_priv *priv) {
    priv->gps = g_new0(struct gps_data_t, 1);
    priv->cbt = callback_new_1(callback_cast(vehicle_gpsd_try_open), priv);
    priv->retry_timer2=event_add_timeout(priv->retry_interval*1000, 1, priv->cbt);
    vehicle_gpsd_try_open(priv);
}

static void vehicle_gpsd_close(struct vehicle_priv *priv) {
    if (priv->retry_timer2) {
        event_remove_timeout(priv->retry_timer2);
        priv->retry_timer2=NULL;
    }
    if (priv->evwatch) {
        event_remove_watch(priv->evwatch);
        priv->evwatch = NULL;
    }
    if (priv->cb) {
        callback_destroy(priv->cb);
        priv->cb = NULL;
    }
    if (priv->cbt) {
        callback_destroy(priv->cbt);
        priv->cbt = NULL;
    }
    if (priv->gps) {
        gps_close(priv->gps);
        g_free(priv->gps);
        priv->gps = NULL;
    }
}

static void vehicle_gpsd_io(struct vehicle_priv *priv) {
    dbg(lvl_debug, "enter\n");
    if (priv->gps) {
        vehicle_last = priv;
        int read_result;
        /* Read until EOF, in case we are lagging behind.
         * No point in processing old GPS reports. */
#if GPSD_API_MAJOR_VERSION >= 7
        int numMessages = 0;
        while((read_result=gps_read(priv->gps, NULL, 0))>0) numMessages++;
        if (numMessages>1)
            dbg(lvl_debug,"Skipped %d messages\n",numMessages-1);
        dbg(lvl_info,"Mask=%llx\n",priv->gps->set);
        dbg(lvl_info,"Num Devices=%d\n",priv->gps->devices.ndevices);
        dbg(lvl_info,"Dev:path=%s,drv=%s,st=%s,st1=%s;\n",
            priv->gps->dev.path,priv->gps->dev.driver,priv->gps->dev.subtype,priv->gps->dev.subtype1);
#else
        while((read_result=gps_read(priv->gps))>0);
#endif
        if(read_result==-1) {
            dbg(lvl_error,"gps_poll failed\n");
            vehicle_gpsd_close(priv);
            vehicle_gpsd_open(priv);
        } else {
            const gps_mask_t useful_set = TIME_SET | LATLON_SET | SPEED_SET;
            if((priv->gps->set & useful_set) == useful_set) {
                const char *buf;
                buf = gps_data(priv->gps);
                vehicle_gpsd_callback(priv->gps,buf,strlen(buf));
            } else {
                dbg(lvl_info,"Skipped low quality GPS message\n");
            }
        }
    }
}

static void vehicle_gpsd_destroy(struct vehicle_priv *priv) {
    vehicle_gpsd_close(priv);
    if (priv->source)
        g_free(priv->source);
    if (priv->gpsd_query)
        g_free(priv->gpsd_query);
    g_free(priv->gps);
    g_free(priv);
}

static int vehicle_gpsd_position_attr_get(struct vehicle_priv *priv,
        enum attr_type type, struct attr *attr) {
    struct attr * active=NULL;
    switch (type) {
    case attr_position_fix_type:
        attr->u.num = priv->fix_type;
        break;
    case attr_position_height:
        attr->u.numd = &priv->height;
        break;
    case attr_position_speed:
        attr->u.numd = &priv->speed;
        break;
    case attr_position_direction:
        attr->u.numd = &priv->direction;
        break;
    case attr_position_hdop:
        attr->u.numd = &priv->hdop;
        break;
    case attr_position_qual:
        attr->u.num = priv->sats;
        break;
    case attr_position_sats_signal:
        attr->u.num = priv->sats_signal;
        break;
    case attr_position_sats_used:
        attr->u.num = priv->sats_used;
        break;
    case attr_position_coord_geo:
        attr->u.coord_geo = &priv->geo;
        break;
    case attr_position_nmea:
        attr->u.str=priv->nmea_data;
        if (! attr->u.str)
            return 0;
        break;
    case attr_position_time_iso8601: {
        struct tm tm;
        if (!priv->fix_time)
            return 0;
        if (gmtime_r(&priv->fix_time, &tm)) {
            strftime(priv->fixiso8601, sizeof(priv->fixiso8601),
                     "%Y-%m-%dT%TZ", &tm);
            attr->u.str=priv->fixiso8601;
        } else
            return 0;
    }
    break;
    case attr_position_last_update:
        attr->u.num = priv->update_time;
        break;
    case attr_active:
        active = attr_search(priv->attrs,NULL,attr_active);
        if(active != NULL) {
            attr->u.num=active->u.num;
            return 1;
        } else
            return 0;
        break;
    default:
        return 0;
    }
    attr->type = type;
    return 1;
}

static struct vehicle_methods vehicle_gpsd_methods = {
    vehicle_gpsd_destroy,
    vehicle_gpsd_position_attr_get,
};

static struct vehicle_priv *vehicle_gpsd_new_gpsd(struct vehicle_methods
        *meth, struct callback_list
        *cbl, struct attr **attrs) {
    struct vehicle_priv *ret;
    struct attr *source, *query, *retry_int;

    dbg(lvl_debug, "enter\n");
    source = attr_search(attrs, NULL, attr_source);
    ret = g_new0(struct vehicle_priv, 1);
    ret->source = g_strdup(source->u.str);
    query = attr_search(attrs, NULL, attr_gpsd_query);
    if (query) {
        ret->gpsd_query = g_strconcat(query->u.str, "\n", NULL);
    } else {
        ret->gpsd_query = g_strdup("w+x\n");
    }
    dbg(lvl_debug,"Format string for gpsd_query: %s\n",ret->gpsd_query);
    retry_int = attr_search(attrs, NULL, attr_retry_interval);
    if (retry_int) {
        ret->retry_interval = retry_int->u.num;
        if (ret->retry_interval < MIN_RETRY_INTERVAL) {
            dbg(lvl_error, "Retry interval %d too small, setting to %d\n", ret->retry_interval, MIN_RETRY_INTERVAL);
            ret->retry_interval = MIN_RETRY_INTERVAL;
        }
    } else {
        dbg(lvl_debug, "Retry interval not defined, setting to %d\n", DEFAULT_RETRY_INTERVAL);
        ret->retry_interval = DEFAULT_RETRY_INTERVAL;
    }
    ret->cbl = cbl;
    *meth = vehicle_gpsd_methods;
    ret->attrs = attrs;
    vehicle_gpsd_open(ret);
    return ret;
}

void plugin_init(void) {
    dbg(lvl_debug, "enter\n");
    plugin_register_category_vehicle("gpsd", vehicle_gpsd_new_gpsd);
    char *bug = getenv("GPSD_DEBUG_LEVEL");
    if ( bug )
        debug_level_set(dbg_module,(dbg_level)(*bug-'0'));
}
