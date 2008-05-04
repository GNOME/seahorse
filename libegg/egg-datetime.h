/*
 * Copyright (C) 2002, 2003  Sebastian Rittau <srittau@jroger.in-berlin.de>
 * $Id$
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __EGG_DATETIME_H__
#define __EGG_DATETIME_H__

#include <time.h>

#include <glib.h>

#include <gtk/gtkhbox.h>

G_BEGIN_DECLS

typedef enum
{
   /* don't use the following values for now... */
   EGG_DATETIME_DISPLAY_YEAR       = 1 << 0,
   EGG_DATETIME_DISPLAY_MONTH      = 1 << 1,
   EGG_DATETIME_DISPLAY_DAY        = 1 << 2,
   EGG_DATETIME_DISPLAY_HOUR       = 1 << 3,
   EGG_DATETIME_DISPLAY_MINUTE     = 1 << 4,
   EGG_DATETIME_DISPLAY_SECOND     = 1 << 5,
   EGG_DATETIME_DISPLAY_YEAR_OPT   = 1 << 6,
   EGG_DATETIME_DISPLAY_MONTH_OPT  = 1 << 7,
   EGG_DATETIME_DISPLAY_DAY_OPT    = 1 << 8,
   EGG_DATETIME_DISPLAY_HOUR_OPT   = 1 << 9,
   EGG_DATETIME_DISPLAY_MINUTE_OPT = 1 << 10,
   EGG_DATETIME_DISPLAY_SECOND_OPT = 1 << 11
} EggDateTimeDisplayMode;

/* ... use these instead */
#define EGG_DATETIME_DISPLAY_DATE (EGG_DATETIME_DISPLAY_YEAR | EGG_DATETIME_DISPLAY_MONTH | EGG_DATETIME_DISPLAY_DAY)
#define EGG_DATETIME_DISPLAY_TIME (EGG_DATETIME_DISPLAY_HOUR | EGG_DATETIME_DISPLAY_MINUTE)
#define EGG_DATETIME_DISPLAY_TIME_SECONDS (EGG_DATETIME_DISPLAY_HOUR | EGG_DATETIME_DISPLAY_MINUTE | EGG_DATETIME_DISPLAY_SECOND)

#define EGG_DATETIME_INVALID_DATE (0)
#define EGG_DATETIME_INVALID_TIME (0xff)


#define EGG_TYPE_DATETIME     (egg_datetime_get_type ())
#define EGG_DATETIME(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), EGG_TYPE_DATETIME, EggDateTime))
#define EGG_DATETIME_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), EGG_TYPE_DATETIME, EggDateTimeClass))
#define EGG_IS_DATETIME(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EGG_TYPE_DATETIME))
#define EGG_IS_DATETIME_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EGG_TYPE_DATETIME))
#define EGG_DATETIME_GET_CLASS(obj) (G_TYPE_CHECK_GET_CLASS ((obj), EGG_TYPE_DATETIME, EggDateTimeClass))

typedef struct _EggDateTime      EggDateTime;
typedef struct _EggDateTimeClass EggDateTimeClass;
typedef struct _EggDateTimePrivate  EggDateTimePrivate;

struct _EggDateTime
{
   GtkHBox parent;

   EggDateTimePrivate *priv;
};

struct _EggDateTimeClass
{
   GtkHBoxClass parent_class;

   /* Signals */

   void (*date_changed) (EggDateTime *edt);
   void (*time_changed) (EggDateTime *edt);
};


/* Constructors */
GtkType         egg_datetime_get_type        (void);
GtkWidget      *egg_datetime_new       (void); 
GtkWidget      *egg_datetime_new_from_time_t    (time_t t);
GtkWidget      *egg_datetime_new_from_struct_tm (struct tm *tm);
GtkWidget      *egg_datetime_new_from_gdate     (GDate *date);
GtkWidget      *egg_datetime_new_from_datetime     (GDateYear year, GDateMonth month, GDateDay day, guint8 hour, guint8 minute, guint8 second);

/* Accessors */
void         egg_datetime_set_none        (EggDateTime *edt);
void         egg_datetime_set_from_time_t    (EggDateTime *edt, time_t t);
gboolean     egg_datetime_get_as_time_t      (EggDateTime *edt, time_t *t);
void         egg_datetime_set_from_struct_tm (EggDateTime *edt, struct tm *tm);
gboolean     egg_datetime_get_as_struct_tm      (EggDateTime *edt, struct tm *tm);
void         egg_datetime_set_from_gdate     (EggDateTime *edt, GDate *date);
gboolean     egg_datetime_get_as_gdate    (EggDateTime *edt, GDate *date);
void         egg_datetime_set_date        (EggDateTime *edt, GDateYear year, GDateMonth month, GDateDay day);
gboolean     egg_datetime_get_date        (EggDateTime *edt, GDateYear *year, GDateMonth *month, GDateDay *day);
void         egg_datetime_set_time        (EggDateTime *edt, guint8 hour, guint8 minute, guint8 second);
gboolean     egg_datetime_get_time        (EggDateTime *edt, guint8 *hour, guint8 *minute, guint8 *second);

void         egg_datetime_set_lazy        (EggDateTime *edt, gboolean lazy);
gboolean     egg_datetime_get_lazy        (EggDateTime *edt);
void         egg_datetime_set_display_mode      (EggDateTime *edt, EggDateTimeDisplayMode mode);
EggDateTimeDisplayMode   egg_datetime_get_display_mode      (EggDateTime *edt);

void         egg_datetime_set_clamp_date     (EggDateTime *edt, GDateYear minyear, GDateMonth minmonth, GDateDay minday, GDateYear maxyear, GDateMonth maxmonth, GDateDay maxday);
void         egg_datetime_set_clamp_time     (EggDateTime *edt, guint8 minhour, guint8 minminute, guint8 minsecond, guint8 maxhour, guint8 maxminute, guint8 maxsecond);
void         egg_datetime_set_clamp_time_t      (EggDateTime *edt);
void         egg_datetime_get_clamp_date     (EggDateTime *edt, GDateYear *minyear, GDateMonth *minmonth, GDateDay *minday, GDateYear *maxyear, GDateMonth *maxmonth, GDateDay *maxday);
void         egg_datetime_get_clamp_time     (EggDateTime *edt, guint8 *minhour, guint8 *minminute, guint8 *minsecond, guint8 *maxhour, guint8 *maxminute, guint8 *maxsecond);

PangoLayout    *egg_datetime_get_date_layout    (EggDateTime *edt);
PangoLayout    *egg_datetime_get_time_layout    (EggDateTime *edt);

G_END_DECLS

#endif /* __EGG_DATETIME_H__ */
