#include "header.h"

static void on_terminal_spawn_callback(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data) {
    if (error) {
        g_warning("Failed to spawn terminal: %s", error->message);
    } else {
        g_print("Terminal spawned successfully with PID: %d\n", pid);
    }
}

void setup_terminal(void) {
    GtkWidget *terminal_box, *terminal_header, *terminal_label;

    // Create terminal widget
    editor->terminal = vte_terminal_new();

    // Configure terminal properties
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(editor->terminal), 1000);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(editor->terminal), TRUE);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(editor->terminal), VTE_CURSOR_BLINK_ON);
    vte_terminal_set_size(VTE_TERMINAL(editor->terminal), 80, 24);

    // Set terminal colors
    GdkRGBA bg_color = {0.12, 0.12, 0.12, 1.0};
    GdkRGBA fg_color = {0.83, 0.83, 0.83, 1.0};
    vte_terminal_set_color_background(VTE_TERMINAL(editor->terminal), &bg_color);
    vte_terminal_set_color_foreground(VTE_TERMINAL(editor->terminal), &fg_color);

    // Set font
    PangoFontDescription *font_desc = pango_font_description_from_string("Monospace 10");
    vte_terminal_set_font(VTE_TERMINAL(editor->terminal), font_desc);
    pango_font_description_free(font_desc);

    // Prepare environment and command
    char **envp = g_get_environ();
    const char *shell = g_getenv("SHELL") ? g_getenv("SHELL") : "/bin/bash";
    
    char **argv = g_new0(char *, 2);
    argv[0] = g_strdup(shell);
    argv[1] = NULL;

    vte_terminal_spawn_async(VTE_TERMINAL(editor->terminal),
                             VTE_PTY_DEFAULT,
                             NULL,
                             argv,
                             envp,
                             G_SPAWN_SEARCH_PATH | G_SPAWN_FILE_AND_ARGV_ZERO,
                             NULL, NULL,
                             NULL,
                             -1,
                             NULL,
                             on_terminal_spawn_callback,
                             NULL);

    g_strfreev(argv);
    g_strfreev(envp);

    // Create terminal container
    editor->terminal_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // Terminal header
    terminal_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(terminal_header), "terminal-header");

    terminal_label = gtk_label_new("Terminal");
    gtk_widget_set_margin_start(terminal_label, 10);
    gtk_widget_set_margin_top(terminal_label, 5);
    gtk_widget_set_margin_bottom(terminal_label, 5);
    gtk_box_pack_start(GTK_BOX(terminal_header), terminal_label, FALSE, FALSE, 0);

    // Close button
    GtkWidget *close_button = gtk_button_new_from_icon_name("window-close", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_margin_end(close_button, 5);
    gtk_widget_set_margin_top(close_button, 2);
    gtk_widget_set_margin_bottom(close_button, 2);
    gtk_box_pack_end(GTK_BOX(terminal_header), close_button, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(editor->terminal_container), terminal_header, FALSE, FALSE, 0);

    // Terminal in scrolled window
    GtkWidget *terminal_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(terminal_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(terminal_scroll, -1, 200);
    
    gtk_container_add(GTK_CONTAINER(terminal_scroll), editor->terminal);
    gtk_box_pack_start(GTK_BOX(editor->terminal_container), terminal_scroll, TRUE, TRUE, 0);

    // Add terminal to paned widget
    gtk_paned_pack2(GTK_PANED(editor->paned), editor->terminal_container, FALSE, TRUE);

    editor->terminal_visible = FALSE;
}