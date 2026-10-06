// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <gtk/gtk.h>

#include <filesystem>
#include <string>

namespace infiltrator::files {

class ItemOperationController final {
public:
    ItemOperationController(GtkWindow *window,
                            GtkDirectoryList *directory_list,
                            GtkSingleSelection *selection,
                            GtkWidget *status_label);
    ~ItemOperationController();

    ItemOperationController(const ItemOperationController &) = delete;
    ItemOperationController &operator=(const ItemOperationController &) = delete;

private:
    enum class Kind {
        Rename,
        Copy,
        Move,
    };

    static void on_window_finalized(gpointer user_data, GObject *where_object_was);
    static void on_rename_clicked(GtkButton *button, gpointer user_data);
    static void on_copy_clicked(GtkButton *button, gpointer user_data);
    static void on_move_clicked(GtkButton *button, gpointer user_data);
    static gboolean on_key_pressed(GtkEventControllerKey *controller,
                                   guint keyval,
                                   guint keycode,
                                   GdkModifierType state,
                                   gpointer user_data);
    static void on_selection_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
    static void on_location_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
    static void on_items_changed(GListModel *model,
                                 guint position,
                                 guint removed,
                                 guint added,
                                 gpointer user_data);
    static void on_rename_dialog_destroy(GtkWidget *widget, gpointer user_data);
    static void on_rename_cancel(GtkButton *button, gpointer user_data);
    static void on_rename_submit(GtkButton *button, gpointer user_data);
    static void on_rename_entry_activate(GtkEntry *entry, gpointer user_data);
    static void on_destination_chosen(GObject *source_object,
                                      GAsyncResult *result,
                                      gpointer user_data);
    static void on_operation_thread(GTask *task,
                                    gpointer source_object,
                                    gpointer task_data,
                                    GCancellable *cancellable);
    static void on_operation_finished(GObject *source_object,
                                      GAsyncResult *result,
                                      gpointer user_data);

    void show_rename_dialog();
    void submit_rename();
    void choose_destination(Kind kind);
    void start_operation(Kind kind,
                         const std::string &source_path,
                         const std::string &destination_parent,
                         const std::string &new_name);
    void set_busy(bool busy);
    void update_action_state();
    void select_pending_item();
    void close_menu();
    void show_alert(const char *title, const std::string &detail) const;
    [[nodiscard]] bool selected_source(std::string &path, std::string &name) const;
    [[nodiscard]] bool current_location_is_path(const std::filesystem::path &path) const;

    GtkWindow *window_{nullptr};
    GtkDirectoryList *directory_list_{nullptr};
    GtkSingleSelection *selection_{nullptr};
    GtkWidget *status_label_{nullptr};
    GtkWidget *menu_button_{nullptr};
    GtkWidget *popover_{nullptr};
    GtkWidget *rename_button_{nullptr};
    GtkWidget *copy_button_{nullptr};
    GtkWidget *move_button_{nullptr};
    GtkWidget *rename_dialog_{nullptr};
    GtkWidget *rename_entry_{nullptr};
    GtkWidget *rename_message_{nullptr};
    gulong selection_handler_{0U};
    gulong location_handler_{0U};
    gulong items_handler_{0U};
    bool busy_{false};
    bool chooser_busy_{false};
    Kind chooser_kind_{Kind::Copy};
    std::string chooser_source_path_;
    std::string rename_source_path_;
    std::string pending_selection_name_;
};

} // namespace infiltrator::files
