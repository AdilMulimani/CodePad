#include "header.h"

void setup_editor(void) {
    // Create editor area
    GtkWidget *editor_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    // Line numbers
    editor->line_numbers = gtk_label_new("1\n");
    gtk_label_set_xalign(GTK_LABEL(editor->line_numbers), 1.0);
    gtk_label_set_yalign(GTK_LABEL(editor->line_numbers), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(editor->line_numbers), "line-numbers");
    gtk_box_pack_start(GTK_BOX(editor_hbox), editor->line_numbers, FALSE, FALSE, 0);

    // Text view with scrolling
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    editor->text_view = gtk_text_view_new();
    editor->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor->text_view));
    
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(editor->text_view), GTK_WRAP_NONE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(editor->text_view), TRUE);

    gtk_container_add(GTK_CONTAINER(scrolled), editor->text_view);
    gtk_box_pack_start(GTK_BOX(editor_hbox), scrolled, TRUE, TRUE, 0);

    // Add editor to paned widget
    gtk_paned_pack1(GTK_PANED(editor->paned), editor_hbox, TRUE, FALSE);
}

void update_status_bar(void) {
    GtkTextIter iter;
    GtkTextMark *mark = gtk_text_buffer_get_insert(editor->buffer);
    gtk_text_buffer_get_iter_at_mark(editor->buffer, &iter, mark);

    int line = gtk_text_iter_get_line(&iter) + 1;
    int col = gtk_text_iter_get_line_offset(&iter) + 1;

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(editor->buffer, &end);
    int total_lines = gtk_text_iter_get_line(&end) + 1;
    gint char_count = gtk_text_buffer_get_char_count(editor->buffer);

    gchar *msg = g_strdup_printf("Line %d/%d, Column %d • %d characters",
                                 line, total_lines, col, char_count);
    gtk_statusbar_pop(GTK_STATUSBAR(editor->status_bar), 0);
    gtk_statusbar_push(GTK_STATUSBAR(editor->status_bar), 0, msg);
    g_free(msg);
}

void update_line_numbers(void) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(editor->buffer, &end);
    int total_lines = gtk_text_iter_get_line(&end) + 1;

    GString *line_str = g_string_new("");
    for (int i = 1; i <= total_lines; i++) {
        g_string_append_printf(line_str, "%d\n", i);
    }

    gtk_label_set_text(GTK_LABEL(editor->line_numbers), line_str->str);
    g_string_free(line_str, TRUE);
}

void update_window_title(void) {
    gchar *title;
    if (editor->current_file) {
        gchar *basename = g_path_get_basename(editor->current_file);
        title = g_strdup_printf("%s%s", basename, editor->is_modified ? " •" : "");
        g_free(basename);
    } else {
        title = g_strdup_printf("Untitled%s", editor->is_modified ? " •" : "");
    }
    gtk_header_bar_set_title(GTK_HEADER_BAR(editor->header_bar), title);
    g_free(title);
}