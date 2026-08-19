// Standard includes
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cstdio>

// ChuckStation2 includes
#include "chuckstation2.hpp"
#include "config.hpp"
#include "ee/ee_def.hpp"
#include "ee/vu_def.hpp"

// ImGui includes
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"

// SDL3 includes
#include <SDL3/SDL.h>

// stb_image stuff
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    SDL_SetAppMetadata("ChuckStation2", STR(_CS2_VERSION), "com.chuckstation.ChuckStation2");

    // Check if we got --help or --version in the commandline args
    // if so, don't do anything else.
    if (chuckstation2::settings::check_for_quick_exit(argc, (const char**)argv)) {
        return SDL_APP_SUCCESS;
    }

    chuckstation2::instance* iris = chuckstation2::create();

    if (!chuckstation2::init(iris, argc, (const char**)argv)) {
        fprintf(stderr, "ChuckStation2: Failed to initialize instance\n");

        return SDL_APP_FAILURE;
    }

    // Initialize appstate
    *appstate = iris;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    chuckstation2::instance* iris = (chuckstation2::instance*)appstate;

    return chuckstation2::update(iris);
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    chuckstation2::instance* iris = (chuckstation2::instance*)appstate;

    return chuckstation2::handle_events(iris, event);
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    chuckstation2::instance* iris = (chuckstation2::instance*)appstate;

    chuckstation2::destroy(iris);
}