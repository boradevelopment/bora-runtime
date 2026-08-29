// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#if defined(BORA_HAS_GTK4) || defined(BORA_HAS_GTK3)
#include "gtkWindowUtilities.h"
#include <gtk/gtk.h>
#include "Utilities.h"

const char* gtkWindowUtilities::getIconName(MessageBoxIcon icon)
{
    switch (icon) {
    case MessageBoxIcon::Error:    return "dialog-error";
    case MessageBoxIcon::Warning:  return "dialog-warning";
    case MessageBoxIcon::Question: return "dialog-question";
    case MessageBoxIcon::Info:
    default:                        return "dialog-information";
    }
}

bool gtkWindowUtilities::createMessageBox(SysHandle handle, const char* title, const char* message,
    MessageBoxIcon icon)
{
#if defined(BORA_HAS_GTK4)
    if (!gtk_is_initialized()) {
        gtk_init();
    }

    GtkAlertDialog* alert = gtk_alert_dialog_new("%s", title ? title : "");
    gtk_alert_dialog_set_detail(alert, message ? message : "");
    gtk_alert_dialog_set_modal(alert, TRUE);
    GtkWindow* parentWindow = handle ? GTK_WINDOW(handle) : nullptr;

    // todo icon support

    gtk_alert_dialog_show(alert, parentWindow);
    g_object_unref(alert);

    while (g_main_context_pending(nullptr)) {
        g_main_context_iteration(nullptr, FALSE);
    }
    return true;
#elif defined(BORA_HAS_GTK3)
    if (!gtk_is_initialized()) {
        gtk_init(nullptr, nullptr);
    }

    GtkMessageType msgType = GTK_MESSAGE_INFO;
    switch (icon) {
    case MessageBoxIcon::Error:    msgType = GTK_MESSAGE_ERROR; break;
    case MessageBoxIcon::Warning:  msgType = GTK_MESSAGE_WARNING; break;
    case MessageBoxIcon::Question: msgType = GTK_MESSAGE_QUESTION; break;
    default:                       msgType = GTK_MESSAGE_INFO; break;
    }

    GtkWidget* dialog = gtk_message_dialog_new(
        handle ? GTK_WINDOW(handle) : nullptr,
        GTK_DIALOG_MODAL,
        msgType,
        GTK_BUTTONS_OK,
        "%s", title ? title : ""
    );
    if (message) {
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", message);
    }

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
    return true;

#else
    return false;
#endif
}

bool gtkWindowUtilities::createMessageBox(SysHandle handle, const wchar_t* title, const wchar_t* message,
    MessageBoxIcon icon)
{
#if defined(BORA_HAS_GTK4)
    if (!gtk_is_initialized()) {
        gtk_init();
    }

    GtkAlertDialog* alert = gtk_alert_dialog_new("%s", !wstringToUtf8(title).empty() ? wstringToUtf8(title).c_str() : "");
    gtk_alert_dialog_set_detail(alert, !wstringToUtf8(message).empty() ? wstringToUtf8(message).c_str() : "");
    gtk_alert_dialog_set_modal(alert, TRUE);
    GtkWindow* parentWindow = handle ? GTK_WINDOW(handle) : nullptr;

    // todo icon support

    gtk_alert_dialog_show(alert, parentWindow);
    g_object_unref(alert);

    while (g_main_context_pending(nullptr)) {
        g_main_context_iteration(nullptr, FALSE);
    }
    return true;
#elif defined(BORA_HAS_GTK3)
    if (!gtk_is_initialized()) {
        gtk_init(nullptr, nullptr);
    }

    GtkMessageType msgType = GTK_MESSAGE_INFO;
    switch (icon) {
    case MessageBoxIcon::Error:    msgType = GTK_MESSAGE_ERROR; break;
    case MessageBoxIcon::Warning:  msgType = GTK_MESSAGE_WARNING; break;
    case MessageBoxIcon::Question: msgType = GTK_MESSAGE_QUESTION; break;
    default:                       msgType = GTK_MESSAGE_INFO; break;
    }

    GtkWidget* dialog = gtk_message_dialog_new(
        handle ? GTK_WINDOW(handle) : nullptr,
        GTK_DIALOG_MODAL,
        msgType,
        GTK_BUTTONS_OK,
        "%s", title ? title : ""
    );
    if (message) {
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", message);
    }

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
    return true;

#else
    return false;
#endif
}

#endif
