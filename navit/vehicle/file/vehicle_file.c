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

#include "config.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <glib.h>
#include <sys/stat.h>
#include <termios.h>
#include <math.h>
#include "debug.h"
#include "callback.h"
#include "plugin.h"
#include "coord.h"
#include "item.h"
#include "event.h"
#include "vehicle.h"
#include "file.h"
#ifdef HAVE_SOCKET
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

static void vehicle_file_disable_watch(struct vehicle_priv *priv);
static void vehicle_file_enable_watch(struct vehicle_priv *priv);
static int vehicle_file_parse(struct vehicle_priv *priv, char *buffer);
static int vehicle_file_open(struct vehicle_priv *priv);
static void vehicle_file_close(struct vehicle_priv *priv);


enum file_type {
	file_type_pipe = 1, file_type_device, file_type_file, file_type_socket, file_type_serial
};

static int buffer_size = 1024;

struct gps_sat {
	int prn;
	int elevation;
	int azimuth;
	int snr;
};


struct vehicle_priv {
	char *source;
	struct callback_list *cbl;
	int fd;
	struct callback *cb,*cbt;
	char *buffer;
	int buffer_pos;
	char *nmea_data;
	char *nmea_data_buf;

	struct coord_geo geo;
	double speed;
	double direction;
	double height;
	double hdop;
	double vdop;
	char fixtime[20];
	int fixyear;
	int fixmonth;
	int fixday;
	int status;
	int sats_used;
	int sats_visible;
	int sats_signal;
	int time;
	int on_eof;
	enum file_type file_type;
	FILE *file;
	struct event_watch *watch;
	speed_t baudrate;
	struct attr ** attrs;
	char fixiso8601[128];
	int checksum_ignore;
	int magnetic_direction;
	int current_count;
	struct gps_sat current[24];
	int next_count;
	struct gps_sat next[24];
	struct item sat_item;
	int valid;
	char *statefile;
	int process_statefile;
};

//***************************************************************************
/** @fn static int vehicle_file_open(struct vehicle_priv *priv)
*****************************************************************************
* @b Description: open dialogue with the GPS
*****************************************************************************
* @param      priv : pointer on the private data of the plugin
*****************************************************************************
* @return     1 if ok
*             0 if error
*****************************************************************************
**/
static int
vehicle_file_open(struct vehicle_priv *priv)
{
	char *name;
	struct termios tio;

	name = priv->source + 5;
	if (!strncmp(priv->source, "file:", 5)) {
		priv->fd = open(name, O_RDONLY | O_NDELAY);
		if (priv->fd < 0)
			return 0;
		if (file_is_reg(name)) {
			priv->file_type = file_type_file;
		}
		else {
			tcgetattr(priv->fd, &tio);
			cfmakeraw(&tio);
			cfsetispeed(&tio, priv->baudrate);
			cfsetospeed(&tio, priv->baudrate);
			tio.c_cc[VMIN] = 0;
			tio.c_cc[VTIME] = 200;
			tcsetattr(priv->fd, TCSANOW, &tio);
			priv->file_type = file_type_device;
		}
	} else if (!strncmp(priv->source,"pipe:", 5)) {
		priv->file = popen(name, "r");
		if (!priv->file)
			return 0;
		priv->fd = fileno(priv->file);
		priv->file_type = file_type_pipe;
#if defined(HAVE_SOCKET)
	} else if (!strncmp(priv->source,"socket:", 7)) {
		char *p,*s=g_strdup(priv->source+7);
		struct sockaddr_in sin;
		p=strchr(s,':');
		if (!p) {
			dbg(lvl_error,"port number missing in %s\n",s);
			g_free(s);
			return 0;
		}
		*p++='\0';
		sin.sin_family=AF_INET;
		sin.sin_port=ntohs(atoi(p));
		if (!inet_aton(s, &sin.sin_addr)) {
			dbg(lvl_error,"failed to parse %s\n",s);
			g_free(s);
			return 0;
		}
		priv->fd = socket(PF_INET, SOCK_STREAM, 0);
		if (priv->fd != -1) {
			if (connect(priv->fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
				dbg(lvl_error,"failed to connect to %s:%s\n",s,p);
				g_free(s);
				return 0;
			}
		}
		p=strchr(p,':');
		if (p) {
			p++;
			write(priv->fd, p, strlen(p));
		}
		priv->file_type = file_type_socket;
#endif //HAVE_SOCKET
	} else if (!strncmp(priv->source,"serial:",7)) {
		//TODO - add linux serial
    }
    return(priv->fd != -1);
}

//***************************************************************************
/** @fn static void vehicle_file_close(struct vehicle_priv *priv)
*****************************************************************************
* @b Description: close dialogue with the GPS
*****************************************************************************
* @param      priv : pointer on the private data of the plugin
*****************************************************************************
**/
static void
vehicle_file_close(struct vehicle_priv *priv)
{
    dbg(lvl_debug, "enter, priv->fd='%d'\n", priv->fd);
	vehicle_file_disable_watch(priv);
    {
	if (priv->file) {
		pclose(priv->file);
    }
	else if (priv->fd >= 0) {
		close(priv->fd);
    }
	priv->file = NULL;
	priv->fd = -1;
    }
}

//***************************************************************************
/** @fn static int vehicle_file_enable_watch_timer(struct vehicle_priv *priv)
*****************************************************************************
* @b Description: Enable watch timer to get GPS data
*****************************************************************************
* @param      priv : pointer on the private data of the plugin
*****************************************************************************
* @return     always 0
*****************************************************************************
**/
static int
vehicle_file_enable_watch_timer(struct vehicle_priv *priv)
{
	dbg(lvl_debug, "enter\n");
	vehicle_file_enable_watch(priv);

	return FALSE;
}


//***************************************************************************
/** @fn static int vehicle_file_parse( struct vehicle_priv *priv,
*                                      char *buffer)
*****************************************************************************
* @b Description: Parse the buffer
*****************************************************************************
* @param      priv : pointer on the private data of the plugin
* @param      buffer : data buffer (null terminated)
*****************************************************************************
* @return     1 if The GPRMC Sentence is found
*             0 if not found
*****************************************************************************
**/
static int
vehicle_file_parse(struct vehicle_priv *priv, char *buffer)
{
	char *nmea_data_buf, *p, *item[32];
	double lat, lng;
	int i, j, bcsum;
	int len = strlen(buffer);
	unsigned char csum = 0;
	int valid=0;
	int ret = 0;

	dbg(lvl_info, "enter: buffer='%s'\n", buffer);
	for (;;) {
		if (len < 4) {
			dbg(lvl_error, "'%s' too short\n", buffer);
			return ret;
		}
		if (buffer[len - 1] == '\r' || buffer[len - 1] == '\n') {
			buffer[--len] = '\0';
            if (buffer[len - 1] == '\r')
                buffer[--len] = '\0';
        } else
			break;
	}
	if (buffer[0] != '$') {
		dbg(lvl_error, "no leading $ in '%s'\n", buffer);
		return ret;
	}
	if (buffer[len - 3] != '*') {
		dbg(lvl_error, "no *XX in '%s'\n", buffer);
		return ret;
	}
	for (i = 1; i < len - 3; i++) {
		csum ^= (unsigned char) (buffer[i]);
	}
	if (!sscanf(buffer + len - 2, "%x", &bcsum) && priv->checksum_ignore != 2) {
		dbg(lvl_error, "no checksum in '%s'\n", buffer);
		return ret;
	}
	if (bcsum != csum && priv->checksum_ignore == 0) {
		dbg(lvl_error, "wrong checksum in '%s was %x should be %x'\n", buffer,bcsum,csum);
		return ret;
	}

	if (!priv->nmea_data_buf || strlen(priv->nmea_data_buf) < 65536) {
		nmea_data_buf=g_strconcat(priv->nmea_data_buf ? priv->nmea_data_buf : "", buffer, "\n", NULL);
		g_free(priv->nmea_data_buf);
		priv->nmea_data_buf=nmea_data_buf;
	} else {
		dbg(lvl_error, "nmea buffer overflow (len %zu), discarding '%s'\n", priv->nmea_data_buf?strlen(priv->nmea_data_buf):-1,buffer);
	}
	i = 0;
	p = buffer;
	while (i < 31) {
		item[i++] = p;
		while (*p && *p != ',')
			p++;
		if (!*p)
			break;
		*p++ = '\0';
	}

	if (!strncmp(buffer, "$GPGGA", 6)) {
		/*                                                           1 1111
		   0      1          2         3 4          5 6 7  8   9     0 1234
		   $GPGGA,184424.505,4924.2811,N,01107.8846,E,1,05,2.5,408.6,M,,,,0000*0C
		   UTC of Fix[1],Latitude[2],N/S[3],Longitude[4],E/W[5],Quality(0=inv,1=gps,2=dgps)[6],Satelites used[7],
		   HDOP[8],Altitude[9],"M"[10],height of geoid[11], "M"[12], time since dgps update[13], dgps ref station [14]
		 */
		if (*item[2] && *item[3] && *item[4] && *item[5]) {
			lat = g_ascii_strtod(item[2], NULL);
			priv->geo.lat = floor(lat / 100);
			lat -= priv->geo.lat * 100;
			priv->geo.lat += lat / 60;

			if (!g_ascii_strcasecmp(item[3],"S"))
				priv->geo.lat=-priv->geo.lat;

			lng = g_ascii_strtod(item[4], NULL);
			priv->geo.lng = floor(lng / 100);
			lng -= priv->geo.lng * 100;
			priv->geo.lng += lng / 60;

			if (!g_ascii_strcasecmp(item[5],"W"))
				priv->geo.lng=-priv->geo.lng;
			priv->valid=attr_position_valid_valid;
            dbg(lvl_info, "latitude '%2.4f' longitude %2.4f\n", priv->geo.lat, priv->geo.lng);

		} else
			priv->valid=attr_position_valid_invalid;
		if (*item[6])
			sscanf(item[6], "%d", &priv->status);
		if (*item[7])
		sscanf(item[7], "%d", &priv->sats_used);
		if (*item[8])
			sscanf(item[8], "%lf", &priv->hdop);
		if (*item[1]) 
			strncpy(priv->fixtime, item[1], sizeof(priv->fixtime));
		if (*item[9])
			sscanf(item[9], "%lf", &priv->height);

		g_free(priv->nmea_data);
		priv->nmea_data=priv->nmea_data_buf;
		priv->nmea_data_buf=NULL;
		if (priv->file_type == file_type_file) {
			if (priv->watch) {
				vehicle_file_disable_watch(priv);
				event_add_timeout(priv->time, 0, priv->cbt);
			}
		}
	}
	if (!strncmp(buffer, "$GPVTG", 6)) {
		/* 0      1      2 34 5    6 7   8
		   $GPVTG,143.58,T,,M,0.26,N,0.5,K*6A
		   Course Over Ground Degrees True[1],"T"[2],Course Over Ground Degrees Magnetic[3],"M"[4],
		   Speed in Knots[5],"N"[6],"Speed in KM/H"[7],"K"[8]
		 */
		if (item[1] && item[7])
			valid = 1;
		if (i >= 10 && (*item[9] == 'A' || *item[9] == 'D'))
			valid = 1;
		if (valid) {
			priv->direction = g_ascii_strtod( item[1], NULL );
			priv->speed = g_ascii_strtod( item[7], NULL );
			dbg(lvl_info,"direction %lf, speed %2.1lf\n", priv->direction, priv->speed);
		}
	}
	if (!strncmp(buffer, "$GPRMC", 6)) {
		/*                                                           1     1
		   0      1      2 3        4 5         6 7     8     9      0     1
		   $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
		   Time[1],Active/Void[2],lat[3],N/S[4],long[5],W/E[6],speed in knots[7],track angle[8],date[9],
		   magnetic variation[10],magnetic variation direction[11]
		 */
		if (*item[2] == 'A')
			valid = 1;
		if (i >= 13 && (*item[12] == 'A' || *item[12] == 'D'))
			valid = 1;
		if (valid) {
			priv->direction = g_ascii_strtod( item[8], NULL );
			priv->speed = g_ascii_strtod( item[7], NULL );
			priv->speed *= 1.852;
			sscanf(item[9], "%02d%02d%02d",
				&priv->fixday,
				&priv->fixmonth,
				&priv->fixyear);
			priv->fixyear += 2000;
		}
		ret = 1;
	}
	if (!strncmp(buffer, "$GPGSV", 6) && i >= 4) {
	/*
		0 GSV	   Satellites in view
		1 2 	   Number of sentences for full data
		2 1 	   sentence 1 of 2
		3 08	   Number of satellites in view

		4 01	   Satellite PRN number
		5 40	   Elevation, degrees
		6 083	   Azimuth, degrees
		7 46	   SNR - higher is better
			   for up to 4 satellites per sentence
		*75	   the checksum data, always begins with *
	*/
		if (item[3]) {
			sscanf(item[3], "%d", &priv->sats_visible);
		}
		j=4;
		while (j+4 <= i && priv->current_count < 24) {
			struct gps_sat *sat=&priv->next[priv->next_count++];
			sat->prn=atoi(item[j]);
			sat->elevation=atoi(item[j+1]);
			sat->azimuth=atoi(item[j+2]);
			sat->snr=atoi(item[j+3]);
			j+=4;
		}
		if (!strcmp(item[1], item[2])) {
			priv->sats_signal=0;
			for (i = 0 ; i < priv->next_count ; i++) {
				priv->current[i]=priv->next[i];
				if (priv->current[i].snr)
					priv->sats_signal++;
			}
			priv->current_count=priv->next_count;
			priv->next_count=0;
		}
	}
	if (!strncmp(buffer, "$GPZDA", 6)) {
	/*
		0        1        2  3  4    5  6
		$GPZDA,hhmmss.ss,dd,mm,yyyy,xx,yy*CC
			hhmmss    HrMinSec(UTC)
			dd,mm,yyy Day,Month,Year
			xx        local zone hours -13..13
			yy        local zone minutes 0..59
	*/
		if (item[1] && item[2] && item[3] && item[4]) {
			strncpy(priv->fixtime, item[1], strlen(priv->fixtime));
			priv->fixday = atoi(item[2]);
			priv->fixmonth = atoi(item[3]);
			priv->fixyear = atoi(item[4]);
		}
	}
	if (!strncmp(buffer, "$IISMD", 6)) {
	/*
		0      1   2     3      4
		$IISMD,dir,press,height,temp*CC"
			dir 	  Direction (0-359)
			press	  Pressure (hpa, i.e. 1032)
			height    Barometric height above ground (meter)
			temp      Temperature (Degree Celsius)
	*/
		if (item[1]) {
			priv->magnetic_direction = g_ascii_strtod( item[1], NULL );
			dbg(lvl_debug,"magnetic %d\n", priv->magnetic_direction);
		}
	}
	return ret;
}

//***************************************************************************
/** @fn static void vehicle_file_io(struct vehicle_priv *priv)
*****************************************************************************
* @b Description: function to get data from GPS
*****************************************************************************
* @param      priv : pointer on the private data of the plugin
*****************************************************************************
* @remarks 
*****************************************************************************
**/
static void
vehicle_file_io(struct vehicle_priv *priv)
{
	int size, rc = 0;
	char *str, *tok;
    dbg(lvl_debug, "vehicle_file_io : enter\n");

	if (priv->process_statefile) {
		unsigned char *data;
		priv->process_statefile=0;
		if (file_get_contents(priv->statefile, &data, &size)) {
			if (size > buffer_size)
				size=buffer_size;
			memcpy(priv->buffer, data, size);
			priv->buffer_pos=0;
			g_free(data);
		} else
			return;
	} else {
		size = read(priv->fd, priv->buffer + priv->buffer_pos, buffer_size - priv->buffer_pos - 1);
	}
	if (size <= 0) {
		switch (priv->on_eof) {
		case 0:
			vehicle_file_close(priv);
			vehicle_file_open(priv);
			break;
		case 1:
			vehicle_file_disable_watch(priv);
			break;
		case 2:
			exit(0);
			break;
		}
		return;
	}
	priv->buffer_pos += size;
	priv->buffer[priv->buffer_pos] = '\0';
	dbg(lvl_debug, "size=%d pos=%d buffer='%s'\n", size,
	    priv->buffer_pos, priv->buffer);
	str = priv->buffer;
	while ((tok = strchr(str, '\n'))) {
		*tok++ = '\0';
		dbg(lvl_debug, "line='%s'\n", str);
		rc +=vehicle_file_parse(priv, str);
		str = tok;
		if (priv->file_type == file_type_file && rc)
			break;
	}

	if (str != priv->buffer) {
		size = priv->buffer + priv->buffer_pos - str;
		memmove(priv->buffer, str, size + 1);
		priv->buffer_pos = size;
		dbg(lvl_debug, "now pos=%d buffer='%s'\n",
		    priv->buffer_pos, priv->buffer);
	} else if (priv->buffer_pos == buffer_size - 1) {
		dbg(lvl_debug,
		    "Overflow. Most likely wrong baud rate or no nmea protocol\n");
		priv->buffer_pos = 0;
	}
	if (rc)
		callback_list_call_attr_0(priv->cbl, attr_position_coord_geo);
}

//***************************************************************************
/** @fn static void vehicle_file_enable_watch(struct vehicle_priv *priv)
*****************************************************************************
* @b Description: Enable watch
*****************************************************************************
* @param      priv : pointer on the private data of the plugin
*****************************************************************************
**/
static void
vehicle_file_enable_watch(struct vehicle_priv *priv)
{
	dbg(lvl_debug, "enter\n");
    {
	if (! priv->watch)
		priv->watch = event_add_watch(priv->fd, event_watch_cond_read, priv->cb);
    }
}

//***************************************************************************
/** @fn static void vehicle_file_disable_watch(struct vehicle_priv *priv)
*****************************************************************************
* @b Description: Disable watch
*****************************************************************************
* @param      priv : pointer on the private data of the plugin
*****************************************************************************
**/
static void
vehicle_file_disable_watch(struct vehicle_priv *priv)
{
	dbg(lvl_debug, "vehicle_file_disable_watch : enter\n");
    {
	if (priv->watch)
		event_remove_watch(priv->watch);
	priv->watch = NULL;
    }
}

//***************************************************************************
/** @fn static void vehicle_priv vehicle_file_destroy(struct vehicle_priv *priv)
*****************************************************************************
* @b Description: Function called to uninitialize the plugin
*****************************************************************************
* @param      priv : pointer on the private data of the plugin
*****************************************************************************
* @remarks private data is freed by this function (g_free)
*****************************************************************************
**/
static void
vehicle_file_destroy(struct vehicle_priv *priv)
{
	if (priv->statefile && priv->nmea_data) {
		struct attr readwrite={attr_readwrite};
		struct attr create={attr_create};
		struct attr *attrs[]={&readwrite,&create,NULL};
		struct file *f;
		readwrite.u.num=1;
		create.u.num=1;
		f=file_create(priv->statefile, attrs);
		if (f) {
			file_data_write(f, 0, strlen(priv->nmea_data), priv->nmea_data);
			file_fsync(f);
			file_destroy(f);
		}	
	}
	vehicle_file_close(priv);
	callback_destroy(priv->cb);
	callback_destroy(priv->cbt);
	if (priv->statefile)
		g_free(priv->statefile);
	if (priv->source)
		g_free(priv->source);
	if (priv->buffer)
		g_free(priv->buffer);
	g_free(priv);
}

//***************************************************************************
/** @fn static int vehicle_file_position_attr_get(struct vehicle_priv *priv,
*                                                 enum attr_type type,
*                                                 struct attr *attr)
*****************************************************************************
* @b Description: Function called to get attribute
*****************************************************************************
* @param      priv : pointer on the private data of the plugin
* @param      type : attribute type called
* @param      attr : structure to return the attribute value
*****************************************************************************
* @return     1 if ok
*             0 for unkown or invalid attribute
*****************************************************************************
**/
static int
vehicle_file_position_attr_get(struct vehicle_priv *priv,
			       enum attr_type type, struct attr *attr)
{
	switch (type) {
	case attr_position_fix_type:
		attr->u.num = priv->status;
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
	case attr_position_magnetic_direction:
		attr->u.num = priv->magnetic_direction;
		break;
	case attr_position_hdop:
		attr->u.numd = &priv->hdop;
		break;
	case attr_position_qual:
		attr->u.num = priv->sats_visible;
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
	case attr_position_time_iso8601:
		if (!priv->fixyear || !priv->fixtime[0])
			return 0;
		sprintf(priv->fixiso8601, "%04d-%02d-%02dT%.2s:%.2s:%sZ",
			priv->fixyear, priv->fixmonth, priv->fixday,
						priv->fixtime, (priv->fixtime+2), (priv->fixtime+4));
		attr->u.str=priv->fixiso8601;
		break;
	case attr_position_sat_item:
		dbg(lvl_debug,"at here\n");
		priv->sat_item.id_lo++;
		if (priv->sat_item.id_lo > priv->current_count) {
			priv->sat_item.id_lo=0;
			return 0;
		}
		attr->u.item=&priv->sat_item;
		break;
	case attr_position_valid:
		attr->u.num=priv->valid;
		break;
	default:
		return 0;
	}
	if (type != attr_position_sat_item)
		priv->sat_item.id_lo=0;
	attr->type = type;
	return 1;
}

//***************************************************************************
/** @fn static int vehicle_file_sat_attr_get(struct vehicle_priv *priv,
*                                                 enum attr_type type,
*                                                 struct attr *attr)
*****************************************************************************
* @b Description: Function called to get satellite attribute
*****************************************************************************
* @param      priv : pointer on the private data of the plugin
* @param      type : attribute type called
* @param      attr : structure to return the attribute value
*****************************************************************************
* @return     1 if ok
*             0 for unkown attribute
*****************************************************************************
**/
static int
vehicle_file_sat_attr_get(void *priv_data, enum attr_type type, struct attr *attr)
{
    struct gps_sat *sat;
	struct vehicle_priv *priv=priv_data;
	if (priv->sat_item.id_lo < 1)
		return 0;
	if (priv->sat_item.id_lo > priv->current_count)
		return 0;
	sat=&priv->current[priv->sat_item.id_lo-1];
	switch (type) {
	case attr_sat_prn:
		attr->u.num=sat->prn;
		break;
	case attr_sat_elevation:
		attr->u.num=sat->elevation;
		break;
	case attr_sat_azimuth:
		attr->u.num=sat->azimuth;
		break;
	case attr_sat_snr:
		attr->u.num=sat->snr;
		break;
	default:
		return 0;
	}
	attr->type = type;
	return 1;
}

static struct item_methods vehicle_file_sat_methods = {
	NULL,
	NULL,
	NULL,
	vehicle_file_sat_attr_get,
};

static struct vehicle_methods vehicle_file_methods = {
	vehicle_file_destroy,
	vehicle_file_position_attr_get,
};

//***************************************************************************
/** @fn static struct vehicle_priv * vehicle_file_new_file(
*                                       struct vehicle_methods *meth,
*                                       struct callback_list   *cbl,
*                                       struct attr            **attrs)
*****************************************************************************
* @b Description: Function called to initialize the plugin
*****************************************************************************
* @param      meth  : ?
* @param      cbl   : ?
* @param      attrs : ?
*****************************************************************************
* @return     pointer on the private data of the plugin
*****************************************************************************
* @remarks private data is allocated by this function (g_new0)
*****************************************************************************
**/
static struct vehicle_priv *
vehicle_file_new_file(struct vehicle_methods
		      *meth, struct callback_list
		      *cbl, struct attr **attrs)
{
	struct vehicle_priv *ret;
	struct attr *source;
	struct attr *time;
	struct attr *on_eof;
	struct attr *baudrate;
	struct attr *checksum_ignore;
	struct attr *state_file;

	dbg(lvl_debug, "enter\n");

	source = attr_search(attrs, NULL, attr_source);
	if(source == NULL){
		 dbg(lvl_error,"Missing source attribute\n");
		 return NULL;
    }
	ret = g_new0(struct vehicle_priv, 1);   // allocate and initialize to 0
	ret->fd = -1;
	ret->cbl = cbl;
	ret->source = g_strdup(source->u.str);
	ret->buffer = g_malloc(buffer_size);
	ret->time=1000;
	ret->baudrate=B4800;
	state_file=attr_search(attrs, NULL, attr_state_file);
	if (state_file) 
		ret->statefile=g_strdup(state_file->u.str);
	time = attr_search(attrs, NULL, attr_time);
	if (time)
		ret->time=time->u.num;
	baudrate = attr_search(attrs, NULL, attr_baudrate);
	if (baudrate) {
		switch (baudrate->u.num) {
		case 4800:
			ret->baudrate=B4800;
			break;
		case 9600:
			ret->baudrate=B9600;
			break;
		case 19200:
			ret->baudrate=B19200;
			break;
#ifdef B38400
		case 38400:
			ret->baudrate=B38400;
			break;
#endif
#ifdef B57600
		case 57600:
			ret->baudrate=B57600;
			break;
#endif
#ifdef B115200
		case 115200:
			ret->baudrate=B115200;
			break;
#endif
		}
	}
	checksum_ignore = attr_search(attrs, NULL, attr_checksum_ignore);
	if (checksum_ignore)
		ret->checksum_ignore=checksum_ignore->u.num;
	ret->attrs = attrs;
	on_eof = attr_search(attrs, NULL, attr_on_eof);
	if (on_eof && !g_ascii_strcasecmp(on_eof->u.str, "stop"))
		ret->on_eof=1;
	if (on_eof && !g_ascii_strcasecmp(on_eof->u.str, "exit"))
		ret->on_eof=2;
	dbg(lvl_debug,"on_eof=%d\n", ret->on_eof);
	*meth = vehicle_file_methods;
	ret->cb=callback_new_1(callback_cast(vehicle_file_io), ret);
	ret->cbt=callback_new_1(callback_cast(vehicle_file_enable_watch_timer), ret);
	if (ret->statefile && file_exists(ret->statefile)) {
		ret->process_statefile=1;
		event_add_timeout(1000, 0, ret->cb);
	}
	ret->sat_item.type=type_position_sat;
	ret->sat_item.id_hi=ret->sat_item.id_lo=0;
	ret->sat_item.priv_data=ret;
	ret->sat_item.meth=&vehicle_file_sat_methods;

	dbg(lvl_debug, "vehicle_file_new_file:open\n");
	if (!vehicle_file_open(ret)) {
        dbg(lvl_error, "Failed to open '%s'\n", ret->source);
	}

	vehicle_file_enable_watch(ret);
	// vehicle_file_destroy(ret);
	// return NULL;
	dbg(lvl_debug, "leave\n");
	return ret;
}

//***************************************************************************
/** @fn void plugin_init(void)
*****************************************************************************
* @b Description: Initialisation of vehicle_file plugin
*****************************************************************************
**/
void plugin_init(void)
{
	dbg(lvl_debug, "vehicle_file:plugin_init:enter\n");
	plugin_register_category_vehicle("file", vehicle_file_new_file);
	plugin_register_category_vehicle("pipe", vehicle_file_new_file);
	plugin_register_category_vehicle("socket", vehicle_file_new_file);
	plugin_register_category_vehicle("serial", vehicle_file_new_file);
}
