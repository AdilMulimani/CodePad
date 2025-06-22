#include "header.h"

// Function prototypes
void on_save_as_file(GtkButton *button, gpointer data);

void on_new_file(GtkButton *button, gpointer data) {
    gtk_text_buffer_set_text(editor->buffer, "", 0);
    if (editor->current_file) {
        g_free(editor->current_file);
        editor->current_file = NULL;
    }
    editor->is_modified = FALSE;
    update_window_title();
    update_status_bar();
    update_line_numbers();
}

void on_open_file(GtkButton *button, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open File",
                                     GTK_WINDOW(editor->window),
                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                     "_Cancel", GTK_RESPONSE_CANCEL,
                                     "_Open", GTK_RESPONSE_ACCEPT,
                                     NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Source Code");
    gtk_file_filter_add_pattern(filter, "*.[ch]");
    gtk_file_filter_add_pattern(filter, "*.cpp");
    gtk_file_filter_add_pattern(filter, "*.py");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char *contents;
        gsize length;
        GError *error = NULL;

        if (g_file_get_contents(filename, &contents, &length, &error)) {
            gtk_text_buffer_set_text(editor->buffer, contents, length);
            g_free(editor->current_file);
            editor->current_file = g_strdup(filename);
            editor->is_modified = FALSE;
            g_free(contents);
            update_window_title();
            update_status_bar();
            update_line_numbers();
        } else {
            GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(editor->window),
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_CLOSE,
                                             "Error opening file: %s", error->message);
            gtk_dialog_run(GTK_DIALOG(error_dialog));
            gtk_widget_destroy(error_dialog);
            g_error_free(error);
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

void on_save_file(GtkButton *button, gpointer data) {
    if (!editor->current_file) {
        on_save_as_file(button, data);
        return;
    }

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(editor->buffer, &start, &end);
    gchar *text = gtk_text_buffer_get_text(editor->buffer, &start, &end, FALSE);

    GError *error = NULL;
    if (g_file_set_contents(editor->current_file, text, -1, &error)) {
        editor->is_modified = FALSE;
        update_window_title();
        update_status_bar();
    } else {
        GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(editor->window),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "Error saving file: %s", error->message);
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
        g_error_free(error);
    }
    g_free(text);
}

void on_save_as_file(GtkButton *button, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save File",
                                    GTK_WINDOW(editor->window),
                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                    "_Save", GTK_RESPONSE_ACCEPT,
                                    NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        g_free(editor->current_file);
        editor->current_file = g_strdup(filename);
        g_free(filename);
        on_save_file(button, data);
    }
    gtk_widget_destroy(dialog);
}

void on_quit(GtkButton *button, gpointer data) {
    gtk_main_quit();
}

void on_cut(GtkButton *button, gpointer data) {
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_text_buffer_cut_clipboard(editor->buffer, clipboard, TRUE);
}

void on_copy(GtkButton *button, gpointer data) {
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_text_buffer_copy_clipboard(editor->buffer, clipboard);
}

void on_paste(GtkButton *button, gpointer data) {
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_text_buffer_paste_clipboard(editor->buffer, clipboard, NULL, TRUE);
}

void on_find(GtkButton *button, gpointer data) {
    gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(editor->search_bar),
                                   !gtk_search_bar_get_search_mode(GTK_SEARCH_BAR(editor->search_bar)));
    if (gtk_search_bar_get_search_mode(GTK_SEARCH_BAR(editor->search_bar))) {
        gtk_widget_grab_focus(editor->search_entry);
    }
}

void on_search_changed(GtkSearchEntry *entry, gpointer data) {
    const gchar *search_text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(search_text) > 0) {
        GtkTextIter start, match_start, match_end;
        gtk_text_buffer_get_start_iter(editor->buffer, &start);
        if (gtk_text_iter_forward_search(&start, search_text, GTK_TEXT_SEARCH_TEXT_ONLY,
                                         &match_start, &match_end, NULL)) {
            gtk_text_buffer_select_range(editor->buffer, &match_start, &match_end);
            gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(editor->text_view),
                                         &match_start, 0.0, FALSE, 0.0, 0.0);
        }
    }
}

void on_zoom_in(GtkButton *button, gpointer data) {
    editor->zoom_level = MIN(editor->zoom_level + 2, 24);
    apply_theme();
}

void on_zoom_out(GtkButton *button, gpointer data) {
    editor->zoom_level = MAX(editor->zoom_level - 2, 8);
    apply_theme();
}

void on_toggle_theme(GtkButton *button, gpointer data) {
    editor->dark_mode = !editor->dark_mode;
    apply_theme();
}

void on_toggle_terminal(GtkButton *button, gpointer data) {
    if (editor->terminal_visible) {
        gtk_widget_hide(editor->terminal_container);
        gtk_button_set_label(GTK_BUTTON(editor->terminal_button), "Show Terminal");
        editor->terminal_visible = FALSE;
    } else {
        gtk_widget_show_all(editor->terminal_container);
        gtk_button_set_label(GTK_BUTTON(editor->terminal_button), "Hide Terminal");
        editor->terminal_visible = TRUE;
        gtk_widget_grab_focus(editor->terminal);
    }
}

void on_about(GtkButton *button, gpointer data) {
    GtkWidget *dialog = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "Code Editor");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "1.0");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), "A simple code editor with terminal");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(editor->window));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void on_text_changed(GtkTextBuffer *buffer, gpointer data) {
    editor->is_modified = TRUE;
    update_window_title();
    update_status_bar();
    update_line_numbers();
}

gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    if (editor->is_modified) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(editor->window),
                                           GTK_DIALOG_MODAL,
                                           GTK_MESSAGE_QUESTION,
                                           GTK_BUTTONS_NONE,
                                           "Save changes before closing?");
        gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                               "_Don't Save", GTK_RESPONSE_NO,
                               "_Cancel", GTK_RESPONSE_CANCEL,
                               "_Save", GTK_RESPONSE_YES,
                               NULL);
        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        if (response == GTK_RESPONSE_YES) {
            on_save_file(NULL, NULL);
            return FALSE;
        } else if (response == GTK_RESPONSE_CANCEL) {
            return TRUE;
        }
    }
    return FALSE;
}

void setup_callbacks(void) {
    // Connect signals
    g_signal_connect(editor->buffer, "changed", G_CALLBACK(on_text_changed), NULL);
    g_signal_connect(editor->window, "delete-event", G_CALLBACK(on_window_delete), NULL);
    g_signal_connect(editor->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(editor->search_entry, "search-changed", G_CALLBACK(on_search_changed), NULL);

    // Setup keyboard shortcuts
    GtkAccelGroup *accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(editor->window), accel_group);

    // File operations
    gtk_accel_group_connect(accel_group, GDK_KEY_n, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_new_file), NULL, NULL));
    gtk_accel_group_connect(accel_group, GDK_KEY_o, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_open_file), NULL, NULL));
    gtk_accel_group_connect(accel_group, GDK_KEY_s, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_save_file), NULL, NULL));

    // Edit operations
    gtk_accel_group_connect(accel_group, GDK_KEY_x, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_cut), NULL, NULL));
    gtk_accel_group_connect(accel_group, GDK_KEY_c, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_copy), NULL, NULL));
    gtk_accel_group_connect(accel_group, GDK_KEY_v, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_paste), NULL, NULL));
    gtk_accel_group_connect(accel_group, GDK_KEY_f, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_find), NULL, NULL));

    // Terminal toggle
    gtk_accel_group_connect(accel_group, GDK_KEY_t, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_toggle_terminal), NULL, NULL));

    // Zoom
    gtk_accel_group_connect(accel_group, GDK_KEY_plus, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_zoom_in), NULL, NULL));
    gtk_accel_group_connect(accel_group, GDK_KEY_minus, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_zoom_out), NULL, NULL));
}