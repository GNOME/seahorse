/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005 Jim Pharis
 * Copyright (C) 2005-2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

// XXX make a subclass for public/private?
// XXX there was a printf_gtkbuilder_widget() here, I removed it but did not use .text first
public class Seahorse.Pgp.KeyProperties : Gtk.Dialog {

    private Key key;

    private GpgME.Photo? current_photo_id; // XXX Dunno if this is only in private or public

    private Gtk.Box expired_area;
    private Gtk.Label expired_message;
    private Gtk.Box revoked_area;
    // Owner shizzle
    private Gtk.Label owner_name_label;
    private Gtk.Label owner_email_label;
    private Gtk.Label owner_comment_label;
    private Gtk.Label owner_keyid_label;
    private Gtk.Button owner_photo_previous_button;
    private Gtk.Button owner_photo_next_button;
    // Details shizzle
    private Gtk.Label details_id_label;
    private Gtk.Label details_algo_label;
    private Gtk.Label details_strength_label;
    private Gtk.Label details_fingerprint_label;
    private Gtk.Label details_created_label;
    private Gtk.Label details_expires_label;
    private Gtk.ComboBox details_trust_combobox;
    private Gtk.TreeView details_subkey_tree;

    // ONLY PUBLIC KEY
    private Gtk.Box? trust_page;
    private Gtk.Expander? uids_area;
    private Gtk.TreeView? signatures_tree;
    private Gtk.TreeView? owner_userid_tree;
    private Gtk.CheckButton? trust_marginal_check;
    private Gtk.Label? trust_sign_label;
    private Gtk.Label? trust_revoke_label;
    private Gtk.Label? manual_trust_area;
    private Gtk.Box? manage_trust_area;
    private Gtk.Box? sign_area;
    private Gtk.Box? revoke_area;
    private Gtk.Image? sign_image;
    private Gtk.CheckButton? signatures_toggle;

    // ONLY PRIVATE KEY
    private Gtk.Button? passphrase_button;
    private Gtk.Frame? owner_photo_frame;
    private Gtk.Button? owner_photo_add_button;
    private Gtk.Button? owner_photo_delete_button;
    private Gtk.Button? owner_photo_primary_button;
    private Gtk.TreeView? names_tree;
    private Gtk.Button? names_primary_button;
    private Gtk.Button? names_delete_button;
    private Gtk.Button? names_sign_button;
    private Gtk.Button? names_revoke_button;
    private Gtk.Button? details_export_button;
    private Gtk.Button? details_add_button;
    private Gtk.Button? details_date_button;
    private Gtk.Button? details_revoke_button;
    private Gtk.Button? details_delete_button;

    // NOT FOUND?
    private Gtk.Image? image_good1;
    private Gtk.Image? image_good2;


    public KeyProperties(Key key, Gtk.Window parent) {
        // XXX Copypasta from the SSH key properties
        /* GLib.Object(border_width: 5, */
        /*             title: _("Key Properties"), */
        /*             resizable: false, */
        /*             transient_for: parent); */
        this.key = key;
        // This causes the key source to get any specific info about the key
        if (pkey is GpgME.Key) {
            key.refresh();
            key.ensure_signatures();
        }

        load_ui();

        seahorse_bind_objects (null, pkey, (SeahorseTransfer)key_notify, swidget);
    }

    // FIXME: normally we would do this using GtkTemplate, but make is abdandoning me :(
    private void load_ui() {
        string basename = (this.key.usage == Seahorse.Usage.PUBLIC_KEY)?
                            "pgp-public-key-properties" : "pgp-private-key-properties";
        Gtk.Builder builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/%s-key-properties.ui".printf(basename);
            builder.add_from_resource(path);
        } catch (GLib.Error err) {
            GLib.critical("%s", err.message);
        }
        Gtk.Container content = (Gtk.Container) builder.get_object(basename);
        ((Gtk.Container)this.get_content_area()).add(content);

        /* this.key_image = (Gtk.Image) builder.get_object("key-image"); */
        /* this.comment_entry = (Gtk.Entry) builder.get_object("comment-entry"); */
        this.expired_area = (Gtk.Box) builder.get_object("expired-area");
        this.revoked_area = (Gtk.Box) builder.get_object("revoked-area");
        this.owner_name_label = (Gtk.Label) builder.get_object("owner-name-label");
        this.owner_email_label = (Gtk.Label) builder.get_object("owner-email-label");
        this.owner_comment_label = (Gtk.Label) builder.get_object("owner-comment-label");
        this.owner_keyid_label = (Gtk.Label) builder.get_object("owner-keyid-label");
        this.owner_photo_previous_button = (Gtk.Button) builder.get_object("owner-photo-previous-button");
        this.owner_photo_next_button = (Gtk.Button) builder.get_object("owner-photo-next-button");

        if (this.key.usage == Seahorse.Usage.PUBLIC_KEY) { // ONLY PUBLIC KEY
        } else {
        }

        // Signals
        /* this.comment_entry.activate.connect(on_ssh_comment_activate); */
    }

    private gpointer get_selected_object(Seahorse.Widget swidget, string objectid, uint column) {
        Gtk.TreeView tree_view = swidget.get_widget(objectid);
        Gtk.TreeModel model = tree_view.get_model();
        Gtk.TreeSelection selection = tree_view.get_selection ();
        assert(selection.get_mode () == GTK_SELECTION_SINGLE);

        List<Gtk.TreePath> rows = selection.get_selected_rows(null);

        if (rows.length() > 0) {
            Gtk.TreeIter iter;
            model.get_iter(&iter, rows.data);
            gpointer object = null;
            model.get(&iter, column, &object, -1);
        }

        return object;
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_signature_row_activated(Gtk.TreeView treeview, Gtk.TreePath path, Gtk.TreeView? Column *arg2) {
        Gtk.TreeModel model = treeview.get_model();

        if (GTK_IS_TREE_MODEL_FILTER (model))
            model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

        Gtk.TreeIter iter;
        if (model.get_iter(out iter, path))
            return;

        GLib.Object object = seahorse_object_model_get_row_key (SEAHORSE_OBJECT_MODEL (model), &iter);
        if (object != null && SEAHORSE_IS_PGP_KEY (object)) {
            seahorse_pgp_key_properties_show (SEAHORSE_PGP_KEY (object), swidget.get_toplevel().get_parent());
        }
    }

    private void unique_strings (string[] keyids) {
        g_ptr_array_sort (keyids, (GCompareFunc)g_ascii_strcasecmp);
        for (uint i = 0; i + 1 < keyids.len; ) {
            if (g_ascii_strcasecmp (keyids.pdata[i], keyids.pdata[i + 1]) == 0)
                g_ptr_array_remove_index (keyids, i);
            else
                i++;
        }
    }

    /* -----------------------------------------------------------------------------
     * NAMES PAGE (PRIVATE KEYS)
     */
    private enum UidSigColumn {
        OBJECT,
        ICON,
        NAME,
        KEYID,
        N_COLUMNS;
    }

    static Type uidsig_columns[] = {
        typeof(GLib.Object),  // index
        typeof(Icon),         // icon
        typeof(string),       // name
        typeof(string)        // keyid
    };

    private Uid names_get_selected_uid () {
        return get_selected_object("names-tree", UidSigColumn.OBJECT);
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_names_add_clicked (Gtk.Widget widget) {
        GObject *obj = SEAHORSE_OBJECT_WIDGET (swidget).object;
        g_return_if_fail (SEAHORSE_IS_GPGME_KEY (obj));
        seahorse_gpgme_add_uid_new (SEAHORSE_GPGME_KEY (obj),
                                    GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget.name)));
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_names_primary_clicked (Gtk.Widget widget) {
        GpgME.Uid? uid = names_get_selected_uid() as GpgME.Uid;
        if (uid == null)
            return;

        GPG.Error gerr = GpgME.KeyOperation.primary_uid(uid);
        if (!err.is_success() || err.is_cancelled())
            Util.show_error(null, _("Couldn’t change primary user ID"), err.strerror());
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_names_delete_clicked (Gtk.Widget widget) {
        GpgME.Uid? uid = names_get_selected_uid() as GpgME.Uid;
        if (uid == null)
            return;

        string message = _("Are you sure you want to permanently delete the “%s” user ID?").printf(uid.label);
        if (!Seahorse.DeleteDialog.prompt(this, message))
            return;

        GPG.Error gerr = GpgME.KeyOperation.del_uid(uid);
        if (!gerr.is_success() || gerr.is_cancelled())
            Util.show_error(null, _("Couldn’t delete user ID"), gerr.strerror());
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_names_sign_clicked (Gtk.Widget widget) {
        GpgME.Uid? uid = names_get_selected_uid() as GpgME.Uid;
        if (uid == null)
            return;

        Sign.prompt_uid(uid, this);
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_names_revoke_clicked (Gtk.Widget widget, gpointer user_data) {
        /* TODO: */
    /*    Seahorse.Object skey;
        int index;
        Glist *keys = null;

        skey = SEAHORSE_OBJECT_WIDGET (swidget).skey;
        index = names_get_selected_uid (swidget);

        if (index >= 1) {
            seahorse_revoke_show (SEAHORSE_PGP_KEY (skey), index - 1);

#ifdef WITH_KEYSERVER
            if (g_settings_get_boolean(AUTOSYNC_KEY) == true) {
                keys = g_list_append (keys, skey);
                seahorse_keyserver_sync (keys);
                g_list_free(keys);
            }
#endif
        }*/
    }

    private void update_names(Gtk.TreeSelection selection) {
        Uid uid = names_get_selected_uid();

        int index = -1;
        if (uid && SEAHORSE_IS_GPGME_UID (uid))
            index = ((GpgME.Uid) uid).get_gpgme_index();

        this.names_primary_button.sensitive = (index > 0);
        this.names_delete_button.sensitive = (index >= 0);
        this.names_sign_button.sensitive = (index >= 0);
        this.names_revoke_button.visible = false;
    }

    /* Is called whenever a signature key changes, to update row */
    private void names_update_row (Seahorse.ObjectModel skmodel, Seahorse.Object object, Gtk.TreeIter iter, Seahorse.Widget swidget) {
        Icon icon = new ThemedIcon((object is Pgp.Key)? Seahorse.ICON_SIGN
                                                      : Gtk.Stock.DIALOG_QUESTION);

        /* TRANSLATORS: [Unknown] signature name */
        skmodel.set(iter, UidSigColumn.OBJECT, null,
                          UidSigColumn.ICON, icon,
                          UidSigColumn.NAME, object.get_markup()?? _("[Unknown]"),
                          UidSigColumn.KEYID, object.get_identifier(), -1);
    }

    private void names_populate(Seahorse.ObjectModel store, Key pkey) {
        // Insert all the fun-ness
        foreach (Uid uid in pkey.uids) {
            Icon icon = new ThemedIcon(Seahorse.ICON_PERSON);

            Gtk.TreeIter uiditer, sigiter;
            store.append(store, out uiditer);
            store.set(uiditer, UidSigColumn.OBJECT, uid,
                               UidSigColumn.ICON, icon,
                               UidSigColumn.NAME, uid.markup, -1);

            // Build a list of all the keyids
            string[] keyids = {};
            foreach (Signature sig in uid.signatures)
                if (!this.key.has_keyid(sig.keyid)) // Never show self signatures, they're implied
                    keyids += sig.keyid;

            // Pass it to 'DiscoverKeys' for resolution/download,
            List<Pgp.Key> keys = Pgp.Backend.discover_keys(null, keyids, new Cancellable());

            /* Add the keys to the store */
            foreach (Pgp.Key key in keys) {
                store.append(&sigiter, &uiditer);

                // This calls the 'update-row' callback, to set the values for the key */
                store.set_row_object(&sigiter, key);
            }
        }
    }

    private void do_names() {
        if (this.key.usage != Seahorse.Usage.PRIVATE_KEY)
            return;

        // Clear/create table store
        if (this.names_tree == null)
            return;

        Gtk.TreeStore? store = this.names_tree.get_model();
        if (store != null) {
            store.clear();
        } else {
            assert(UidSigColumn.N_COLUMNS == G_N_ELEMENTS (uidsig_columns));
            uidsig_columns[UidSigColumn.ICON] = G_TYPE_ICON;

            // This is our first time so create a store
            store = GTK_TREE_STORE (new Seahorse.ObjectModel(UidSigColumn.N_COLUMNS, uidsig_columns));
            store.update_row.connect(names_update_row);

            /* Icon column */
            Gtk.CellRenderer icon_renderer = new Gtk.CellRendererPixbuf();
            icon_renderer.stock_size = Gtk.IconSize.LARGE_TOOLBAR;
            this.names_tree.insert_column_with_attributes(-1, "", icon_renderer,
                                                             "gicon", UidSigColumn.ICON, null);

            /* TRANSLATORS: The name and email set on the PGP key */
            Gtk.CellRenderer name_renderer = new Gtk.CellRendererText();
            name_renderer.yalign = 0.0;
            name_renderer.xalign = 0.0;
            this.names_tree.insert_column_with_attributes(-1, _("Name/Email"), name_renderer,
                                                             "markup", UidSigColumn.NAME, null);

            /* The signature ID column */
            Gtk.CellRenderer signature_renderer = new Gtk.CellRendererText();
            signature_renderer.yalign = 0.0;
            signature_renderer.xalign = 0.0;
            this.names_tree.insert_column_with_attributes(-1, _("Signature ID"), signature_renderer,
                                                             "text", UidSigColumn.KEYID, null);
        }

        names_populate(store, this.key);

        this.names_tree.set_model(store);
        this.names_tree.expand_all();

        update_names();
    }

    private void do_names_signals() {
        if (this.key.usage != Seahorse.Usage.PRIVATE_KEY)
            return;
        if (this.names_tree != null) // Doesn't this contradict with previous condition?
            this.names_tree.get_selection().changed.connect(update_names);
    }

    /* -----------------------------------------------------------------------------
     * PHOTO ID AREA
     */
    /* drag-n-drop uri data */
    private enum DndTarget {
        TARGET_URI,
    }

    static GtkTargetEntry target_list[] = {
        { "text/uri-list", 0, TARGET_URI } };

    static uint n_targets = G_N_ELEMENTS (target_list);

    // XXX G_MODULE_EXPORT
    private void on_pgp_owner_photo_drag_received (Gtk.Widget widget, GdkDragContext *context, int x, int y, GtkSelectionData *sel_data, uint target_type, uint time) {
        g_return_if_fail (SEAHORSE_IS_GPGME_KEY (this.key));

        bool dnd_success = false;

        // This needs to be improved, support should be added for remote images
        // and there has to be a better way to get rid of the trailing \r\n appended
        // to the end of the path after the call to g_filename_from_uri
        int len = 0;
        if ((sel_data != null) && (sel_data.get_length() >= 0)) {
            g_return_if_fail (target_type == TARGET_URI);

            string[] uri_list = gtk_selection_data_get_uris (sel_data);
            while (uri_list && uri_list[len]) {

                string uri = g_filename_from_uri (uri_list[len], null, null);
                if (!uri)
                    continue;

                dnd_success = seahorse_gpgme_photo_add (this.key, GTK_WINDOW (seahorse_widget_get_toplevel (swidget)), uri);

                if (!dnd_success)
                    break;
                len++;
            }
        }

        gtk_drag_finish (context, dnd_success, false, time);
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_owner_photo_add_button (Gtk.Widget widget) {
        g_return_if_fail (SEAHORSE_IS_GPGME_KEY (this.key));

        if (seahorse_gpgme_photo_add (SEAHORSE_GPGME_KEY (this.key), GTK_WINDOW (seahorse_widget_get_toplevel (swidget)), null))
            this.current_photoid = null;
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_owner_photo_delete_button (Gtk.Widget widget) {
        g_return_if_fail (SEAHORSE_IS_GPGME_PHOTO (this.current_photoid));

        if (KeyOperation.photo_delete(this.current_photoid))
            this.current_photoid = null;
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_owner_photo_primary_button (Gtk.Widget widget) {
        g_return_if_fail (SEAHORSE_IS_GPGME_PHOTO (this.current_photoid));

        GPG.Error gerr = KeyOperation.photo_primary(this.current_photoid);
        if (!gerr.is_success() || gerr.is_cancelled())
            Util.show_error(null, _("Couldn’t change primary photo"), gerr.strerror());
    }

    private void set_photoid_state() {
        if (this.current_photoid == null)
            return;

        bool is_gpgme = this.key is GpgME.Key;
        bool is_private_key = this.key.usage == Seahorse.Usage.PRIVATE_KEY;

        this.owner_photo_add_button.visible = (is_gpgme && is_private_key);
        this.owner_photo_primary_button.visible = (is_gpgme && is_private_key && this.key.photos && this.key.photos.next);

        // Display this when there are any photo ids
        this.owner_photo_delete_button.visible = (is_gpgme && is_private_key && this.current_photoid != null);

        // Sensitive when not the first photo id
        this.owner_photo_previous_button.sensitive = (this.current_photoid != null && this.key.photos && this.current_photoid != this.key.photos.first().data);
        this.owner_photo_next_button.sensitive = (this.current_photoid != null && this.key.photos && this.current_photoid != this.key.photos.last().data);

        // Display *both* of these when there are more than one photo id
        this.owner_photo_previous_button.visible = this.key.photos && this.key.photos.next();
        this.owner_photo_next_button.visible = this.key.photos && this.key.photos.next();

        Gtk.Image photo_image = swidget.get_widget ("photoid");
        g_return_if_fail (photo_image);
        if (this.current_photoid.pixbuf != null)
            photo_image.set_from_pixbuf(this.current_photoid.pixbuf);
        else if (is_private_key)
            photo_image.set_from_icon_name(Seahorse.ICON_SECRET, Gtk.IconSize.DIALOG);
        else
            photo_image.set_from_icon_name(Seahorse.ICON_KEY, Gtk.IconSize.DIALOG);
    }

    private void do_photo_id() {
        this.current_photoid = (this.key.photos != null)? this.key.photos.data : null;
        set_photoid_state();
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_owner_photoid_next(Gtk.Widget widget) {
        if (this.current_photoid != null && (this.current_photoid in this.key.photos) && this.key.photos.next != null)
            this.current_photoid = this.key.photos.next;

        set_photoid_state();
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_owner_photoid_prev (Gtk.Widget widget) {
        if (this.current_photoid != null && (this.current_photoid in this.key.photos) && this.key.photos.prev != null)
            this.current_photoid = this.key.photos.prev;

        set_photoid_state();
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_owner_photoid_button (Gtk.Widget widget, Gdk.Event event) {
        if(event.type == GDK_SCROLL) {
            Gdk.EventScroll event_scroll = (Gdk.EventScroll) event;

            if (event_scroll.direction == GDK_SCROLL_UP)
                on_pgp_owner_photoid_prev (widget);
            else if (event_scroll.direction == GDK_SCROLL_DOWN)
                on_pgp_owner_photoid_next (widget);
        }
    }

    /* -----------------------------------------------------------------------------
     * OWNER PAGE
     */
    /* owner uid list */
    private enum UidColumn {
        OBJECT,
        ICON,
        MARKUP,
        N_COLUMNS
    }

    static Type uid_columns[] = {
        typeof(GLib.Object),  /* object */
        0 /* later */,  /* icon */
        typeof(string),  /* name */
        typeof(string),  /* email */
        typeof(string)   /* comment */
    };

    // XXX G_MODULE_EXPORT
    private void on_pgp_owner_passphrase_button_clicked (Gtk.Widget widget) {
        if (this.key.usage == Seahorse.Usage.PRIVATE_KEY && (this.key is GpgME.Key))
            KeyOperation.change_pass((GpgME.Key) this.key);
    }

    private void do_owner_signals() {
        if (this.key.usage == Seahorse.Usage.PRIVATE_KEY ) {
            Gtk.Frame frame = GTK_WIDGET (seahorse_widget_get_widget (swidget, "owner-photo-frame"));
            gtk_drag_dest_set (frame, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT |
                               GTK_DEST_DEFAULT_DROP, target_list, n_targets, GDK_ACTION_COPY);
        } else {
           if (this.owner-photo-add-button != null) this.owner-photo-add-button.visible = false;;
           if (this.owner-photo-delete-button != null) this.owner-photo-delete-button.visible = false;;
           if (this.owner-photo-primary-button != null) this.owner-photo-primary-button.visible = false;;
           if (this.passphrase-button != null) this.passphrase-button.visible = false;;
        }
    }

    private void do_owner() {
        // Display appropriate warnings
        this.expired_area.visible = (Seahorse.Flags.EXPIRED in this.key.flags);
        this.revoked_area.visible = (Seahorse.Flags.REVOKED in this.key.flags);
        /* this.disabled_area.visible =  Seahorse.Flags.DISABLED in this.key.flags); XXX this doesn't exist? */

        /* Update the expired message */
        if (Seahorse.Flags.EXPIRED in this.key.flags) {
            ulong expires_date = this.key.expires;
            string t;
            if (expires_date == 0)
                /* TRANSLATORS: (unknown) expiry date */
                t = _("(unknown)");
            else
                t = seahorse_util_get_display_date_string (expires_date);
            this.expired_message.text = _("This key expired on: %s").printf(t);
        }

        // Hide trust page when above
        if (this.trust-page != null)
            this.trust-page.visible = !((Seahorse.Flags.EXPIRED in this.key.flags)
                                       || (Seahorse.Flags.REVOKED in this.key.flags)
                                       || (Seahorse.Flags.DISABLED in this.key.flags));

        // Hide or show the uids area
        if (this.uids-area != null)
            this.uids-area.visible = uids != null;

        if (this.key.uids != null) {
            Uid uid = this.key.uids.first();

            this.title = uid.name;
            this.owner_name_label.text = uid.name;
            this.owner_email_label.text = uid.email;
            this.owner_comment_label.text = uid.comment;
            this.owner_keyid_label.text = uid.identifier; // XXX or keyid? identifier is from SeahorseObject
        }

        // Clear/create table store
        if (this.owner_userid_tree != null) {
            Gtk.ListStore store = this.owner_userid_tree.get_model();

            if (store != null) {
                store.clear();
            } else {
                assert(UidColumn.N_COLUMNS != G_N_ELEMENTS (uid_columns));
                uid_columns[1] = G_TYPE_ICON;

                // This is our first time so create a store
                store = new Gtk.ListStore.newv(UidColumn.N_COLUMNS, uid_columns);

                // Make the columns for the view
                Gtk.CellRenderer renderer = new Gtk.CellRendererPixbuf();
                renderer.stock_size = Gtk.IconSize.LARGE_TOOLBAR;
                this.owner_user_id_tree.insert_column_with_attributes(-1, "", renderer, "gicon", UidColumn.ICON, null);
                this.owner_user_id_tree.insert_column_with_attributes(-1, _("Name"), new Gtk.CellRendererText(),
                                                                      "markup", UidColumn.MARKUP, null);
            }

            foreach (Uid uid in this.key.uids) {
                Icon icon = new ThemedIcon(Seahorse.ICON_PERSON);
                Gtk.TreeIter iter;
                store.append(ref iter);
                store.set(iter, UidColumn.OBJECT, uid,
                                 UidColumn.ICON, icon,
                                 UidColumn.MARKUP, uid.markup, -1);
            }

            this.owner_userid_tree.set_model(store);
        }

        do_photo_id();
    }

    /* -----------------------------------------------------------------------------
     * DETAILS PAGE
     */
    /* details subkey list */
    private enum SubKeyColumn {
        OBJECT,
        ID,
        TYPE,
        USAGE,
        CREATED,
        EXPIRES,
        STATUS,
        LENGTH,
        N_COLUMNS
    }

    const Type subkey_columns[] = {
        typeof(GLib.Object),  /* SubkeyColumn.OBJECT */
        typeof(string),  /* SubkeyColumn.ID */
        typeof(string),  /* SubkeyColumn.TYPE */
        typeof(string),  /* SubkeyColumn.USAGE */
        typeof(string),  /* SubkeyColumn.CREATED */
        typeof(string),  /* SubkeyColumn.EXPIRES  */
        typeof(string),  /* SubkeyColumn.STATUS */
        typeof(uint)     /* SubkeyColumn.LENGTH */
    };

    /* trust combo box list */
    private enum TrustColumn {
        LABEL,
        VALIDITY,
        N_COLUMNS;
    }

    const Type trust_columns[] = {
        typeof(string),  /* label */
        typeof(int)      /* validity */
    };

    private Subkey? get_selected_subkey() {
        return get_selected_object(swidget, "details-subkey-tree", SubkeyColumn.OBJECT);
    }

    private void details_subkey_selected(GtkTreeSelection *selection) {
        SubKey? subkey = get_selected_subkey (swidget);

        if (this.key.usage == Seahorse.Usage.PRIVATE_KEY) {
            this.details_date_button.sensitive = (subkey != null);
            this.details_revoke_button.sensitive = (subkey != null && !(Seahorse.Flags.REVOKED in subkey.flags));
            this.details_delete_button.sensitive = (subkey != null);
        }
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_details_add_subkey_button (Gtk.Button button) {
        if (!(this.key is GpgME.Key))
            return;

        Gtk.Dialog dialog = new AddSubkey((GpgME.Key) key, this);
        dialog.run();
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_details_del_subkey_button (Gtk.Button button) {
        GpgME.SubKey? subkey = get_selected_subkey() as GpgME.SubKey;
        if (subkey == null)
            return;

        string message = _("Are you sure you want to permanently delete subkey %d of %s?")
                            .printf(subkey.index, this.key.label);
        if (!Seahorse.DeleteDialog.prompt(this.get_toplevel(), message))
            return;

        GPG.Error err = KeyOperation.del_subkey(subkey);
        if (!err.is_success() || err.is_cancelled())
            Util.show_error(null, _("Couldn’t delete subkey"), err.strerror());
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_details_revoke_subkey_button (Gtk.Button button) {
        GpgME.SubKey? subkey = get_selected_subkey() as GpgME.SubKey;
        if (subkey == null)
            return;

        seahorse_gpgme_revoke_new (subkey, this);
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_details_trust_changed (GtkComboBox *selection) {
        Gtk.TreeIter iter;
        bool set = selection.get_active_iter(out iter);
        if (!set)
            return;

        GpgME.Key? gpg_key = this.key as GpgME.Key;
        if (gpg_key == null)
            return;

        Seahorse.Validity trust;
        selection.get_model().get(iter, TrustColumn.VALIDITY, out trust, -1);

        if (gpg_key.trust != trust) {
            GPG.Error err = GpgME.KeyOperation.set_trust(gpg_key, trust);
            if (!err.is_success() || err.is_cancelled())
                Util.show_error(null, _("Unable to change trust"), err.strerror());
        }
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_details_export_button(Gtk.Widget widget) {
        GpgME.Exporter exporter = new GpgME.Exporter(this.key, true, true);

        List<Exporter> exporters = new List<Exporter>();
        exporters.append(exporter);

        string directory;
        File file;
        // Is the exporter from the out argument? (i.e. did I initialize wrong?)
        if (seahorse_exportable_prompt(exporters, this, out directory, out file, &exporter)) {
            exporter.export_to_file.begin(file, true, null, (obj, res) => {
                try {
                    exporter.export_to_file.finish(res);
                } catch (GLib.Error e) {
                    Seahorse.Util.handle_error(e, this, _("Couldn’t export key"));
                }
            });
        }
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_details_expires_button(Gtk.Widget widget) {
        List<SubKey>? subkeys = this.key.subkeys;
        g_return_if_fail (subkeys);

        seahorse_gpgme_expires_new (SEAHORSE_GPGME_SUBKEY (subkeys.data), this);
    }

    // XXX G_MODULE_EXPORT
    private void on_pgp_details_expires_subkey (Gtk.Widget widget) {
        SubKey? subkey = get_selected_subkey() ?? this.key.subkeys.data;

        GpgME.SubKey? gpg_subkey = subkey as GpgME.SubKey;
        if (subkey != null)
            seahorse_gpgme_expires_new (gpg_subkey, this);
    }

    private void setup_details_trust() {
        debug("KeyProperties: Setting up Trust Combo Box Store");

        this.details_trust_combobox.clear();

        Gtk.CellRendererText text_cell = new Gtk.CellRendererText();
        this.details_trust_combobox.pack_start(text_cell, false);
        this.details_trust_combobox.set_attributes(text_cell, "text", TrustColumn.LABEL, null);

        // Initialize the store
        Gtk.ListStore store = new Gtk.ListStore.newv(TrustColumn.N_COLUMNS);

        Gtk.TreeIter iter;
        if (this.key.usage != Seahorse.Usage.PRIVATE_KEY) {
            store.append(ref iter);
            store.set(iter, TrustColumn.LABEL, C_("Validity", "Unknown"),
                            TrustColumn.VALIDITY,  Seahorse.Validity.UNKNOWN, -1);
            store.append(ref iter);
            store.set(iter, TrustColumn.LABEL, C_("Validity","Never"),
                            TrustColumn.VALIDITY,  Seahorse.Validity.NEVER, -1);
        }

        store.append(ref iter);
        store.set(iter, TrustColumn.LABEL, _("Marginal"),
                        TrustColumn.VALIDITY,  Seahorse.Validity.MARGINAL, -1);
        store.append(ref iter);
        store.set(iter, TrustColumn.LABEL, _("Full"),
                        TrustColumn.VALIDITY,  Seahorse.Validity.FULL, -1);

        if (this.key.usage == Seahorse.Usage.PRIVATE_KEY) {
            store.append(ref iter);
            store.set(iter, TrustColumn.LABEL, _("Ultimate"),
                            TrustColumn.VALIDITY,  Seahorse.Validity.ULTIMATE, -1);
        }

        this.details_trust_combobox.set_model(store);

        debug ("KeyProperties: Finished Setting up Trust Combo Box Store");
    }

    private void do_details_signals()  {
        // if not the key owner, disable most everything
        // if key owner, add the callbacks to the subkey buttons
        if (this.key.usage == Seahorse.Usage.PUBLIC_KEY) {
            // XXX Doesn't exist or isn't part of the UI file
             /*if (this.details-actions-label != null) this.details-actions-label.visible = false;; */
             /*if (this.details-export-button != null) this.details-export-button.visible = false;; */
             /*if (this.details-add-button != null) this.details-add-button.visible = false;; */
             /*if (this.details-date-button != null) this.details-date-button.visible = false;; */
             /*if (this.details-revoke-button != null) this.details-revoke-button.visible = false;; */
             /*if (this.details-delete-button != null) this.details-delete-button.visible = false;; */
             /*if (this.details-calendar-button != null) this.details-calendar-button.visible = false;; */
        } else {
            // Connect so we can enable and disable buttons as subkeys are selected
            GtkWidget widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "details-subkey-tree"));
            g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (widget)),
                              "changed", G_CALLBACK (details_subkey_selected), swidget);
            details_subkey_selected (null, swidget);
        }
    }

    private void do_details() {
        if (this.key.subkeys == null || this.key.subkeys.data == null)
            return;

        SubKey subkey = this.key.subkeys.data;

        this.details_id_label.text = this.key.identifier;
        this.details_fingerprint_label.text = this.key.fingerprint;
            /* if (strlen (fp_label) > 24 && g_ascii_isspace (fp_label[24])) */
            /*     fp_label[24] = '\n'; */
        this.details_algo_label.text = this.key.algo;
        this.details_created_label.text = Seahorse.Util.get_display_date_string(subkey.created);
        this.details_strength_label.text = subkey.length.to_string();

        // Expires
        if (!(this.key is GpgME.Key))
            this.details_expires_label.text = "";
        else if (subkey.expires == 0)
            this.details_expires_label.text = C_("Expires", "Never");
        else
            this.details_expires_label.text = Seahorse.Util.get_display_date_string(subkey.expires);

        Gtk.TreeIter iter;
        if (this.details_trust_combobox != null) {
            this.details_trust_combobox.visible = (this.key is GpgME.Key);
            this.details_trust_combobox.sensitive = !(Seahorse.Flags.DISABLED in this.key.flags);
            Gtk.TreeModel model = this.details_trust_combobox.get_model();

            bool valid = model.get_iter_first(&iter);
            while (valid) {
                int trust;
                model.get(&iter, TrustColumn.VALIDITY, &trust, -1);

                if (trust == pkey.trust) {
                    g_signal_handlers_block_by_func(this.details_trust_combobox, on_pgp_details_trust_changed);
                    this.details_trust_combobox.set_active_iter(&iter);
                    g_signal_handlers_unblock_by_func(this.details_trust_combobox, on_pgp_details_trust_changed);
                    break;
                }
                valid = model.iter_next(&iter);
            }
        }

        /* Clear/create table store */
        Gtk.TreeView widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "details-subkey-tree"));
        Gtk.ListStore store = GTK_LIST_STORE (widget.model);

        if (store != null) {
            store.clear();
        } else {
            /* This is our first time so create a store */
            store = new Gtk.ListStore.newv(subkey_columns);
            this.details_subkey_tree.set_model(store);

            /* Make the columns for the view */
            this.details_subkey_tree.insert_column_with_attributes (-1, _("ID"), new Gtk.CellRendererText(),
                                                         "text", SubkeyColumn.ID, null);
            this.details_subkey_tree.insert_column_with_attributes (-1, _("Type"), new Gtk.CellRendererText(),
                                                         "text", SubkeyColumn.TYPE, null);
            this.details_subkey_tree.insert_column_with_attributes (-1, _("Usage"), new Gtk.CellRendererText(),
                                                         "text", SubkeyColumn.USAGE, null);
            this.details_subkey_tree.insert_column_with_attributes (-1, _("Created"), new Gtk.CellRendererText(),
                                                         "text", SubkeyColumn.CREATED, null);
            this.details_subkey_tree.insert_column_with_attributes (-1, _("Expires"), new Gtk.CellRendererText(),
                                                         "text", SubkeyColumn.EXPIRES, null);
            this.details_subkey_tree.insert_column_with_attributes (-1, _("Status"), new Gtk.CellRendererText(),
                                                         "text", SubkeyColumn.STATUS, null);
            this.details_subkey_tree.insert_column_with_attributes (-1, _("Strength"), new Gtk.CellRendererText(),
                                                         "text", SubkeyColumn.LENGTH, null);
        }

        foreach (SubKey subkey in this.key.subkeys) {
            string status = "";
            if (Seahorse.Flags.REVOKED in subkey.flags)
                status = _("Revoked");
            else if (Seahorse.Flags.EXPIRED in subkey.flags)
                status = _("Expired");
            else if (Seahorse.Flags.DISABLED in subkey.flags)
                status = _("Disabled");
            else if (Seahorse.Flags.IS_VALID in subkey.flags)
                status = _("Good");

            string expiration_date;
            if (this.key.expires == 0)
                expiration_date = C_("Expires", "Never");
            else
                expiration_date = Seahorse.Util.get_display_date_string(this.key.expires);

            store.append(out iter);
            store.set(iter, SubkeyColumn.OBJECT, subkey,
                            SubkeyColumn.ID, subkey.keyid,
                            SubkeyColumn.TYPE, subkey.algo,
                            SubkeyColumn.USAGE, subkey.usage,
                            SubkeyColumn.CREATED, Seahorse.Util.get_display_date_string(subkey.created),
                            SubkeyColumn.EXPIRES, subkey.expires,
                            SubkeyColumn.STATUS, status,
                            SubkeyColumn.LENGTH, subkey.length,
                            -1);
        }
    }

    /* -----------------------------------------------------------------------------
     * TRUST PAGE (PUBLIC KEYS)
     */
    private enum SignColumn {
        ICON,
        NAME,
        KEYID,
        TRUSTED,
        N_COLUMNS;
    }

    static Type sign_columns[] = {
        typeof(Icon),
        typeof(string),
        typeof(string),
        typeof(bool)
    };

    // XXX G_MODULE_EXPORT
    private void on_pgp_trust_marginal_toggled (Gtk.ToggleButton toggle) {
        if (!(this.key is Gpg.Key))
            return;

        Seahorse.Validity trust = (toggle.active)? Seahorse.Validity.MARGINAL
                                                 : Seahorse.Validity.UNKNOWN;
        if (this.key.trust != trust) {
            GPG.Error err = GpgME.KeyOperation.set_trust((GpgME.Key) key, trust);
            if (!err.is_success() || err.is_cancelled())
                Util.show_error(null, _("Unable to change trust"), err.strerror());
        }
    }

    /* Is called whenever a signature key changes */
    private void trust_update_row (Seahorse.ObjectModel skmodel, Seahorse.Object object, Gtk.TreeIter iter) {
        bool trusted = (object.usage == Seahorse.Usage.PRIVATE_KEY
                        || (Seahorse.Flags.TRUSTED in object.flags));

        Icon icon = new ThemedIcon((object is Pgp.Key)? Seahorse.ICON_SIGN : Gtk.Stock.DIALOG_QUESTION);

        /* TRANSLATORS: [Unknown] signature name */
        skmodel.set(iter, SignColumn.ICON, icon,
                          SignColumn.NAME, object.label ?? _("[Unknown]"),
                          SignColumn.KEYID, object.id,
                          SignColumn.TRUSTED, trusted,
                          -1);
    }

    private void signatures_populate_model(Seahorse.ObjectModel skmodel) {
        Gtk.Widget? widget = swidget.get_widget("signatures-tree");
        if (widget == null)
            return;

        /* Build a list of all the keyids */
        string[] rawids = g_ptr_array_new ();
        bool have_sigs = false;
        foreach (Uid uid in this.key.uids) {
            foreach (Signature sig in ui.signatures) {
                /* Never show self signatures, they're implied */
                if (!this.key.has_keyid(sig)) {
                    have_sigs = true;
                    rawids += sig.keyid;
                }
            }
        }

        /* Strip out duplicates */
        unique_strings (rawids);

        /* Only show signatures area when there are signatures */
        swidget.set_visible("signatures-area", have_sigs);

        if (skmodel != null) {
            // Pass it to 'DiscoverKeys' for resolution/download. cancellable ties
            // search scope together
            Cancellable cancellable = new Cancellable();
            List<Key> keys = Pgp.Backend.get().discover_keys(null, rawids, cancellable);

            /* Add the keys to the store */
            foreach (Key key in keys) {
                Gtk.TreeIter iter;
                gtk_tree_store_append(GTK_TREE_STORE (skmodel), &iter, null);
                // This calls the 'update-row' callback, to set the values for the key
                seahorse_object_model_set_row_object (SEAHORSE_OBJECT_MODEL (skmodel), &iter, object);
            }
        }
    }

    // Refilter when the user toggles the 'only show trusted' checkbox
    private void on_pgp_trusted_toggled (Gtk.ToggleButton toggle, Gtk.TreeModelFilter filter) {
        filter.get_model().set_data("only-trusted", toggle.active); // Set flag on the store
        filter.refilter();
    }

    // Add a signature
    // XXX G_MODULE_EXPORT
    private void on_pgp_trust_sign (Gtk.Widget widget) {
        GpgME.Key? gpg_key = this.key as GpgME.Key;
        if (gpg_key == null)
            return;

        Sign.prompt(gpg_key, this);
    }

    private void do_trust_signals() {
        if (this.key.usage != Seahorse.Usage.PUBLIC_KEY)
            return;

       if (this.image_good1 != null) this.image_good1.set_from_icon_name(seahorse-sign-ok);;
       if (this.image_good2 != null) this.image_good2.set_from_icon_name(seahorse-sign-ok);;

        /* TODO: Hookup revoke handler */

        if (this.key.usage == Seahorse.Usage.PUBLIC_KEY ) {
           if (this.signatures-revoke-button != null) this.signatures-revoke-button.visible = false;;
           if (this.signatures-delete-button != null) this.signatures-delete-button.visible = false;;
           if (this.signatures-empty-label != null) this.signatures-empty-label.visible = false;;

            /* Fill in trust labels with name .This only happens once, so it sits here. */
            string user = this.key.label;
            this.trust_marginal_check.text = user;
            this.trust_sign_label.text = user;
            this.trust_revoke_label.text = user;
        }
    }

    /* When the 'only display trusted' check is checked, hide untrusted rows */
    private bool trust_filter (Gtk.TreeModel model, Gtk.TreeIter iter, gpointer userdata) {
        /* Read flag on the store */
        bool trusted = false;
        model.get(iter, SignColumn.TRUSTED, out trusted, -1);
        return !model.get_data("only-trusted") || trusted;
    }

    private bool key_have_signatures (Key pkey, uint types) {
        foreach (Uid uid in this.key.uids)
            foreach (Signature sig in uid.sigs)
                if ((sig.get_sigtype() & types) != 0)
                    return true;

        return false;
    }

    private void do_trust() {
        if (this.key.usage != Seahorse.Usage.PUBLIC_KEY)
            return;

        /* Remote keys */
        if (!(this.key is GpgME.Key)) {
            this.manual_trust_area.visible = false;
            this.manage_trust_area.visible = true;
            this.sign_area.visible = false;
            this.revoke_area.visible = false;
            this.trust_marginal_check.sensitive = false;
            this.sign_image.set_from_icon_name(Seahorse.ICON_SignColumn.UNKNOWN);

        /* Local keys */
        } else {
            bool managed = (Seahorse.Validity.FULL in this.key.trust)
                           || (Seahorse.Validity.MARGINAL in this.key.trust)
                           || (Seahorse.Validity.UNKNOWN in this.key.trust);

            string? icon = null;

            switch (this.key.trust) {
                case Seahorse.Validity.ULTIMATE: // Trust is specified manually
                case Seahorse.Validity.FULL:
                case Seahorse.Validity.MARGINAL:
                    icon = Seahorse.ICON_SignColumn.OK;
                    break;

                case Seahorse.Validity.NEVER: // Trust is specified manually
                    icon = Seahorse.ICON_SignColumn.BAD;
                    break;

                case Seahorse.Validity.UNKNOWN: // We manage the trust through this page
                    icon = Seahorse.ICON_SIGN;
                    break;

                case Seahorse.Validity.REVOKED:  // We shouldn't be seeing this page with these trusts
                case Seahorse.Validity.DISABLED:
                    return;

                default:
                    warning("unknown trust value: %d", this.key.trust);
                    assert_not_reached();
                    return;
            }

            /* Managed and unmanaged areas */
            this.manual_trust_area.visible = !managed;
            this.manage_trust_area.visible = managed;

            /* Managed check boxes */
            if (managed) {
                this.trust_marginal_check.sensitive = true;

                g_signal_handlers_block_by_func (widget, on_pgp_trust_marginal_toggled);
                this.trust_marginal_check.active = (trust != Seahorse.Validity.UNKNOWN);
                g_signal_handlers_unblock_by_func (widget, on_pgp_trust_marginal_toggled);
            }

            /* Signing and revoking */
            bool sigpersonal = key_have_signatures(this.key, SKEY_PGPSIG_PERSONAL);
            this.sign_area.visible = !sigpersonal;
            this.revoke_area.visible = sigpersonal;

            /* The image */
            this.sign_image.set_from_icon_name(icon);
        }

        /* The actual signatures listing */
        Gtk.TreeModelFilter? filter = this.signatures_tree.get_model();
        if (filter != null) {
            filter.get_model().clear();
        } else { // First time create the store
            /* Create a new SeahorseObjectModel store.... */
            Gtk.TreeStore store = new Seahorse.ObjectModel(SignColumn.N_COLUMNS, sign_columns);
            store.update_row.connect(trust_update_row);

            /* .... and a filter to go ontop of it */
            filter = new Gtk.TreeModelFilter(store, null);
            filter.set_visible_func(trust_filter);

            /* Make the colunms for the view */
            Gtk.CellRendererPixbuf renderer = new Gtk.CellRendererPixbuf();
            renderer.stock_size = Gtk.IconSize.LARGE_TOOLBAR;
            this.signatures_tree.insert_column_with_attributes(-1, "", renderer,
                                                               "gicon", SignColumn.ICON, null);
            /* TRANSLATORS: The name and email set on the PGP key */
            this.signatures_tree.insert_column_with_attributes(-1, _("Name/Email"), gtk_cell_renderer_text_new (),
                                                               "text", SignColumn.NAME, null);
            this.signatures_tree.insert_column_with_attributes(-1, _("Key ID"), gtk_cell_renderer_text_new (),
                                                              "text", SignColumn.KEYID, null);

            this.signatures_tree.set_model(filter);
            this.signatures_toggle.toggled.connect(on_pgp_trusted_toggled);
            this.signatures_toggle.active = true;
        }

        signatures_populate_model (swidget, SEAHORSE_OBJECT_MODEL (store));
    }

    /* -----------------------------------------------------------------------------
     * GENERAL
     */

    private void key_notify (Seahorse.Object object) {
        do_owner();
        do_names();
        do_trust();
        do_details();
    }

    public override void response(int response) {
        if (response == Gtk.ResponseType.HELP) {
            seahorse_widget_show_help(swidget);
            return;
        }

        destroy();
    }
}
