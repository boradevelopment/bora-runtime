
// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#ifdef BORA_HAS_GIO
#include "gioWindowUtilities.h"
#include <gio/gio.h>

#include "Utilities.h"
#include "nWindow/linuxAbstracts.h"

const char* gioWindowUtilities::getIconName(MessageBoxIcon icon)
{
    switch (icon) {
    case MessageBoxIcon::Error:    return "dialog-error";
    case MessageBoxIcon::Warning:  return "dialog-warning";
    case MessageBoxIcon::Question: return "dialog-question";
    case MessageBoxIcon::Info:
    default:                        return "dialog-information";
    }
}

bool gioWindowUtilities::createMessageBox(SysHandle handle, const char* title, const char* message,
    MessageBoxIcon icon)
{
    GError* error = nullptr;
    GDBusConnection* conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!conn) {
        if (error) g_error_free(error);
        return false;
    }

    const char* iconName = getIconName(icon);
    GIcon* gicon = g_themed_icon_new(iconName);
    GVariant* serializedIcon = g_icon_serialize(gicon);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", "title", g_variant_new_string(title));
    g_variant_builder_add(&builder, "{sv}", "body", g_variant_new_string(message));
    g_variant_builder_add(&builder, "{sv}", "icon", serializedIcon);

    sString handleID = sString("bora_app_msg_") + (handle != nullptr ? handle->id : "");
    g_dbus_connection_call_sync(
        conn,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Notification",
        "AddNotification",
        g_variant_new("(sa{sv})", handle != nullptr ? handleID.c_str() : "bora_app_msg_default" , &builder),
        nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error
    );

    g_object_unref(gicon);
    if (error) g_error_free(error);

    return true;
}

bool gioWindowUtilities::createMessageBox(SysHandle handle, const wchar_t* title, const wchar_t* message,
    MessageBoxIcon icon)
{
    GError* error = nullptr;
    GDBusConnection* conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!conn) {
        if (error) g_error_free(error);
        return false;
    }

    const char* iconName = getIconName(icon);
    GIcon* gicon = g_themed_icon_new(iconName);
    GVariant* serializedIcon = g_icon_serialize(gicon);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", "title", g_variant_new_string(wstringToUtf8(title).c_str()));
    g_variant_builder_add(&builder, "{sv}", "body", g_variant_new_string(wstringToUtf8(message).c_str()));
    g_variant_builder_add(&builder, "{sv}", "icon", serializedIcon);

    sString handleID = sString("bora_app_msg_") + (handle != nullptr ? handle->id : "");
    g_dbus_connection_call_sync(
        conn,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Notification",
        "AddNotification",
        g_variant_new("(sa{sv})", handle != nullptr ? handleID.c_str() : "bora_app_msg_default" , &builder),
        nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error
    );

    g_object_unref(gicon);
    if (error) g_error_free(error);

    return true;
}

#endif
