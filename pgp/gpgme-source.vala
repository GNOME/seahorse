/*
 * Seahorse
 *
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

public class Seahorse.GpgME.Source : GLib.Source {
    private GPG.Context ctx;
    private struct gpgme_io_cbs io_cbs;
    private bool busy;
    private List<WatchData> watches;
    private Cancellable cancellable;
    private int cancelled_sig;
    private bool finished;
    private GPG.ErrorCode status;

    struct WatchData {
        GSource *gsource;
        bool registered;
        GPollFD poll_fd;

        // GPGME watch info
        gpgme_io_cb_t fnc;
        void *fnc_data;
    }

    delegate bool SeahorseGpgmeCallback(GPG.ErrorCode status);

    public Source(GPG.Context ctx, GCancellable *cancellable) {
        base();
        this.set_funcs(&seahorse_gpgme_gsource_funcs);

        this.ctx = ctx;
        this.io_cbs.add = on_gpgme_add_watch;
        this.io_cbs.add_priv = gsource;
        this.io_cbs.remove = on_gpgme_remove_watch;
        this.io_cbs.event = on_gpgme_event;
        this.io_cbs.event_priv = gsource;
        this.ctx.set_io_cbs(&gpgme_gsource.io_cbs);

        this.cancellable = cancellable;
        if (this.cancellable != null)
            this.cancelled_sig = this.cancellable.connect(on_gpgme_gsource_cancelled);
    }

    private bool prepare (int *timeout) {
        if (this.finished)
            return true;

        // No other way, but to poll
        *timeout = -1;
        return false;
    }

    private bool check() {
        foreach (WatchData watch in this.watches) {
            if (watch.registered && watch.poll_fd.revents)
                return true;
        }

        return this.finished;
    }

    private bool dispatch(SourceFunc callback, gpointer user_data) {
        foreach (WatchData watch in this.watches.copy()) {
            if (watch.registered && watch.poll_fd.revents) {
                debug("GPGME OP: io for GPGME on %d", watch.poll_fd.fd);
                assert(watch.fnc);
                watch.poll_fd.revents = 0;
                (watch.fnc) (watch.fnc_data, watch.poll_fd.fd);
            }
        }

        if (this.finished)
            return ((SeahorseGpgmeCallback)callback) (gpgme_gsource.status, user_data);

        return true;
    }

    private void finalize() {
        g_cancellable_disconnect(this.cancellable, this.cancelled_sig);
        g_clear_object (&this.cancellable);
    }

    static GSourceFuncs seahorse_gpgme_gsource_funcs = {
        prepare,
        check,
        dispatch,
        finalize,
    };

    private void register_watch (WatchData *watch) {
        if (watch.registered)
            return;

        debug("GPGME OP: registering watch %d", watch.poll_fd.fd);

        watch.registered = true;
        g_source_add_poll (watch.gsource, &watch.poll_fd);
    }

    private void unregister_watch (WatchData *watch) {
        if (!watch.registered)
            return;

        debug("GPGME OP: unregistering watch %d", watch.poll_fd.fd);

        watch.registered = false;
        g_source_remove_poll (watch.gsource, &watch.poll_fd);
    }

    // Register a callback.
    private gpg_error_t on_gpgme_add_watch(int fd, int dir, gpgme_io_cb_t fnc, void *fnc_data, void **tag) {
        debug ("PGPOP: request to register watch %d", fd);

        WatchData watch = g_new0 (WatchData, 1);
        watch.registered = false;
        watch.poll_fd.fd = fd;
        if (dir)
            watch.poll_fd.events = (G_IO_IN | G_IO_HUP | G_IO_ERR);
        else
            watch.poll_fd.events = (G_IO_OUT | G_IO_ERR);
        watch.fnc = fnc;
        watch.fnc_data = fnc_data;
        watch.gsource = (GSource*)gpgme_gsource;

        // If the context is busy, we already have a START event, and can
        // register watches freely.
        if (this.busy)
            register_watch(watch);

        this.watches.append(watch);
        *tag = watch;

        return GPG_OK;
    }

    private void on_gpgme_remove_watch (void *tag) {
        WatchData *watch = (WatchData*)tag;
        SeahorseGpgmeGSource *gpgme_gsource = (SeahorseGpgmeGSource*)watch.gsource;

        this.watches.remove(watch);
        unregister_watch (watch);
    }

    private void on_gpgme_event(gpgme_event_io_t type, void *type_data) {
        switch (type) {

        // Called when the GPGME context starts an operation
        case GPGME_EVENT_START:
            this.busy = true;
            this.finished = false;
            debug("PGPOP: start event");

            // Since we weren't supposed to register these before, do it now
            foreach (WatchData watch in this.watches)
                register_watch(watch);
            break;

        // Called when the GPGME context is finished with an op
        case GPGME_EVENT_DONE:
            this.busy = false;
            gpg_error_t gerr = (GPG.ErrorCode *)type_data;
            debug ("PGPOP: done event (err: %d)", *gerr);

            // Make sure we have no extra watches left over
            foreach (WatchData watch in this.watches)
                unregister_watch(watch);

            // And try to figure out a good response
            this.finished = true;
            this.status = *gerr;
            break;

        case GPGME_EVENT_NEXT_KEY:
        case GPGME_EVENT_NEXT_TRUSTITEM:
        default:
            // Ignore unsupported event types
            break;
        }
    }

    private void on_gpgme_gsource_cancelled (GCancellable *cancellable) {
        if (this.busy)
            gpgme_cancel (gpgme_gsource.gctx);
    }
}
