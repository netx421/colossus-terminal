// COLOSSUS Terminal v0.4 (Monochrome Edition)
//
// C++ + GTK3 + VTE
// - Pure grayscale palette (no hue)
// - Ctrl+C / Ctrl+V copy/paste (selection-aware)
// - Ctrl+Shift+C / Ctrl+Shift+V also supported
// - Right-click context menu
// - CWD handling: honors process cwd + supports --cwd PATH / --cwd=PATH (also file:// URIs)
// - Drag & drop paths: drop files/folders to insert shell-escaped paths
// - Smart title updating: "COLOSSUS — <terminal title>"
// - CLI execution: -e, --execute, and "-- cmd args..."
// - Logs to /tmp/colossus-terminal.log
// - Spawn failures are printed inside the terminal
//
// Build:
//   g++ colossus-terminal.cpp -o colossus-terminal `pkg-config --cflags --libs gtk+-3.0 vte-2.91`
//
// Notes:
//   - If your distro uses a different vte pkg-config name, check: pkg-config --list-all | grep vte

#include <gtk/gtk.h>
#include <vte/vte.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

// ─────────────────────────────────────────────
//  Logging
// ─────────────────────────────────────────────
static void log_line(const std::string& s) {
    std::ofstream f("/tmp/colossus-terminal.log", std::ios::app);
    if (!f) return;
    f << s << "\n";
}

static std::string join_argv(const std::vector<std::string>& v) {
    std::ostringstream oss;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) oss << " ";
        oss << "\"" << v[i] << "\"";
    }
    return oss.str();
}

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b-1])) b--;
    return s.substr(a, b - a);
}

static bool starts_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && std::equal(p.begin(), p.end(), s.begin());
}

// ─────────────────────────────────────────────
//  Zoom helper
// ─────────────────────────────────────────────
static void adjust_font_scale(VteTerminal* term, double factor_delta) {
    double scale = vte_terminal_get_font_scale(term);
    scale += factor_delta;
    if (scale < 0.5) scale = 0.5;
    if (scale > 3.0) scale = 3.0;
    vte_terminal_set_font_scale(term, scale);
}

// ─────────────────────────────────────────────
//  Close window when child exits
// ─────────────────────────────────────────────
static void on_child_exited(VteTerminal* /*term*/, gint /*status*/, gpointer user_data) {
    GtkWindow* win = GTK_WINDOW(user_data);
    gtk_window_close(win);
}

// ─────────────────────────────────────────────
//  Helper: show a message inside the terminal
// ─────────────────────────────────────────────
static void terminal_notice(VteTerminal* term, const std::string& msg) {
    std::string m = "\r\n[COLOSSUS] " + msg + "\r\n";
    vte_terminal_feed(term, m.c_str(), (gssize)m.size());
}

// ─────────────────────────────────────────────
//  Spawn callback: prints/logs errors
// ─────────────────────────────────────────────
static void on_spawn_ready(VteTerminal* term, GPid pid, GError* error, gpointer /*user_data*/) {
    if (error) {
        std::string e = std::string("spawn failed: ") + error->message;
        log_line(e);
        terminal_notice(term, e);
        return;
    }
    log_line(std::string("spawn ok, pid=") + std::to_string((int)pid));
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
    parse_rgba("#d0d0d0", &fg);
    parse_rgba("#050505", &bg);

    const char* hex[16] = {
        "#000000", "#202020", "#404040", "#606060",
        "#808080", "#9a9a9a", "#bcbcbc", "#dcdcdc",
        "#101010", "#303030", "#505050", "#707070",
        "#909090", "#b0b0b0", "#d0d0d0", "#ffffff"
    };

    GdkRGBA palette[16];
    for (int i = 0; i < 16; ++i) parse_rgba(hex[i], &palette[i]);

    vte_terminal_set_colors(term, &fg, &bg, palette, 16);
}

// ─────────────────────────────────────────────
//  Keyboard shortcuts
// ─────────────────────────────────────────────
static gboolean on_key(GtkWidget*, GdkEventKey* e, gpointer data) {
    VteTerminal* term = VTE_TERMINAL(data);
    bool ctrl  = (e->state & GDK_CONTROL_MASK) != 0;
    bool shift = (e->state & GDK_SHIFT_MASK) != 0;

    if (!ctrl) return FALSE;

    // Ctrl+Shift+C / Ctrl+Shift+V (expected terminal behavior)
    if (shift && (e->keyval == GDK_KEY_c || e->keyval == GDK_KEY_C)) {
        if (vte_terminal_get_has_selection(term))
            vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
        return TRUE;
    }
    if (shift && (e->keyval == GDK_KEY_v || e->keyval == GDK_KEY_V)) {
        vte_terminal_paste_clipboard(term);
        return TRUE;
    }

    // Ctrl+C / Ctrl+V (selection-aware: let ^C through if no selection)
    if (e->keyval == GDK_KEY_c || e->keyval == GDK_KEY_C) {
        if (vte_terminal_get_has_selection(term)) {
            vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
            return TRUE;
        }
        return FALSE; // let SIGINT pass
    }
    if (e->keyval == GDK_KEY_v || e->keyval == GDK_KEY_V) {
        vte_terminal_paste_clipboard(term);
        return TRUE;
    }

    // Zoom
    if (e->keyval == GDK_KEY_plus || e->keyval == GDK_KEY_equal || e->keyval == GDK_KEY_KP_Add) {
        adjust_font_scale(term, 0.1);
        return TRUE;
    }
    if (e->keyval == GDK_KEY_minus || e->keyval == GDK_KEY_KP_Subtract) {
        adjust_font_scale(term, -0.1);
        return TRUE;
    }
    if (e->keyval == GDK_KEY_0 || e->keyval == GDK_KEY_KP_0) {
        vte_terminal_set_font_scale(term, 1.0);
        return TRUE;
    }

    return FALSE;
}

// ─────────────────────────────────────────────
//  Right-click menu
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
    if (event->button != 3) return FALSE;

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
//  CLI: execution argv for VTE spawn
// ─────────────────────────────────────────────
static std::vector<std::string> build_spawn_argv(int argc, char** argv) {
    // Supported:
    //   terminal -e "cmd..."
    //   terminal -e cmd arg1 arg2...
    //   terminal --execute "cmd..."
    //   terminal -- cmd arg1...

    int i = 1;
    bool exec_mode = false;

    for (; i < argc; ++i) {
        std::string a = argv[i];

        if (a == "--") {
            exec_mode = true;
            ++i;
            break;
        }
        if (a == "-e" || a == "--execute") {
            exec_mode = true;
            ++i;
            break;
        }
    }

    if (!exec_mode || i >= argc) return {};

    const int remaining = argc - i;
    std::vector<std::string> out;

    const char* shell = std::getenv("SHELL");
    if (!shell || !*shell) shell = "/bin/bash";

    if (remaining == 1) {
        out = {shell, "-lc", std::string(argv[i])};
    } else {
        out.reserve((size_t)remaining);
        for (; i < argc; ++i) out.emplace_back(argv[i]);
    }
    return out;
}

// ─────────────────────────────────────────────
//  CWD parsing / normalization
// ─────────────────────────────────────────────
static std::string uri_to_path_if_needed(const std::string& s) {
    if (g_str_has_prefix(s.c_str(), "file://")) {
        GError* err = nullptr;
        char* fn = g_filename_from_uri(s.c_str(), nullptr, &err);
        if (fn) {
            std::string out = fn;
            g_free(fn);
            if (err) g_error_free(err);
            return out;
        }
        if (err) g_error_free(err);
    }
    return s;
}

static std::string parse_cwd_arg(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cwd" && i + 1 < argc) {
            return uri_to_path_if_needed(argv[i + 1]);
        }
        if (starts_with(a, "--cwd=")) {
            return uri_to_path_if_needed(a.substr(6));
        }
    }
    return {};
}

static std::string resolve_workdir(int argc, char** argv) {
    const char* home = std::getenv("HOME");
    if (!home) home = "/";

    std::string wd = parse_cwd_arg(argc, argv);

    if (!wd.empty()) {
        // If it's a file, switch to its directory
        if (g_file_test(wd.c_str(), G_FILE_TEST_IS_REGULAR)) {
            char* dir = g_path_get_dirname(wd.c_str());
            if (dir) { wd = dir; g_free(dir); }
        }
        // Validate directory
        if (!g_file_test(wd.c_str(), G_FILE_TEST_IS_DIR)) wd.clear();
    }

    if (wd.empty()) {
        // Default: process cwd (this is what makes Thunar "Open Terminal Here" work)
        char* cwd = g_get_current_dir();
        if (cwd && *cwd) wd = cwd;
        g_free(cwd);
    }

    if (wd.empty()) wd = home;
    return wd;
}

// ─────────────────────────────────────────────
//  Shell escaping for dropped paths
// ─────────────────────────────────────────────
static std::string shell_escape_single_quotes(const std::string& s) {
    // Return a POSIX-safe single-quoted string:
    // 'abc' becomes 'abc'
    // "a'b" becomes 'a'"'"'b'
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'') out += "'\"'\"'";
        else out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

// ─────────────────────────────────────────────
//  Drag & drop: insert paths into terminal
// ─────────────────────────────────────────────
static void on_drag_data_received(GtkWidget* /*widget*/,
                                 GdkDragContext* context,
                                 gint /*x*/, gint /*y*/,
                                 GtkSelectionData* data,
                                 guint /*info*/,
                                 guint time,
                                 gpointer user_data)
{
    VteTerminal* term = VTE_TERMINAL(user_data);

    if (!data) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    gchar** uris = gtk_selection_data_get_uris(data);
    if (!uris) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    std::string chunk;
    for (int i = 0; uris[i]; ++i) {
        GError* err = nullptr;
        char* path = g_filename_from_uri(uris[i], nullptr, &err);
        if (!path) {
            if (err) g_error_free(err);
            continue;
        }

        std::string p = path;
        g_free(path);

        if (!chunk.empty()) chunk.push_back(' ');
        chunk += shell_escape_single_quotes(p);
    }

    g_strfreev(uris);

    if (!chunk.empty()) {
        // Feed to child so it appears at prompt/cursor
        vte_terminal_feed_child(term, chunk.c_str(), (gssize)chunk.size());
    }

    gtk_drag_finish(context, TRUE, FALSE, time);
}

// ─────────────────────────────────────────────
//  Smart title updating
// ─────────────────────────────────────────────
static void on_title_changed(VteTerminal* term, gpointer user_data) {
    GtkWindow* win = GTK_WINDOW(user_data);
    const char* t = vte_terminal_get_window_title(term);
    std::string title = "COLOSSUS — ";
    title += (t && *t) ? t : "Terminal";
    gtk_window_set_title(win, title.c_str());
}

// ─────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────
int main(int argc, char** argv) {
    // Parse BEFORE GTK touches argv
    auto cmd = build_spawn_argv(argc, argv);
    std::string workdir = resolve_workdir(argc, argv);

    // Reset log each run
    {
        std::ofstream f("/tmp/colossus-terminal.log", std::ios::trunc);
        if (f) f << "COLOSSUS Terminal start\n";
    }

    log_line("workdir: " + workdir);

    if (!cmd.empty()) log_line("exec requested: " + join_argv(cmd));
    else              log_line("no exec requested: spawning default shell");

    gtk_init(&argc, &argv);

    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "COLOSSUS — Terminal");
    gtk_window_set_default_size(GTK_WINDOW(win), 1100, 700);

    GtkWidget* term_widget = vte_terminal_new();
    VteTerminal* term = VTE_TERMINAL(term_widget);

    // QoL defaults
    vte_terminal_set_scrollback_lines(term, 10000);
    vte_terminal_set_scroll_on_keystroke(term, TRUE);
    vte_terminal_set_scroll_on_output(term, FALSE);
    vte_terminal_set_audible_bell(term, FALSE);
    vte_terminal_set_visible_bell(term, TRUE);
    vte_terminal_set_allow_hyperlink(term, TRUE);

    set_grayscale_palette(term);

    // Build spawn argv (cmd strings remain alive in main scope)
    std::vector<char*> spawn_argv;
    if (!cmd.empty()) {
        for (auto& s : cmd) spawn_argv.push_back(const_cast<char*>(s.c_str()));
        spawn_argv.push_back(nullptr);
    } else {
        const char* shell = std::getenv("SHELL");
        if (!shell || !*shell) shell = "/bin/bash";
        spawn_argv.push_back(const_cast<char*>(shell));
        spawn_argv.push_back(nullptr);
    }

    // Spawn shell/program in desired working directory
    vte_terminal_spawn_async(
        term,
        VTE_PTY_DEFAULT,
        workdir.c_str(),
        spawn_argv.data(),
        nullptr,                 // inherit environment
        G_SPAWN_SEARCH_PATH,
        nullptr, nullptr, nullptr,
        -1,
        nullptr,
        on_spawn_ready,
        nullptr
    );

    // Smart title updates
    g_signal_connect(term, "window-title-changed", G_CALLBACK(on_title_changed), win);
    on_title_changed(term, win); // initialize title

    // Signals
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key), term);
    gtk_widget_add_events(term_widget, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(term_widget, "button-press-event", G_CALLBACK(on_button_press), term);
    g_signal_connect(term, "child-exited", G_CALLBACK(on_child_exited), win);

    // Drag & drop setup
    GtkTargetEntry targets[] = {
        {(gchar*)"text/uri-list", 0, 0}
    };
    gtk_drag_dest_set(term_widget, GTK_DEST_DEFAULT_ALL, targets, 1, (GdkDragAction)(GDK_ACTION_COPY));
    g_signal_connect(term_widget, "drag-data-received", G_CALLBACK(on_drag_data_received), term);

    gtk_container_add(GTK_CONTAINER(win), term_widget);

    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
