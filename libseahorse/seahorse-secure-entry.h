/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/*
 * Heavily stripped down for use in pinentry-gtk-2 by Albrecht Dreß
 * <albrecht.dress@arcor.de> Feb. 2004.
 *
 * (C) by Albrecht Dreß 2004 unter the terms of the GNU Lesser General
 * Public License.
 *
 * The entry is now always invisible, uses secure memory methods to
 * allocate the text memory, and all potentially dangerous methods
 * (copy & paste, popup, etc.) have been removed.
 */

#ifndef __SEAHORSE_SECURE_ENTRY_H__
#define __SEAHORSE_SECURE_ENTRY_H__


#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif
    
#define SEAHORSE_TYPE_SECURE_ENTRY                  (seahorse_secure_entry_get_type ())
#define SEAHORSE_SECURE_ENTRY(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SECURE_ENTRY, SeahorseSecureEntry))
#define SEAHORSE_SECURE_ENTRY_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SECURE_ENTRY, SeahorseSecureEntryClass))
#define SEAHORSE_IS_SECURE_ENTRY(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SECURE_ENTRY))
#define SEAHORSE_IS_SECURE_ENTRY_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SECURE_ENTRY))
#define SEAHORSE_SECURE_ENTRY_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SECURE_ENTRY, SeahorseSecureEntryClass))
    
typedef struct _SeahorseSecureEntry SeahorseSecureEntry;
typedef struct _SeahorseSecureEntryClass SeahorseSecureEntryClass;

struct _SeahorseSecureEntry {
    GtkWidget widget;

    gchar *text;

    guint overwrite_mode : 1;

    /* length in use, in chars */
    guint16 text_length; 
    guint16 text_max_length;

    /*< private > */
    GdkWindow *text_area;
    GtkIMContext *im_context;

    gint current_pos;
    gint selection_bound;

    PangoLayout *cached_layout;
    guint cache_includes_preedit : 1;

    guint need_im_reset : 1;

    guint has_frame : 1;

    guint activates_default : 1;

    guint cursor_visible : 1;

    /* Flag so we don't select all when clicking in entry to focus in */
    guint in_click : 1;

    /* Only used by GtkCellRendererText */
    guint is_cell_renderer : 1;
    guint editing_canceled : 1;    

    guint mouse_cursor_obscured : 1;

    /* PangoDirection */
    guint resolved_dir : 4; 

    guint button;
    guint blink_timeout;
    guint recompute_idle;
    gint scroll_offset;
    gint ascent;            /* font ascent, in pango units  */
    gint descent;           /* font descent, in pango units  */

    guint16 text_size;      /* allocated size, in bytes */
    guint16 n_bytes;        /* length in use, in bytes */

    guint16 preedit_length; /* length of preedit string, in bytes */
    guint16 preedit_cursor; /* offset of cursor within preedit string, in chars */

    gunichar invisible_char;
    gint width_chars;
    
    gboolean visibility;
    
    /* Keeps track of whether changed between resets */
    gboolean changed;
};

struct _SeahorseSecureEntryClass {
    GtkWidgetClass parent_class;

    /* Action signals */
    void (*activate) (SeahorseSecureEntry *entry);
    void (*move_cursor) (SeahorseSecureEntry *entry, GtkMovementStep step,
                         gint count, gboolean extend_selection);
    void (*insert_at_cursor) (SeahorseSecureEntry *entry, const gchar *str);
    void (*delete_from_cursor) (SeahorseSecureEntry *entry, GtkDeleteType type, gint count);

    /* Padding for future expansion */
    void (*_gtk_reserved1) (void);
    void (*_gtk_reserved2) (void);
    void (*_gtk_reserved3) (void);
    void (*_gtk_reserved4) (void);
};

GType               seahorse_secure_entry_get_type              (void) G_GNUC_CONST;

GtkWidget*          seahorse_secure_entry_new                   (void);

void                seahorse_secure_entry_reset_changed         (SeahorseSecureEntry *entry);

gboolean            seahorse_secure_entry_get_changed           (SeahorseSecureEntry *entry);

void                seahorse_secure_entry_set_visibility        (SeahorseSecureEntry *entry, gboolean setting);

gboolean            seahorse_secure_entry_get_visibility        (SeahorseSecureEntry *entry);

void                seahorse_secure_entry_set_invisible_char    (SeahorseSecureEntry *entry, gunichar ch);

gunichar            seahorse_secure_entry_get_invisible_char    (SeahorseSecureEntry *entry);

void                seahorse_secure_entry_set_has_frame         (SeahorseSecureEntry *entry, gboolean setting);

gboolean            seahorse_secure_entry_get_has_frame         (SeahorseSecureEntry *entry);

/* text is truncated if needed */
void                seahorse_secure_entry_set_max_length        (SeahorseSecureEntry *entry, gint max);

gint                seahorse_secure_entry_get_max_length        (SeahorseSecureEntry *entry);

void                seahorse_secure_entry_set_activates_default (SeahorseSecureEntry *entry, gboolean setting);

gboolean            seahorse_secure_entry_get_activates_default (SeahorseSecureEntry *entry);

void                seahorse_secure_entry_set_width_chars       (SeahorseSecureEntry *entry, gint n_chars);

gint                seahorse_secure_entry_get_width_chars       (SeahorseSecureEntry *entry);

/* Somewhat more convenient than the GtkEditable generic functions */
void                seahorse_secure_entry_set_text              (SeahorseSecureEntry *entry, const gchar *text);

/* returns a reference to the text */
const gchar*        seahorse_secure_entry_get_text              (SeahorseSecureEntry *entry);

PangoLayout*        seahorse_secure_entry_get_layout            (SeahorseSecureEntry *entry);

void                seahorse_secure_entry_get_layout_offsets    (SeahorseSecureEntry *entry, gint *x, gint *y);

#ifdef __cplusplus
}
#endif

#endif  /*__SEAHORSE_SECURE_ENTRY_H__ */
