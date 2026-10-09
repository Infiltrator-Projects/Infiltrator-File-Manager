// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_manager_window.hpp"

#include <infiltratr/design.h>

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>

namespace infiltrator::files {

namespace {

constexpr const char *kDirectoryAttributes =
    "standard::name,standard::display-name,standard::type,standard::size,"
    "standard::content-type,standard::icon,standard::is-hidden,time::modified";

std::string colour_hex(const std::uint32_t rgb)
{
    char buffer[8]{};
    std::snprintf(buffer, sizeof(buffer), "#%06X", rgb & 0x00FFFFFFU);
    return buffer;
}

GtkWidget *make_sidebar_content(const char *title, const char *icon_name)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 7);
    gtk_widget_set_margin_bottom(box, 7);

    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 18);
    gtk_box_append(GTK_BOX(box), icon);

    GtkWidget *label = gtk_label_new(title);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(box), label);
    return box;
}

GtkWidget *make_scroller(GtkWidget *child)
{
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), child);
    return scroll;
}

} // namespace

FileManagerWindow::FileManagerWindow(GtkApplication *application)
{
    build_ui(application);
    apply_theme();

    g_object_weak_ref(G_OBJECT(window_), &FileManagerWindow::on_window_finalized, this);

    GFile *home = g_file_new_for_path(g_get_home_dir());
    navigate_to(home, true);
    g_object_unref(home);
}

FileManagerWindow::~FileManagerWindow()
{
    // Stop mount callbacks before any UI/model references owned by this object are released.
    mounted_places_monitor_.reset();

    if (primary_selection_ != nullptr) {
        g_object_unref(primary_selection_);
        primary_selection_ = nullptr;
    }
    if (current_location_ != nullptr) {
        g_object_unref(current_location_);
        current_location_ = nullptr;
    }
}

void FileManagerWindow::present()
{
    gtk_window_present(GTK_WINDOW(window_));
}

void FileManagerWindow::on_window_finalized(gpointer user_data, GObject *where_object_was)
{
    (void)where_object_was;
    delete static_cast<FileManagerWindow *>(user_data);
}

void FileManagerWindow::build_ui(GtkApplication *application)
{
    window_ = gtk_application_window_new(application);
    gtk_window_set_title(GTK_WINDOW(window_), "Files");
    gtk_window_set_default_size(GTK_WINDOW(window_), 1120, 760);

    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window_), header);

    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    back_button_ = gtk_button_new_from_icon_name("go-previous-symbolic");
    forward_button_ = gtk_button_new_from_icon_name("go-next-symbolic");
    up_button_ = gtk_button_new_from_icon_name("go-up-symbolic");
    gtk_widget_set_tooltip_text(back_button_, "Back");
    gtk_widget_set_tooltip_text(forward_button_, "Forward");
    gtk_widget_set_tooltip_text(up_button_, "Up");
    gtk_box_append(GTK_BOX(nav_box), back_button_);
    gtk_box_append(GTK_BOX(nav_box), forward_button_);
    gtk_box_append(GTK_BOX(nav_box), up_button_);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), nav_box);

    location_entry_ = gtk_entry_new();
    gtk_widget_set_hexpand(location_entry_, TRUE);
    gtk_widget_set_size_request(location_entry_, 430, -1);
    gtk_entry_set_placeholder_text(GTK_ENTRY(location_entry_), "Location");
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), location_entry_);

    GtkWidget *view_switcher = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(view_switcher, "linked");
    gtk_widget_add_css_class(view_switcher, "ifm-view-switcher");

    list_view_button_ = gtk_toggle_button_new_with_label("List");
    icon_view_button_ = gtk_toggle_button_new_with_label("Icons");
    compact_view_button_ = gtk_toggle_button_new_with_label("Small");
    gtk_widget_set_tooltip_text(list_view_button_, "List view");
    gtk_widget_set_tooltip_text(icon_view_button_, "Icon view");
    gtk_widget_set_tooltip_text(compact_view_button_, "Small icon view");

    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(icon_view_button_),
                                GTK_TOGGLE_BUTTON(list_view_button_));
    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(compact_view_button_),
                                GTK_TOGGLE_BUTTON(list_view_button_));

    g_object_set_data(G_OBJECT(list_view_button_), "ifm-view-mode", GINT_TO_POINTER(1));
    g_object_set_data(G_OBJECT(icon_view_button_), "ifm-view-mode", GINT_TO_POINTER(2));
    g_object_set_data(G_OBJECT(compact_view_button_), "ifm-view-mode", GINT_TO_POINTER(3));

    g_signal_connect(list_view_button_, "toggled", G_CALLBACK(on_view_mode_toggled), this);
    g_signal_connect(icon_view_button_, "toggled", G_CALLBACK(on_view_mode_toggled), this);
    g_signal_connect(compact_view_button_, "toggled", G_CALLBACK(on_view_mode_toggled), this);

    gtk_box_append(GTK_BOX(view_switcher), list_view_button_);
    gtk_box_append(GTK_BOX(view_switcher), icon_view_button_);
    gtk_box_append(GTK_BOX(view_switcher), compact_view_button_);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), view_switcher);

    g_signal_connect(back_button_, "clicked", G_CALLBACK(on_back_clicked), this);
    g_signal_connect(forward_button_, "clicked", G_CALLBACK(on_forward_clicked), this);
    g_signal_connect(up_button_, "clicked", G_CALLBACK(on_up_clicked), this);
    g_signal_connect(location_entry_, "activate", G_CALLBACK(on_location_activate), this);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window_), root);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_box_append(GTK_BOX(root), paned);

    sidebar_ = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(sidebar_), GTK_SELECTION_NONE);
    gtk_widget_set_size_request(sidebar_, 220, -1);
    gtk_widget_add_css_class(sidebar_, "ifm-sidebar");

    GtkWidget *sidebar_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sidebar_scroll), sidebar_);
    gtk_paned_set_start_child(GTK_PANED(paned), sidebar_scroll);
    gtk_paned_set_position(GTK_PANED(paned), 220);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);

    add_sidebar_location("Home", "user-home-symbolic", g_get_home_dir());
    if (const char *desktop = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP); desktop != nullptr) {
        add_sidebar_location("Desktop", "user-desktop-symbolic", desktop);
    }
    if (const char *documents = g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS); documents != nullptr) {
        add_sidebar_location("Documents", "folder-documents-symbolic", documents);
    }
    if (const char *downloads = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD); downloads != nullptr) {
        add_sidebar_location("Downloads", "folder-download-symbolic", downloads);
    }
    add_sidebar_location("Computer", "drive-harddisk-symbolic", "/");
    add_sidebar_location("Trash", "user-trash-symbolic", "trash:///");
    mounted_places_monitor_ = std::make_unique<MountedPlacesMonitor>([this]() {
        refresh_mounted_places();
    });
    refresh_mounted_places();
    g_signal_connect(sidebar_, "row-activated", G_CALLBACK(on_sidebar_row_activated), this);

    directory_list_ = gtk_directory_list_new(kDirectoryAttributes, nullptr);
    gtk_directory_list_set_monitored(directory_list_, TRUE);
    g_signal_connect(directory_list_, "notify::loading", G_CALLBACK(on_loading_changed), this);
    g_signal_connect(directory_list_, "notify::error", G_CALLBACK(on_loading_changed), this);

    selection_ = gtk_multi_selection_new(G_LIST_MODEL(directory_list_));
    primary_selection_ = gtk_single_selection_new(G_LIST_MODEL(directory_list_));
    gtk_single_selection_set_autoselect(primary_selection_, FALSE);
    g_signal_connect(selection_, "selection-changed",
                     G_CALLBACK(on_multi_selection_changed), this);
    g_signal_connect(primary_selection_, "notify::selected",
                     G_CALLBACK(on_primary_selection_changed), this);

    GtkListItemFactory *list_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(list_factory, "setup", G_CALLBACK(on_factory_setup), this);
    g_signal_connect(list_factory, "bind", G_CALLBACK(on_factory_bind), this);

    GtkWidget *list = gtk_list_view_new(GTK_SELECTION_MODEL(selection_), list_factory);
    gtk_list_view_set_single_click_activate(GTK_LIST_VIEW(list), FALSE);
    gtk_widget_add_css_class(list, "ifm-file-list");
    g_signal_connect(list, "activate", G_CALLBACK(on_list_activate), this);

    GtkListItemFactory *icon_factory = gtk_signal_list_item_factory_new();
    g_object_set_data(G_OBJECT(icon_factory), "ifm-icon-size", GINT_TO_POINTER(64));
    g_object_set_data(G_OBJECT(icon_factory), "ifm-tile-width", GINT_TO_POINTER(132));
    g_signal_connect(icon_factory, "setup", G_CALLBACK(on_icon_factory_setup), this);
    g_signal_connect(icon_factory, "bind", G_CALLBACK(on_icon_factory_bind), this);

    GtkWidget *icons = gtk_grid_view_new(GTK_SELECTION_MODEL(selection_), icon_factory);
    gtk_grid_view_set_single_click_activate(GTK_GRID_VIEW(icons), FALSE);
    gtk_widget_add_css_class(icons, "ifm-icon-grid");
    g_signal_connect(icons, "activate", G_CALLBACK(on_grid_activate), this);

    GtkListItemFactory *compact_factory = gtk_signal_list_item_factory_new();
    g_object_set_data(G_OBJECT(compact_factory), "ifm-icon-size", GINT_TO_POINTER(32));
    g_object_set_data(G_OBJECT(compact_factory), "ifm-tile-width", GINT_TO_POINTER(92));
    g_signal_connect(compact_factory, "setup", G_CALLBACK(on_icon_factory_setup), this);
    g_signal_connect(compact_factory, "bind", G_CALLBACK(on_icon_factory_bind), this);

    GtkWidget *compact = gtk_grid_view_new(GTK_SELECTION_MODEL(selection_), compact_factory);
    gtk_grid_view_set_single_click_activate(GTK_GRID_VIEW(compact), FALSE);
    gtk_widget_add_css_class(compact, "ifm-icon-grid");
    gtk_widget_add_css_class(compact, "ifm-compact-grid");
    g_signal_connect(compact, "activate", G_CALLBACK(on_grid_activate), this);

    content_stack_ = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(content_stack_), GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_stack_add_named(GTK_STACK(content_stack_), make_scroller(list), "list");
    gtk_stack_add_named(GTK_STACK(content_stack_), make_scroller(icons), "icons");
    gtk_stack_add_named(GTK_STACK(content_stack_), make_scroller(compact), "compact");
    gtk_paned_set_end_child(GTK_PANED(paned), content_stack_);
    gtk_paned_set_resize_end_child(GTK_PANED(paned), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(paned), FALSE);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(list_view_button_), TRUE);

    GtkWidget *status = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(status, 12);
    gtk_widget_set_margin_end(status, 12);
    gtk_widget_set_margin_top(status, 5);
    gtk_widget_set_margin_bottom(status, 5);
    gtk_widget_add_css_class(status, "ifm-status");

    spinner_ = gtk_spinner_new();
    gtk_widget_set_visible(spinner_, FALSE);
    gtk_box_append(GTK_BOX(status), spinner_);

    status_label_ = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(status_label_), 0.0F);
    gtk_widget_set_hexpand(status_label_, TRUE);
    gtk_box_append(GTK_BOX(status), status_label_);
    gtk_box_append(GTK_BOX(root), status);
}

void FileManagerWindow::apply_theme()
{
    GtkSettings *settings = gtk_settings_get_default();
    gboolean system_dark = FALSE;
    if (settings != nullptr) {
        g_object_get(settings, "gtk-application-prefer-dark-theme", &system_dark, nullptr);
    }

    const InfiltratrThemePalette *palette =
        infiltratr_theme_resolve(INFILTRATR_THEME_SYSTEM, system_dark != FALSE);
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    const InfiltratrTypography *typography = infiltratr_typography();
    if (palette == nullptr || metrics == nullptr || typography == nullptr) {
        return;
    }

    std::ostringstream css;
    css << "window { background: " << colour_hex(palette->background_rgb)
        << "; color: " << colour_hex(palette->text_rgb)
        << "; font-family: '" << typography->ui_family << "', " << typography->gtk_fallback << "; }\n"
        << "headerbar { background: " << colour_hex(palette->titlebar_rgb)
        << "; color: " << colour_hex(palette->title_rgb)
        << "; border-bottom: 1px solid " << colour_hex(palette->border_rgb) << "; }\n"
        << ".ifm-sidebar { background: " << colour_hex(palette->panel_rgb)
        << "; border-right: 1px solid " << colour_hex(palette->border_rgb) << "; }\n"
        << ".ifm-sidebar row { margin: 2px 6px; border-radius: " << metrics->control_radius << "px; }\n"
        << ".ifm-sidebar row:hover { background: " << colour_hex(palette->surface_hover_rgb) << "; }\n"
        << ".ifm-file-list, .ifm-icon-grid { background: " << colour_hex(palette->background_rgb) << "; }\n"
        << ".ifm-file-list row { margin: 2px 8px; border-radius: " << metrics->control_radius << "px; }\n"
        << ".ifm-file-list row:hover { background: " << colour_hex(palette->surface_hover_rgb) << "; }\n"
        << ".ifm-file-list row:selected { background: " << colour_hex(palette->selection_background_rgb)
        << "; color: " << colour_hex(palette->selection_foreground_rgb) << "; }\n"
        << ".ifm-icon-tile { margin: 4px; padding: 8px 6px; border-radius: "
        << metrics->control_radius << "px; }\n"
        << ".ifm-icon-tile:hover { background: " << colour_hex(palette->surface_hover_rgb) << "; }\n"
        << ".ifm-compact-tile { margin: 2px; padding: 6px 4px; }\n"
        << ".ifm-file-name { color: " << colour_hex(palette->text_rgb) << "; }\n"
        << ".ifm-file-meta { color: " << colour_hex(palette->muted_rgb) << "; font-size: 0.88em; }\n"
        << ".ifm-status { background: " << colour_hex(palette->panel_rgb)
        << "; color: " << colour_hex(palette->muted_rgb)
        << "; border-top: 1px solid " << colour_hex(palette->border_rgb) << "; }\n"
        << "entry { background: " << colour_hex(palette->input_rgb)
        << "; color: " << colour_hex(palette->text_rgb)
        << "; border-radius: " << metrics->control_radius << "px; }\n";

    GtkCssProvider *provider = gtk_css_provider_new();
    const std::string stylesheet = css.str();
    gtk_css_provider_load_from_data(provider, stylesheet.c_str(), static_cast<gssize>(stylesheet.size()));
    gtk_style_context_add_provider_for_display(gtk_widget_get_display(window_),
                                               GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

void FileManagerWindow::add_sidebar_location(const char *title, const char *icon_name, const char *target)
{
    if (target == nullptr || target[0] == '\0') {
        return;
    }

    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), make_sidebar_content(title, icon_name));
    g_object_set_data_full(G_OBJECT(row), "ifm-target", g_strdup(target), g_free);
    gtk_list_box_append(GTK_LIST_BOX(sidebar_), row);
}

void FileManagerWindow::refresh_mounted_places()
{
    if (mounted_places_monitor_ == nullptr) {
        return;
    }

    const std::vector<MountedPlace> next_places = mounted_places_monitor_->snapshot();
    std::string current_uri;
    if (current_location_ != nullptr) {
        char *uri = g_file_get_uri(current_location_);
        if (uri != nullptr) {
            current_uri = uri;
            g_free(uri);
        }
    }

    const bool was_on_mounted_place =
        location_is_within_mounted_places(current_uri, mounted_places_);
    const bool remains_on_mounted_place =
        location_is_within_mounted_places(current_uri, next_places);

    for (auto it = mounted_place_rows_.begin(); it != mounted_place_rows_.end();) {
        const bool still_present = std::any_of(next_places.begin(), next_places.end(),
                                               [&it](const MountedPlace &place) {
                                                   return place.uri == it->place.uri;
                                               });
        if (still_present) {
            ++it;
            continue;
        }
        gtk_list_box_remove(GTK_LIST_BOX(sidebar_), it->row);
        it = mounted_place_rows_.erase(it);
    }

    for (const MountedPlace &place : next_places) {
        auto existing = std::find_if(mounted_place_rows_.begin(), mounted_place_rows_.end(),
                                     [&place](const MountedPlaceRow &entry) {
                                         return entry.place.uri == place.uri;
                                     });
        if (existing != mounted_place_rows_.end()) {
            if (existing->place.name != place.name ||
                existing->place.removable != place.removable) {
                const char *icon_name = place.removable ? "drive-removable-media-symbolic"
                                                        : "drive-harddisk-symbolic";
                gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(existing->row),
                                           make_sidebar_content(place.name.c_str(), icon_name));
            }
            existing->place = place;
            continue;
        }

        GtkWidget *row = gtk_list_box_row_new();
        const char *icon_name = place.removable ? "drive-removable-media-symbolic"
                                                : "drive-harddisk-symbolic";
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row),
                                   make_sidebar_content(place.name.c_str(), icon_name));
        g_object_set_data_full(G_OBJECT(row), "ifm-target", g_strdup(place.uri.c_str()), g_free);
        gtk_list_box_append(GTK_LIST_BOX(sidebar_), row);
        mounted_place_rows_.push_back(MountedPlaceRow{place, row});
    }

    mounted_places_ = next_places;
    if (was_on_mounted_place && !remains_on_mounted_place) {
        mark_current_location_unavailable();
    }
}

void FileManagerWindow::mark_current_location_unavailable()
{
    if (!current_location_available_) {
        return;
    }

    current_location_available_ = false;
    if (selection_ != nullptr) {
        gtk_selection_model_unselect_all(GTK_SELECTION_MODEL(selection_));
    }
    if (directory_list_ != nullptr) {
        gtk_directory_list_set_file(directory_list_, nullptr);
    }
    if (spinner_ != nullptr) {
        gtk_spinner_stop(GTK_SPINNER(spinner_));
        gtk_widget_set_visible(spinner_, FALSE);
    }
    if (status_label_ != nullptr) {
        gtk_label_set_text(GTK_LABEL(status_label_),
                           "Location unavailable — device was disconnected.");
    }
    update_navigation_state();
}

void FileManagerWindow::navigate_to(GFile *file, const bool record_history)
{
    if (file == nullptr) {
        return;
    }

    char *uri_text = g_file_get_uri(file);
    char *parse_name = g_file_get_parse_name(file);
    if (uri_text == nullptr || parse_name == nullptr) {
        g_free(uri_text);
        g_free(parse_name);
        return;
    }

    Location location{uri_text, parse_name, classify_location_uri(uri_text)};

    if (record_history) {
        const bool duplicate_current = !history_.empty() && history_index_ < history_.size() &&
                                       history_[history_index_].uri == location.uri;
        if (!duplicate_current) {
            if (!history_.empty() && history_index_ + 1U < history_.size()) {
                history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_index_ + 1U),
                               history_.end());
            }
            history_.push_back(location);
            history_index_ = history_.size() - 1U;
        }
    }

    if (current_location_ != nullptr) {
        g_object_unref(current_location_);
    }
    current_location_ = G_FILE(g_object_ref(file));
    current_location_available_ = true;

    gtk_selection_model_unselect_all(GTK_SELECTION_MODEL(selection_));
    gtk_directory_list_set_file(directory_list_, file);
    gtk_editable_set_text(GTK_EDITABLE(location_entry_), parse_name);

    const std::string title = location.display_name.empty() ? "Files" : "Files — " + location.display_name;
    gtk_window_set_title(GTK_WINDOW(window_), title.c_str());

    g_free(uri_text);
    g_free(parse_name);
    update_navigation_state();
    update_status();
}

void FileManagerWindow::navigate_history(const std::ptrdiff_t delta)
{
    if (history_.empty()) {
        return;
    }

    const std::ptrdiff_t current = static_cast<std::ptrdiff_t>(history_index_);
    const std::ptrdiff_t target = current + delta;
    if (target < 0 || target >= static_cast<std::ptrdiff_t>(history_.size())) {
        return;
    }

    history_index_ = static_cast<std::size_t>(target);
    GFile *file = g_file_new_for_uri(history_[history_index_].uri.c_str());
    navigate_to(file, false);
    g_object_unref(file);
}

void FileManagerWindow::activate_position(const guint position)
{
    if (current_location_ == nullptr) {
        return;
    }

    GFileInfo *info = G_FILE_INFO(g_list_model_get_item(G_LIST_MODEL(directory_list_), position));
    if (info == nullptr) {
        return;
    }

    const char *name = g_file_info_get_name(info);
    if (name == nullptr) {
        g_object_unref(info);
        return;
    }

    GFile *child = g_file_get_child(current_location_, name);
    if (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
        navigate_to(child, true);
    } else {
        char *uri = g_file_get_uri(child);
        if (uri != nullptr) {
            g_app_info_launch_default_for_uri_async(uri, nullptr, nullptr,
                                                    &FileManagerWindow::on_launch_finished, this);
            g_free(uri);
        }
    }

    g_object_unref(child);
    g_object_unref(info);
}

void FileManagerWindow::set_view_mode(const ViewMode mode)
{
    if (content_stack_ == nullptr) {
        return;
    }

    view_mode_ = mode;
    switch (mode) {
    case ViewMode::List:
        gtk_stack_set_visible_child_name(GTK_STACK(content_stack_), "list");
        break;
    case ViewMode::Icons:
        gtk_stack_set_visible_child_name(GTK_STACK(content_stack_), "icons");
        break;
    case ViewMode::Compact:
        gtk_stack_set_visible_child_name(GTK_STACK(content_stack_), "compact");
        break;
    }
}

void FileManagerWindow::update_navigation_state()
{
    gtk_widget_set_sensitive(back_button_, !history_.empty() && history_index_ > 0U);
    gtk_widget_set_sensitive(forward_button_, !history_.empty() && history_index_ + 1U < history_.size());

    bool has_parent = false;
    if (current_location_available_ && current_location_ != nullptr) {
        GFile *parent = g_file_get_parent(current_location_);
        has_parent = parent != nullptr;
        if (parent != nullptr) {
            g_object_unref(parent);
        }
    }
    gtk_widget_set_sensitive(up_button_, has_parent);
}

void FileManagerWindow::update_status()
{
    if (directory_list_ == nullptr) {
        return;
    }

    if (!current_location_available_) {
        gtk_spinner_stop(GTK_SPINNER(spinner_));
        gtk_widget_set_visible(spinner_, FALSE);
        gtk_label_set_text(GTK_LABEL(status_label_),
                           "Location unavailable — device was disconnected.");
        return;
    }

    if (gtk_directory_list_is_loading(directory_list_)) {
        gtk_widget_set_visible(spinner_, TRUE);
        gtk_spinner_start(GTK_SPINNER(spinner_));
        gtk_label_set_text(GTK_LABEL(status_label_), "Loading…");
        return;
    }

    gtk_spinner_stop(GTK_SPINNER(spinner_));
    gtk_widget_set_visible(spinner_, FALSE);

    if (const GError *error = gtk_directory_list_get_error(directory_list_); error != nullptr) {
        gtk_label_set_text(GTK_LABEL(status_label_), error->message);
        return;
    }

    const guint count = g_list_model_get_n_items(G_LIST_MODEL(directory_list_));
    const std::string text = std::to_string(count) + (count == 1U ? " item" : " items");
    gtk_label_set_text(GTK_LABEL(status_label_), text.c_str());
}

void FileManagerWindow::sync_primary_from_multi()
{
    if (selection_syncing_ || selection_ == nullptr || primary_selection_ == nullptr) {
        return;
    }

    selection_syncing_ = true;
    guint selected_index = GTK_INVALID_LIST_POSITION;
    guint selected_count = 0U;
    const guint count = g_list_model_get_n_items(G_LIST_MODEL(directory_list_));
    for (guint index = 0U; index < count; ++index) {
        if (!gtk_selection_model_is_selected(GTK_SELECTION_MODEL(selection_), index)) {
            continue;
        }
        ++selected_count;
        if (selected_count == 1U) {
            selected_index = index;
        } else {
            selected_index = GTK_INVALID_LIST_POSITION;
            break;
        }
    }
    gtk_single_selection_set_selected(primary_selection_, selected_index);
    selection_syncing_ = false;
}

void FileManagerWindow::sync_multi_from_primary()
{
    if (selection_syncing_ || selection_ == nullptr || primary_selection_ == nullptr) {
        return;
    }

    selection_syncing_ = true;
    gtk_selection_model_unselect_all(GTK_SELECTION_MODEL(selection_));
    const guint selected = gtk_single_selection_get_selected(primary_selection_);
    if (selected != GTK_INVALID_LIST_POSITION) {
        gtk_selection_model_select_item(GTK_SELECTION_MODEL(selection_), selected, TRUE);
    }
    selection_syncing_ = false;
}

GFile *FileManagerWindow::file_from_location_text(const char *text) const
{
    if (text == nullptr || text[0] == '\0') {
        return nullptr;
    }

    std::string value(text);
    if (value == "~") {
        value = g_get_home_dir();
    } else if (value.rfind("~/", 0U) == 0U) {
        value = std::string(g_get_home_dir()) + value.substr(1U);
    }

    char *scheme = g_uri_parse_scheme(value.c_str());
    if (scheme != nullptr) {
        g_free(scheme);
        return g_file_new_for_uri(value.c_str());
    }
    return g_file_new_for_commandline_arg(value.c_str());
}

void FileManagerWindow::on_back_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    static_cast<FileManagerWindow *>(user_data)->navigate_history(-1);
}

void FileManagerWindow::on_forward_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    static_cast<FileManagerWindow *>(user_data)->navigate_history(1);
}

void FileManagerWindow::on_up_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    auto *self = static_cast<FileManagerWindow *>(user_data);
    if (self->current_location_ == nullptr) {
        return;
    }
    GFile *parent = g_file_get_parent(self->current_location_);
    if (parent != nullptr) {
        self->navigate_to(parent, true);
        g_object_unref(parent);
    }
}

void FileManagerWindow::on_location_activate(GtkEntry *entry, gpointer user_data)
{
    auto *self = static_cast<FileManagerWindow *>(user_data);
    GFile *file = self->file_from_location_text(gtk_editable_get_text(GTK_EDITABLE(entry)));
    if (file != nullptr) {
        self->navigate_to(file, true);
        g_object_unref(file);
    }
}

void FileManagerWindow::on_sidebar_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
    (void)box;
    auto *self = static_cast<FileManagerWindow *>(user_data);
    const auto *target = static_cast<const char *>(g_object_get_data(G_OBJECT(row), "ifm-target"));
    GFile *file = self->file_from_location_text(target);
    if (file != nullptr) {
        self->navigate_to(file, true);
        g_object_unref(file);
    }
}

void FileManagerWindow::on_list_activate(GtkListView *view, const guint position, gpointer user_data)
{
    (void)view;
    static_cast<FileManagerWindow *>(user_data)->activate_position(position);
}

void FileManagerWindow::on_grid_activate(GtkGridView *view, const guint position, gpointer user_data)
{
    (void)view;
    static_cast<FileManagerWindow *>(user_data)->activate_position(position);
}

void FileManagerWindow::on_view_mode_toggled(GtkToggleButton *button, gpointer user_data)
{
    if (!gtk_toggle_button_get_active(button)) {
        return;
    }

    const int encoded = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "ifm-view-mode"));
    if (encoded < 1 || encoded > 3) {
        return;
    }

    auto *self = static_cast<FileManagerWindow *>(user_data);
    self->set_view_mode(static_cast<ViewMode>(encoded - 1));
}

void FileManagerWindow::on_factory_setup(GtkSignalListItemFactory *factory,
                                         GtkListItem *item,
                                         gpointer user_data)
{
    (void)factory;
    (void)user_data;

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(row, 12);
    gtk_widget_set_margin_end(row, 12);
    gtk_widget_set_margin_top(row, 8);
    gtk_widget_set_margin_bottom(row, 8);

    GtkWidget *icon = gtk_image_new();
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 30);
    gtk_box_append(GTK_BOX(row), icon);

    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(text, TRUE);
    gtk_box_append(GTK_BOX(row), text);

    GtkWidget *name = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(name), 0.0F);
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(name, "ifm-file-name");
    gtk_box_append(GTK_BOX(text), name);

    GtkWidget *meta = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(meta), 0.0F);
    gtk_label_set_ellipsize(GTK_LABEL(meta), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(meta, "ifm-file-meta");
    gtk_box_append(GTK_BOX(text), meta);

    g_object_set_data(G_OBJECT(row), "ifm-icon", icon);
    g_object_set_data(G_OBJECT(row), "ifm-name", name);
    g_object_set_data(G_OBJECT(row), "ifm-meta", meta);
    gtk_list_item_set_child(item, row);
}

void FileManagerWindow::on_factory_bind(GtkSignalListItemFactory *factory,
                                        GtkListItem *item,
                                        gpointer user_data)
{
    (void)factory;
    (void)user_data;

    auto *info = G_FILE_INFO(gtk_list_item_get_item(item));
    GtkWidget *row = gtk_list_item_get_child(item);
    if (info == nullptr || row == nullptr) {
        return;
    }

    auto *icon = GTK_IMAGE(g_object_get_data(G_OBJECT(row), "ifm-icon"));
    auto *name = GTK_LABEL(g_object_get_data(G_OBJECT(row), "ifm-name"));
    auto *meta = GTK_LABEL(g_object_get_data(G_OBJECT(row), "ifm-meta"));

    if (GIcon *file_icon = g_file_info_get_icon(info); file_icon != nullptr) {
        gtk_image_set_from_gicon(icon, file_icon);
    } else {
        gtk_image_set_from_icon_name(icon, "text-x-generic-symbolic");
    }

    const char *display_name = g_file_info_get_display_name(info);
    gtk_label_set_text(name, display_name != nullptr ? display_name : "");

    if (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
        gtk_label_set_text(meta, "Folder");
    } else {
        char *formatted_size = g_format_size(static_cast<guint64>(g_file_info_get_size(info)));
        gtk_label_set_text(meta, formatted_size != nullptr ? formatted_size : "File");
        g_free(formatted_size);
    }
}

void FileManagerWindow::on_icon_factory_setup(GtkSignalListItemFactory *factory,
                                              GtkListItem *item,
                                              gpointer user_data)
{
    (void)user_data;

    const int icon_size = std::max(24, GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(factory), "ifm-icon-size")));
    const int tile_width = std::max(72, GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(factory), "ifm-tile-width")));

    GtkWidget *tile = gtk_box_new(GTK_ORIENTATION_VERTICAL, icon_size >= 48 ? 7 : 4);
    gtk_widget_set_size_request(tile, tile_width, -1);
    gtk_widget_set_halign(tile, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(tile, GTK_ALIGN_START);
    gtk_widget_add_css_class(tile, "ifm-icon-tile");
    if (icon_size < 48) {
        gtk_widget_add_css_class(tile, "ifm-compact-tile");
    }

    GtkWidget *icon = gtk_image_new();
    gtk_image_set_pixel_size(GTK_IMAGE(icon), icon_size);
    gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(tile), icon);

    GtkWidget *name = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(name), 0.5F);
    gtk_label_set_justify(GTK_LABEL(name), GTK_JUSTIFY_CENTER);
    gtk_label_set_wrap(GTK_LABEL(name), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(name), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_lines(GTK_LABEL(name), icon_size >= 48 ? 2 : 1);
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(name), icon_size >= 48 ? 18 : 12);
    gtk_widget_add_css_class(name, "ifm-file-name");
    gtk_box_append(GTK_BOX(tile), name);

    g_object_set_data(G_OBJECT(tile), "ifm-icon", icon);
    g_object_set_data(G_OBJECT(tile), "ifm-name", name);
    gtk_list_item_set_child(item, tile);
}

void FileManagerWindow::on_icon_factory_bind(GtkSignalListItemFactory *factory,
                                             GtkListItem *item,
                                             gpointer user_data)
{
    (void)factory;
    (void)user_data;

    auto *info = G_FILE_INFO(gtk_list_item_get_item(item));
    GtkWidget *tile = gtk_list_item_get_child(item);
    if (info == nullptr || tile == nullptr) {
        return;
    }

    auto *icon = GTK_IMAGE(g_object_get_data(G_OBJECT(tile), "ifm-icon"));
    auto *name = GTK_LABEL(g_object_get_data(G_OBJECT(tile), "ifm-name"));

    if (GIcon *file_icon = g_file_info_get_icon(info); file_icon != nullptr) {
        gtk_image_set_from_gicon(icon, file_icon);
    } else {
        gtk_image_set_from_icon_name(icon, "text-x-generic-symbolic");
    }

    const char *display_name = g_file_info_get_display_name(info);
    gtk_label_set_text(name, display_name != nullptr ? display_name : "");
}

void FileManagerWindow::on_loading_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    static_cast<FileManagerWindow *>(user_data)->update_status();
}

void FileManagerWindow::on_launch_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
    (void)source;
    auto *self = static_cast<FileManagerWindow *>(user_data);
    GError *error = nullptr;
    if (!g_app_info_launch_default_for_uri_finish(result, &error)) {
        if (error != nullptr) {
            gtk_label_set_text(GTK_LABEL(self->status_label_), error->message);
            g_error_free(error);
        }
    }
}

void FileManagerWindow::on_multi_selection_changed(GtkSelectionModel *model,
                                                   const guint position,
                                                   const guint n_items,
                                                   gpointer user_data)
{
    (void)model;
    (void)position;
    (void)n_items;
    static_cast<FileManagerWindow *>(user_data)->sync_primary_from_multi();
}

void FileManagerWindow::on_primary_selection_changed(GObject *object,
                                                     GParamSpec *pspec,
                                                     gpointer user_data)
{
    (void)object;
    (void)pspec;
    static_cast<FileManagerWindow *>(user_data)->sync_multi_from_primary();
}

} // namespace infiltrator::files
