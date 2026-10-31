// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: waylandEvents.h
 * Purpose: ?
*/
#pragma once
#include <linux/input-event-codes.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

extern const struct xdg_wm_base_listener xdg_wm_base_listener;
extern const xdg_surface_listener g_xdg_surface_listener;
extern const struct wl_output_listener g_OutputListener;
extern const xdg_toplevel_listener g_XdgToplevelListener;
extern const struct wl_pointer_listener g_pointer_listener;
extern const struct wl_keyboard_listener g_keyboard_listener;
extern const struct wl_seat_listener g_seat_listener;
extern const struct wl_registry_listener registry_listener;