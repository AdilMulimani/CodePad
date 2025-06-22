#include "header.h"

CodeEditor *editor = NULL;

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Initialize editor structure
    editor = g_new0(CodeEditor, 1);
    editor->dark_mode = TRUE;
    editor->zoom_level = 12;
    editor->is_modified = FALSE;
    editor->current_file = NULL;

    // Setup components
    setup_ui();
    setup_editor();
    setup_terminal();
    setup_callbacks();

    // Apply initial theme
    apply_theme();
    update_status_bar();
    update_line_numbers();

    // Show window
    gtk_widget_show_all(editor->window);
    gtk_widget_hide(editor->terminal_container);

    gtk_main();

    // Cleanup
    if (editor->current_file) {
        g_free(editor->current_file);
    }
    g_free(editor);

    return 0;
}