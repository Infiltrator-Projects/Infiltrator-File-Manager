// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../core/location.hpp"
#include "../core/mounted_places.hpp"
#include "batch_item_operation_controller.hpp"
#include "create_folder_controller.hpp"
#include "destructive_operation_controller.hpp"
#include "item_operation_controller.hpp"
#include "journal_recovery_controller.hpp"

#include <gtk/gtk.h>

#include <cstddef>
#include <cstdint>
#include <memory>
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
    void install_operation_surface()
    {
        if (operation_surface_installed_) {
            return;
        }
        new CreateFolderController(GTK_WINDOW(window_), directory_list_, primary_selection_, status_label_);
        new ItemOperationController(GTK_WINDOW(window_), directory_list_, primary_selection_, status_label_);
        new DestructiveOperationController(GTK_WINDOW(window_), directory_list_, primary_selection_, status_label_);
        new BatchItemOperationController(GTK_WINDOW(window_), directory_list_, selection_, status_label_);
        new JournalRecoveryController(GTK_WINDOW(window_), status_label_);
        operation_surface_installed_ = true;
    }
    [[nodiscard]] GtkWindow *native_window() const noexcept { return GTK_WINDOW(window_); }

private:
    static void on_window_finalized(gpointer user_data, GObject *where_object_was);
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
    static void on_multi_selection_changed(GtkSelectionModel *model,
                                           guint position,
                                           guint n_items,
                                           gpointer user_data);
    static void on_primary_selection_changed(GObject *object,
                                             GParamSpec *pspec,
                                             gpointer user_data);

    void build_ui(GtkApplication *application);
    void apply_theme();
    void add_sidebar_location(const char *title, const char *icon_name, const char *target);
    void refresh_mounted_places();
    void navigate_to(GFile *file, bool record_history);
    void navigate_history(std::ptrdiff_t delta);
    void update_navigation_state();
    void update_status();
    void sync_primary_from_multi();
    void sync_multi_from_primary();
    [[nodiscard]] GFile *file_from_location_text(const char *text) const;

    GtkWidget *window_{nullptr};
    GtkWidget *back_button_{nullptr};
    GtkWidget *forward_button_{nullptr};
    GtkWidget *up_button_{nullptr};
    GtkWidget *location_entry_{nullptr};
    GtkWidget *sidebar_{nullptr};
    GtkWidget *status_label_{nullptr};
    GtkWidget *spinner_{nullptr};
    std::unique_ptr<MountedPlacesMonitor> mounted_places_monitor_;
    std::vector<GtkWidget *> mounted_place_rows_;
    GtkDirectoryList *directory_list_{nullptr};
    GtkMultiSelection *selection_{nullptr};
    GtkSingleSelection *primary_selection_{nullptr};
    GFile *current_location_{nullptr};
    std::vector<Location> history_;
    std::size_t history_index_{0};
    bool operation_surface_installed_{false};
    bool selection_syncing_{false};
};

} // namespace infiltrator::files
