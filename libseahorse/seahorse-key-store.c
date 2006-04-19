/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <gnome.h>

#include "seahorse-key-store.h"
#include "seahorse-pgp-key.h"
#include "seahorse-gconf.h"
#include "seahorse-gtkstock.h"

#ifdef WITH_SSH
#include "seahorse-ssh-key.h"
#endif 

enum {
	PROP_0,
	PROP_KEYSET,
    PROP_MODE,
    PROP_FILTER
};

static const gchar* col_ids[] = {
    KEY_STORE_BASE_IDS
};

static const GType col_types[] = {
    KEY_STORE_BASE_TYPES
};

struct _SeahorseKeyStorePriv {    
    GtkTreeModelFilter      *filter;
    GtkTreeModelSort        *sort;
    
    SeahorseKeyStoreMode    filter_mode;
    gchar*                  filter_text;
    guint                   filter_stag;
};

G_DEFINE_TYPE (SeahorseKeyStore, seahorse_key_store, SEAHORSE_TYPE_KEY_MODEL);

static GObject* seahorse_key_store_constructor  (GType type, guint n_props, 
                                                 GObjectConstructParam* props);
static void seahorse_key_store_dispose          (GObject       *gobject);
static void	seahorse_key_store_finalize         (GObject       *gobject);

static void	seahorse_key_store_set_property		(GObject		*gobject,
							 guint			prop_id,
							 const GValue		*value,
							 GParamSpec		*pspec);
static void	seahorse_key_store_get_property		(GObject		*gobject,
							 guint			prop_id,
							 GValue			*value,
							 GParamSpec		*pspec);
/* Virtual methods */
static gboolean  seahorse_key_store_append  (SeahorseKeyStore   *skstore,
                                             SeahorseKey        *skey,
                                             guint              uid,
	                                         GtkTreeIter        *iter);
static void	     seahorse_key_store_set     (SeahorseKeyStore   *skstore,
                                             SeahorseKey        *skey,
                                             guint              uid,
                							 GtkTreeIter        *iter);
static void	     seahorse_key_store_changed (SeahorseKeyStore   *skstore,
                                             SeahorseKey        *skey,
                                             guint              uid,
                                             GtkTreeIter        *iter,
                                             SeahorseKeyChange  change);

/* Key source signals */
static void seahorse_key_store_key_added    (SeahorseKeyset     *skset,
                                             SeahorseKey        *skey,
                                             SeahorseKeyStore   *skstore);
static void seahorse_key_store_key_removed  (SeahorseKeyset     *skset,
                                             SeahorseKey        *skey,
                                             gpointer           closure,
                                             SeahorseKeyStore   *skstore);
static void seahorse_key_store_key_changed  (SeahorseKeyset     *skset,
                                             SeahorseKey        *skey,
                                             SeahorseKeyChange  change,
                                             gpointer           closure,
                                             SeahorseKeyStore   *skstore);

/* Filter row method */
static gboolean filter_callback             (GtkTreeModel *model,
                                             GtkTreeIter *iter,
                                             gpointer data);

static void
seahorse_key_store_class_init (SeahorseKeyStoreClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
    seahorse_key_store_parent_class = g_type_class_peek_parent (klass);

    gobject_class->constructor = seahorse_key_store_constructor;
    gobject_class->finalize = seahorse_key_store_finalize;
    gobject_class->dispose = seahorse_key_store_dispose;
    gobject_class->set_property = seahorse_key_store_set_property;
    gobject_class->get_property = seahorse_key_store_get_property;
    
    klass->append = seahorse_key_store_append;
    klass->set = seahorse_key_store_set;
    klass->changed = seahorse_key_store_changed;
  
    /* Class defaults. Derived classes should override */
    klass->use_check = FALSE;
    klass->use_icon = FALSE;
    klass->n_columns = KEY_STORE_NCOLS;
    klass->col_types = col_types;
    klass->col_ids = col_ids;
    
    g_object_class_install_property (gobject_class, PROP_KEYSET,
        g_param_spec_object ("keyset", "Seahorse Keyset", "Current Seahorse Key Source to use",
                             SEAHORSE_TYPE_KEYSET, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_MODE,
        g_param_spec_uint ("mode", "Key Store Mode", "Key store mode controls which keys to display",
                           0, KEY_STORE_MODE_FILTERED, KEY_STORE_MODE_ALL, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_FILTER,
        g_param_spec_string ("filter", "Key Store Filter", "Key store filter for when in filtered mode",
                             "", G_PARAM_READWRITE));
}

static void
seahorse_key_store_init (SeahorseKeyStore *skstore)
{
    
}

static GObject*  
seahorse_key_store_constructor (GType type, guint n_props, GObjectConstructParam* props)
{
    GObject* obj = G_OBJECT_CLASS (seahorse_key_store_parent_class)->constructor (type, n_props, props);
    SeahorseKeyStore* skstore = SEAHORSE_KEY_STORE (obj);

    /* init private vars */
    skstore->priv = g_new0 (SeahorseKeyStorePriv, 1);
 
    /* Setup the store */
    guint cols = SEAHORSE_KEY_STORE_GET_CLASS (skstore)->n_columns;
    GType* types = (GType*)SEAHORSE_KEY_STORE_GET_CLASS (skstore)->col_types;
    seahorse_key_model_set_column_types (SEAHORSE_KEY_MODEL (obj), cols, types);
    
    /* Setup the sort and filter */
    skstore->priv->filter = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (obj), NULL));
    gtk_tree_model_filter_set_visible_func (skstore->priv->filter, filter_callback, skstore, NULL);
    skstore->priv->sort = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (skstore->priv->filter)));
    
    return obj;
}

/* dispose of all our internal references */
static void
seahorse_key_store_dispose (GObject *gobject)
{
    SeahorseKeyStore *skstore;
    
    /*
     * Note that after this executes the rest of the object should
     * still work without a segfault. This basically nullifies the 
     * object, but doesn't free it.
     * 
     * This function should also be able to run multiple times.
     */

    skstore = SEAHORSE_KEY_STORE (gobject);  

    if (skstore->skset) {
        g_signal_handlers_disconnect_by_func (skstore->skset, 
                                seahorse_key_store_key_added, skstore);
        g_signal_handlers_disconnect_by_func (skstore->skset, 
                                seahorse_key_store_key_removed, skstore);
        g_signal_handlers_disconnect_by_func (skstore->skset, 
                                seahorse_key_store_key_changed, skstore);
        g_object_unref (skstore->skset);        
        skstore->skset = NULL;
    }
    
    G_OBJECT_CLASS (seahorse_key_store_parent_class)->dispose (gobject);
}
    
static void
seahorse_key_store_finalize (GObject *gobject)
{
    SeahorseKeyStore *skstore = SEAHORSE_KEY_STORE (gobject);
    g_assert (skstore->skset == NULL);
    
    /* These were allocated in the constructor */
    g_object_unref (skstore->priv->sort);
    g_object_unref (skstore->priv->filter);
     
    /* Allocated in property setter */
    g_free (skstore->priv->filter_text); 
    
    G_OBJECT_CLASS (seahorse_key_store_parent_class)->finalize (gobject);
}

/* Refilter the tree */
static gboolean
refilter_now(SeahorseKeyStore* skstore)
{
    seahorse_keyset_refresh (skstore->skset);
    gtk_tree_model_filter_refilter (skstore->priv->filter);    
    skstore->priv->filter_stag = 0;
    return FALSE;
}

/* Refilter the tree after a timeout has passed */
static void
refilter_later(SeahorseKeyStore* skstore)
{
    if (skstore->priv->filter_stag != 0)
        g_source_remove (skstore->priv->filter_stag);
    skstore->priv->filter_stag = g_timeout_add (200, (GSourceFunc)refilter_now, skstore);
}

static void
seahorse_key_store_set_property (GObject *gobject, guint prop_id,
				 const GValue *value, GParamSpec *pspec)
{
	SeahorseKeyStore *skstore;
    const gchar* t;
	
	skstore = SEAHORSE_KEY_STORE (gobject);
	
	switch (prop_id) {
        case PROP_KEYSET:
            g_assert (skstore->skset == NULL);
            skstore->skset = g_value_get_object (value);
            g_object_ref (skstore->skset);
            g_signal_connect_after (skstore->skset, "added",
                G_CALLBACK (seahorse_key_store_key_added), skstore);
            g_signal_connect_after (skstore->skset, "removed",
                G_CALLBACK (seahorse_key_store_key_removed), skstore);
            g_signal_connect_after (skstore->skset, "changed",
                G_CALLBACK (seahorse_key_store_key_changed), skstore);
			break;
			
		/* The filtering mode */
        case PROP_MODE:
            if (skstore->priv->filter_mode != g_value_get_uint (value)) {
                skstore->priv->filter_mode = g_value_get_uint (value);
                refilter_later (skstore);
            }
            break;
            
         /* The filter text */
        case PROP_FILTER:
            t = g_value_get_string (value);
            
            /* 
             * If we're not in filtered mode and there is text OR
             * we're in filtered mode (regardless of text or not)
             * then update the filter
             */
            if ((skstore->priv->filter_mode != KEY_STORE_MODE_FILTERED && t && t[0]) ||
                (skstore->priv->filter_mode == KEY_STORE_MODE_FILTERED)) {
                skstore->priv->filter_mode = KEY_STORE_MODE_FILTERED;
                g_free (skstore->priv->filter_text);
				/* We always use lower case text (see filter_callback) */
                skstore->priv->filter_text = g_utf8_strdown (t, -1);
                refilter_later (skstore);
            }
            break;
		default:
			break;
	}
}

static void
seahorse_key_store_get_property (GObject *gobject, guint prop_id,
				 GValue *value, GParamSpec *pspec)
{
	SeahorseKeyStore *skstore;
	
	skstore = SEAHORSE_KEY_STORE (gobject);
	
	switch (prop_id) {
		case PROP_KEYSET:
			g_value_set_object (value, skstore->skset);
			break;
			
		/* The filtering mode */
        case PROP_MODE:
            g_value_set_uint (value, skstore->priv->filter_mode);
            break;
        
        /* The filter text. Note that we act as if we don't have any 
         * filter text when not in filtering mode */
        case PROP_FILTER:
            g_value_set_string (value, 
                skstore->priv->filter_mode == KEY_STORE_MODE_FILTERED ? skstore->priv->filter_text : "");
            break;
            
		default:
			break;
	}
}

/* Sets attributes then appends a new #SeahorseKeyRow */
static gboolean
seahorse_key_store_append (SeahorseKeyStore *skstore, SeahorseKey *skey, 
                           guint uid, GtkTreeIter *iter)
{
    SEAHORSE_KEY_STORE_GET_CLASS (skstore)->set (skstore, skey, uid, iter);
    seahorse_key_model_set_row_key (SEAHORSE_KEY_MODEL (skstore), iter, skey);
    return FALSE;
}

/* Sets Name and KeyID */
static void
seahorse_key_store_set (SeahorseKeyStore *skstore, SeahorseKey *skey, 
                        guint uid, GtkTreeIter *iter)
{
    gchar *userid = seahorse_key_get_name (skey, uid);
    gboolean sec = seahorse_key_get_etype (skey) == SKEY_PRIVATE;
    const gchar *stockid = NULL;

#ifdef WITH_SSH    
    if (seahorse_key_get_ktype (skey) == SKEY_SSH)
        stockid = SEAHORSE_STOCK_KEY_SSH;
    else
#endif
    if(seahorse_key_get_ktype (skey) == SKEY_PGP) {
        if (uid == 0)
            stockid = sec ? SEAHORSE_STOCK_SECRET : SEAHORSE_STOCK_KEY;
    }
    
	gtk_tree_store_set (GTK_TREE_STORE (skstore), iter,
        KEY_STORE_CHECK, FALSE,
        KEY_STORE_PAIR, uid == 0 ? sec : FALSE,
        KEY_STORE_STOCK_ID, stockid,
		KEY_STORE_NAME, userid,
		KEY_STORE_KEYID, seahorse_key_get_short_keyid (skey),
        KEY_STORE_UID, uid, -1);
    g_free (userid);
}

/* Refreshes key if uids have changed */
static void
seahorse_key_store_changed (SeahorseKeyStore *skstore, SeahorseKey *skey, guint uid,
                            GtkTreeIter *iter, SeahorseKeyChange change)
{
	switch (change) {
        case SKEY_CHANGE_ALL:     
		case SKEY_CHANGE_UIDS:
			SEAHORSE_KEY_STORE_GET_CLASS (skstore)->set (skstore, skey, uid, iter);
			break;
		default:
			break;
	}
}

/* Appends @skey and all uids */
static void
seahorse_key_store_key_added (SeahorseKeyset *skset, SeahorseKey *skey, SeahorseKeyStore *skstore)
{
	GtkTreeIter iter;
	guint i, uids = seahorse_key_get_num_names (skey);
 
    for (i = 0; i < uids; i++) {
    	if(!SEAHORSE_KEY_STORE_GET_CLASS (skstore)->append (skstore, skey, i, &iter))
            break;
    }
}

/* Removes all rows for key */
static void
seahorse_key_store_key_removed (SeahorseKeyset *skset, SeahorseKey *skey, 
                                gpointer closure, SeahorseKeyStore *skstore)
{
    seahorse_key_model_remove_rows_for_key (SEAHORSE_KEY_MODEL (skstore), skey);
}

/* Calls virtual |changed| for all relevant uids. adds new uids if necessary */
static void
seahorse_key_store_key_changed (SeahorseKeyset *skset, SeahorseKey *skey, 
                                SeahorseKeyChange change, gpointer closure, SeahorseKeyStore *skstore)
{
    guint i, uid, old_uids, num_uids;
    GtkTreeIter first;
    GtkTreeIter *iter;
    GSList *rows, *l;
    
    old_uids = 0;
    num_uids = seahorse_key_get_num_names (skey);
    rows = seahorse_key_model_get_rows_for_key (SEAHORSE_KEY_MODEL (skstore), skey);
    
    for (l = rows; l; l = g_slist_next (l)) {
        
        iter = (GtkTreeIter*)l->data;
        gtk_tree_model_get (GTK_TREE_MODEL (skstore), iter, KEY_STORE_UID, &uid, -1);
        
        /* Remove any extra rows */
        if (uid >= num_uids) {
            seahorse_key_model_set_row_key (SEAHORSE_KEY_MODEL (skstore), iter, NULL);
            gtk_tree_store_remove (GTK_TREE_STORE (skstore), iter);
            continue;
        }

        SEAHORSE_KEY_STORE_GET_CLASS (skstore)->changed (skstore, skey, uid, iter, change);

        /* The top parent row */
        if (uid == 0)
            memcpy (&first, iter, sizeof (first));            

        /* Find the max uid on the rows */
        if (uid >= old_uids)
            old_uids = uid + 1;
    }

    /* Add all new rows */    
    for (i = old_uids; i < num_uids; i++)
        SEAHORSE_KEY_STORE_GET_CLASS (skstore)->append (skstore, skey, i, &first);
    
    seahorse_key_model_free_rows (rows);
}

/* Update the sort order for a column */
static void
set_sort_to (SeahorseKeyStore *skstore, const gchar *name)
{
    GtkTreeSortable* sort;
    gint i, id = -1;
    GtkSortType ord = GTK_SORT_ASCENDING;
    const gchar* n;
    
    /* Prefix with a minus means descending */
    if (name[0] == '-') {
        ord = GTK_SORT_DESCENDING;
        name++;
    }
    
    /* Prefix with a plus means ascending */
    else if (name[0] == '+') {
        ord = GTK_SORT_ASCENDING;
        name++;
    }
    
    /* Find the column id */
    for (i = SEAHORSE_KEY_STORE_GET_CLASS (skstore)->n_columns - 1; i >= 0 ; i--) {
        n = SEAHORSE_KEY_STORE_GET_CLASS (skstore)->col_ids[i];
        if (n && g_ascii_strcasecmp (name, n) == 0) {
            id = i;
            break;
        }
    }
    
    if (id != -1) {
        sort = GTK_TREE_SORTABLE (skstore->priv->sort);
        gtk_tree_sortable_set_sort_column_id (sort, id, ord);
    }
}

/* Called when the column sort is changed */
static void
sort_changed(GtkTreeSortable *sort, gpointer user_data)
{
    gint id;
    GtkSortType ord;
    SeahorseKeyStore *skstore;
    SeahorseKeyStoreClass *klass;
    const gchar* t;
    gchar* x;
    
    skstore = SEAHORSE_KEY_STORE (user_data);
    klass = SEAHORSE_KEY_STORE_GET_CLASS (skstore);
    
    if (!klass->gconf_sort_key)
        return;
        
    /* We have a sort so save it */
    if (gtk_tree_sortable_get_sort_column_id (sort, &id, &ord)) {
        if (id >= 0 && id < klass->n_columns) {
            t = klass->col_ids[id];
            if (t != NULL) {
                x = g_strconcat (ord == GTK_SORT_DESCENDING ? "-" : "", t, NULL);
                seahorse_gconf_set_string (klass->gconf_sort_key, x);
                g_free (x);
            }
        }
    }
    
    /* No sort so save blank */
    else {
        seahorse_gconf_set_string (klass->gconf_sort_key, "");
    }
}

/* Try to find our key store given a tree model */
static SeahorseKeyStore* 
key_store_from_model (GtkTreeModel *model)
{
	/* Sort models are what's set on the tree */
    if (GTK_IS_TREE_MODEL_SORT (model)) {
        model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (model));
        model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
    }
    
    if (SEAHORSE_IS_KEY_STORE (model))
        return SEAHORSE_KEY_STORE (model);
    
    g_assert_not_reached ();
    return NULL;
}

/* Called when a checkbox is toggled, toggle the value */
static void
check_toggled(GtkCellRendererToggle *cellrenderertoggle, gchar *path, gpointer user_data)
{
    GtkTreeView *view = GTK_TREE_VIEW (user_data);
    SeahorseKeyStore *skstore = key_store_from_model (gtk_tree_view_get_model (view));
    GtkTreeModel* fmodel = GTK_TREE_MODEL (skstore->priv->sort);
    GtkTreeSelection *selection;
    gboolean prev = FALSE;
    GtkTreeIter iter;
    GtkTreeIter child;
    GValue v;

    memset (&v, 0, sizeof(v));
    g_assert (path != NULL);
    g_return_if_fail (gtk_tree_model_get_iter_from_string (fmodel, &iter, path));
    
    /* We get notified in filtered coordinates, we have to convert those to base */
    seahorse_key_store_get_base_iter (skstore, &child, &iter);
 
    gtk_tree_model_get_value (GTK_TREE_MODEL (skstore), &child, KEY_STORE_CHECK, &v);
    if(G_VALUE_TYPE (&v) == G_TYPE_BOOLEAN)
        prev = g_value_get_boolean (&v);
    g_value_unset (&v);    
    
    gtk_tree_store_set (GTK_TREE_STORE (skstore), &child, KEY_STORE_CHECK, prev ? FALSE : TRUE, -1);

    selection = gtk_tree_view_get_selection (view);
    g_signal_emit_by_name (selection, "changed");    
}

static void
row_activated (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *arg2, 
               SeahorseKeyStore *skstore)
{
    GtkTreeModel* fmodel = GTK_TREE_MODEL (skstore->priv->sort);
    GtkTreeSelection *selection;
    gboolean prev = FALSE;
    GtkTreeIter iter;
    GtkTreeIter child;
    GValue v;

    memset (&v, 0, sizeof(v));
    g_assert (path != NULL);
    g_return_if_fail (gtk_tree_model_get_iter (fmodel, &iter, path));
    
    /* We get notified in filtered coordinates, we have to convert those to base */
    seahorse_key_store_get_base_iter (skstore, &child, &iter);
 
    gtk_tree_model_get_value (GTK_TREE_MODEL (skstore), &child, KEY_STORE_CHECK, &v);
    if(G_VALUE_TYPE (&v) == G_TYPE_BOOLEAN)
        prev = g_value_get_boolean (&v);
    g_value_unset (&v);    
    
    gtk_tree_store_set (GTK_TREE_STORE (skstore), &child, KEY_STORE_CHECK, prev ? FALSE : TRUE, -1);

    selection = gtk_tree_view_get_selection (treeview);
    g_signal_emit_by_name (selection, "changed");    
}

/**
 * seahorse_key_store_init:
 * @skstore: #SeahorseKeyStore to initialize
 * @view: #GtkTreeView that will show @skstore
 *
 * Initializes @skstore with default columns and embeds in @view.
 * This must be called after creating a new #SeahorseKeyStore.
 **/
void
seahorse_key_store_initialize (SeahorseKeyStore *skstore, GtkTreeView *view)
{
	GtkTreeViewColumn *col;

    seahorse_key_store_populate (skstore);

    /* The sorted model is the top level model */   
    g_assert (GTK_IS_TREE_MODEL (skstore->priv->sort));
    gtk_tree_view_set_model (view, GTK_TREE_MODEL (skstore->priv->sort));
 
 	/* When using checks we add a check column */
    if (SEAHORSE_KEY_STORE_GET_CLASS (skstore)->use_check) {
        GtkCellRenderer *renderer;
        renderer = gtk_cell_renderer_toggle_new ();
        g_signal_connect (renderer, "toggled", G_CALLBACK (check_toggled), view);
        col = gtk_tree_view_column_new_with_attributes ("", renderer, "active", KEY_STORE_CHECK, NULL);
        gtk_tree_view_column_set_resizable (col, FALSE);
        gtk_tree_view_append_column (view, col);
        g_signal_connect (view, "row_activated", G_CALLBACK (row_activated), skstore);
    }
    
    /* When using key pair icons, we add an icon column */
    if (SEAHORSE_KEY_STORE_GET_CLASS (skstore)->use_icon) {
    	GtkCellRenderer  *renderer = gtk_cell_renderer_pixbuf_new ();
        g_object_set (renderer, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
    	col = gtk_tree_view_column_new_with_attributes ("", renderer, 
   												"stock-id", KEY_STORE_STOCK_ID,
   												NULL);
										
        gtk_tree_view_column_set_resizable (col, FALSE);
        gtk_tree_view_append_column (view, col);
        
        gtk_tree_view_column_set_sort_column_id (col, KEY_STORE_PAIR);
      }
      
	col = seahorse_key_store_append_column (view, _("Name"), KEY_STORE_NAME);
	gtk_tree_view_column_set_sort_column_id (col, KEY_STORE_NAME);
	
	seahorse_key_store_append_column (view, _("Key ID"), KEY_STORE_KEYID);

    if (SEAHORSE_KEY_STORE_GET_CLASS (skstore)->gconf_sort_key) {
        /* Also watch for sort-changed on the store */
        g_signal_connect (skstore->priv->sort, "sort-column-changed", G_CALLBACK (sort_changed), skstore);
    }
}

/**
 * seahorse_key_store_populate:
 * @skstore: #SeahorseKeyStore to populate with #SeahorseKeys
 *
 * A new #SeahorseKeyStore is initially empty. Call this method to populate
 * the #SeahorseKeyStore with #SeahorseKeys.
 **/
void
seahorse_key_store_populate (SeahorseKeyStore *skstore)
{
	GList *keys, *list = NULL;
	SeahorseKey *skey;
	
    g_return_if_fail (SEAHORSE_IS_KEY_STORE (skstore));
    g_return_if_fail (SEAHORSE_IS_KEYSET (skstore->skset));
	
    /* Don't precipitate a load */
    if (seahorse_keyset_get_count (skstore->skset) > 0) {

    	keys = list = seahorse_keyset_get_keys (skstore->skset);
	
    	while (list != NULL && (skey = list->data) != NULL) {
            seahorse_key_store_key_added (skstore->skset, skey, skstore);
    		list = g_list_next (list);
    	}
     
        g_list_free (keys);
    }
}

/* Given an iterator find the associated key */
static SeahorseKey*
key_from_iterator (GtkTreeModel* model, GtkTreeIter* iter, guint *uid)
{
    GtkTreeIter i;
    SeahorseKey *skey = NULL;
    
    /* Convert to base iter if necessary */
    if (!SEAHORSE_IS_KEY_STORE (model)) {
        SeahorseKeyStore* skstore = key_store_from_model(model);
        seahorse_key_store_get_base_iter (skstore, &i, iter);
        
        iter = &i;
        model = GTK_TREE_MODEL (skstore);
    }
    
    skey = seahorse_key_model_get_row_key (SEAHORSE_KEY_MODEL (model), iter);
    if (skey && uid)
        gtk_tree_model_get (model, iter, KEY_STORE_UID, &uid, -1);
    return skey;
}


     
/**
 * seahorse_key_store_get_key_from_path:
 * @view: #GtkTreeView containing @path
 * @path: #GtkTreePath containing a #SeahorseKey
 *
 * Gets the #SeahorseKey at @path in @view.
 *
 * Returns: The #SeahorseKey at @path in @view
 **/
SeahorseKey*
seahorse_key_store_get_key_from_path (GtkTreeView *view, GtkTreePath *path, guint *uid)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
	g_return_val_if_fail (path != NULL, NULL);
	
	model = gtk_tree_view_get_model (view);
	g_return_val_if_fail (gtk_tree_model_get_iter (model, &iter, path), NULL);
    return key_from_iterator (model, &iter, uid);
}

/**
 * seahorse_key_store_append_column:
 * @view: #GtkTreeView to append column to
 * @name: Title of new column
 * @index: Index of new column
 *
 * Creates a new #GtkTreeViewColumn with @name as the title,
 * and appends it to @view at @index.
 *
 * Returns: The created column
 **/
GtkTreeViewColumn*
seahorse_key_store_append_column (GtkTreeView *view, const gchar *label, const gint index)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	SeahorseKeyStore *skstore;
    const gchar *key;
    gchar *sort;
    
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (label, renderer, "text", index, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (view, column);
	
    skstore = key_store_from_model (gtk_tree_view_get_model(view));
    key = SEAHORSE_KEY_STORE_GET_CLASS (skstore)->gconf_sort_key;
    
    /* Update sort order in case the sorted column was added */
    if (key && (sort = seahorse_gconf_get_string (key)) != NULL) {
        set_sort_to (skstore, sort);
        g_free (sort);
    }  
    
	return column;
}

static gint 
compare_pointers (gconstpointer a, gconstpointer b)
{
    if (a == b)
        return 0;
    return a > b ? 1 : -1;
}

GList*              
seahorse_key_store_get_all_keys (GtkTreeView *view)
{
    SeahorseKeyStore* skstore;
    
    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    skstore = key_store_from_model (gtk_tree_view_get_model (view));
    
    return seahorse_keyset_get_keys (skstore->skset);
}

/**
 * seahorse_key_store_get_selected_keys:
 * @view: #GtkTreeView with selection
 *
 * Gets a list of the selected #SeahorseKeys.
 *
 * Returns: The selected #SeahorseKeys
 **/
GList*
seahorse_key_store_get_selected_keys (GtkTreeView *view)
{
    SeahorseKey *skey;
    GList *l, *keys = NULL;
    SeahorseKeyStore* skstore;
    
    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    skstore = key_store_from_model (gtk_tree_view_get_model (view));
    
    if (SEAHORSE_KEY_STORE_GET_CLASS (skstore)->use_check) {
        GtkTreeModel* model = GTK_TREE_MODEL (skstore);
        GtkTreeIter iter;
        gboolean check;
            
        if (gtk_tree_model_get_iter_first (model, &iter)) {
            do {
                check = FALSE;
                gtk_tree_model_get (model, &iter, KEY_STORE_CHECK, &check, -1); 
                if (check) {
                    skey = key_from_iterator (model, &iter, NULL);
                    if (skey != NULL)
                        keys = g_list_append (keys, skey);
                }
            } while (gtk_tree_model_iter_next (model, &iter));
        }
    }
    
    /* Fall back if none checked, or not using checks */
    if (keys == NULL) {
        GList *list, *paths = NULL;
        GtkTreeSelection *selection;    
        
        selection = gtk_tree_view_get_selection (view);
        paths = gtk_tree_selection_get_selected_rows (selection, NULL);
    
        /* make key list */
        for (list = paths; list != NULL; list = g_list_next (list)) {
            skey = seahorse_key_store_get_key_from_path (view, list->data, NULL);
            if (skey != NULL)
                keys = g_list_append (keys, skey);
        }
            
        /* free selected paths */
        g_list_foreach (paths, (GFunc)gtk_tree_path_free, NULL);
        g_list_free (paths);
    }
    
    /* Remove duplicates */
    keys = g_list_sort (keys, compare_pointers);
    for (l = keys; l; l = g_list_next (l)) {
        while (l->next && l->data == l->next->data)
            keys = g_list_delete_link (keys, l->next);
    }    
    
    return keys;
}

/**
 * seahorse_key_store_get_selected_key:
 * @view: #GtkTreeView with selection
 * @uid: Optionally return the selected uid here
 *
 * Sugar method for getting the selected #SeahorseKey from @view.
 *
 * Returns: The selected #SeahorseKey, or NULL there are no selections or
 * more than one selection.
 **/
SeahorseKey*
seahorse_key_store_get_selected_key (GtkTreeView *view, guint *uid)
{
    SeahorseKeyStore* skstore;
	SeahorseKey *skey = NULL;
	
    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    skstore = key_store_from_model (gtk_tree_view_get_model (view));
    
    if (SEAHORSE_KEY_STORE_GET_CLASS (skstore)->use_check) {
        GtkTreeModel* model = GTK_TREE_MODEL (skstore);
        GtkTreeIter iter;
        gboolean check;
            
        if (gtk_tree_model_get_iter_first (model, &iter)) {
            do {
                check = FALSE;
                gtk_tree_model_get (model, &iter, KEY_STORE_CHECK, &check, -1); 
                if (check) {
					skey = key_from_iterator (model, &iter, uid);
					break;
				}
            } while (gtk_tree_model_iter_next (model, &iter));
        }
    }

    /* Fall back if none checked, or not using checks */
    if (skey == NULL) {
    	GList *paths = NULL;
        GtkTreeSelection *selection;	
        
    	selection = gtk_tree_view_get_selection (view);
    	paths = gtk_tree_selection_get_selected_rows (selection, NULL);
	
    	/* make key list */
		if (paths != NULL)
		    skey = seahorse_key_store_get_key_from_path (view, paths->data, uid);
            
        /* free selected paths */
        g_list_foreach (paths, (GFunc)gtk_tree_path_free, NULL);
        g_list_free (paths);
    }
		
	return skey;
}

/* Search through row for text */
static gboolean
row_contains_filtered_text (GtkTreeModel* model, GtkTreeIter* iter, const gchar* text)
{
    gchar* name = NULL;
    gchar* id = NULL;
    gchar* t;
    gboolean ret = FALSE;
    
    /* Empty search text results in a match */
    if (!text || !text[0])
        return TRUE;
    
    /* Note that we only search the name and id */
    gtk_tree_model_get (model, iter, KEY_STORE_NAME, &name, KEY_STORE_KEYID, &id, -1);
    
    if(name) {
        t = g_utf8_strdown (name, -1);
        if (strstr (t, text))
            ret = TRUE;
        g_free (t);
    }
    
    if (!ret && id) {
        t = g_utf8_strdown (id, -1);
        if (strstr (t, text))
            ret = TRUE;
        g_free (t);
    }
    
    g_free (name);
    g_free (id);
    return ret;
}

/* Called to filter each row */
static gboolean 
filter_callback (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    SeahorseKeyStore* skstore = SEAHORSE_KEY_STORE(data);
    GtkTreeIter child;
    gboolean ret = FALSE;
    
    /* Check the row requested */
    switch (skstore->priv->filter_mode)
    {
    case KEY_STORE_MODE_FILTERED:
        ret = row_contains_filtered_text (model, iter, skstore->priv->filter_text);
        break;
        
    case KEY_STORE_MODE_SELECTED:
        if (SEAHORSE_KEY_STORE_GET_CLASS (skstore)->use_check) {
            gboolean check = FALSE;
            gtk_tree_model_get (model, iter, KEY_STORE_CHECK, &check, -1); 
            ret = check;
        }
        break;
        
    case KEY_STORE_MODE_ALL:
        ret = TRUE;
        break;
        
    default:
        g_assert_not_reached ();
        break;
    };
    
    /* If current is not being shown, double check with children */
    if (!ret && gtk_tree_model_iter_children (model, &child, iter)) {
        do {
            ret = filter_callback (model, &child, data);
        } while (!ret && gtk_tree_model_iter_next (model, &child));
    }

    return ret;        
}

/* Given a treeview iter, get the base store iterator */
void 
seahorse_key_store_get_base_iter(SeahorseKeyStore* skstore, GtkTreeIter* base_iter, 
                                        const GtkTreeIter* iter)
{
    GtkTreeIter i;
    
    g_return_if_fail (SEAHORSE_IS_KEY_STORE (skstore));
    g_assert (skstore->priv->sort && skstore->priv->filter);
    
    gtk_tree_model_sort_convert_iter_to_child_iter (skstore->priv->sort, &i, (GtkTreeIter*)iter);
    gtk_tree_model_filter_convert_iter_to_child_iter (skstore->priv->filter, base_iter, &i);
}
