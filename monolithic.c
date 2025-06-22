#include <gtk/gtk.h>
#include <vte/vte.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    GtkWidget *window;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    GtkWidget *status_bar;
    GtkWidget *line_numbers;
    GtkWidget *header_bar;
    GtkWidget *search_bar;
    GtkWidget *search_entry;

    // Terminal components
    GtkWidget *terminal;
    GtkWidget *terminal_container;
    GtkWidget *paned;
    GtkWidget *terminal_button;
    gboolean terminal_visible;

    gchar *current_file;
    gboolean is_modified;
    gboolean dark_mode;
    gint zoom_level;
} CodeEditor;

static CodeEditor *editor = NULL;

// Function prototypes
static void on_new_file(GtkButton *button, gpointer data);
static void on_open_file(GtkButton *button, gpointer data);
static void on_save_file(GtkButton *button, gpointer data);
static void on_save_as_file(GtkButton *button, gpointer data);
static void on_quit(GtkButton *button, gpointer data);
static void on_cut(GtkButton *button, gpointer data);
static void on_copy(GtkButton *button, gpointer data);
static void on_paste(GtkButton *button, gpointer data);
static void on_find(GtkButton *button, gpointer data);
static void on_about(GtkButton *button, gpointer data);
static void on_zoom_in(GtkButton *button, gpointer data);
static void on_zoom_out(GtkButton *button, gpointer data);
static void on_toggle_theme(GtkButton *button, gpointer data);
static void on_toggle_terminal(GtkButton *button, gpointer data);
static void on_text_changed(GtkTextBuffer *buffer, gpointer data);
static void update_status_bar(void);
static void update_window_title(void);
static void update_line_numbers(void);
static void apply_theme(void);
static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data);

// Terminal spawn callback
static void on_terminal_spawn_callback(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data)
{
    if (error) {
        g_warning("Failed to spawn terminal: %s", error->message);
    } else {
        g_print("Terminal spawned successfully with PID: %d\n", pid);
    }
}

static void on_toggle_terminal(GtkButton *button, gpointer data)
{
    if (editor->terminal_visible)
    {
        // Hide terminal
        gtk_widget_hide(editor->terminal_container);
        gtk_button_set_label(GTK_BUTTON(editor->terminal_button), "Show Terminal");
        editor->terminal_visible = FALSE;
    }
    else
    {
        // Show terminal
        gtk_widget_show_all(editor->terminal_container);
        gtk_button_set_label(GTK_BUTTON(editor->terminal_button), "Hide Terminal");
        editor->terminal_visible = TRUE;

        // Focus the terminal
        gtk_widget_grab_focus(editor->terminal);
    }
}

static GtkWidget *create_terminal(void)
{
    GtkWidget *terminal_box, *terminal_header, *terminal_label;

    // Create terminal widget
    editor->terminal = vte_terminal_new();

    // Configure terminal properties
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(editor->terminal), 1000);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(editor->terminal), TRUE);
    
    // Enable cursor blinking
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(editor->terminal), VTE_CURSOR_BLINK_ON);
    
    // Set initial size
    vte_terminal_set_size(VTE_TERMINAL(editor->terminal), 80, 24);

    // Set terminal colors
    GdkRGBA bg_color = {0.12, 0.12, 0.12, 1.0}; // Dark background
    GdkRGBA fg_color = {0.83, 0.83, 0.83, 1.0}; // Light foreground

    vte_terminal_set_color_background(VTE_TERMINAL(editor->terminal), &bg_color);
    vte_terminal_set_color_foreground(VTE_TERMINAL(editor->terminal), &fg_color);

    // Set font
    PangoFontDescription *font_desc = pango_font_description_from_string("Monospace 10");
    vte_terminal_set_font(VTE_TERMINAL(editor->terminal), font_desc);
    pango_font_description_free(font_desc);

    // Prepare environment and command
    char **envp = g_get_environ();
    
    // Get shell from environment or use default
    const char *shell = g_getenv("SHELL");
    if (!shell) {
        shell = "/bin/bash";
    }
    
    // Create command array
    char **argv = g_new0(char *, 2);
    argv[0] = g_strdup(shell);
    argv[1] = NULL;

    // Spawn shell asynchronously with proper error handling
    vte_terminal_spawn_async(VTE_TERMINAL(editor->terminal),
                             VTE_PTY_DEFAULT,
                             NULL,                    // working directory (NULL = current)
                             argv,                    // command and args
                             envp,                    // environment
                             G_SPAWN_SEARCH_PATH | G_SPAWN_FILE_AND_ARGV_ZERO,
                             NULL, NULL,              // child setup function
                             NULL,                    // child setup data
                             -1,                      // timeout (-1 = no timeout)
                             NULL,                    // cancellable
                             on_terminal_spawn_callback, // callback
                             NULL);                   // user data

    g_strfreev(argv);
    g_strfreev(envp);

    // Create terminal container with header
    editor->terminal_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // Terminal header
    terminal_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(terminal_header), "terminal-header");

    terminal_label = gtk_label_new("Terminal");
    gtk_widget_set_margin_start(terminal_label, 10);
    gtk_widget_set_margin_top(terminal_label, 5);
    gtk_widget_set_margin_bottom(terminal_label, 5);
    gtk_box_pack_start(GTK_BOX(terminal_header), terminal_label, FALSE, FALSE, 0);

    // Add close button to terminal header
    GtkWidget *close_button = gtk_button_new_from_icon_name("window-close", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_margin_end(close_button, 5);
    gtk_widget_set_margin_top(close_button, 2);
    gtk_widget_set_margin_bottom(close_button, 2);
    gtk_box_pack_end(GTK_BOX(terminal_header), close_button, FALSE, FALSE, 0);
    g_signal_connect(close_button, "clicked", G_CALLBACK(on_toggle_terminal), NULL);

    gtk_box_pack_start(GTK_BOX(editor->terminal_container), terminal_header, FALSE, FALSE, 0);

    // Add terminal in a scrolled window
    GtkWidget *terminal_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(terminal_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    // Set minimum size for terminal
    gtk_widget_set_size_request(terminal_scroll, -1, 200);
    
    gtk_container_add(GTK_CONTAINER(terminal_scroll), editor->terminal);
    gtk_box_pack_start(GTK_BOX(editor->terminal_container), terminal_scroll, TRUE, TRUE, 0);

    // Initially hide terminal
    editor->terminal_visible = FALSE;

    return editor->terminal_container;
}

static void on_new_file(GtkButton *button, gpointer data)
{
    gtk_text_buffer_set_text(editor->buffer, "", 0);
    if (editor->current_file)
    {
        g_free(editor->current_file);
        editor->current_file = NULL;
    }
    editor->is_modified = FALSE;
    update_window_title();
    update_status_bar();
    update_line_numbers();
}

static void on_open_file(GtkButton *button, gpointer data)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;

    dialog = gtk_file_chooser_dialog_new("Open File",
                                         GTK_WINDOW(editor->window),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Open", GTK_RESPONSE_ACCEPT,
                                         NULL);

    // Add file filters for common programming languages
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Source Code");
    gtk_file_filter_add_pattern(filter, "*.c");
    gtk_file_filter_add_pattern(filter, "*.cpp");
    gtk_file_filter_add_pattern(filter, "*.h");
    gtk_file_filter_add_pattern(filter, "*.hpp");
    gtk_file_filter_add_pattern(filter, "*.py");
    gtk_file_filter_add_pattern(filter, "*.js");
    gtk_file_filter_add_pattern(filter, "*.html");
    gtk_file_filter_add_pattern(filter, "*.css");
    gtk_file_filter_add_pattern(filter, "*.java");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Text files");
    gtk_file_filter_add_pattern(filter, "*.txt");
    gtk_file_filter_add_pattern(filter, "*.md");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "All files");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char *contents;
        gsize length;
        GError *error = NULL;

        if (g_file_get_contents(filename, &contents, &length, &error))
        {
            gtk_text_buffer_set_text(editor->buffer, contents, length);
            g_free(editor->current_file);
            editor->current_file = g_strdup(filename);
            editor->is_modified = FALSE;
            g_free(contents);
            update_window_title();
            update_status_bar();
            update_line_numbers();

            // Change terminal directory to file's directory
            if (editor->current_file && editor->terminal)
            {
                gchar *dir = g_path_get_dirname(editor->current_file);
                gchar *command = g_strdup_printf("cd '%s'\n", dir);
                vte_terminal_feed_child(VTE_TERMINAL(editor->terminal), command, -1);
                g_free(dir);
                g_free(command);
            }
        }
        else
        {
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

static void on_save_file(GtkButton *button, gpointer data)
{
    if (!editor->current_file)
    {
        on_save_as_file(button, data);
        return;
    }

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(editor->buffer, &start, &end);
    gchar *text = gtk_text_buffer_get_text(editor->buffer, &start, &end, FALSE);

    GError *error = NULL;
    if (g_file_set_contents(editor->current_file, text, -1, &error))
    {
        editor->is_modified = FALSE;
        update_window_title();
        update_status_bar();
    }
    else
    {
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

static void on_save_as_file(GtkButton *button, gpointer data)
{
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save File",
                                                    GTK_WINDOW(editor->window),
                                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Save", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        g_free(editor->current_file);
        editor->current_file = g_strdup(filename);
        g_free(filename);
        on_save_file(button, data);
    }

    gtk_widget_destroy(dialog);
}

static void on_quit(GtkButton *button, gpointer data)
{
    gtk_main_quit();
}

static void on_cut(GtkButton *button, gpointer data)
{
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_text_buffer_cut_clipboard(editor->buffer, clipboard, TRUE);
}

static void on_copy(GtkButton *button, gpointer data)
{
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_text_buffer_copy_clipboard(editor->buffer, clipboard);
}

static void on_paste(GtkButton *button, gpointer data)
{
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_text_buffer_paste_clipboard(editor->buffer, clipboard, NULL, TRUE);
}

static void on_find(GtkButton *button, gpointer data)
{
    gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(editor->search_bar),
                                   !gtk_search_bar_get_search_mode(GTK_SEARCH_BAR(editor->search_bar)));
    if (gtk_search_bar_get_search_mode(GTK_SEARCH_BAR(editor->search_bar)))
    {
        gtk_widget_grab_focus(editor->search_entry);
    }
}

static void on_search_changed(GtkSearchEntry *entry, gpointer data)
{
    const gchar *search_text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(search_text) > 0)
    {
        GtkTextIter start, match_start, match_end;
        gtk_text_buffer_get_start_iter(editor->buffer, &start);
        if (gtk_text_iter_forward_search(&start, search_text, GTK_TEXT_SEARCH_TEXT_ONLY,
                                         &match_start, &match_end, NULL))
        {
            gtk_text_buffer_select_range(editor->buffer, &match_start, &match_end);
            gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(editor->text_view),
                                         &match_start, 0.0, FALSE, 0.0, 0.0);
        }
    }
}

static void on_zoom_in(GtkButton *button, gpointer data)
{
    editor->zoom_level = MIN(editor->zoom_level + 2, 24);
    apply_theme();
}

static void on_zoom_out(GtkButton *button, gpointer data)
{
    editor->zoom_level = MAX(editor->zoom_level - 2, 8);
    apply_theme();
}

static void on_toggle_theme(GtkButton *button, gpointer data)
{
    editor->dark_mode = !editor->dark_mode;
    apply_theme();
}

static void on_about(GtkButton *button, gpointer data)
{
    GtkWidget *dialog = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "CodePad - Code Editor with Terminal");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "1.0");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog),
                                  "A beautiful, modern code editor with integrated terminal\n"
                                  "Built with GTK+ 3 and VTE terminal emulator");
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "https://github.com/your-repo");
    gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(dialog), "applications-development");

    const char *authors[] = {"Adil Mulimani", NULL};
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dialog), authors);

    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(editor->window));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void on_text_changed(GtkTextBuffer *buffer, gpointer data)
{
    editor->is_modified = TRUE;
    update_window_title();
    update_status_bar();
    update_line_numbers();
}

static void update_status_bar(void)
{
    GtkTextIter iter;
    GtkTextMark *mark = gtk_text_buffer_get_insert(editor->buffer);
    gtk_text_buffer_get_iter_at_mark(editor->buffer, &iter, mark);

    int line = gtk_text_iter_get_line(&iter) + 1;
    int col = gtk_text_iter_get_line_offset(&iter) + 1;

    // Get total lines
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(editor->buffer, &end);
    int total_lines = gtk_text_iter_get_line(&end) + 1;

    // Get character count
    gint char_count = gtk_text_buffer_get_char_count(editor->buffer);

    gchar *msg = g_strdup_printf("Line %d/%d, Column %d • %d characters",
                                 line, total_lines, col, char_count);
    gtk_statusbar_pop(GTK_STATUSBAR(editor->status_bar), 0);
    gtk_statusbar_push(GTK_STATUSBAR(editor->status_bar), 0, msg);
    g_free(msg);
}

static void update_line_numbers(void)
{
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(editor->buffer, &end);
    int total_lines = gtk_text_iter_get_line(&end) + 1;

    GString *line_str = g_string_new("");
    for (int i = 1; i <= total_lines; i++)
    {
        g_string_append_printf(line_str, "%d\n", i);
    }

    gtk_label_set_text(GTK_LABEL(editor->line_numbers), line_str->str);
    g_string_free(line_str, TRUE);
}

static void update_window_title(void)
{
    gchar *title;
    if (editor->current_file)
    {
        gchar *basename = g_path_get_basename(editor->current_file);
        title = g_strdup_printf("%s%s", basename, editor->is_modified ? " •" : "");
        g_free(basename);
    }
    else
    {
        title = g_strdup_printf("Untitled%s", editor->is_modified ? " •" : "");
    }
    gtk_header_bar_set_title(GTK_HEADER_BAR(editor->header_bar), title);
    g_free(title);
}

static void apply_theme(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();

    gchar *css;
    if (editor->dark_mode)
    {
        css = g_strdup_printf(
            "window { background-color: #1e1e1e; color: #d4d4d4; }"
            "textview { background-color: #1e1e1e; color: #d4d4d4; font-family: 'Fira Code', 'Source Code Pro', 'Consolas', monospace; font-size: %dpt; padding: 12px; caret-color: white; }"
            "textview text { background-color: #1e1e1e; color: #d4d4d4; }"
            ".line-numbers { background-color: #252526; color: #858585; font-family: 'Fira Code', 'Source Code Pro', 'Consolas', monospace; font-size: %dpt; padding: 12px 8px; border-right: 1px solid #3c3c3c; }"
            "headerbar { background: linear-gradient(to bottom, #3c3c3c, #2d2d30); border-bottom: 1px solid #1e1e1e; }"
            "headerbar button { background: #404040; border: 1px solid #555; color: #d4d4d4; margin: 2px; }"
            "headerbar button:hover { background: #505050; }"
            "statusbar { background-color: #007acc; color: white; padding: 4px 12px; }"
            "searchbar { background-color: #3c3c3c; border-bottom: 1px solid #555; }"
            "entry { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #555; }"
            "scrollbar { background-color: #2d2d30; }"
            "scrollbar slider { background-color: #555; border-radius: 3px; }"
            "scrollbar slider:hover { background-color: #666; }"
            ".terminal-header { background-color: #2d2d30; border-bottom: 1px solid #555; }"
            ".terminal-header label { color: #d4d4d4; font-weight: bold; }",
            editor->zoom_level, editor->zoom_level);

        // Update terminal colors
        if (editor->terminal)
        {
            GdkRGBA bg_color = {0.12, 0.12, 0.12, 1.0};
            GdkRGBA fg_color = {0.83, 0.83, 0.83, 1.0};
            vte_terminal_set_color_background(VTE_TERMINAL(editor->terminal), &bg_color);
            vte_terminal_set_color_foreground(VTE_TERMINAL(editor->terminal), &fg_color);
        }
    }
    else
    {
        css = g_strdup_printf(
            "window { background-color: #ffffff; color: #333333; }"
            "textview { background-color: #ffffff; color: #333333; font-family: 'Fira Code', 'Source Code Pro', 'Consolas', monospace; font-size: %dpt; padding: 12px; }"
            "textview text { background-color: #ffffff; color: #333333; }"
            ".line-numbers { background-color: #f8f8f8; color: #999999; font-family: 'Fira Code', 'Source Code Pro', 'Consolas', monospace; font-size: %dpt; padding: 12px 8px; border-right: 1px solid #e0e0e0; }"
            "headerbar { background: linear-gradient(to bottom, #f0f0f0, #e0e0e0); border-bottom: 1px solid #d0d0d0; }"
            "headerbar button { background: #ffffff; border: 1px solid #ccc; color: #333; margin: 2px; }"
            "headerbar button:hover { background: #f0f0f0; }"
            "statusbar { background-color: #0078d4; color: white; padding: 4px 12px; }"
            "searchbar { background-color: #f8f8f8; border-bottom: 1px solid #e0e0e0; }"
            "entry { background-color: #ffffff; color: #333333; border: 1px solid #ccc; }"
            "scrollbar { background-color: #f8f8f8; }"
            "scrollbar slider { background-color: #ccc; border-radius: 3px; }"
            "scrollbar slider:hover { background-color: #999; }"
            ".terminal-header { background-color: #e0e0e0; border-bottom: 1px solid #ccc; }"
            ".terminal-header label { color: #333; font-weight: bold; }",
            editor->zoom_level, editor->zoom_level);

        // Update terminal colors
        if (editor->terminal)
        {
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

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    if (editor->is_modified)
    {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(editor->window),
                                                   GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                                   "Save changes before closing?");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                 "Your changes will be lost if you don't save them.");
        gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                               "_Don't Save", GTK_RESPONSE_NO,
                               "_Cancel", GTK_RESPONSE_CANCEL,
                               "_Save", GTK_RESPONSE_YES,
                               NULL);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);

        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        if (response == GTK_RESPONSE_YES)
        {
            on_save_file(NULL, NULL);
        }
        else if (response == GTK_RESPONSE_CANCEL)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static GtkWidget *create_header_bar(void)
{
    editor->header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(editor->header_bar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(editor->header_bar), "Code Editor with Terminal");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(editor->header_bar), "Ready to code");

    // Left side buttons
    GtkWidget *new_button = gtk_button_new_from_icon_name("document-new", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(new_button, "New File (Ctrl+N)");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(editor->header_bar), new_button);

    GtkWidget *open_button = gtk_button_new_from_icon_name("document-open", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(open_button, "Open File (Ctrl+O)");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(editor->header_bar), open_button);

    GtkWidget *save_button = gtk_button_new_from_icon_name("document-save", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(save_button, "Save File (Ctrl+S)");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(editor->header_bar), save_button);

    // Terminal button (continuing from where the code was cut off)
    editor->terminal_button = gtk_button_new_with_label("Show Terminal");
    gtk_widget_set_tooltip_text(editor->terminal_button, "Toggle Terminal (Ctrl+T)");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(editor->header_bar), editor->terminal_button);

    // Add separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(editor->header_bar), separator);

    // Edit buttons
    GtkWidget *cut_button = gtk_button_new_from_icon_name("edit-cut", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(cut_button, "Cut (Ctrl+X)");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(editor->header_bar), cut_button);

    GtkWidget *copy_button = gtk_button_new_from_icon_name("edit-copy", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(copy_button, "Copy (Ctrl+C)");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(editor->header_bar), copy_button);

    GtkWidget *paste_button = gtk_button_new_from_icon_name("edit-paste", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(paste_button, "Paste (Ctrl+V)");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(editor->header_bar), paste_button);

    // Right side buttons
    GtkWidget *find_button = gtk_button_new_from_icon_name("edit-find", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(find_button, "Find (Ctrl+F)");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(editor->header_bar), find_button);

    GtkWidget *theme_button = gtk_button_new_from_icon_name("weather-clear-night", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(theme_button, "Toggle Theme");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(editor->header_bar), theme_button);

    GtkWidget *zoom_in_button = gtk_button_new_from_icon_name("zoom-in", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(zoom_in_button, "Zoom In (Ctrl++)");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(editor->header_bar), zoom_in_button);

    GtkWidget *zoom_out_button = gtk_button_new_from_icon_name("zoom-out", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(zoom_out_button, "Zoom Out (Ctrl+-)");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(editor->header_bar), zoom_out_button);

    GtkWidget *about_button = gtk_button_new_from_icon_name("help-about", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(about_button, "About");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(editor->header_bar), about_button);

    // Connect signals
    g_signal_connect(new_button, "clicked", G_CALLBACK(on_new_file), NULL);
    g_signal_connect(open_button, "clicked", G_CALLBACK(on_open_file), NULL);
    g_signal_connect(save_button, "clicked", G_CALLBACK(on_save_file), NULL);
    g_signal_connect(editor->terminal_button, "clicked", G_CALLBACK(on_toggle_terminal), NULL);
    g_signal_connect(cut_button, "clicked", G_CALLBACK(on_cut), NULL);
    g_signal_connect(copy_button, "clicked", G_CALLBACK(on_copy), NULL);
    g_signal_connect(paste_button, "clicked", G_CALLBACK(on_paste), NULL);
    g_signal_connect(find_button, "clicked", G_CALLBACK(on_find), NULL);
    g_signal_connect(theme_button, "clicked", G_CALLBACK(on_toggle_theme), NULL);
    g_signal_connect(zoom_in_button, "clicked", G_CALLBACK(on_zoom_in), NULL);
    g_signal_connect(zoom_out_button, "clicked", G_CALLBACK(on_zoom_out), NULL);
    g_signal_connect(about_button, "clicked", G_CALLBACK(on_about), NULL);

    return editor->header_bar;
}

static GtkWidget *create_search_bar(void)
{
    editor->search_bar = gtk_search_bar_new();
    editor->search_entry = gtk_search_entry_new();
    gtk_widget_set_hexpand(editor->search_entry, TRUE);
    gtk_container_add(GTK_CONTAINER(editor->search_bar), editor->search_entry);

    g_signal_connect(editor->search_entry, "search-changed", G_CALLBACK(on_search_changed), NULL);

    return editor->search_bar;
}

// Updated terminal spawn callback with better error handling
static void on_terminal_spawn_callback_improved(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data)
{
    if (error) {
        g_warning("Failed to spawn terminal: %s", error->message);
        // Try alternative shells if the default fails
        const char *fallback_shells[] = {"/bin/sh", "/usr/bin/bash", "/bin/zsh", NULL};
        static int retry_count = 0;
        
        if (retry_count < 3) {
            char **argv = g_new0(char *, 2);
            argv[0] = g_strdup(fallback_shells[retry_count]);
            argv[1] = NULL;
            
            if (argv[0] && g_file_test(argv[0], G_FILE_TEST_EXISTS)) {
                retry_count++;
                vte_terminal_spawn_async(terminal,
                                       VTE_PTY_DEFAULT,
                                       NULL,
                                       argv,
                                       NULL,
                                       G_SPAWN_SEARCH_PATH,
                                       NULL, NULL,
                                       NULL,
                                       -1,
                                       NULL,
                                       on_terminal_spawn_callback_improved,
                                       NULL);
            }
            g_strfreev(argv);
        }
    } else {
        g_print("Terminal spawned successfully with PID: %d\n", pid);
    }
}

// Improved terminal creation with better compatibility
static GtkWidget *create_terminal_improved(void)
{
    GtkWidget *terminal_box, *terminal_header, *terminal_label;

    // Create terminal widget
    editor->terminal = vte_terminal_new();

    // Configure terminal properties with better defaults
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(editor->terminal), 1000);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(editor->terminal), TRUE);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(editor->terminal), VTE_CURSOR_BLINK_ON);
    vte_terminal_set_size(VTE_TERMINAL(editor->terminal), 80, 24);

    // Set terminal colors
    GdkRGBA bg_color = {0.12, 0.12, 0.12, 1.0};
    GdkRGBA fg_color = {0.83, 0.83, 0.83, 1.0};
    vte_terminal_set_color_background(VTE_TERMINAL(editor->terminal), &bg_color);
    vte_terminal_set_color_foreground(VTE_TERMINAL(editor->terminal), &fg_color);

    // Set font with fallbacks
    const char *fonts[] = {"Monospace 10", "Liberation Mono 10", "DejaVu Sans Mono 10", "Courier New 10"};
    PangoFontDescription *font_desc = NULL;
    
    for (int i = 0; i < 4; i++) {
        font_desc = pango_font_description_from_string(fonts[i]);
        if (font_desc) break;
    }
    
    if (font_desc) {
        vte_terminal_set_font(VTE_TERMINAL(editor->terminal), font_desc);
        pango_font_description_free(font_desc);
    }

    // Get environment
    char **envp = g_get_environ();
    
    // Try multiple shell options for better compatibility
    const char *shells[] = {
        g_getenv("SHELL"),
        "/bin/bash",
        "/bin/sh", 
        "/usr/bin/bash",
        "/bin/zsh"
    };
    
    const char *selected_shell = NULL;
    for (int i = 0; i < 5; i++) {
        if (shells[i] && g_file_test(shells[i], G_FILE_TEST_EXISTS)) {
            selected_shell = shells[i];
            break;
        }
    }
    
    if (!selected_shell) {
        selected_shell = "/bin/sh"; // Last resort
    }

    // Create command array
    char **argv = g_new0(char *, 2);
    argv[0] = g_strdup(selected_shell);
    argv[1] = NULL;

    // Spawn shell with improved error handling
    vte_terminal_spawn_async(VTE_TERMINAL(editor->terminal),
                             VTE_PTY_DEFAULT,
                             g_get_home_dir(),        // Start in home directory
                             argv,
                             envp,
                             G_SPAWN_SEARCH_PATH | G_SPAWN_FILE_AND_ARGV_ZERO,
                             NULL, NULL,
                             NULL,
                             -1,
                             NULL,
                             on_terminal_spawn_callback_improved,
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
    g_signal_connect(close_button, "clicked", G_CALLBACK(on_toggle_terminal), NULL);

    gtk_box_pack_start(GTK_BOX(editor->terminal_container), terminal_header, FALSE, FALSE, 0);

    // Terminal in scrolled window
    GtkWidget *terminal_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(terminal_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(terminal_scroll, -1, 200);
    
    gtk_container_add(GTK_CONTAINER(terminal_scroll), editor->terminal);
    gtk_box_pack_start(GTK_BOX(editor->terminal_container), terminal_scroll, TRUE, TRUE, 0);

    editor->terminal_visible = FALSE;
    return editor->terminal_container;
}

static void setup_keyboard_shortcuts(void)
{
    GtkAccelGroup *accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(editor->window), accel_group);

    // File operations
    gtk_accel_group_connect(accel_group, GDK_KEY_n, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_new_file), NULL, NULL));
    gtk_accel_group_connect(accel_group, GDK_KEY_o, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_open_file), NULL, NULL));
    gtk_accel_group_connect(accel_group, GDK_KEY_s, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_save_file), NULL, NULL));
    gtk_accel_group_connect(accel_group, GDK_KEY_q, GDK_CONTROL_MASK, 0,
                           g_cclosure_new_swap(G_CALLBACK(on_quit), NULL, NULL));

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

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    // Initialize editor structure
    editor = g_new0(CodeEditor, 1);
    editor->dark_mode = TRUE;
    editor->zoom_level = 12;
    editor->is_modified = FALSE;
    editor->current_file = NULL;

    // Create main window
    editor->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(editor->window), 1200, 800);
    gtk_window_set_position(GTK_WINDOW(editor->window), GTK_WIN_POS_CENTER);

    // Create header bar
    create_header_bar();
    gtk_window_set_titlebar(GTK_WINDOW(editor->window), editor->header_bar);

    // Create main container
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(editor->window), main_vbox);

    // Create search bar
    create_search_bar();
    gtk_box_pack_start(GTK_BOX(main_vbox), editor->search_bar, FALSE, FALSE, 0);

    // Create paned widget for editor and terminal
    editor->paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(main_vbox), editor->paned, TRUE, TRUE, 0);

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
    // Note: show_line_numbers and auto_indent are not available in GTK+3
    // Line numbers are handled manually with our custom line_numbers widget

    gtk_container_add(GTK_CONTAINER(scrolled), editor->text_view);
    gtk_box_pack_start(GTK_BOX(editor_hbox), scrolled, TRUE, TRUE, 0);

    // Add editor to paned widget
    gtk_paned_pack1(GTK_PANED(editor->paned), editor_hbox, TRUE, FALSE);

    // Create terminal (using improved version)
    GtkWidget *terminal_widget = create_terminal_improved();
    gtk_paned_pack2(GTK_PANED(editor->paned), terminal_widget, FALSE, TRUE);

    // Status bar
    editor->status_bar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(main_vbox), editor->status_bar, FALSE, FALSE, 0);

    // Connect signals
    g_signal_connect(editor->buffer, "changed", G_CALLBACK(on_text_changed), NULL);
    g_signal_connect(editor->window, "delete-event", G_CALLBACK(on_window_delete), NULL);
    g_signal_connect(editor->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Set up keyboard shortcuts
    setup_keyboard_shortcuts();

    // Apply initial theme
    apply_theme();
    update_status_bar();
    update_line_numbers();

    // Show window
    gtk_widget_show_all(editor->window);
    
    // Hide terminal initially
    gtk_widget_hide(editor->terminal_container);

    gtk_main();

    // Cleanup
    if (editor->current_file) {
        g_free(editor->current_file);
    }
    g_free(editor);

    return 0;
}

