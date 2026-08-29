// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: waylandEvents.h
 * Purpose: ?
*/
#pragma once
#include <linux/input-event-codes.h>
#include "nWindow/bnWindow.h"
#include <wayland-client.h>

static void xdg_wm_base_ping(void* data, struct xdg_wm_base* xdg_wm_base, uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static constexpr struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

// Surface configuration callback
static void xdg_surface_configure(void* data, struct xdg_surface* xdg_surface, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
}

static constexpr xdg_surface_listener g_xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

// Called to report spatial location, physical size, subpixel geometry, and make/model
static void output_geometry(void* data, struct wl_output* wl_output,
                            int32_t x, int32_t y,
                            int32_t physical_width, int32_t physical_height,
                            int32_t subpixel,
                            const char* make, const char* model,
                            int32_t transform);

// Called for available resolutions and refresh rates
static void output_mode(void* data, struct wl_output* wl_output,
                        uint32_t flags, int32_t width, int32_t height, int32_t refresh);

// Called when all output events for an atomic state update have been sent (Wayland version >= 2)
static void output_done(void* data, struct wl_output* wl_output);

// Called to specify the integer scale factor of the output (Wayland version >= 2)
static void output_scale(void* data, struct wl_output* wl_output, int32_t factor);

// Instantiate the listener struct
static constexpr struct wl_output_listener g_OutputListener = {
    .geometry = output_geometry,
    .mode     = output_mode,
    .done     = output_done,     // Required for version >= 2
    .scale    = output_scale,    // Required for version >= 2
    .name     = nullptr,         // Optional: add if binding version >= 4
    .description = nullptr,      // Optional: add if binding version >= 4
};

void xdg_toplevel_handle_configure(void* data, struct xdg_toplevel* xdg_toplevel,
                                           int32_t width, int32_t height, struct wl_array* states);

void xdg_toplevel_handle_close(void* data, struct xdg_toplevel* xdg_toplevel);

static constexpr xdg_toplevel_listener g_XdgToplevelListener = {
    .configure = xdg_toplevel_handle_configure,
    .close = xdg_toplevel_handle_close,
};

void pointer_handle_enter(void* data, struct wl_pointer* pointer,
                                 uint32_t serial, struct wl_surface* surface,
                                 wl_fixed_t sx, wl_fixed_t sy);

void pointer_handle_leave(void* data, struct wl_pointer* pointer,
                                 uint32_t serial, struct wl_surface* surface);

void pointer_handle_motion(void* data, struct wl_pointer* pointer,
                                  uint32_t time, wl_fixed_t sx, wl_fixed_t sy);

void pointer_handle_button(void* data, struct wl_pointer* pointer,
                                  uint32_t serial, uint32_t time, uint32_t button, uint32_t state);

void pointer_handle_axis(void* data, struct wl_pointer* pointer,
                                uint32_t time, uint32_t axis, wl_fixed_t value);

static constexpr struct wl_pointer_listener g_pointer_listener = {
    .enter = pointer_handle_enter,
    .leave = pointer_handle_leave,
    .motion = pointer_handle_motion,
    .button = pointer_handle_button,
    .axis = pointer_handle_axis,
};

void keyboard_handle_key(void* data, struct wl_keyboard* keyboard,
                                uint32_t serial, uint32_t time, uint32_t key, uint32_t state);

static void keyboard_handle_keymap(void* data, struct wl_keyboard* keyboard,
                                   uint32_t format, int32_t fd, uint32_t size) {}
static void keyboard_handle_enter(void* data, struct wl_keyboard* keyboard,
                                  uint32_t serial, struct wl_surface* surface, struct wl_array* keys) {}
static void keyboard_handle_leave(void* data, struct wl_keyboard* keyboard,
                                  uint32_t serial, struct wl_surface* surface) {}
static void keyboard_handle_modifiers(void* data, struct wl_keyboard* keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {}

static constexpr struct wl_keyboard_listener g_keyboard_listener = {
    .keymap = keyboard_handle_keymap,
    .enter = keyboard_handle_enter,
    .leave = keyboard_handle_leave,
    .key = keyboard_handle_key,
    .modifiers = keyboard_handle_modifiers,
};

static void seat_handle_capabilities(void* data, struct wl_seat* seat, uint32_t caps);

static void seat_handle_name(void* data, struct wl_seat* seat, const char* name) {}

static constexpr struct wl_seat_listener g_seat_listener = {
    .capabilities = seat_handle_capabilities,
    .name = seat_handle_name,
};

// Global registry listener to bind required interfaces
static void registry_handler(void* data, struct wl_registry* registry, uint32_t id,
                             const char* interface, uint32_t version);

static void registry_handle_global_remove(void* data, struct wl_registry* registry, uint32_t id);

static constexpr struct wl_registry_listener registry_listener = {
    .global = registry_handler,
    .global_remove = registry_handle_global_remove,
};
