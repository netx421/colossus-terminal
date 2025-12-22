// COLOSSUS Terminal v0.4 (Monochrome Edition)
//
// BUILD-CLEAN for GTK3 + VTE 2.91
//
// Features:
// - Grayscale palette
// - Ctrl+C/V (selection-aware) + Ctrl+Shift+C/V
// - Right-click context menu
// - CWD handling (--cwd, file:// URIs, Thunar-compatible)
// - Drag & drop paths (shell-escaped)
// - Smart window title ("COLOSSUS — <shell title>")
// - Exec args (-e, --execute, -- cmd)
// - Logs to /tmp/colossus-terminal.log
// - Spawn errors printed inside terminal

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
// Logging
// ─────────────────────────────────────────────
static void log_line(const std::string& s) {
    std::ofstream f("/tmp/colossus-terminal.log", std::ios::app);
    if (f) f << s << "\n";
}

static std::string join_argv(const std::vector<std::string>& v) {
    std::ostringstream oss;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) oss << " ";
        oss << "\"" << v[i] << "\"";
    }
    return oss.str();
}

static bool starts_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && std::equal(p.begin(), p.end(), s.begin());
}

// ─────────────────────────────────────────────
// Zoom
// ─────────────────────────────────────────────
static void adjust_font_scale(VteTerminal* term, double delta) {
    double scale = vte_terminal_get_font_scale(term) + delta;
    if (scale < 0.5) scale = 0.5;
    if (scale > 3.0) scale = 3.0;
    vte_terminal_set_font_scale(term, scale);
}

// ─────────────────────────────────────────────
// Exit when shell exits
// ─────────────────────────────────────────────
static void on_child_exited(VteTerminal*, gint, gpointer win) {
    gtk_window_close(GTK_WINDOW(win));
}

// ─────────────────────────────────────────────
// Terminal notice
// ─────────────────────────────────────────────
static void terminal_notice(VteTerminal* term, const std::string& msg) {
    std::string m = "\r\n[COLOSSUS] " + msg + "\r\n";
    vte_terminal_feed(term, m.c_str(), m.size());
}

// ─────────────────────────────────────────────
// Spawn callback
// ─────────────────────────────────────────────
static void on_spawn_ready(VteTerminal* term, GPid pid, GError* err, gpointer) {
    if (err) {
        terminal_notice(term, err->message);
        log_line(err->message);
        return;
    }
    log_line("spawn ok pid=" + std::to_string(pid));
}

// ─────────────────────────────────────────────
// Colors
// ─────────────────────────────────────────────
static void parse_rgba(const char* hex, GdkRGBA* out) {
    if (!gdk_rgba_parse(out, hex)) {
        out->red = out->green = out->blue = 0.0;
        out->alpha = 1.0;
    }
}

static void set_grayscale_palette(VteTerminal* term) {
    GdkRGBA fg, bg;
    parse_rgba("#d0d0d0", &fg);
    parse_rgba("#050505", &bg);

    const char* hex[16] = {
        "#000000","#202020","#404040","#606060",
        "#808080","#9a9a9a","#bcbcbc","#dcdcdc",
        "#101010","#303030","#505050","#707070",
        "#909090","#b0b0b0","#d0d0d0","#ffffff"
    };

    GdkRGBA palette[16];
    for (int i = 0; i < 16; ++i) parse_rgba(hex[i], &palette[i]);

    vte_terminal_set_colors(term, &fg, &bg, palette, 16);
}

// ─────────────────────────────────────────────
// Keyboard handling
// ─────────────────────────────────────────────
static gboolean on_key(GtkWidget*, GdkEventKey* e, gpointer data) {
    VteTerminal* term = VTE_TERMINAL(data);
    bool ctrl  = e->state & GDK_CONTROL_MASK;
    bool shift = e->state & GDK_SHIFT_MASK;

    if (!ctrl) return FALSE;

    if (shift && (e->keyval == GDK_KEY_c || e->keyval == GDK_KEY_C)) {
        vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
        return TRUE;
    }
    if (shift && (e->keyval == GDK_KEY_v || e->keyval == GDK_KEY_V)) {
        vte_terminal_paste_clipboard(term);
        return TRUE;
    }

    if (e->keyval == GDK_KEY_c || e->keyval == GDK_KEY_C) {
        if (vte_terminal_get_has_selection(term)) {
            vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
            return TRUE;
        }
        return FALSE;
    }

    if (e->keyval == GDK_KEY_v || e->keyval == GDK_KEY_V) {
        vte_terminal_paste_clipboard(term);
        return TRUE;
    }

    if (e->keyval == GDK_KEY_plus || e->keyval == GDK_KEY_equal)
        return adjust_font_scale(term, 0.1), TRUE;

    if (e->keyval == GDK_KEY_minus)
        return adjust_font_scale(term, -0.1), TRUE;

    if (e->keyval == GDK_KEY_0)
        return vte_terminal_set_font_scale(term, 1.0), TRUE;

    return FALSE;
}

// ─────────────────────────────────────────────
// Context menu
// ─────────────────────────────────────────────
static gboolean on_button(GtkWidget*, GdkEventButton* e, gpointer term) {
    if (e->button != 3) return FALSE;

    GtkWidget* menu = gtk_menu_new();
    auto add = [&](const char* name, GCallback cb) {
        GtkWidget* i = gtk_menu_item_new_with_label(name);
        g_signal_connect(i, "activate", cb, term);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i);
    };

    add("Copy",  G_CALLBACK(vte_terminal_copy_clipboard));
    add("Paste", G_CALLBACK(vte_terminal_paste_clipboard));
    add("Select All", G_CALLBACK(vte_terminal_select_all));

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)e);
    return TRUE;
}

// ─────────────────────────────────────────────
// Exec parsing
// ─────────────────────────────────────────────
static std::vector<std::string> build_spawn_argv(int argc, char** argv) {
    int i = 1;
    bool exec = false;

    for (; i < argc; ++i) {
        if (!strcmp(argv[i], "--") || !strcmp(argv[i], "-e") || !strcmp(argv[i], "--execute")) {
            exec = true;
            ++i;
            break;
        }
    }
    if (!exec || i >= argc) return {};

    const char* shell = getenv("SHELL");
    if (!shell) shell = "/bin/bash";

    if (i + 1 == argc)
        return {shell, "-lc", argv[i]};

    std::vector<std::string> out;
    for (; i < argc; ++i) out.emplace_back(argv[i]);
    return out;
}

// ─────────────────────────────────────────────
// Working directory resolution
// ─────────────────────────────────────────────
static std::string uri_to_path(const std::string& s) {
    if (starts_with(s, "file://")) {
        char* p = g_filename_from_uri(s.c_str(), nullptr, nullptr);
        if (p) {
            std::string out = p;
            g_free(p);
            return out;
        }
    }
    return s;
}

static std::string resolve_workdir(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (!strcmp(argv[i], "--cwd"))
            return uri_to_path(argv[i + 1]);
        if (starts_with(argv[i], "--cwd="))
            return uri_to_path(argv[i] + 6);
    }

    char* cwd = g_get_current_dir();
    std::string out = cwd ? cwd : "/";
    g_free(cwd);
    return out;
}

// ─────────────────────────────────────────────
// Drag & drop paths
// ─────────────────────────────────────────────
static std::string shell_quote(const std::string& s) {
    std::string o = "'";
    for (char c : s) o += (c == '\'') ? "'\"'\"'" : std::string(1, c);
    return o + "'";
}

static void on_drop(GtkWidget*, GdkDragContext* ctx, gint, gint,
                    GtkSelectionData* data, guint, guint t, gpointer term)
{
    gchar** uris = gtk_selection_data_get_uris(data);
    if (!uris) return gtk_drag_finish(ctx, FALSE, FALSE, t);

    std::string out;
    for (int i = 0; uris[i]; ++i) {
        char* p = g_filename_from_uri(uris[i], nullptr, nullptr);
        if (p) {
            if (!out.empty()) out += " ";
            out += shell_quote(p);
            g_free(p);
        }
    }
    g_strfreev(uris);

    if (!out.empty())
        vte_terminal_feed_child(VTE_TERMINAL(term), (out + " ").c_str(), -1);

    gtk_drag_finish(ctx, TRUE, FALSE, t);
}

// ─────────────────────────────────────────────
// Title update
// ─────────────────────────────────────────────
static void on_title_changed(VteTerminal* term, gpointer win) {
    char* t = nullptr;
    g_object_get(term, "window-title", &t, nullptr);

    std::string title = "COLOSSUS — ";
    title += (t && *t) ? t : "Terminal";
    gtk_window_set_title(GTK_WINDOW(win), title.c_str());

    if (t) g_free(t);
}

// ─────────────────────────────────────────────
// MAIN
// ─────────────────────────────────────────────
int main(int argc, char** argv) {
    auto cmd = build_spawn_argv(argc, argv);
    std::string cwd = resolve_workdir(argc, argv);

    gtk_init(&argc, &argv);

    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(win), 1100, 700);

    GtkWidget* w = vte_terminal_new();
    VteTerminal* term = VTE_TERMINAL(w);

    vte_terminal_set_scrollback_lines(term, 10000);
    vte_terminal_set_scroll_on_keystroke(term, TRUE);
    vte_terminal_set_scroll_on_output(term, FALSE);
    vte_terminal_set_audible_bell(term, FALSE);

    set_grayscale_palette(term);

    std::vector<char*> argv_spawn;
    if (!cmd.empty()) {
        for (auto& s : cmd) argv_spawn.push_back(const_cast<char*>(s.c_str()));
    } else {
        const char* sh = getenv("SHELL");
        if (!sh) sh = "/bin/bash";
        argv_spawn.push_back(const_cast<char*>(sh));
    }
    argv_spawn.push_back(nullptr);

    vte_terminal_spawn_async(
        term, VTE_PTY_DEFAULT, cwd.c_str(),
        argv_spawn.data(), nullptr, G_SPAWN_SEARCH_PATH,
        nullptr, nullptr, nullptr, -1, nullptr,
        on_spawn_ready, nullptr
    );

    g_signal_connect(term, "window-title-changed", G_CALLBACK(on_title_changed), win);
    on_title_changed(term, win);

    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key), term);
    gtk_widget_add_events(w, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(w, "button-press-event", G_CALLBACK(on_button), term);
    g_signal_connect(term, "child-exited", G_CALLBACK(on_child_exited), win);

    GtkTargetEntry targets[] = {{(gchar*)"text/uri-list", 0, 0}};
    gtk_drag_dest_set(w, GTK_DEST_DEFAULT_ALL, targets, 1, GDK_ACTION_COPY);
    g_signal_connect(w, "drag-data-received", G_CALLBACK(on_drop), term);

    gtk_container_add(GTK_CONTAINER(win), w);
    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
