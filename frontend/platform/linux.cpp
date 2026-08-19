#include "chuckstation2.hpp"

#ifndef __linux__
#error "This file should only be compiled for Linux targets"
#endif

#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>
#include <cstring>

namespace chuckstation2::platform {

// Get the X11 Display and Window from SDL (for future X11-specific features)
static bool get_x11_handles(chuckstation2::instance* iris, void** display, unsigned long* window) {
    SDL_PropertiesID props = SDL_GetWindowProperties(iris->window);

#ifdef SDL_PROP_WINDOW_X11_DISPLAY_POINTER
    *display = (void*)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    *window = (unsigned long)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    return (*display != nullptr && *window != 0);
#else
    *display = nullptr;
    *window = 0;
    return false;
#endif
}

// Check if running under Wayland
static bool is_wayland(chuckstation2::instance* iris) {
    SDL_PropertiesID props = SDL_GetWindowProperties(iris->window);

#ifdef SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL) != nullptr;
#else
    (void)props;
    return false;
#endif
}

// Set process nice level for better performance
static void set_process_priority() {
    // Try to set a slightly higher priority for the emulator process
    if (setpriority(PRIO_PROCESS, 0, -5) != 0) {
        // Non-fatal: just means we don't have permission
    }
}

bool init(chuckstation2::instance* iris) {
    // Set process priority
    set_process_priority();

    apply_settings(iris);
    return true;
}

bool apply_settings(chuckstation2::instance* iris) {
    void* x11_display = nullptr;
    unsigned long x11_window = 0;

    if (get_x11_handles(iris, &x11_display, &x11_window)) {
        // Running under X11
        // Future: X11-specific settings can be applied here
        // (e.g., _NET_WM_BYPASS_COMPOSITOR for latency reduction)
    } else if (is_wayland(iris)) {
        // Running under Wayland
        // SDL3 handles most Wayland-specific setup automatically
    }

    return true;
}

void destroy(chuckstation2::instance* iris) {
    // No Linux-specific cleanup needed beyond SDL
}

}  // namespace chuckstation2::platform
