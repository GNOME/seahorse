/*
 * Copyright (C) 2002-2005  Sebastian Rittau <srittau@jroger.in-berlin.de>
 * $Id$
 *
 * Based on GnomeDateEdit by Miguel de Icaza.
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

#include <config.h>

#include <libintl.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkkeysyms-compat.h>

#include "egg-datetime.h"

#ifdef G_OS_WIN32
/* The gmtime() in Microsoft's C library *is* thread-safe. It has no gmtime_r(). */

static struct tm *
gmtime_r (const time_t *timep, struct tm *result)
{
  struct tm *tmp = gmtime (timep);

  if (tmp == NULL)
    return NULL;

  memcpy (result, tmp, sizeof (struct tm));

  return result;
}

/* Ditto for localtime() and localtime_r(). */

static struct tm *
localtime_r (const time_t *timep, struct tm *result)
{
  struct tm *tmp = localtime (timep);

  if (tmp == NULL)
    return NULL;

  memcpy (result, tmp, sizeof (struct tm));

  return result;
}

#endif

#ifndef _
#define _(x) (x)
#endif


/* TODO:
 *  o "now" button in calendar popup?
 *  o timezone support
 *  o Implement time list as a popup a la Evo, or time as a spin button?
 *  o In lazy mode: choose a different way to mark an invalid date instead
 *    of just blanking the widget.
 */

/* FIXME:
 *  o limit GtkCalendar to clamp times
 */


/* from libgnomeui */
static void
_add_atk_name_desc (GtkWidget *widget, gchar *name, gchar *desc)
{
   AtkObject *aobj;

   g_return_if_fail (GTK_IS_WIDGET (widget));

   aobj = gtk_widget_get_accessible (widget);

   if (name)
      atk_object_set_name (aobj, name);
   if (desc)
      atk_object_set_description (aobj, desc);
}

static void
_add_atk_relation (GtkWidget *widget1, GtkWidget *widget2,
         AtkRelationType w1_to_w2, AtkRelationType w2_to_w1)
{
   AtkObject      *atk_widget1;
   AtkObject      *atk_widget2;
   AtkRelationSet *relation_set;
   AtkRelation    *relation;
   AtkObject      *targets[1];

   atk_widget1 = gtk_widget_get_accessible (widget1);
   atk_widget2 = gtk_widget_get_accessible (widget2);

   /* Create the widget1 -> widget2 relation */
   relation_set = atk_object_ref_relation_set (atk_widget1);
   targets[0] = atk_widget2;
   relation = atk_relation_new (targets, 1, w1_to_w2);
   atk_relation_set_add (relation_set, relation);
   g_object_unref (relation);

   /* Create the widget2 -> widget1 relation */
   relation_set = atk_object_ref_relation_set (atk_widget2);
   targets[0] = atk_widget1;
   relation = atk_relation_new (targets, 1, w2_to_w1);
   atk_relation_set_add (relation_set, relation);
   g_object_unref (relation);
}

/*
 * Time List Declarations
 */

#define TIMELIST(x) GTK_SCROLLED_WINDOW(x)
#define Timelist GtkScrolledWindow

static GtkWidget *timelist_new (void);
static void timelist_set_list (Timelist *timelist, guint8 minhour, guint8 minminute, guint8 maxhour, guint8 maxminute);
static void timelist_set_time (Timelist *timelist, guint8 hour, guint8 minute);
static gboolean timelist_get_time (Timelist *timelist, guint8 *hour, guint8 *minute);
static void timelist_clamp (Timelist *timelist, guint8 minhour, guint8 minminute, guint8 maxhour, guint8 maxminute);
static void timelist_set_selection_callback (Timelist *timelist, void (*cb)(void), gpointer data);

/*
 * Class and Object Handling
 */

struct _EggDateTimePrivate
{
   /* Children */

   GtkWidget *date_box;
   GtkWidget *date_entry;
   GtkWidget *date_button;
   GtkWidget *time_box;
   GtkWidget *time_entry;
   GtkWidget *time_button;
   GtkWidget *cal_popup;
   GtkWidget *calendar;
   GtkWidget *time_popup;
   GtkWidget *timelist;

   /* Flags */

   EggDateTimeDisplayMode display_mode;
   gboolean lazy;
   gboolean week_start_monday;

   /* Current Date/Time */

   gboolean   date_valid;
   GDateYear  year;
   GDateMonth month;
   GDateDay   day;
   gboolean   time_valid;
   guint8     hour;
   guint8     minute;
   guint8     second;

   /* Clamp Values */

   guint16 clamp_minyear,   clamp_maxyear;
   guint8  clamp_minmonth,  clamp_maxmonth;
   guint8  clamp_minday,    clamp_maxday;
   guint8  clamp_minhour,   clamp_maxhour;
   guint8  clamp_minminute, clamp_maxminute;
   guint8  clamp_minsecond, clamp_maxsecond;
};

enum {
   SIGNAL_DATE_CHANGED,
   SIGNAL_TIME_CHANGED,
   SIGNAL_LAST
};

enum {
   ARG_DISPLAY_MODE = 1,
   ARG_LAZY,
   ARG_YEAR,
   ARG_MONTH,
   ARG_DAY,
   ARG_HOUR,
   ARG_MINUTE,
   ARG_SECOND,
   ARG_CLAMP_MINYEAR,
   ARG_CLAMP_MINMONTH,
   ARG_CLAMP_MINDAY,
   ARG_CLAMP_MINHOUR,
   ARG_CLAMP_MINMINUTE,
   ARG_CLAMP_MINSECOND,
   ARG_CLAMP_MAXYEAR,
   ARG_CLAMP_MAXMONTH,
   ARG_CLAMP_MAXDAY,
   ARG_CLAMP_MAXHOUR,
   ARG_CLAMP_MAXMINUTE,
   ARG_CLAMP_MAXSECOND
};


static gint egg_datetime_signals [SIGNAL_LAST] = { 0 };


static void  egg_datetime_class_init   (EggDateTimeClass *klass);
static void  egg_datetime_init      (EggDateTime      *edt);
static void  egg_datetime_set_property (GObject    *object,
                   guint          property_id,
                   const GValue     *value,
                   GParamSpec    *pspec);
static void  egg_datetime_get_property (GObject    *object,
                   guint          property_id,
                   GValue        *value,
                   GParamSpec    *pspec);

static void  egg_datetime_destroy      (GtkWidget *widget);

static void  egg_datetime_finalize     (GObject    *object);

static gchar   *get_time_string     (guint8 hour, guint8 minute, guint8 second);

static gboolean    date_focus_out         (EggDateTime *edt,   GtkEntry *entry);
static gboolean    time_focus_out         (EggDateTime *edt,   GtkEntry *entry);
static void  date_arrow_toggled     (EggDateTime *edt,   GtkToggleButton *button);
static void  time_arrow_toggled     (EggDateTime *edt,   GtkToggleButton *button);
static void  cal_popup_changed      (EggDateTime *edt,   GtkCalendar *calendar);
static void  cal_popup_double_click    (EggDateTime *edt,   GtkCalendar *calendar);
static gboolean    cal_popup_key_pressed     (EggDateTime *edt,   GdkEventKey *event, GtkWidget *widget);
static gboolean    cal_popup_button_pressed  (EggDateTime *edt,   GdkEventButton *event, GtkWidget *widget);
static gboolean    cal_popup_closed    (EggDateTime *edt,   GtkWindow *popup);
static void  cal_popup_hide         (EggDateTime *edt);
static void  time_popup_changed     (EggDateTime *edt,   Timelist *timelist);
static gboolean    time_popup_key_pressed    (EggDateTime *edt,   GdkEventKey *event, GtkWidget *widget);
static gboolean    time_popup_button_pressed (EggDateTime *edt,   GdkEventButton *event, GtkWidget *widget);
static gboolean    time_popup_closed      (EggDateTime *edt,   GtkWindow *popup);
static void  time_popup_hide     (EggDateTime *edt);

static void  apply_display_mode     (EggDateTime *edt);
static void  clamp_time_labels      (EggDateTime *edt);
static void  parse_date       (EggDateTime *edt);
static void  parse_time       (EggDateTime *edt);
static void  normalize_date         (EggDateTime *edt);
static void  normalize_time         (EggDateTime *edt);
static void  parse_and_update_date     (EggDateTime *edt);
static void  parse_and_update_time     (EggDateTime *edt);
static void  update_date_label      (EggDateTime *edt);
static void  update_time_label      (EggDateTime *edt);


static GtkHBoxClass *parent_class = NULL;


GType
egg_datetime_get_type (void)
{
   static GType datetime_type = 0;

   if (!datetime_type) {
      static const GTypeInfo datetime_info = {
         sizeof (EggDateTimeClass),
         NULL,    /* base_init */
         NULL,    /* base_finalize */
         (GClassInitFunc) egg_datetime_class_init,
         NULL,    /* class_finalize */
         NULL,    /* class_data */
         sizeof (EggDateTime),
         0,    /* n_preallocs */
         (GInstanceInitFunc) egg_datetime_init
      };

      datetime_type = g_type_register_static (GTK_TYPE_HBOX, "EggDateTime", &datetime_info, 0);
   }

   return datetime_type;
}

static void
egg_datetime_class_init (EggDateTimeClass *klass)
{
   GObjectClass *o_class;
   GParamSpec *pspec;

   parent_class = g_type_class_peek_parent (klass);

   o_class  = (GObjectClass *)   klass;

   o_class->finalize     = egg_datetime_finalize;
        o_class->set_property = egg_datetime_set_property;
        o_class->get_property = egg_datetime_get_property;

    ((GtkWidgetClass*)klass)->destroy     = egg_datetime_destroy;


   /* Properties */

   pspec = g_param_spec_uint ("display-flags",
               _("Display flags"),
               _("Displayed date and/or time properties"),
               0, G_MAXUINT, EGG_DATETIME_DISPLAY_DATE,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_DISPLAY_MODE, pspec);
   pspec = g_param_spec_boolean ("lazy",
                  _("Lazy mode"),
                  _("Lazy mode doesn't normalize entered date and time values"),
                  FALSE,
                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_LAZY, pspec);
   pspec = g_param_spec_uint ("year",
               _("Year"),
               _("Displayed year"),
               1, 9999, 2000,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_YEAR, pspec);
   pspec = g_param_spec_uint ("month",
               _("Month"),
               _("Displayed month"),
               G_DATE_JANUARY, G_DATE_DECEMBER, G_DATE_JANUARY,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_MONTH, pspec);
   pspec = g_param_spec_uint ("day",
               _("Day"),
               _("Displayed day of month"),
               1, 31, 1,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_DAY, pspec);
   pspec = g_param_spec_uint ("hour",
               _("Hour"),
               _("Displayed hour"),
               0, 23, 0,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_HOUR, pspec);
   pspec = g_param_spec_uint ("minute",
               _("Minute"),
               _("Displayed minute"),
               0, 59, 0,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_MINUTE, pspec);
   pspec = g_param_spec_uint ("second",
               _("Second"),
               _("Displayed second"),
               0, 59, 0,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_SECOND, pspec);
   pspec = g_param_spec_uint ("clamp-minyear",
               _("Lower limit year"),
               _("Year part of the lower date limit"),
               1, 9999, 1,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_CLAMP_MINYEAR, pspec);
   pspec = g_param_spec_uint ("clamp-maxyear",
               _("Upper limit year"),
               _("Year part of the upper date limit"),
               1, 9999, 9999,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_CLAMP_MAXYEAR, pspec);
   pspec = g_param_spec_uint ("clamp-minmonth",
               _("Lower limit month"),
               _("Month part of the lower date limit"),
               G_DATE_JANUARY, G_DATE_DECEMBER, G_DATE_JANUARY,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_CLAMP_MINMONTH, pspec);
   pspec = g_param_spec_uint ("clamp-maxmonth",
               _("Upper limit month"),
               _("Month part of the upper date limit"),
               G_DATE_JANUARY, G_DATE_DECEMBER, G_DATE_DECEMBER,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_CLAMP_MAXMONTH, pspec);
   pspec = g_param_spec_uint ("clamp-minday",
               _("Lower limit day"),
               _("Day of month part of the lower date limit"),
               1, 31, 1,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_CLAMP_MINDAY, pspec);
   pspec = g_param_spec_uint ("clamp-maxday",
               _("Upper limit day"),
               _("Day of month part of the upper date limit"),
               1, 31, 31,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_CLAMP_MAXDAY, pspec);
   pspec = g_param_spec_uint ("clamp-minhour",
               _("Lower limit hour"),
               _("Hour part of the lower time limit"),
               0, 23, 0,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_CLAMP_MINHOUR, pspec);
   pspec = g_param_spec_uint ("clamp-maxhour",
               _("Upper limit hour"),
               _("Hour part of the upper time limit"),
               0, 23, 23,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_CLAMP_MAXHOUR, pspec);
   pspec = g_param_spec_uint ("clamp-minminute",
               _("Lower limit minute"),
               _("Minute part of the lower time limit"),
               0, 59, 0,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_CLAMP_MINMINUTE, pspec);
   pspec = g_param_spec_uint ("clamp-maxminute",
               _("Upper limit minute"),
               _("Minute part of the upper time limit"),
               0, 59, 59,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_CLAMP_MAXMINUTE, pspec);
   pspec = g_param_spec_uint ("clamp-minsecond",
               _("Lower limit second"),
               _("Second part of the lower time limit"),
               0, 59, 0,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_CLAMP_MINSECOND, pspec);
   pspec = g_param_spec_uint ("clamp-maxsecond",
               _("Upper limit second"),
               _("Second part of the upper time limit"),
               0, 59, 59,
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
   g_object_class_install_property (o_class, ARG_CLAMP_MAXSECOND, pspec);

   /* Signals */

   egg_datetime_signals [SIGNAL_DATE_CHANGED] =
      g_signal_new ("date-changed",
               G_TYPE_FROM_CLASS (klass),
               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
               G_STRUCT_OFFSET (EggDateTimeClass, date_changed),
               NULL, NULL,
               g_cclosure_marshal_VOID__VOID,
               G_TYPE_NONE, 0);

   egg_datetime_signals [SIGNAL_TIME_CHANGED] =
      g_signal_new ("time-changed",
               G_TYPE_FROM_CLASS (klass),
               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
               G_STRUCT_OFFSET (EggDateTimeClass, time_changed),
               NULL, NULL,
               g_cclosure_marshal_VOID__VOID,
               G_TYPE_NONE, 0);
}

static void
egg_datetime_init (EggDateTime *edt)
{
   EggDateTimePrivate *priv;
   GtkWidget *arrow;
   GtkCalendarDisplayOptions cal_options;
   const gchar *week_start;

   priv = g_new0 (EggDateTimePrivate, 1);
   edt->priv = priv;

   /* Translate to calendar:week_start:1 if you want Monday to be the
    * first day of the week; otherwise translate to calendar:week_start:0.
    * Do *not* translate it to anything else, if it isn't calendar:week_start:1
    * or calendar:week_start:0 it will not work.
    */
   week_start = _("calendar:week_start:0");
   if (strcmp (week_start, "calendar:week_start:1") == 0)
      priv->week_start_monday = TRUE;
   else if (strcmp (week_start, "calendar:week_start:0") == 0)
      priv->week_start_monday = FALSE;
   else
      g_warning ("Whoever translated calendar:week_start:0 did so wrongly.\n");

   priv->date_valid = FALSE;
   priv->time_valid = FALSE;

   /* Initialize Widgets */

   gtk_box_set_spacing (GTK_BOX (edt), 4);

   /* Date Widgets */

   priv->date_box = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (edt), priv->date_box, TRUE, TRUE, 0);

   priv->date_entry = gtk_entry_new ();
   gtk_entry_set_width_chars (GTK_ENTRY (priv->date_entry), 12);
   _add_atk_name_desc (priv->date_entry, _("Date"), _("Enter the date directly"));
   g_signal_connect_swapped (G_OBJECT (priv->date_entry), "focus-out-event",
              G_CALLBACK (date_focus_out), edt);
   gtk_widget_show (priv->date_entry);
   gtk_box_pack_start (GTK_BOX (priv->date_box), priv->date_entry, TRUE, TRUE, 0);

   priv->date_button = gtk_toggle_button_new ();
   _add_atk_name_desc (priv->date_button, _("Select Date"), _("Select the date from a calendar"));
   gtk_box_pack_start (GTK_BOX (priv->date_box), priv->date_button, FALSE, FALSE, 0);
   arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
   gtk_container_add (GTK_CONTAINER (priv->date_button), arrow);
   gtk_widget_show (arrow);
   g_signal_connect_swapped (G_OBJECT (priv->date_button), "toggled",
              G_CALLBACK (date_arrow_toggled), edt);

   _add_atk_relation (priv->date_button, priv->date_entry,
            ATK_RELATION_CONTROLLER_FOR, ATK_RELATION_CONTROLLED_BY);

   /* Time Widgets */

   priv->time_box = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (edt), priv->time_box, TRUE, TRUE, 0);

   priv->time_entry = gtk_entry_new ();
   gtk_entry_set_width_chars (GTK_ENTRY (priv->time_entry), 10);
   _add_atk_name_desc (priv->time_entry, _("Time"), _("Enter the time directly"));
   g_signal_connect_swapped (G_OBJECT (priv->time_entry), "focus-out-event",
              G_CALLBACK (time_focus_out), edt);
   gtk_widget_show (priv->time_entry);
   gtk_box_pack_start (GTK_BOX (priv->time_box), priv->time_entry, TRUE, TRUE, 0);

   priv->time_button = gtk_toggle_button_new ();
   _add_atk_name_desc (priv->date_button, _("Select Time"), _("Select the time from a list"));
   gtk_box_pack_start (GTK_BOX (priv->time_box), priv->time_button, FALSE, FALSE, 0);
   arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
   gtk_container_add (GTK_CONTAINER (priv->time_button), arrow);
   gtk_widget_show (arrow);
   g_signal_connect_swapped (G_OBJECT (priv->time_button), "toggled",
              G_CALLBACK (time_arrow_toggled), edt);

   _add_atk_relation (priv->time_button, priv->time_entry,
            ATK_RELATION_CONTROLLER_FOR, ATK_RELATION_CONTROLLED_BY);

   /* Calendar Popup */

   priv->cal_popup = gtk_window_new (GTK_WINDOW_POPUP);
   gtk_widget_set_events (priv->cal_popup,
                gtk_widget_get_events (priv->cal_popup) | GDK_KEY_PRESS_MASK);
   gtk_window_set_resizable (GTK_WINDOW (priv->cal_popup), FALSE);
   g_signal_connect_swapped (G_OBJECT (priv->cal_popup), "delete-event",
              G_CALLBACK (cal_popup_closed), edt);
   g_signal_connect_swapped (G_OBJECT (priv->cal_popup), "key-press-event",
              G_CALLBACK (cal_popup_key_pressed), edt);
   g_signal_connect_swapped (G_OBJECT (priv->cal_popup), "button-press-event",
              G_CALLBACK (cal_popup_button_pressed), edt);

   priv->calendar = gtk_calendar_new ();
   cal_options = GTK_CALENDAR_SHOW_DAY_NAMES | GTK_CALENDAR_SHOW_HEADING;
   gtk_calendar_set_display_options (GTK_CALENDAR (priv->calendar), cal_options);
        gtk_container_add (GTK_CONTAINER (priv->cal_popup), priv->calendar);
   g_signal_connect_swapped (G_OBJECT (priv->calendar), "day-selected",
              G_CALLBACK (cal_popup_changed), edt);
   g_signal_connect_swapped (G_OBJECT (priv->calendar), "day-selected-double-click",
              G_CALLBACK (cal_popup_double_click), edt);
        gtk_widget_show (priv->calendar);

   /* Time Popup */

   priv->time_popup = gtk_window_new (GTK_WINDOW_POPUP);
   gtk_widget_set_events (priv->time_popup,
                gtk_widget_get_events (priv->time_popup) | GDK_KEY_PRESS_MASK);
   gtk_window_set_resizable (GTK_WINDOW (priv->time_popup), FALSE);
   g_signal_connect_swapped (G_OBJECT (priv->time_popup), "delete-event",
              G_CALLBACK (time_popup_closed), edt);
   g_signal_connect_swapped (G_OBJECT (priv->time_popup), "key-press-event",
              G_CALLBACK (time_popup_key_pressed), edt);
   g_signal_connect_swapped (G_OBJECT (priv->time_popup), "button-press-event",
              G_CALLBACK (time_popup_button_pressed), edt);

   priv->timelist = timelist_new ();
   timelist_set_selection_callback (TIMELIST (priv->timelist), G_CALLBACK (time_popup_changed), edt);
   gtk_widget_set_size_request (priv->timelist, -1, 400);
        gtk_container_add (GTK_CONTAINER (priv->time_popup), priv->timelist);
        gtk_widget_show (priv->timelist);
}

static void
egg_datetime_set_property (GObject  *object,
            guint  property_id,
            const GValue   *value,
            GParamSpec  *pspec)
{
   EggDateTime *edt;
   EggDateTimePrivate *priv;

   g_return_if_fail (object != NULL);
   g_return_if_fail (EGG_IS_DATETIME (object));
 
   edt = EGG_DATETIME (object);
   priv = edt->priv;
 
   switch (property_id) {
   case ARG_DISPLAY_MODE:
      egg_datetime_set_display_mode (edt, g_value_get_uint (value));
      break;
   case ARG_LAZY:
      egg_datetime_set_lazy (edt, g_value_get_boolean (value));
      break;
   case ARG_YEAR:
      priv->year = g_value_get_uint (value);
      break;
   case ARG_MONTH:
      priv->minute = g_value_get_uint (value);
      break;
   case ARG_DAY:
      priv->day = g_value_get_uint (value);
      break;
   case ARG_HOUR:
      priv->hour = g_value_get_uint (value);
      break;
   case ARG_MINUTE:
      priv->minute = g_value_get_uint (value);
      break;
   case ARG_SECOND:
      priv->second = g_value_get_uint (value);
      break;
   case ARG_CLAMP_MINYEAR:
      priv->clamp_minyear = g_value_get_uint (value);
      break;
   case ARG_CLAMP_MINMONTH:
      priv->clamp_minmonth = g_value_get_uint (value);
      break;
   case ARG_CLAMP_MINDAY:
      priv->clamp_minday = g_value_get_uint (value);
      break;
   case ARG_CLAMP_MINHOUR:
      priv->clamp_minhour = g_value_get_uint (value);
      break;
   case ARG_CLAMP_MINMINUTE:
      priv->clamp_minminute = g_value_get_uint (value);
      break;
   case ARG_CLAMP_MINSECOND:
      priv->clamp_minsecond = g_value_get_uint (value);
      break;
   case ARG_CLAMP_MAXYEAR:
      priv->clamp_maxyear = g_value_get_uint (value);
      break;
   case ARG_CLAMP_MAXMONTH:
      priv->clamp_maxmonth = g_value_get_uint (value);
      break;
   case ARG_CLAMP_MAXDAY:
      priv->clamp_maxday = g_value_get_uint (value);
      break;
   case ARG_CLAMP_MAXHOUR:
      priv->clamp_maxhour = g_value_get_uint (value);
      break;
   case ARG_CLAMP_MAXMINUTE:
      priv->clamp_maxminute = g_value_get_uint (value);
      break;
   case ARG_CLAMP_MAXSECOND:
      priv->clamp_maxsecond = g_value_get_uint (value);
      break;
   }
}

static void
egg_datetime_get_property (GObject  *object,
            guint  property_id,
            GValue   *value,
            GParamSpec  *pspec)
{
   EggDateTimePrivate *priv;

   g_return_if_fail (object != NULL);
   g_return_if_fail (EGG_IS_DATETIME (object));
 
   priv = EGG_DATETIME (object)->priv;
 
   switch (property_id) {
   case ARG_DISPLAY_MODE:
      g_value_set_uint (value, (guint) priv->display_mode);
      break;
   case ARG_LAZY:
      g_value_set_boolean (value, priv->lazy);
      break;
   case ARG_YEAR:
      g_value_set_uint (value, priv->year);
      break;
   case ARG_MONTH:
      g_value_set_uint (value, priv->month);
      break;
   case ARG_DAY:
      g_value_set_uint (value, priv->day);
      break;
   case ARG_HOUR:
      g_value_set_uint (value, priv->hour);
      break;
   case ARG_MINUTE:
      g_value_set_uint (value, priv->minute);
      break;
   case ARG_SECOND:
      g_value_set_uint (value, priv->second);
      break;
   case ARG_CLAMP_MINYEAR:
      g_value_set_uint (value, priv->clamp_minyear);
      break;
   case ARG_CLAMP_MINMONTH:
      g_value_set_uint (value, priv->clamp_minmonth);
      break;
   case ARG_CLAMP_MINDAY:
      g_value_set_uint (value, priv->clamp_minday);
      break;
   case ARG_CLAMP_MINHOUR:
      g_value_set_uint (value, priv->clamp_minhour);
      break;
   case ARG_CLAMP_MINMINUTE:
      g_value_set_uint (value, priv->clamp_minminute);
      break;
   case ARG_CLAMP_MINSECOND:
      g_value_set_uint (value, priv->clamp_minsecond);
      break;
   case ARG_CLAMP_MAXYEAR:
      g_value_set_uint (value, priv->clamp_maxyear);
      break;
   case ARG_CLAMP_MAXMONTH:
      g_value_set_uint (value, priv->clamp_maxmonth);
      break;
   case ARG_CLAMP_MAXDAY:
      g_value_set_uint (value, priv->clamp_maxday);
      break;
   case ARG_CLAMP_MAXHOUR:
      g_value_set_uint (value, priv->clamp_maxhour);
      break;
   case ARG_CLAMP_MAXMINUTE:
      g_value_set_uint (value, priv->clamp_maxminute);
      break;
   case ARG_CLAMP_MAXSECOND:
      g_value_set_uint (value, priv->clamp_maxsecond);
      break;
   }
}

static void
egg_datetime_destroy (GtkWidget *widget)
{
   EggDateTime *edt = EGG_DATETIME (widget);
   EggDateTimePrivate *priv = edt->priv;

   if (priv->cal_popup) {
      gtk_widget_destroy (priv->cal_popup);
      priv->cal_popup = NULL;
   }

   if (priv->time_popup) {
      gtk_widget_destroy (priv->time_popup);
      priv->time_popup = NULL;
   }

	if (GTK_WIDGET_CLASS (parent_class)->destroy)
		(* GTK_WIDGET_CLASS (parent_class)->destroy) (widget);
}

static void
egg_datetime_finalize (GObject *object)
{
   EggDateTime *edt = EGG_DATETIME (object);

   g_free (edt->priv);

   if (G_OBJECT_CLASS (parent_class)->finalize)
      (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/*
 * Utility Functions
 */

/* Determine the number of bits, time_t uses. */
static guint
time_t_bits (void) {
   guint i;
   time_t t;

   for (t = 1, i = 0; t != 0; t = t << 1, i++)
      ;

   return i;
}

static gchar *
get_time_string (guint8 hour, guint8 minute, guint8 second)
{
   gchar *s;

   /* Translators: set this to anything else if you want to use a
    * 24 hour clock.
    */
   if (!strcmp (_("24hr: no"), "24hr: no")) {
      const gchar *ampm_s;

      if (hour < 12)
         ampm_s = _("AM");
      else
         ampm_s = _("PM");

      hour %= 12;
      if (hour == 0)
         hour = 12;

      if (second <= 59)
         /* Translators: This is hh:mm:ss AM/PM. */
         s = g_strdup_printf (_("%02d:%02d:%02d %s"), hour, minute, second, ampm_s);
      else
         /* Translators: This is hh:mm AM/PM. */
         s = g_strdup_printf (_("%02d:%02d %s"), hour, minute, ampm_s);
   } else {
      if (second <= 59)
         /* Translators: This is hh:mm:ss. */
         s = g_strdup_printf (_("%02d:%02d:%02d"), hour, minute, second);
      else
         /* Translators: This is hh:mm. */
         s = g_strdup_printf (_("%02d:%02d"), hour, minute);
   }

   return s;
}

static void
popup_position (GtkWidget *widget, GtkWindow *popup)
{
   GtkAllocation allocation;
   GtkRequisition requisition;
   gint x, y, width, height;

   gtk_widget_get_preferred_size (GTK_WIDGET (popup), &requisition, NULL);
   gdk_window_get_origin (gtk_widget_get_window (widget), &x, &y);

   gtk_widget_get_allocation (widget, &allocation);
   x += allocation.x;
   y += allocation.y;
   width  = allocation.width;
   height = allocation.height;

   x += width - requisition.width;
   y += height;

   if (x < 0)
      x = 0;
   if (y < 0)
      y = 0;

   gtk_window_move (popup, x, y);
}

static void
popup_show (GtkWindow *popup)
{
   GdkCursor *cursor;

   gtk_widget_show (GTK_WIDGET (popup));
   gtk_widget_grab_focus (GTK_WIDGET (popup));
   gtk_grab_add (GTK_WIDGET (popup));

   cursor = gdk_cursor_new (GDK_ARROW);
   gdk_device_grab (gdk_device_manager_get_client_pointer (gdk_display_get_device_manager (gdk_display_get_default ())),
                    gtk_widget_get_window (GTK_WIDGET (popup)),
                    GDK_OWNERSHIP_APPLICATION,
                    TRUE,
                    (GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK),
                    cursor,
                    GDK_CURRENT_TIME);
   g_object_unref (cursor);
}

static void
popup_hide (GtkWindow *popup)
{
   gtk_widget_hide (GTK_WIDGET (popup));
   gtk_grab_remove (GTK_WIDGET (popup));
   gdk_device_ungrab (gdk_device_manager_get_client_pointer (gdk_display_get_device_manager (gdk_display_get_default ())),
                      GDK_CURRENT_TIME);
}

/*
 * Calendar Popup
 */

static void
date_arrow_toggled (EggDateTime *edt, GtkToggleButton *button)
{
   EggDateTimePrivate *priv = edt->priv;

   if (!gtk_toggle_button_get_active (button))
      return;

   /* Set Date */

   parse_date (edt);

   gtk_calendar_select_month (GTK_CALENDAR (priv->calendar), priv->month - 1, priv->year);
   gtk_calendar_select_day (GTK_CALENDAR (priv->calendar), priv->day);

   /* Position Popup */

   popup_position (priv->date_button, GTK_WINDOW (priv->cal_popup));

   /* Show Popup */

   popup_show (GTK_WINDOW (priv->cal_popup));
}

static void
cal_popup_changed (EggDateTime *edt, GtkCalendar *calendar)
{
   guint year, month, day;

   gtk_calendar_get_date (GTK_CALENDAR (edt->priv->calendar), &year, &month, &day);

   edt->priv->year  = year;
   edt->priv->month = month + 1;
   edt->priv->day   = day;
   edt->priv->date_valid = TRUE;

   normalize_date (edt);
   update_date_label (edt);

   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_DATE_CHANGED], 0);
}

static void
cal_popup_double_click (EggDateTime *edt, GtkCalendar *calendar)
{
   cal_popup_hide (edt);
}

static gboolean
cal_popup_key_pressed (EggDateTime *edt, GdkEventKey *event, GtkWidget *widget)
{
   if (event->keyval != GDK_Escape)
      return FALSE;

   g_signal_stop_emission_by_name (G_OBJECT (widget), "key_press_event");

   cal_popup_hide (edt);

   return TRUE;
}

static gboolean
cal_popup_button_pressed (EggDateTime *edt, GdkEventButton *event, GtkWidget *widget)
{
   GtkWidget *child;

   child = gtk_get_event_widget ((GdkEvent *) event);

   /* We don't ask for button press events on the grab widget, so
    * if an event is reported directly to the grab widget, it must
    * be on a window outside the application (and thus we remove
    * the popup window). Otherwise, we check if the widget is a child
    * of the grab widget, and only remove the popup window if it
    * is not.
    */
   if (child != widget) {
      while (child) {
         if (child == widget)
            return FALSE;
         child = gtk_widget_get_parent (child);
      }
   }

   cal_popup_hide (edt);

   return TRUE;
}

static gboolean
cal_popup_closed (EggDateTime *edt, GtkWindow *popup)
{
   cal_popup_hide (edt);

   return TRUE;
}

static void
cal_popup_hide (EggDateTime *edt)
{
   EggDateTimePrivate *priv = edt->priv;

   popup_hide (GTK_WINDOW (priv->cal_popup));

   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->date_button), FALSE);
   gtk_widget_grab_focus (priv->date_entry);

   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_DATE_CHANGED], 0);
}

/*
 * Time Popup
 */

static void
time_arrow_toggled (EggDateTime *edt, GtkToggleButton *button)
{
   EggDateTimePrivate *priv = edt->priv;

   if (!gtk_toggle_button_get_active (button))
      return;

   /* Set Time */

   parse_time (edt);

   timelist_set_time (TIMELIST (priv->timelist), priv->hour, priv->minute);

   /* Position Popup */

   popup_position (priv->time_button, GTK_WINDOW (priv->time_popup));

   /* Show Popup */

   popup_show (GTK_WINDOW (priv->time_popup));
}

static void
time_popup_changed (EggDateTime *edt, Timelist *timelist)
{
   EggDateTimePrivate *priv = edt->priv;

   if (!timelist_get_time (timelist, &priv->hour, &priv->minute))
      return;

   priv->second = 0;
   priv->time_valid = TRUE;

   normalize_time (edt);
   update_time_label (edt);

   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_TIME_CHANGED], 0);
}

static gboolean
time_popup_key_pressed (EggDateTime *edt, GdkEventKey *event, GtkWidget *widget)
{
   if (event->keyval != GDK_Escape)
      return FALSE;

   g_signal_stop_emission_by_name (G_OBJECT (widget), "key_press_event");

   time_popup_hide (edt);

   return TRUE;
}

static gboolean
time_popup_button_pressed (EggDateTime *edt, GdkEventButton *event, GtkWidget *widget)
{
   GtkWidget *child;

   child = gtk_get_event_widget ((GdkEvent *) event);

   /* We don't ask for button press events on the grab widget, so
    * if an event is reported directly to the grab widget, it must
    * be on a window outside the application (and thus we remove
    * the popup window). Otherwise, we check if the widget is a child
    * of the grab widget, and only remove the popup window if it
    * is not.
    */
   if (child != widget) {
      while (child) {
         if (child == widget)
            return FALSE;
         child = gtk_widget_get_parent (child);
      }
   }

   time_popup_hide (edt);

   return TRUE;
}

static gboolean
time_popup_closed (EggDateTime *edt, GtkWindow *popup)
{
   time_popup_hide (edt);

   return TRUE;
}

static void
time_popup_hide (EggDateTime *edt)
{
   EggDateTimePrivate *priv = edt->priv;

   popup_hide (GTK_WINDOW (priv->time_popup));

   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->time_button), FALSE);
   gtk_widget_grab_focus (priv->time_entry);

   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_TIME_CHANGED], 0);
}

/*
 * Callbacks
 */

static gboolean
date_focus_out (EggDateTime *edt, GtkEntry *entry)
{
   parse_date (edt);
   update_date_label (edt);

   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_DATE_CHANGED], 0);

   return FALSE;
}

static gboolean
time_focus_out (EggDateTime *edt, GtkEntry *entry)
{
   parse_time (edt);
   update_time_label (edt);

   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_TIME_CHANGED], 0);

   return FALSE;
}

/*
 * Private Methods
 */

static void
apply_display_mode (EggDateTime *edt)
{
   if (edt->priv->display_mode & EGG_DATETIME_DISPLAY_DATE)
      gtk_widget_show (edt->priv->date_box);
   else
      gtk_widget_hide (edt->priv->date_box);
   if (edt->priv->display_mode & EGG_DATETIME_DISPLAY_MONTH)
      gtk_widget_show (edt->priv->date_button);
   else
      gtk_widget_hide (edt->priv->date_button);

   if (edt->priv->display_mode & EGG_DATETIME_DISPLAY_TIME)
      gtk_widget_show (edt->priv->time_box);
   else
      gtk_widget_hide (edt->priv->time_box);
   if (edt->priv->display_mode & EGG_DATETIME_DISPLAY_HOUR)
      gtk_widget_show (edt->priv->time_button);
   else
      gtk_widget_hide (edt->priv->time_button);
}

static void
clamp_time_labels (EggDateTime *edt)
{
   EggDateTimePrivate *priv = edt->priv;

   timelist_clamp (TIMELIST (priv->timelist),
         priv->clamp_minhour,
         priv->clamp_minminute,
         priv->clamp_maxhour,
         priv->clamp_maxminute);
}

/* Updates the date entry to the current date. */
static void
update_date_label (EggDateTime *edt)
{
   EggDateTimePrivate *priv = edt->priv;
   gchar *s;

   if (!priv->date_valid) {
      gtk_entry_set_text (GTK_ENTRY (priv->date_entry), "");
      return;
   }

   /* TODO: should handle other display modes as well... */

   /* Translators: This is YYYY-MM-DD */
   s = g_strdup_printf (_("%04d-%02d-%02d"), priv->year, priv->month, priv->day);
   gtk_entry_set_text (GTK_ENTRY (priv->date_entry), s);
   g_free (s);
}

/* Updates the time entry to the current time. */
static void
update_time_label (EggDateTime *edt)
{
   EggDateTimePrivate *priv = edt->priv;
   gchar *s;

   if (!priv->time_valid) {
      gtk_entry_set_text (GTK_ENTRY (priv->time_entry), "");
      return;
   }

   /* TODO: should handle other display modes as well... */

   if ((priv->display_mode & EGG_DATETIME_DISPLAY_SECOND) ||
       (priv->display_mode & EGG_DATETIME_DISPLAY_SECOND_OPT))
      s = get_time_string (priv->hour, priv->minute, priv->second);
   else
      s = get_time_string (priv->hour, priv->minute, 0xff);

   gtk_entry_set_text (GTK_ENTRY (priv->time_entry), s);

   g_free (s);
}

/* Parse the current date entry and normalize the date. */
static void
parse_date (EggDateTime *edt)
{
   GDate date;

   g_date_set_parse (&date, gtk_entry_get_text (GTK_ENTRY (edt->priv->date_entry)));
   if (!g_date_valid (&date)) {
      if (edt->priv->lazy)
         edt->priv->date_valid = FALSE;
      return;
   }

   edt->priv->year  = date.year;
   edt->priv->month = date.month;
   edt->priv->day   = date.day;
   edt->priv->date_valid = TRUE;

   normalize_date (edt);
}

/* Parse the current time entry and normalize the time. */
static void
parse_time (EggDateTime *edt)
{
   const gchar *s;
   gchar *scp;
   unsigned int hour, minute, second = 0;
   size_t len1, len2, len3;

   /* Retrieve and Parse String */

   s = gtk_entry_get_text (GTK_ENTRY (edt->priv->time_entry));

   /* Translators: This is hh:mm:ss. */
   if (sscanf (s, _("%u:%u:%u"), &hour, &minute, &second) < 2) {
      if (sscanf (s, "%u:%u:%u", &hour, &minute, &second) < 2) {
         if (edt->priv->lazy)
            edt->priv->time_valid = FALSE;
         return;
      }
   }

   if (hour > 23 || minute > 59 || second > 59) {
      if (edt->priv->lazy)
         edt->priv->time_valid = FALSE;
      return;
   }

   /* AM/PM Handling */

   scp = g_strdup (s);
   scp = g_strchomp (scp);

   len1 = strlen (_("AM"));
   len2 = strlen (_("PM"));
   len3 = strlen (scp);

   if (len1 < len3 && !strcasecmp (scp + len3 - len1, _("AM"))) {
      if (hour == 12)
         hour = 0;
   }
   if (len2 < len3 && !strcasecmp (scp + len3 - len2, _("PM"))) {
      if (hour == 12)
         hour = 0;
      hour += 12;
   }

   /* Store Time */

   edt->priv->hour   = hour;
   edt->priv->minute = minute;
   edt->priv->second = second;
   edt->priv->time_valid = TRUE;

   /* Cleanup */

   g_free (scp);

   normalize_time (edt);
}

/* Clamp the current date to the date clamp range if lazy is turned off. */
static void
normalize_date (EggDateTime *edt)
{
   GDate date, min_date, max_date;

   if (edt->priv->lazy)
      return;

   g_date_clear (&date, 1);
   g_date_set_dmy (&date, edt->priv->day, edt->priv->month, edt->priv->year);
   g_date_clear (&min_date, 1);
   g_date_set_dmy (&min_date, edt->priv->clamp_minday, edt->priv->clamp_minmonth, edt->priv->clamp_minyear);
   g_date_clear (&max_date, 1);
   g_date_set_dmy (&max_date, edt->priv->clamp_maxday, edt->priv->clamp_maxmonth, edt->priv->clamp_maxyear);

   g_date_clamp (&date, &min_date, &max_date);

   edt->priv->year  = date.year;
   edt->priv->month = date.month;
   edt->priv->day   = date.day;
   edt->priv->date_valid = TRUE;
}

/* Clamp the current time to the time clamp range if lazy is turned off. */
static void
normalize_time (EggDateTime *edt)
{
   if (edt->priv->lazy)
      return;

   if (edt->priv->hour < edt->priv->clamp_minhour) {
      edt->priv->hour   = edt->priv->clamp_minhour;
      edt->priv->minute = edt->priv->clamp_minminute;
      edt->priv->second = edt->priv->clamp_minsecond;
   } else if (edt->priv->hour == edt->priv->clamp_minhour) {
      if (edt->priv->minute < edt->priv->clamp_minminute) {
         edt->priv->minute = edt->priv->clamp_minminute;
         edt->priv->second = edt->priv->clamp_minsecond;
      } else if (edt->priv->minute == edt->priv->clamp_minminute) {
         if (edt->priv->second < edt->priv->clamp_minsecond)
            edt->priv->second = edt->priv->clamp_minsecond;
      }
   }

   if (edt->priv->hour > edt->priv->clamp_maxhour) {
      edt->priv->hour   = edt->priv->clamp_maxhour;
      edt->priv->minute = edt->priv->clamp_maxminute;
      edt->priv->second = edt->priv->clamp_maxsecond;
   } else if (edt->priv->hour == edt->priv->clamp_maxhour) {
      if (edt->priv->minute > edt->priv->clamp_maxminute) {
         edt->priv->minute = edt->priv->clamp_maxminute;
         edt->priv->second = edt->priv->clamp_maxsecond;
      } else if (edt->priv->minute == edt->priv->clamp_maxminute) {
         if (edt->priv->second > edt->priv->clamp_maxsecond)
            edt->priv->second = edt->priv->clamp_maxsecond;
      }
   }

   edt->priv->time_valid = TRUE;
}

static void
parse_and_update_date (EggDateTime *edt)
{
   if (edt->priv->lazy)
      return;

   parse_date (edt);
   update_date_label (edt);
}

static void
parse_and_update_time (EggDateTime *edt)
{
   if (edt->priv->lazy)
      return;

   parse_time (edt);
   update_time_label (edt);
}

/*
 * Public Methods
 */

/**
 * egg_datetime_new:
 *
 * Creates a new #EggDateTime widget. By default this widget will show
 * only the date component and is set to the current date and time.
 *
 * Return value: a newly created #EggDateTime widget
 **/
GtkWidget *
egg_datetime_new (void)
{
   EggDateTime *edt;

   edt = g_object_new (EGG_TYPE_DATETIME, NULL);
   egg_datetime_set_from_time_t (edt, time (NULL));

   return GTK_WIDGET (edt);
}

/**
 * egg_datetime_new_from_time_t:
 * @t: initial time and date
 *
 * Creates a new #EggDateTime widget and sets it to the date and time
 * given as @t argument. This does also call egg_datetime_set_clamp_time_t().
 * By default this widget will show only the date component.
 *
 * Return value: a newly created #EggDateTime widget
 **/
GtkWidget *
egg_datetime_new_from_time_t (time_t t)
{
   EggDateTime *edt;

   g_return_val_if_fail (t >= 0, NULL);

   edt = g_object_new (EGG_TYPE_DATETIME, NULL);
   egg_datetime_set_from_time_t (edt, t);
   egg_datetime_set_clamp_time_t (edt);

   return GTK_WIDGET (edt);
}

/**
 * egg_datetime_new_from_struct_tm:
 * @tm: initial time and date
 *
 * Creates a new #EggDateTime widget and sets it to the date and time
 * given as @tm argument. By default this widget will show only the date
 * component.
 *
 * Return value: a newly created #EggDateTime widget
 **/
GtkWidget *
egg_datetime_new_from_struct_tm (struct tm *tm)
{
   EggDateTime *edt;

   g_return_val_if_fail (tm != NULL, NULL);

   edt = g_object_new (EGG_TYPE_DATETIME, NULL);
   egg_datetime_set_from_struct_tm (edt, tm);

   return GTK_WIDGET (edt);
}

/**
 * egg_datetime_new_from_gdate:
 * @date: initial time and date
 *
 * Creates a new #EggDateTime widget and sets it to the date and time
 * given as @date argument. By default this widget will show only the
 * date component.
 *
 * Return value: a newly created #EggDateTime widget
 **/
GtkWidget *
egg_datetime_new_from_gdate (GDate *date)
{
   EggDateTime *edt;

   g_return_val_if_fail (date != NULL, NULL);

   edt = g_object_new (EGG_TYPE_DATETIME, NULL);
   egg_datetime_set_from_gdate (edt, date);

   return GTK_WIDGET (edt);
}

/**
 * egg_datetime_new_from_datetime:
 * @year: initial year
 * @month: initial month
 * @day: initial day
 * @hour: initial hour
 * @minute: initial minute
 * @second: initial second
 *
 * Creates a new #EggDateTime widget and sets it to the date and time
 * given as arguments.
 *
 * Return value: a newly created #EggDateTime widget
 **/
GtkWidget *
egg_datetime_new_from_datetime (GDateYear year, GDateMonth month, GDateDay day, guint8 hour, guint8 minute, guint8 second)
{
   EggDateTime *edt;

   edt = g_object_new (EGG_TYPE_DATETIME, NULL);
   egg_datetime_set_date (edt, year, month, day);
   egg_datetime_set_time (edt, hour, minute, second);

   return GTK_WIDGET (edt);
}

/**
 * egg_datetime_set_none:
 * @edt: an #EggDateTime
 *
 * Sets the date to an invalid value. If lazy mode is turned off the date
 * and time will be set to a random value.
 **/
void
egg_datetime_set_none (EggDateTime *edt)
{
   g_return_if_fail (edt != NULL);
   g_return_if_fail (EGG_IS_DATETIME (edt));

   edt->priv->date_valid = FALSE;
   edt->priv->time_valid = FALSE;

   update_date_label (edt);
   update_time_label (edt);

   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_DATE_CHANGED], 0);
   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_TIME_CHANGED], 0);
}

/**
 * egg_datetime_set_from_time_t:
 * @edt: an #EggDateTime
 * @t: date and time to set the widget to
 *
 * Sets the date and time of the widget to @t.
 **/
void
egg_datetime_set_from_time_t (EggDateTime *edt, time_t t)
{
   struct tm tm;

   g_return_if_fail (edt != NULL);
   g_return_if_fail (EGG_IS_DATETIME (edt));

   if (localtime_r (&t, &tm)) {
      egg_datetime_set_date (edt, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
      egg_datetime_set_time (edt, tm.tm_hour, tm.tm_min, tm.tm_sec);
   } else
      egg_datetime_set_none (edt);
}

/**
 * egg_datetime_get_as_time_t:
 * @edt: an #EggDateTime
 * @t: pointer to a %time_t value
 *
 * Returns the current time as a %time_t value. If the currently entered
 * value is invalid and lazy mode is turned on or if the entered date
 * can't be represented as a %time_t value, the value is set to -1 and
 * FALSE is returned.
 *
 * Return value: success indicator
 **/
gboolean
egg_datetime_get_as_time_t (EggDateTime *edt, time_t *t)
{
   struct tm tm;
   GDateYear year;
        GDateMonth month;
   GDateDay day;
   guint8 hour, minute, second;

   g_return_val_if_fail (edt != NULL, FALSE);
   g_return_val_if_fail (EGG_IS_DATETIME (edt), FALSE);

   if (!t)
      return FALSE;

   if (!egg_datetime_get_date (edt, &year, &month, &day)) {
      *t = -1;
      return FALSE;
   }
   if (!egg_datetime_get_time (edt, &hour, &minute, &second)) {
      *t = -1;
      return FALSE;
   }

   memset (&tm, 0, sizeof (struct tm));
   tm.tm_year = year - 1900;
   tm.tm_mon  = month - 1;
   tm.tm_mday = day;
   tm.tm_hour = hour;
   tm.tm_min  = minute;
   tm.tm_sec  = second;

   *t = mktime (&tm);

   if (*t < 0) {
      *t = -1;
      return FALSE;
   }
   
   return TRUE;
}

/**
 * egg_datetime_set_from_struct_tm:
 * @edt: an #EggDateTime
 * @tm: date and time to set the widget to
 *
 * Sets the date and time of the widget to @tm.
 **/
void
egg_datetime_set_from_struct_tm (EggDateTime *edt, struct tm *tm)
{
   g_return_if_fail (edt != NULL);
   g_return_if_fail (EGG_IS_DATETIME (edt));
   g_return_if_fail (tm != NULL);

   egg_datetime_set_date (edt, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
   egg_datetime_set_time (edt, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/**
 * egg_datetime_get_as_struct_tm:
 * @edt: an #EggDateTime
 * @tm: pointer to an allocated struct tm
 *
 * Fill the supplied struct tm with the widget's current date and time.
 * If the currently entered value is invalid and lazy mode is turned
 * on or if the entered date can't be represented as a struct tm, the
 * struct is filled with invalid data and FALSE is returned.
 *
 * Return value: success indicator
 **/
gboolean
egg_datetime_get_as_struct_tm (EggDateTime *edt, struct tm *tm)
{
   GDateYear year;
   GDateMonth month;
   GDateDay day;
   guint8 hour, minute, second;

   g_return_val_if_fail (edt != NULL, FALSE);
   g_return_val_if_fail (EGG_IS_DATETIME (edt), FALSE);

   if (!tm)
      return FALSE;

   memset (tm, 0, sizeof (struct tm));

   if (!egg_datetime_get_date (edt, &year, &month, &day))
      return FALSE;
   if (!egg_datetime_get_time (edt, &hour, &minute, &second))
      return FALSE;

   tm->tm_year = year - 1900;
   tm->tm_mon  = month - 1;
   tm->tm_mday = day;
   tm->tm_hour = hour;
   tm->tm_min  = minute;
   tm->tm_sec  = second;

   mktime (tm);
   
   return TRUE;
}

/**
 * egg_datetime_set_from_gdate:
 * @edt: an #EggDateTime
 * @date: date to set the widget to
 *
 * Sets the date of the widget to @date. The time will remain unchanged.
 **/
void
egg_datetime_set_from_gdate (EggDateTime *edt, GDate *date)
{
   g_return_if_fail (edt != NULL);
   g_return_if_fail (EGG_IS_DATETIME (edt));
   g_return_if_fail (date != NULL);

   if (g_date_valid (date))
      egg_datetime_set_date (edt, date->year, date->month, date->day);
   else
      egg_datetime_set_none (edt);
}

/**
 * egg_datetime_get_as_gdate:
 * @edt: an #EggDateTime
 * @date: pointer to an allocated #GDate
 *
 * Fills the supplied #GDate with the widget's current date. If the
 * currently entered date value is invalid and lazy mode is turned
 * on or if the entered date can't be represented as a #GDate, the
 * @date is set to an invalid value and FALSE is returned.
 *
 * Return value: success indicator
 **/
gboolean
egg_datetime_get_as_gdate (EggDateTime *edt, GDate *date)
{
   GDateYear year;
   GDateMonth month;
   GDateDay day;

   g_return_val_if_fail (edt != NULL, FALSE);
   g_return_val_if_fail (EGG_IS_DATETIME (edt), FALSE);

   if (!date)
      return FALSE;

   g_date_clear (date, 1);

   if (!egg_datetime_get_date (edt, &year, &month, &day))
      return FALSE;

   g_date_set_dmy (date, day, month, year);

   return TRUE;
}

/**
 * egg_datetime_set_date:
 * @edt: an #EggDateTime
 * @year: a #guint16 between 1 and 9999
 * @month: a #guint8 between 1 and 12
 * @day: a #guint8 between 1 and 28-31 (depending on @month)
 *
 * Sets the date of the widget. The time will remain unchanged.
 **/
void
egg_datetime_set_date (EggDateTime *edt, GDateYear year, GDateMonth month, GDateDay day)
{
   g_return_if_fail (edt != NULL);
   g_return_if_fail (EGG_IS_DATETIME (edt));
   g_return_if_fail (year >= 1 && year <= 9999);
   g_return_if_fail (month >= 1 && month <= 12);
   g_return_if_fail (day >= 1 && day <= g_date_get_days_in_month (month, year));

   edt->priv->year  = year;
   edt->priv->month = month;
   edt->priv->day   = day;
   edt->priv->date_valid = TRUE;

   normalize_date (edt);
   update_date_label (edt);

   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_DATE_CHANGED], 0);
}

/**
 * egg_datetime_get_date:
 * @edt: an #EggDateTime
 * @year: a pointer to a #guint16 or %NULL
 * @month: a pointer to #guint8 or %NULL
 * @day: a pointer to a #guint8 or %NULL
 *
 * Fills the supplied arguments with the widget's current date. If the
 * currently entered date value is invalid and lazy mode is turned
 * on, the arguments are set to %EGG_DATETIME_INVALID_DATE and FALSE
 * is returned.
 *
 * Return value: success indicator
 **/
gboolean
egg_datetime_get_date (EggDateTime *edt, GDateYear *year, GDateMonth *month, GDateDay *day)
{
   g_return_val_if_fail (edt != NULL, FALSE);
   g_return_val_if_fail (EGG_IS_DATETIME (edt), FALSE);

   parse_date (edt);

   if (!edt->priv->date_valid) {
      if (year)
         *year = 0;
      if (month)
         *month = 0;
      if (day)
         *day = 0;
      return FALSE;
   }

   if (year)
      *year  = edt->priv->year;
   if (month)
      *month = edt->priv->month;
   if (day)
      *day   = edt->priv->day;
   
   return TRUE;
}

/**
 * egg_datetime_set_time:
 * @edt: an #EggDateTime
 * @hour: a #guint8 between 0 and 23
 * @minute: a #guint8 between 0 and 59
 * @second: a #guint8 between 0 and 59
 *
 * Sets the time of the widget. The date will remain unchanged.
 **/
void
egg_datetime_set_time (EggDateTime *edt, guint8 hour, guint8 minute, guint8 second)
{
   g_return_if_fail (edt != NULL);
   g_return_if_fail (EGG_IS_DATETIME (edt));
   g_return_if_fail (hour <= 23);
   g_return_if_fail (minute <= 59);
   g_return_if_fail (second <= 59);

   edt->priv->hour   = hour;
   edt->priv->minute = minute;
   edt->priv->second = second;
   edt->priv->time_valid = TRUE;

   normalize_time (edt);
   update_time_label (edt);

   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_TIME_CHANGED], 0);
}

/**
 * egg_datetime_get_date:
 * @edt: an #EggDateTime
 * @hour: a pointer to a #guint8 or %NULL
 * @minute: a pointer to #guint8 or %NULL
 * @second: a pointer to a #guint8 or %NULL
 *
 * Fills the supplied arguments with the widget's current time. If the
 * currently entered time value is invalid and lazy mode is turned
 * on, the arguments are set to %EGG_DATETIME_INVALID_TIME and FALSE
 * is returned.
 *
 * Return value: success indicator
 **/
gboolean
egg_datetime_get_time (EggDateTime *edt, guint8 *hour, guint8 *minute, guint8 *second)
{
   g_return_val_if_fail (edt != NULL, FALSE);
   g_return_val_if_fail (EGG_IS_DATETIME (edt), FALSE);

   parse_time (edt);

   if (!edt->priv->time_valid) {
      if (hour)
         *hour = 0xff;
      if (minute)
         *minute = 0xff;
      if (second)
         *second = 0xff;
      return FALSE;
   }

   if (hour)
      *hour   = edt->priv->hour;
   if (minute)
      *minute = edt->priv->minute;
   if (second)
      *second = edt->priv->second;
   
   return TRUE;
}

/**
 * egg_datetime_set_lazy:
 * @edt: an #EggDateTime
 * @lazy: a boolean value
 *
 * Turns the widget's lazy mode on or off. In lazy mode the widget will
 * allow invalid values to be entered. If lazy mode is turned off the
 * widget will normalize all invalid values entered in the date and time
 * widgets to the nearest valid value. This guarantees that the get methods
 * will always return valid values.
 *
 * Lazy mode defaults to %TRUE.
 **/
void
egg_datetime_set_lazy (EggDateTime *edt, gboolean lazy)
{
   g_return_if_fail (edt != NULL);
   g_return_if_fail (EGG_IS_DATETIME (edt));

   edt->priv->lazy = lazy ? TRUE : FALSE;

   parse_and_update_date (edt);
   parse_and_update_time (edt);

   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_DATE_CHANGED], 0);
   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_TIME_CHANGED], 0);
}

/**
 * egg_datetime_get_lazy:
 * @edt: an #EggDateTime
 *
 * Returns whether the widget is in lazy mode.
 *
 * Return value: a boolean value
 **/
gboolean
egg_datetime_get_lazy (EggDateTime *edt)
{
   g_return_val_if_fail (edt != NULL, FALSE);
   g_return_val_if_fail (EGG_IS_DATETIME (edt), FALSE);

   return edt->priv->lazy;
}

/**
 * egg_datetime_set_display_mode:
 * @edt: an #EggDateTime
 * @mode: new display mode
 *
 * Sets the widget's new display mode to @mode. The display mode defaults
 * to %EGG_DATETIME_DISPLAY_DATE.
 **/
void
egg_datetime_set_display_mode (EggDateTime *edt, EggDateTimeDisplayMode mode)
{
   g_return_if_fail (edt != NULL);
   g_return_if_fail (EGG_IS_DATETIME (edt));

   edt->priv->display_mode = mode;

   apply_display_mode (edt);
}

/**
 * egg_datetime_get_display_mode:
 * @edt: an #EggDateTime
 *
 * Returns the current display mode.
 *
 * Return value: The current display mode.
 **/
EggDateTimeDisplayMode
egg_datetime_get_display_mode (EggDateTime *edt)
{
   g_return_val_if_fail (edt != NULL, 0);
   g_return_val_if_fail (EGG_IS_DATETIME (edt), 0);

   return edt->priv->display_mode;
}

/**
 * egg_datetime_set_clamp_date:
 * @edt: an #EggDateTime
 * @minyear: minimum year
 * @minmonth: minimum month
 * @minday: minimum day
 * @maxyear: maximum year
 * @maxmonth: maximum month
 * @maxday: maximum day
 *
 * Limits the allowed dates to the range given. If lazy mode is
 * turned off, dates that are outside of this range are snapped to the
 * minimum or maximum date. Otherwise such dates return an invalid value.
 *
 * This defaults to the minimum date 1-1-1 and maximum date 9999-12-31.
 * The maximum year is always limited to 9999.
 **/
void
egg_datetime_set_clamp_date (EggDateTime *edt,
              GDateYear  minyear,
              GDateMonth minmonth,
              GDateDay   minday,
              GDateYear  maxyear,
              GDateMonth maxmonth,
              GDateDay   maxday)
{
   if (maxyear > 9999)
      maxyear = 9999;

   g_return_if_fail (minyear >= 1 && minyear <= 9999 && maxyear >= 1);
   g_return_if_fail (minmonth >= 1 && minmonth <= 12 && maxmonth >= 1 && maxmonth <= 12);
   g_return_if_fail (minday >= 1 && minday <= g_date_get_days_in_month (minmonth, minyear));
   g_return_if_fail (maxday >= 1 && maxday <= g_date_get_days_in_month (maxmonth, maxyear));
   g_return_if_fail (minyear <= maxyear);
   g_return_if_fail (minyear < maxyear || minmonth <= maxmonth);
   g_return_if_fail (minyear < maxyear || minmonth < maxmonth || minday <= maxday);

   edt->priv->clamp_minyear  = minyear;
   edt->priv->clamp_minmonth = minmonth;
   edt->priv->clamp_minday   = minday;
   edt->priv->clamp_maxyear  = maxyear;
   edt->priv->clamp_maxmonth = maxmonth;
   edt->priv->clamp_maxday   = maxday;

   parse_and_update_date (edt);

   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_DATE_CHANGED], 0);
}

/**
 * egg_datetime_set_clamp_time:
 * @edt: an #EggDateTime
 * @minhour: minimum hour
 * @minminute: minimum minute
 * @minsecond: minimum second
 * @maxhour: maximum hour
 * @maxminute: maximum minute
 * @maxsecond: maximum second
 *
 * Limits the allowed times to the range given. If lazy mode is turned
 * off, times that are outside of this range are snapped to the minimum or
 * maximum time. Otherwise such times return an invalid value.
 **/
void
egg_datetime_set_clamp_time (EggDateTime *edt, guint8 minhour, guint8 minminute, guint8 minsecond, guint8 maxhour, guint8 maxminute, guint8 maxsecond)
{
   g_return_if_fail (minhour <= 23 && maxhour <= 23);
   g_return_if_fail (minminute <= 59 && maxminute <= 59);
   g_return_if_fail (minsecond <= 59 && maxsecond <= 59);
   g_return_if_fail (minhour <= maxhour);
   g_return_if_fail (minhour < maxhour || minminute <= maxminute);
   g_return_if_fail (minhour < maxhour || minminute < maxminute || minsecond <= maxsecond);

   edt->priv->clamp_minhour   = minhour;
   edt->priv->clamp_minminute = minminute;
   edt->priv->clamp_minsecond = minsecond;
   edt->priv->clamp_maxhour   = maxhour;
   edt->priv->clamp_maxminute = maxminute;
   edt->priv->clamp_maxsecond = maxsecond;

   clamp_time_labels (edt);
   parse_and_update_time (edt);

   g_signal_emit (G_OBJECT (edt), egg_datetime_signals[SIGNAL_TIME_CHANGED], 0);
}

/**
 * egg_datetime_set_clamp_time_t:
 * @edt: an #EggDateTime
 *
 * Clamps the allowed dates of the widget to valid #time_t values.
 * The time clamp settings are not changed.
 **/
void
egg_datetime_set_clamp_time_t (EggDateTime *edt)
{
   time_t t;
   struct tm start_tm, end_tm;
   guint bits;
   guint16 year;
   guint8 month, day;

   g_return_if_fail (edt != NULL);
   g_return_if_fail (EGG_IS_DATETIME (edt));

   t = 0;
   gmtime_r (&t, &start_tm);

   /* evil hack */
   bits = time_t_bits ();
   t = ~0;
   t &= ~(1 << (bits - 1));

   gmtime_r (&t, &end_tm);

   /* Subtract one day from the end date, since not all times of
    * the last day can be represented.
    */

   year  = end_tm.tm_year + 1900;
   month = end_tm.tm_mon + 1;
   day   = end_tm.tm_mday;

   if (--day == 0)   {
      if (--month == 0) {
         year--;
         month = 12;
      }

      day = g_date_get_days_in_month (month, year);
   }

   egg_datetime_set_clamp_date (edt, start_tm.tm_year + 1900, start_tm.tm_mon + 1, start_tm.tm_mday, year, month, day);
}

/**
 * egg_datetime_get_clamp_date:
 * @edt: an #EggDateTime
 * @minyear: #guint16 pointer or %NULL
 * @minmonth: #guint8 pointer or %NULL
 * @minday: #guint8 pointer or %NULL
 * @maxyear: #guint16 pointer or %NULL
 * @maxmonth: #guint8 pointer or %NULL
 * @maxday: #guint8 pointer or %NULL
 *
 * Returns the current date limit settings.
 **/
void
egg_datetime_get_clamp_date (EggDateTime *edt,
              GDateYear   *minyear,
              GDateMonth  *minmonth,
              GDateDay    *minday,
              GDateYear   *maxyear,
              GDateMonth  *maxmonth,
              GDateDay    *maxday)
{
   g_return_if_fail (edt != NULL);
   g_return_if_fail (EGG_IS_DATETIME (edt));

   if (minyear)
      *minyear = edt->priv->clamp_minyear;
   if (minmonth)
      *minmonth = edt->priv->clamp_minmonth;
   if (minday)
      *minday = edt->priv->clamp_minday;
   if (maxyear)
      *maxyear = edt->priv->clamp_maxyear;
   if (maxmonth)
      *maxmonth = edt->priv->clamp_maxmonth;
   if (maxday)
      *maxday = edt->priv->clamp_maxday;
}

/**
 * egg_datetime_get_clamp_time:
 * @edt: an #EggDateTime
 * @minhour: #guint8 pointer or %NULL
 * @minminute: #guint8 pointer or %NULL
 * @minsecond: #guint8 pointer or %NULL
 * @maxhour: #guint8 pointer or %NULL
 * @maxminute: #guint8 pointer or %NULL
 * @maxsecond: #guint8 pointer or %NULL
 *
 * Returns the current time limit settings.
 **/
void
egg_datetime_get_clamp_time (EggDateTime *edt, guint8 *minhour, guint8 *minminute, guint8 *minsecond, guint8 *maxhour, guint8 *maxminute, guint8 *maxsecond)
{
   g_return_if_fail (edt != NULL);
   g_return_if_fail (EGG_IS_DATETIME (edt));

   if (minhour)
      *minhour = edt->priv->clamp_minhour;
   if (minminute)
      *minminute = edt->priv->clamp_minminute;
   if (minsecond)
      *minsecond = edt->priv->clamp_minsecond;
   if (maxhour)
      *maxhour = edt->priv->clamp_maxhour;
   if (maxminute)
      *maxminute = edt->priv->clamp_maxminute;
   if (maxsecond)
      *maxsecond = edt->priv->clamp_maxsecond;
}

/**
 * egg_datetime_get_date_layout:
 * @edt: an #EggDateTime
 *
 * Gets the PangoLayout used to display the date. See gtk_entry_get_layout()
 * for more information. The returned layout is owned by the date/time
 * widget so need not be freed by the caller.
 *
 * Return value: the #PangoLayout for this widget's date part
 */
PangoLayout *
egg_datetime_get_date_layout (EggDateTime *edt)
{
   g_return_val_if_fail (edt != NULL, NULL);
   g_return_val_if_fail (EGG_IS_DATETIME (edt), NULL);

   return gtk_entry_get_layout (GTK_ENTRY (edt->priv->date_entry));
}

/**
 * egg_datetime_get_time_layout:
 * @edt: an #EggDateTime
 *
 * Gets the PangoLayout used to display the time. See gtk_entry_get_layout()
 * for more information. The returned layout is owned by the date/time
 * widget so need not be freed by the caller.
 *
 * Return value: the #PangoLayout for this widget's time part
 */
PangoLayout *
egg_datetime_get_time_layout (EggDateTime *edt)
{
   g_return_val_if_fail (edt != NULL, NULL);
   g_return_val_if_fail (EGG_IS_DATETIME (edt), NULL);

   return gtk_entry_get_layout (GTK_ENTRY (edt->priv->time_entry));
}

/**************************************************************************/

/* This is a private time list widget implementation for use as time popup.
 */

static GtkWidget *
timelist_new (void)
{
   GtkWidget *timelist;
   GtkWidget *list;
   GtkListStore *model;
   GtkTreeSelection *selection;
   GtkCellRenderer *renderer;

   timelist = gtk_scrolled_window_new (NULL, NULL);
   gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (timelist),
               GTK_POLICY_NEVER,
               GTK_POLICY_ALWAYS);

   model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_UINT);
   list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
   gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list), FALSE);
   gtk_widget_show (list);

   renderer = gtk_cell_renderer_text_new ();
   gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (list),
                       -1,
                       _("Time"),
                       renderer,
                       "text", 0,
                       NULL);

   gtk_container_add (GTK_CONTAINER (timelist), list);

   selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
   gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

   timelist_set_list (TIMELIST (timelist), 00, 00, 23, 59);

   return timelist;
}

static void
timelist_set_list (Timelist *timelist,
         guint8 minhour, guint8 minminute,
         guint8 maxhour, guint8 maxminute)
{
   GtkWidget *tree = gtk_bin_get_child (GTK_BIN (timelist));
   GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
   gint minidx, maxidx;
   gint i;

   minidx = minhour * 2 + (minminute + 29) / 30;
   maxidx = maxhour * 2 + (maxminute + 29) / 30;

   for (i = minidx; i < maxidx; i++) {
      GtkTreeIter iter;
      gchar *s;
      guint hour, minute;

      hour = i / 2;
      minute = (i % 2) * 30;

      s = get_time_string (hour, minute, 0xff);

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                0, s,
                1, hour * 100 + minute,
                -1);

      g_free (s);
   }
}

static void
timelist_set_time (Timelist *timelist, guint8 hour, guint8 minute)
{
   GtkWidget *tree = gtk_bin_get_child (GTK_BIN (timelist));
   GtkTreeModel *model;
   GtkTreeSelection *selection;
   GtkTreeIter iter;

   model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
   selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));

   if (!gtk_tree_model_get_iter_first (model, &iter))
      return;

   do {
      guint time;

      gtk_tree_model_get (model, &iter, 1, &time, -1);

      if (time / 100 == hour && time % 100 == minute) {
         gtk_tree_selection_select_iter (selection, &iter);
         return;
      }

   } while (gtk_tree_model_iter_next (model, &iter));

   gtk_tree_selection_unselect_all (selection);
}

static gboolean
timelist_get_time (Timelist *timelist, guint8 *hour, guint8 *minute)
{
   GtkWidget *tree = gtk_bin_get_child (GTK_BIN (timelist));
   GtkTreeSelection *selection;
   GtkTreeModel *model;
   GtkTreeIter iter;
   guint time;

   selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));

   if (!gtk_tree_selection_get_selected (selection, &model, &iter))
      return FALSE;

   gtk_tree_model_get (model, &iter, 1, &time, -1);

   if (hour)
      *hour = time / 100;
   if (minute)
      *minute = time % 100;

   return TRUE;
}

static void
timelist_clamp (Timelist *timelist,
      guint8 minhour, guint8 minminute,
      guint8 maxhour, guint8 maxminute)
{
   timelist_set_list (timelist, minhour, minminute, maxhour, maxminute);
}

static void
timelist_selection_cb (Timelist *timelist, GtkTreeSelection *selection)
{
   void (*cb)(gpointer, Timelist *);
   gpointer data;

   cb   = g_object_get_data (G_OBJECT (selection), "cb");
   data = g_object_get_data (G_OBJECT (selection), "data");

   cb (data, timelist);
}

static void
timelist_set_selection_callback (Timelist *timelist, void (*cb)(void), gpointer data)
{
   GtkWidget *tree = gtk_bin_get_child (GTK_BIN (timelist));
   GtkTreeSelection *selection;

   selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));

   g_object_set_data (G_OBJECT (selection), "cb", cb);
   g_object_set_data (G_OBJECT (selection), "data", data);
   g_signal_connect_swapped (G_OBJECT (selection), "changed",
              G_CALLBACK (timelist_selection_cb), timelist);
}
