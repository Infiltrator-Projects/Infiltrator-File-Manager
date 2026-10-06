// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../core/location.hpp"

#include <gtk/gtk.h>

#include <cstddef>
#include <string>
#include <vector>

namespace infiltrator::files {

class FileManagerWindow final {
public:
    explicit FileManagerWindow(GtkApplication *application);
    ~FileManagerWindow();

    FileManagerWindow(const FileManagerWindow &) = delete;
    FileManagerWindow &operator=(const FileManagerWindow &) = delete;

    void present();

private:
    static void on_window_destroy(GtkWidget *widget, gpointer user_data);
    static void on_back_clicked(GtkButton *button, gpointer user_data);
    static void on_forward_clicked(GtkButton *button, gpointer user_data);
    static void on_up_clicked(GtkButton *button, gpointer user_data);
    static void on_location_activate(GtkEntry *entry, gpointer user_data);
    static void on_sidebar_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
    static void on_list_activate(GtkListView *view, guint position, gpointer user_data);
    static void on_factory_setup(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer user_data);
    static void on_factory_bind(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer user_data);
    static void on_loading_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
    static void on_launch_finished(GObject *source, GAsyncResult *result, gpointer user_data);

    void build_ui(GtkApplication *application);
    void apply_theme();
    void add_sidebar_location(const char *title, const char *icon_name, const char *target);
    void navigate_to(GFile *file, bool record_history);
    void navigate_history(std::ptrdiff_t delta);
    void update_navigation_state();
    void update_status();
    [[nodiscard]] GFile *file_from_location_text(const char *text) const;

    GtkWidget *window_{nullptr};
    GtkWidget *back_button_{nullptr};
    GtkWidget *forward_button_{nullptr};
    GtkWidget *up_button_{nullptr};
    GtkWidget *location_entry_{nullptr};
    GtkWidget *sidebar_{nullptr};
    GtkWidget *status_label_{nullptr};
    GtkWidget *spinner_{nullptr};
    GtkDirectoryList *directory_list_{nullptr};
    GtkSingleSelection *selection_{nullptr};
    GFile *current_location_{nullptr};
    std::vector<Location> history_;
    std::size_t history_index_{0};
};

} // namespace infiltrator::files
