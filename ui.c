#include "header.h"

void setup_ui(void) {
    // Create main window
    editor->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(editor->window), 1200, 800);
    gtk_window_set_position(GTK_WINDOW(editor->window), GTK_WIN_POS_CENTER);

    // Create header bar
    editor->header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(editor->header_bar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(editor->header_bar), "Code Editor with Terminal");
    gtk_window_set_titlebar(GTK_WINDOW(editor->window), editor->header_bar);

    // Create main container
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(editor->window), main_vbox);

    // Create search bar
    editor->search_bar = gtk_search_bar_new();
    editor->search_entry = gtk_search_entry_new();
    gtk_widget_set_hexpand(editor->search_entry, TRUE);
    gtk_container_add(GTK_CONTAINER(editor->search_bar), editor->search_entry);
    gtk_box_pack_start(GTK_BOX(main_vbox), editor->search_bar, FALSE, FALSE, 0);

    // Create paned widget for editor and terminal
    editor->paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(main_vbox), editor->paned, TRUE, TRUE, 0);

    // Status bar
    editor->status_bar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(main_vbox), editor->status_bar, FALSE, FALSE, 0);
}

void apply_theme(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gchar *css;

    if (editor->dark_mode) {
        css = g_strdup_printf(
            "window { background-color: #1e1e1e; color: #d4d4d4; }"
            "textview { background-color: #1e1e1e; color: #d4d4d4; font-family: monospace; font-size: %dpt; padding: 12px; }"
            ".line-numbers { background-color: #252526; color: #858585; font-family: monospace; font-size: %dpt; padding: 12px 8px; border-right: 1px solid #3c3c3c; }"
            "headerbar { background: #3c3c3c; border-bottom: 1px solid #1e1e1e; }"
            "headerbar button { background: #404040; border: 1px solid #555; color: #d4d4d4; }"
            "statusbar { background-color: #007acc; color: white; }"
            ".terminal-header { background-color: #2d2d30; border-bottom: 1px solid #555; }",
            editor->zoom_level, editor->zoom_level);

        if (editor->terminal) {
            GdkRGBA bg_color = {0.12, 0.12, 0.12, 1.0};
            GdkRGBA fg_color = {0.83, 0.83, 0.83, 1.0};
            vte_terminal_set_color_background(VTE_TERMINAL(editor->terminal), &bg_color);
            vte_terminal_set_color_foreground(VTE_TERMINAL(editor->terminal), &fg_color);
        }
    } else {
        css = g_strdup_printf(
            "window { background-color: #ffffff; color: #333333; }"
            "textview { background-color: #ffffff; color: #333333; font-family: monospace; font-size: %dpt; padding: 12px; }"
            ".line-numbers { background-color: #f8f8f8; color: #999999; font-family: monospace; font-size: %dpt; padding: 12px 8px; border-right: 1px solid #e0e0e0; }"
            "headerbar { background: #f0f0f0; border-bottom: 1px solid #d0d0d0; }"
            "headerbar button { background: #ffffff; border: 1px solid #ccc; color: #333; }"
            "statusbar { background-color: #0078d4; color: white; }"
            ".terminal-header { background-color: #e0e0e0; border-bottom: 1px solid #ccc; }",
            editor->zoom_level, editor->zoom_level);

        if (editor->terminal) {
            GdkRGBA bg_color = {1.0, 1.0, 1.0, 1.0};
            GdkRGBA fg_color = {0.2, 0.2, 0.2, 1.0};
            vte_terminal_set_color_background(VTE_TERMINAL(editor->terminal), &bg_color);
            vte_terminal_set_color_foreground(VTE_TERMINAL(editor->terminal), &fg_color);
        }
    }

    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    g_free(css);
}