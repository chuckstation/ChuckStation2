#include "chuckstation2.hpp"

#ifndef __APPLE__
#error "This file should only be compiled for macOS targets"
#endif

#import <Cocoa/Cocoa.h>

namespace chuckstation2::platform {

bool init(chuckstation2::instance* iris) {
    apply_settings(iris);
    return true;
}

bool apply_settings(chuckstation2::instance* iris) {
    // Get the NSWindow from SDL's window
    SDL_PropertiesID props = SDL_GetWindowProperties(iris->window);

#ifdef SDL_PROP_WINDOW_COCOA_WINDOW_POINTER
    NSWindow* ns_window = (NSWindow*)SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
#else
    NSWindow* ns_window = (NSWindow*)SDL_GetPointerProperty(
        props, "SDL.window.cocoa.window", NULL);
#endif

    if (!ns_window) {
        printf("ChuckStation2: Could not retrieve NSWindow from SDL\n");
        return false;
    }

    // Set the window title
    [ns_window setTitle:@(CS2_TITLE)];

    // Enable high-DPI support is handled by SDL3 automatically
    // Set the window subtitle for version info
    NSString* version_str = [NSString stringWithFormat:@"v%s", STR(_CS2_VERSION)];
    [ns_window setSubtitle:version_str];

    // On macOS, ensure the window collects garbage properly when minimized
    [ns_window setReleasedWhenClosed:NO];

    return true;
}

void destroy(chuckstation2::instance* iris) {
    // No macOS-specific cleanup needed beyond SDL
}

}  // namespace chuckstation2::platform
