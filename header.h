#ifndef CODE_EDITOR_H
#define CODE_EDITOR_H

#include <gtk/gtk.h>
#include <vte/vte.h>

typedef struct {
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

// Global editor instance
extern CodeEditor *editor;

// Function prototypes
void setup_editor(void);
void setup_terminal(void);
void setup_ui(void);
void setup_callbacks(void);
void apply_theme(void);
void update_status_bar(void);
void update_window_title(void);
void update_line_numbers(void);

#endif