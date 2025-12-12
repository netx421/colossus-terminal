// COLOSSUS Terminal v0.3 (Monochrome Edition)
//
// C++ + GTK3 + VTE
// - Pure grayscale palette (no color/hue at all)
// - Ctrl+C / Ctrl+V copy/paste
// - Right-click context menu
// - Uses system GTK theme and your default $SHELL
//
// This version fixes the spawn_async signature properly.

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <cstdlib>
#include <cstring>

static void adjust_font_scale(VteTerminal* term, double factor_delta) {
    double scale = vte_terminal_get_font_scale(term);
    scale += factor_delta;
    if (scale < 0.5)
        scale = 0.5;
    if (scale > 3.0)
        scale = 3.0;
    vte_terminal_set_font_scale(term, scale);
}

static void on_child_exited(VteTerminal* /*term*/, gint /*status*/, gpointer user_data) {
    GtkWindow* win = GTK_WINDOW(user_data);
    gtk_window_close(win);   // this will trigger the "destroy" signal and gtk_main_quit
}



// ─────────────────────────────────────────────
//  Helper: Parse grayscale hex color
// ─────────────────────────────────────────────
static void parse_rgba(const char* hex, GdkRGBA* out) {
    if (!gdk_rgba_parse(out, hex)) {
        out->red = out->green = out->blue = 0.0;
        out->alpha = 1.0;
    }
}

// ─────────────────────────────────────────────
//  Grayscale ANSI palette (16 colors)
// ─────────────────────────────────────────────
static void set_grayscale_palette(VteTerminal* term) {
    GdkRGBA fg, bg;
    parse_rgba("#d0d0d0", &fg);  // foreground = light gray
    parse_rgba("#050505", &bg);  // background = near black

    const char* hex[16] = {
        "#000000", "#202020", "#404040", "#606060",
        "#808080", "#9a9a9a", "#bcbcbc", "#dcdcdc",
        "#101010", "#303030", "#505050", "#707070",
        "#909090", "#b0b0b0", "#d0d0d0", "#ffffff"
    };

    GdkRGBA palette[16];
    for (int i = 0; i < 16; ++i)
        parse_rgba(hex[i], &palette[i]);

    vte_terminal_set_colors(term, &fg, &bg, palette, 16);
}

// ─────────────────────────────────────────────
//  Keyboard Logic Buttons
// ─────────────────────────────────────────────
static gboolean on_key(GtkWidget*, GdkEventKey* e, gpointer data) {
    VteTerminal* term = VTE_TERMINAL(data);
    bool ctrl = (e->state & GDK_CONTROL_MASK) != 0;

    if (!ctrl)
        return FALSE;

    // Ctrl + C (copy or SIGINT)
    if (e->keyval == GDK_KEY_c || e->keyval == GDK_KEY_C) {
        if (vte_terminal_get_has_selection(term)) {
            vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
            return TRUE;
        }
        return FALSE;  // let ^C go to the shell
    }

    // Ctrl + V (paste)
    if (e->keyval == GDK_KEY_v || e->keyval == GDK_KEY_V) {
        vte_terminal_paste_clipboard(term);
        return TRUE;
    }

    // ── Zoom: Ctrl + + / Ctrl + = ───────────────────────
    if (e->keyval == GDK_KEY_plus || e->keyval == GDK_KEY_equal || e->keyval == GDK_KEY_KP_Add) {
        adjust_font_scale(term, 0.1);   // zoom in
        return TRUE;
    }

    // ── Zoom: Ctrl + - ─────────────────────────────────
    if (e->keyval == GDK_KEY_minus || e->keyval == GDK_KEY_KP_Subtract) {
        adjust_font_scale(term, -0.1);  // zoom out
        return TRUE;
    }

    // ── Reset zoom: Ctrl + 0 ───────────────────────────
    if (e->keyval == GDK_KEY_0 || e->keyval == GDK_KEY_KP_0) {
        vte_terminal_set_font_scale(term, 1.0);
        return TRUE;
    }

    return FALSE;
}


// ─────────────────────────────────────────────
//  Right-click menu callbacks
// ─────────────────────────────────────────────
static void on_copy(GtkMenuItem*, gpointer user_data) {
    VteTerminal* term = VTE_TERMINAL(user_data);
    if (vte_terminal_get_has_selection(term))
        vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
}

static void on_paste(GtkMenuItem*, gpointer user_data) {
    vte_terminal_paste_clipboard(VTE_TERMINAL(user_data));
}

static void on_select_all(GtkMenuItem*, gpointer user_data) {
    vte_terminal_select_all(VTE_TERMINAL(user_data));
}

static gboolean on_button_press(GtkWidget*, GdkEventButton* event, gpointer user_data) {
    if (event->button != 3)
        return FALSE;

    VteTerminal* term = VTE_TERMINAL(user_data);
    GtkWidget* menu = gtk_menu_new();

    GtkWidget* i1 = gtk_menu_item_new_with_label("Copy");
    GtkWidget* i2 = gtk_menu_item_new_with_label("Paste");
    GtkWidget* i3 = gtk_menu_item_new_with_label("Select All");

    g_signal_connect(i1, "activate", G_CALLBACK(on_copy), term);
    g_signal_connect(i2, "activate", G_CALLBACK(on_paste), term);
    g_signal_connect(i3, "activate", G_CALLBACK(on_select_all), term);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), i1);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), i2);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), i3);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);

    return TRUE;
}

// ─────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────
int main(int argc, char** argv) {
    gtk_init(&argc, &argv);

    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "COLOSSUS — Terminal");
    gtk_window_set_default_size(GTK_WINDOW(win), 1100, 700);

    GtkWidget* term_widget = vte_terminal_new();
    VteTerminal* term = VTE_TERMINAL(term_widget);

    set_grayscale_palette(term);

    // Spawn shell (keeps your starship prompt)
    const char* shell = std::getenv("SHELL");
    if (!shell) shell = "/bin/bash";

    const char* home = std::getenv("HOME");
    if (!home) home = "/";

    char* argv_shell[] = { const_cast<char*>(shell), nullptr };

    vte_terminal_spawn_async(
        term,
        VTE_PTY_DEFAULT,
        home,
        argv_shell,
        nullptr,                 // inherit environment
        G_SPAWN_SEARCH_PATH,
        nullptr,                 // child_setup
        nullptr,                 // child_setup_data
        nullptr,                 // child_setup_data_destroy
        -1,                      // no timeout
        nullptr,                 // cancellable
        nullptr,                 // callback
        nullptr                  // user_data
    );

    // Connect signals
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key), term);
    gtk_widget_add_events(term_widget, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(term_widget, "button-press-event", G_CALLBACK(on_button_press), term);

	g_signal_connect(term, "child-exited",
                 	G_CALLBACK(on_child_exited),
                 	win);

    gtk_container_add(GTK_CONTAINER(win), term_widget);

    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
