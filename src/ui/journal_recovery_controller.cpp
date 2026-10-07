// SPDX-License-Identifier: GPL-3.0-or-later
#include "journal_recovery_controller.hpp"

#include <algorithm>
#include <sstream>
#include <string>

namespace infiltrator::files {

JournalRecoveryController::JournalRecoveryController(GtkWindow *window,
                                                     GtkWidget *status_label)
    : window_(window), status_label_(status_label)
{
    if (window_ == nullptr) {
        return;
    }

    OperationJournal journal;
    interrupted_ = journal.interrupted_operations();
    if (!interrupted_.empty()) {
        for (const auto &operation : interrupted_) {
            if (!journal.mark_interrupted(operation)) {
                journal_closed_cleanly_ = false;
            }
        }

        GtkWidget *titlebar = gtk_window_get_titlebar(window_);
        if (GTK_IS_HEADER_BAR(titlebar)) {
            review_button_ = gtk_button_new_from_icon_name("dialog-warning-symbolic");
            const std::string tooltip = std::to_string(interrupted_.size()) +
                (interrupted_.size() == 1U
                     ? " interrupted file operation requires inspection"
                     : " interrupted file operations require inspection");
            gtk_widget_set_tooltip_text(review_button_, tooltip.c_str());
            gtk_header_bar_pack_end(GTK_HEADER_BAR(titlebar), review_button_);
            g_signal_connect(review_button_, "clicked",
                             G_CALLBACK(&JournalRecoveryController::on_review_clicked), this);
        }

        if (status_label_ != nullptr) {
            const std::string message = std::to_string(interrupted_.size()) +
                (interrupted_.size() == 1U
                     ? " interrupted operation requires inspection."
                     : " interrupted operations require inspection.");
            gtk_label_set_text(GTK_LABEL(status_label_), message.c_str());
        }
    }

    g_object_weak_ref(G_OBJECT(window_), &JournalRecoveryController::on_window_finalized, this);
}

void JournalRecoveryController::on_window_finalized(gpointer user_data,
                                                    GObject *where_object_was)
{
    (void)where_object_was;
    auto *self = static_cast<JournalRecoveryController *>(user_data);
    self->window_ = nullptr;
    delete self;
}

void JournalRecoveryController::on_review_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    static_cast<JournalRecoveryController *>(user_data)->show_review();
}

void JournalRecoveryController::show_review() const
{
    if (window_ == nullptr || interrupted_.empty()) {
        return;
    }

    std::ostringstream detail;
    detail << "Files found durable START records without matching END records from a previous session. "
           << "They have been closed conservatively as verification failures; success was not inferred.\n\n";

    const std::size_t shown = std::min<std::size_t>(interrupted_.size(), 12U);
    for (std::size_t index = 0; index < shown; ++index) {
        const auto &operation = interrupted_[index];
        detail << operation.kind << "\n  Source: " << operation.source
               << "\n  Destination: " << operation.destination << "\n";
    }
    if (shown < interrupted_.size()) {
        detail << "\n" << (interrupted_.size() - shown) << " additional interrupted operations are recorded in the journal.";
    }
    if (!journal_closed_cleanly_) {
        detail << "\n\nAt least one recovery END record could not be persisted. The journal itself should also be inspected.";
    }

    GtkAlertDialog *dialog = gtk_alert_dialog_new("Interrupted file operations");
    const std::string text = detail.str();
    gtk_alert_dialog_set_detail(dialog, text.c_str());
    gtk_alert_dialog_show(dialog, window_);
    g_object_unref(dialog);
}

} // namespace infiltrator::files
