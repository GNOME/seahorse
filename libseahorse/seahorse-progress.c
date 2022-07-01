/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#include "config.h"

#include "seahorse-progress.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

typedef enum _TrackedState {
    TASK_PART_PREPPED = 1,
    TASK_PART_BEGUN,
    TASK_PART_ENDED
} TrackedState;

typedef struct _TrackedPart {
    const void *progress_tag;
    char *details;
    TrackedState state;
} TrackedPart;

typedef struct _TrackedTask {
    GCancellable *cancellable;
    unsigned long cancelled_sig;

    GtkWidget *dialog;
    GtkBuilder *builder;
    char *title;
    char *notice;
    gboolean showing;

    GQueue *parts;
    int parts_prepped;
    int parts_begun;
    int parts_ended;
} TrackedTask;

/* maps GCancellables to TrackedTasks */
static GHashTable *tracked_tasks = NULL;

static void
tracked_part_free (void *data)
{
    TrackedPart *part = data;
    g_free (part->details);
    g_free (part);
}

static int
find_part_progress_tag (const void *part_data,
                        const void *progress_tag)
{
    TrackedPart *part = (TrackedPart *) part_data;
    return (part->progress_tag == progress_tag) ? 0 : 1;
}

static int
find_part_begun_with_details (const void *part_data,
                              const void *unused)
{
    TrackedPart *part = (TrackedPart *) part_data;
    return (part->state == TASK_PART_BEGUN && part->details) ? 0 : 1;
}

static TrackedPart *
tracked_part_find (TrackedTask  *task,
                   GCompareFunc  func,
                   const void   *user_data)
{
    GList *part = g_queue_find_custom (task->parts, user_data, func);
    return part ? part->data : NULL;
}

static void
on_cancellable_gone (void    *user_data,
                     GObject *where_the_object_was)
{
    TrackedTask *task;

    g_assert (tracked_tasks);
    task = g_hash_table_lookup (tracked_tasks, where_the_object_was);
    g_assert ((void *) task->cancellable == (void *) where_the_object_was);
    task->cancellable = NULL;
    task->cancelled_sig = 0;

    if (!g_hash_table_remove (tracked_tasks, where_the_object_was))
        g_assert_not_reached ();
}

static void
on_cancellable_cancelled (GCancellable *cancellable,
                          void         *user_data)
{
    g_assert (tracked_tasks);
    g_assert (((TrackedTask *)user_data)->cancellable == cancellable);
    if (!g_hash_table_remove (tracked_tasks, cancellable))
        g_assert_not_reached ();
}

static void
tracked_task_free (void *data)
{
    TrackedTask *task = data;
    if (task->cancellable) {
        g_object_weak_unref (G_OBJECT (task->cancellable),
                             on_cancellable_gone, task);

        /* Use this because g_cancellable_disconnect() deadlocks */
        if (task->cancelled_sig)
            g_signal_handler_disconnect (task->cancellable,
                                         task->cancelled_sig);
    }

    g_queue_foreach (task->parts, (GFunc)tracked_part_free, NULL);
    g_queue_free (task->parts);
    g_free (task->title);
    g_free (task->notice);
    g_clear_object (&task->builder);
    if (task->dialog)
      gtk_window_destroy (GTK_WINDOW (task->dialog));
    g_free (task);
}

static gboolean
on_pulse_timeout (void *user_data)
{
    GtkProgressBar *progress = GTK_PROGRESS_BAR (user_data);

    if (gtk_progress_bar_get_pulse_step (progress) != 0) {
        gtk_progress_bar_pulse (progress);
        return G_SOURCE_CONTINUE;
    }

    return G_SOURCE_REMOVE;
}

static void
start_pulse (GtkProgressBar *progress)
{
    unsigned int stag;

    gtk_progress_bar_set_pulse_step (progress, 0.05);
    gtk_progress_bar_pulse (progress);

    stag = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (progress), "pulse-timer"));

    /* Start a pulse timer */
    if (stag == 0) {
        stag = g_timeout_add (100, on_pulse_timeout, progress);
        g_object_set_data_full (G_OBJECT (progress), "pulse-timer",
                                GINT_TO_POINTER (stag), (GDestroyNotify)g_source_remove);
    }
}

static void
stop_pulse (GtkProgressBar *progress)
{
    gtk_progress_bar_set_pulse_step (progress, 0.0);

    /* This causes the destroy callback to be called */
    g_object_set_data (G_OBJECT (progress), "pulse-timer", NULL);
}

static void
progress_update_display (TrackedTask *task)
{
    GtkProgressBar *progress = NULL;
    GtkLabel *label = NULL;
    GtkWidget *widget;
    TrackedPart *part;
    double fraction;

    if (task->builder == NULL)
        return;

    /* Dig out our progress display stuff */
    widget = GTK_WIDGET (gtk_builder_get_object (task->builder, "progress-bar"));
    if (widget == NULL)
        g_warning ("cannot display progress because seahorse window has no progress widget");
    else
        progress = GTK_PROGRESS_BAR (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (task->builder, "progress-details"));
    if (GTK_IS_LABEL (widget))
        label = GTK_LABEL (widget);

    /* The details is the first on a begun part */
    part = tracked_part_find (task, find_part_begun_with_details, NULL);
    gtk_label_set_text (label, part ? part->details : "");

    /* If all parts are running simultaneously then indeterminate */
    if (task->parts_prepped == 0 && task->parts_ended == 0) {
        fraction = -1;
    } else {
        fraction = (double) task->parts_ended /
                   (double) task->parts->length;
    }

    if (progress) {
        if (fraction >= 0.0) {
            stop_pulse (progress);
            gtk_progress_bar_set_fraction (progress, fraction);
        } else {
            start_pulse (progress);
        }
    }
}

static TrackedTask *
progress_lookup_or_create_task (GCancellable *cancellable)
{
    TrackedTask *task;

    if (tracked_tasks == NULL)
        tracked_tasks = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                               NULL, tracked_task_free);

    task = g_hash_table_lookup (tracked_tasks, cancellable);
    if (task == NULL) {
        if (g_cancellable_is_cancelled (cancellable))
            return NULL;

        task = g_new0 (TrackedTask, 1);
        task->cancellable = cancellable;
        g_object_weak_ref (G_OBJECT (task->cancellable),
                           on_cancellable_gone, task);
        task->parts = g_queue_new ();

        g_hash_table_insert (tracked_tasks, cancellable, task);
        task->cancelled_sig = g_cancellable_connect (cancellable,
                                                     G_CALLBACK (on_cancellable_cancelled),
                                                     task, NULL);
    }

    return task;
}

static void
progress_prep_va (GCancellable *cancellable,
                  const void   *progress_tag,
                  const char   *details,
                  va_list       va)
{
    TrackedTask *task;
    TrackedPart *part;

    task = progress_lookup_or_create_task (cancellable);

    /* Perhaps already cancelled? */
    if (task == NULL)
        return;

    part = tracked_part_find (task, find_part_progress_tag, progress_tag);
    if (part == NULL) {
        part = g_new0 (TrackedPart, 1);
        if (details && details[0])
            part->details = g_strdup_vprintf (details, va);
        part->progress_tag = progress_tag;
        part->state = TASK_PART_PREPPED;
        g_queue_push_tail (task->parts, part);
        task->parts_prepped++;
    } else {
        g_warning ("already tracking progress for this part of the operation");
    }

    /* If the dialog is being displayed, update it */
    g_assert (task->parts_prepped + task->parts_begun + task->parts_ended == (int) task->parts->length);
    progress_update_display (task);
}

void
seahorse_progress_prep (GCancellable *cancellable,
                        const void   *progress_tag,
                        const char   *details,
                        ...)
{
    va_list va;

    if (!cancellable)
        return;

    g_return_if_fail (G_IS_CANCELLABLE (cancellable));

    va_start (va, details);
    progress_prep_va (cancellable, progress_tag, details, va);
    va_end (va);
}

void
seahorse_progress_begin (GCancellable *cancellable,
                         const void   *progress_tag)
{
    TrackedTask *task = NULL;
    TrackedPart *part;

    if (!cancellable)
        return;

    g_return_if_fail (G_IS_CANCELLABLE (cancellable));
    if (g_cancellable_is_cancelled (cancellable))
        return;

    if (tracked_tasks)
        task = g_hash_table_lookup (tracked_tasks, cancellable);
    if (task == NULL) {
        g_warning ("caller is trying to begin part for task that does not exist");
        return;
    }

    part = tracked_part_find (task, find_part_progress_tag, progress_tag);
    if (part == NULL) {
        g_warning ("caller is trying to begin part of task that does not exist");
        return;
    }

    switch (part->state) {
    case TASK_PART_PREPPED:
        part->state = TASK_PART_BEGUN;
        task->parts_begun++;
        task->parts_prepped--;
        break;
    case TASK_PART_BEGUN:
        g_warning ("caller is trying to begin part of task that has already begun");
        return;
    case TASK_PART_ENDED:
        g_warning ("caller is trying to begin part of task that has already ended");
        return;
    default:
        g_assert_not_reached ();
        break;
    }

    g_assert (task->parts_prepped + task->parts_begun + task->parts_ended == (int) task->parts->length);
    progress_update_display (task);
}

void
seahorse_progress_prep_and_begin (GCancellable *cancellable,
                                  const void   *progress_tag,
                                  const char   *details,
                                  ...)
{
    va_list va;

    if (!cancellable)
        return;

    g_return_if_fail (G_IS_CANCELLABLE (cancellable));

    va_start (va, details);
    progress_prep_va (cancellable, progress_tag, details, va);
    seahorse_progress_begin (cancellable, progress_tag);
    va_end (va);
}

void
seahorse_progress_update (GCancellable *cancellable,
                          const void   *progress_tag,
                          const char   *details,
                          ...)
{
    TrackedTask *task = NULL;
    TrackedPart *part;
    va_list va;

    if (!cancellable)
        return;

    g_return_if_fail (G_IS_CANCELLABLE (cancellable));
    if (g_cancellable_is_cancelled (cancellable))
        return;

    if (tracked_tasks)
        task = g_hash_table_lookup (tracked_tasks, cancellable);
    if (task == NULL) {
        g_warning ("caller is trying to update part for task that does not exist");
        return;
    }

    part = tracked_part_find (task, find_part_progress_tag, progress_tag);
    if (part == NULL) {
        g_warning ("caller is trying to update part of task that does not exist");
        return;
    }

    switch (part->state) {
    case TASK_PART_PREPPED:
    case TASK_PART_BEGUN:
        g_free (part->details);
        if (details && details[0]) {
            va_start (va, details);
            part->details = g_strdup_vprintf (details, va);
            va_end (va);
        }
        break;
    case TASK_PART_ENDED:
        g_warning ("caller is trying to update part of task that has already ended");
        return;
    default:
        g_assert_not_reached ();
        return;
    }

    g_assert (task->parts_prepped + task->parts_begun + task->parts_ended == (int) task->parts->length);
    progress_update_display (task);
}

void
seahorse_progress_end (GCancellable *cancellable,
                       const void   *progress_tag)
{
    TrackedTask *task = NULL;
    TrackedPart *part;

    if (!cancellable)
        return;

    g_return_if_fail (G_IS_CANCELLABLE (cancellable));
    if (g_cancellable_is_cancelled (cancellable))
        return;

    if (tracked_tasks)
        task = g_hash_table_lookup (tracked_tasks, cancellable);
    if (task == NULL) {
        g_warning ("caller is trying to end part for task that does not exist");
        return;
    }

    part = tracked_part_find (task, find_part_progress_tag, progress_tag);
    if (part == NULL) {
        g_warning ("caller is trying to end part of task that does not exist");
        return;
    }

    switch (part->state) {
    case TASK_PART_PREPPED:
        g_warning ("caller is trying to end part of task that has not begun");
        return;
    case TASK_PART_BEGUN:
        part->state = TASK_PART_ENDED;
        task->parts_begun--;
        task->parts_ended++;
        break;
    case TASK_PART_ENDED:
        g_warning ("caller is trying to end part of task that has already ended");
        return;
    default:
        g_assert_not_reached ();
        break;
    }

    g_assert (task->parts_prepped + task->parts_begun + task->parts_ended == (int) task->parts->length);
    progress_update_display (task);
}

static int
on_window_delete_event (GtkWidget *widget,
                        GdkEvent  *event,
                        void      *user_data)
{
    TrackedTask *task = NULL;

    /* Guard against going away before we display */
    if (tracked_tasks)
        task = g_hash_table_lookup (tracked_tasks, user_data);
    if (task != NULL)
        g_cancellable_cancel (task->cancellable);

    return TRUE; /* allow window to close */
}

static void
on_cancel_button_clicked (GtkButton *button,
                          void      *user_data)
{
    TrackedTask *task = NULL;

    /* Guard against going away before we display */
    if (tracked_tasks)
        task = g_hash_table_lookup (tracked_tasks, user_data);
    if (task != NULL)
        g_cancellable_cancel (task->cancellable);
}

static gboolean
on_timeout_show_progress (void *user_data)
{
    TrackedTask *task = NULL;
    g_autoptr(GtkBuilder) builder = NULL;
    GtkWidget *widget;

    /* Guard against going away before we display */
    if (tracked_tasks)
        task = g_hash_table_lookup (tracked_tasks, user_data);
    if (task == NULL)
        return G_SOURCE_REMOVE;

    builder = gtk_builder_new_from_resource ("/org/gnome/Seahorse/seahorse-progress.ui");

    task->dialog = GTK_WIDGET (gtk_builder_get_object (builder, "progress"));
    g_signal_connect (task->dialog, "delete-event",
                      G_CALLBACK (on_window_delete_event), task->cancellable);

    /* Setup the title */
    if (task->title) {
        g_autofree char *text = NULL;

        gtk_window_set_title (GTK_WINDOW (task->dialog), task->title);

        /* The main message title */
        widget = GTK_WIDGET (gtk_builder_get_object (builder, "progress-title"));
        text = g_strdup_printf ("<b>%s</b>", task->title);
        gtk_label_set_markup (GTK_LABEL (widget), text);
    }

    /* Setup the notice */
    if (task->notice) {
        widget = GTK_WIDGET (gtk_builder_get_object (builder, "progress-notice"));
        gtk_label_set_label (GTK_LABEL (widget), task->notice);
        gtk_widget_set_visible (widget, TRUE);
    }

    /* Setup the cancel button */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "progress-cancel"));
    g_signal_connect (widget, "clicked", G_CALLBACK (on_cancel_button_clicked),
                      task->cancellable);

    /* Allow attach to work */
    task->showing = FALSE;
    seahorse_progress_attach (task->cancellable, builder);
    gtk_widget_set_visible (task->dialog, TRUE);

    return G_SOURCE_REMOVE;
}

void
seahorse_progress_show (GCancellable *cancellable,
                        const char   *title,
                        gboolean      delayed)
{
    seahorse_progress_show_with_notice (cancellable, title, NULL, delayed);
}

void
seahorse_progress_show_with_notice (GCancellable *cancellable,
                                    const char   *title,
                                    const char   *notice,
                                    gboolean      delayed)
{
    TrackedTask *task;

    g_return_if_fail (title != NULL && title[0] != '\0');

    if (!cancellable)
        return;

    g_return_if_fail (G_IS_CANCELLABLE (cancellable));

    task = progress_lookup_or_create_task (cancellable);

    /* Perhaps already cancelled? */
    if (task == NULL)
        return;

    if (task->showing) {
        g_warning ("caller is trying to show progress for a task which already has displayed progress");
        return;
    }

    g_free (task->title);
    task->title = g_strdup (title);
    task->notice = g_strdup (notice);
    task->showing = TRUE;

    if (delayed)
        g_timeout_add_seconds (2, on_timeout_show_progress, cancellable);
    else
        on_timeout_show_progress (cancellable);
}

void
seahorse_progress_attach (GCancellable *cancellable,
                          GtkBuilder *builder)
{
    TrackedTask *task;

    if (!cancellable)
        return;

    g_return_if_fail (G_IS_CANCELLABLE (cancellable));

    task = progress_lookup_or_create_task (cancellable);

    /* Perhaps already cancelled? */
    if (task == NULL)
        return;

    if (task->showing) {
        g_warning ("caller is trying to show progress for a task which already has displayed progress");
        return;
    }

    task->showing = TRUE;
    task->builder = g_object_ref (builder);

    progress_update_display (task);
}
