// Standard includes
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <cmath>

// ChuckStation2 includes
#include "chuckstation2.hpp"
#include "config.hpp"
#include "ee/ee_def.hpp"
#include "ee/vu_def.hpp"

// SDL3 includes
#include <SDL3/SDL.h>

// External includes
#include "res/IconsMaterialSymbols.h"

namespace chuckstation2 {

void add_recent(chuckstation2::instance* iris, std::string file, int type) {
    auto it = std::find_if(iris->recents.begin(), iris->recents.end(), [file, type](const recent& a) {
        return a.type == type && a.path == file;
    });

    if (it != iris->recents.end()) {
        iris->recents.erase(it);
        iris->recents.push_front({file, type});

        return;
    }

    iris->recents.push_front({file, type});

    if (iris->recents.size() == 11)
        iris->recents.pop_back();
}

int open_file(chuckstation2::instance* iris, std::string file) {
    std::filesystem::path path(file);
    std::string ext = path.extension().string();

    for (char& c : ext)
        c = tolower(c);

    // Load disc image
    if (ext == ".iso" || ext == ".bin" || ext == ".cue" ||
        ext == ".chd" || ext == ".cso" || ext == ".zso") {
        if (ps2_cdvd_open(iris->ps2->cdvd, file.c_str(), 0))
            return 1;

        char* boot_file = disc_get_boot_path(iris->ps2->cdvd->disc);

        if (!boot_file)
            return 2;

        elf::load_symbols_from_disc(ChuckStation2);

        renderer_reset(iris->renderer);

        ps2_set_system(iris->ps2, iris->system);
        ps2_load_bios(iris->ps2, iris->bios_path.c_str());
        ps2_boot_file(iris->ps2, boot_file);

        iris->loaded = file;

        if (iris->autostart) {
            iris->pause = false;
        }

        return 0;
    }

    elf::load_symbols_from_file(iris, file);

    // Note: We need the trailing whitespaces here because of IOMAN HLE
    // Load executable
    file = "host:  " + file;

    renderer_reset(iris->renderer);

    ps2_set_system(iris->ps2, iris->system);
    ps2_load_bios(iris->ps2, iris->bios_path.c_str());
    ps2_boot_file(iris->ps2, file.c_str());

    iris->loaded = file;

    if (iris->autostart) {
        iris->pause = false;
    }

    return 0;
}

void update_title(chuckstation2::instance* iris) {
    char buf[512];

    std::string base = "";

    if (iris->loaded.size()) {
        base = std::filesystem::path(iris->loaded).filename().string();
    }

    sprintf(buf, base.size() ? CS2_TITLE " | %s" : CS2_TITLE,
        base.c_str()
    );

    SDL_SetWindowTitle(iris->window, buf);
}

void update_time(chuckstation2::instance* iris) {
    int t = SDL_GetTicks() - iris->ticks;

    if (t < 500)
        return;

    if (iris->fps == 0.0f) {
        iris->fps = (float)iris->frames;
    } else {
        iris->fps += (float)iris->frames;
        iris->fps /= 2.0f;
    }

    iris->ticks = SDL_GetTicks();
    iris->frames = 0;
}

void sleep_limiter(chuckstation2::instance* iris) {
    uint32_t ticks = (1.0f / iris->fps_cap) * 1000.0f;

    std::this_thread::sleep_for(std::chrono::milliseconds(ticks / 2));

    // uint32_t now = SDL_GetTicks();

    // while ((SDL_GetTicks() - now) < ticks) {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(ticks / 4));
    // }
}

static inline void do_cycle(chuckstation2::instance* iris) {
    ps2_cycle(iris->ps2);

    if (iris->step_out) {
        // jr $ra
        if (iris->ps2->ee->opcode == 0x03e00008) {
            iris->step_out = false;
            iris->pause = true;

            // Consume the delay slot
            ps2_cycle(iris->ps2);
        }
    }

    if (iris->step_over) {
        if (iris->ps2->ee->pc == iris->step_over_addr) {
            iris->step_over = false;
            iris->pause = true;
        }
    }

    for (const chuckstation2::breakpoint& b : iris->breakpoints) {
        if (b.cpu == chuckstation2::BKPT_CPU_EE) {
            if (iris->ps2->ee->pc == b.addr) {
                iris->pause = true;
            }
        } else {
            if (iris->ps2->iop->pc == b.addr) {
                iris->pause = true;
            }
        }
    }
}

void update_window(chuckstation2::instance* iris) {
    using namespace ImGui;

    // Limit FPS to 60 only when paused
    if (iris->limit_fps && iris->pause)
        sleep_limiter(ChuckStation2);

    update_title(ChuckStation2);
    update_time(ChuckStation2);

    ImGuiIO& io = ImGui::GetIO();

    // Start the Dear ImGui frame
    if (SDL_GetWindowFlags(iris->window) & SDL_WINDOW_MINIMIZED) {
        SDL_Delay(1);

        return;
    }

    // Resize swapchain?
    int width, height;

    SDL_GetWindowSize(iris->window, &width, &height);

    if (width > 0 && height > 0 && (iris->swapchain_rebuild || iris->main_window_data.Width != width || iris->main_window_data.Height != height)) {
        ImGui_ImplVulkan_SetMinImageCount(iris->min_image_count);
    
        ImGui_ImplVulkanH_CreateOrResizeWindow(
            iris->instance,
            iris->physical_device,
            iris->device,
            &iris->main_window_data,
            iris->queue_family,
            nullptr,
            width, height,
            iris->min_image_count,
            0
        );

        iris->main_window_data.FrameIndex = 0;
        iris->swapchain_rebuild = false;
    }

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (!iris->fullscreen) {
        show_main_menubar(ChuckStation2);
    }

    DockSpaceOverViewport(0, GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // Drop file fade animation
    if (iris->drop_file_active) {
        iris->drop_file_alpha += iris->drop_file_alpha_delta;

        if (iris->drop_file_alpha_delta > 0.0f) {
            if (iris->drop_file_alpha >= 1.0f) {
                iris->drop_file_alpha = 1.0f;
                iris->drop_file_alpha_delta = 0.0f;
            }
        } else {
            if (iris->drop_file_alpha <= 0.0f) {
                iris->drop_file_alpha = 0.0f;
                iris->drop_file_alpha_delta = 0.0f;
                iris->drop_file_active = false;
            }
        }

        GetForegroundDrawList()->AddRectFilled(
            ImVec2(0, 0),
            ImVec2(width, height),
            ImColor(0.0f, 0.0f, 0.0f, iris->drop_file_alpha * 0.35f)
        );

        ImVec2 text_size = CalcTextSize("Drop file here to launch");

        PushFont(iris->font_icons_big);

        ImVec2 icon_size = CalcTextSize(ICON_MS_DOWNLOAD);

        ImVec2 total_size = ImVec2(
            std::max(icon_size.x, text_size.x),
            icon_size.y + text_size.y
        );

        GetForegroundDrawList()->AddText(
            ImVec2(width / 2 - icon_size.x / 2, height / 2 - icon_size.y),
            ImColor(1.0f, 1.0f, 1.0f, iris->drop_file_alpha),
            ICON_MS_DOWNLOAD
        );

        PopFont();

        GetForegroundDrawList()->AddText(
            ImVec2(width / 2 - text_size.x / 2, height / 2),
            ImColor(1.0f, 1.0f, 1.0f, iris->drop_file_alpha),
            "Drop file here to launch"
        );
    }

    if (iris->show_ee_control) show_ee_control(ChuckStation2);
    if (iris->show_ee_state) show_ee_state(ChuckStation2);
    if (iris->show_ee_logs) show_ee_logs(ChuckStation2);
    if (iris->show_ee_interrupts) show_ee_interrupts(ChuckStation2);
    if (iris->show_ee_dmac) show_ee_dmac(ChuckStation2);
    if (iris->show_iop_control) show_iop_control(ChuckStation2);
    if (iris->show_iop_state) show_iop_state(ChuckStation2);
    if (iris->show_iop_logs) show_iop_logs(ChuckStation2);
    if (iris->show_iop_interrupts) show_iop_interrupts(ChuckStation2);
    if (iris->show_iop_modules) show_iop_modules(ChuckStation2);
    if (iris->show_iop_dma) show_iop_dma(ChuckStation2);
    if (iris->show_gs_debugger) show_gs_debugger(ChuckStation2);
    if (iris->show_spu2_debugger) show_spu2_debugger(ChuckStation2);
    if (iris->show_memory_viewer) show_memory_viewer(ChuckStation2);
    if (iris->show_vu_disassembler) show_vu_disassembler(ChuckStation2);
    if (iris->show_status_bar && !iris->fullscreen) show_status_bar(ChuckStation2);
    if (iris->show_breakpoints) show_breakpoints(ChuckStation2);
    if (iris->show_about_window) show_about_window(ChuckStation2);
    if (iris->show_settings) show_settings(ChuckStation2);
    if (iris->show_pad_debugger) show_pad_debugger(ChuckStation2);
    if (iris->show_symbols) show_symbols(ChuckStation2);
    if (iris->show_threads) show_threads(ChuckStation2);
    if (iris->show_sysmem_logs) show_sysmem_logs(ChuckStation2);
    if (iris->show_memory_card_tool) show_memory_card_tool(ChuckStation2);
    if (iris->show_memory_search) show_memory_search(ChuckStation2);
    // if (iris->show_gamelist) show_gamelist(ChuckStation2);
    if (iris->show_imgui_demo) ShowDemoWindow(&iris->show_imgui_demo);
    if (iris->show_bios_setting_window) show_bios_setting_window(ChuckStation2);
    if (iris->show_overlay) show_overlay(ChuckStation2);

    // Display little pause icon in the top right corner
    if (iris->pause) {
        ImVec2 ts = CalcTextSize(ICON_MS_PAUSE);
        ImVec2 offset = ImVec2(10.0f, 10.0f);
        ImVec2 padding = ImVec2(0.0f, 0.0f);

        ts.x -= 1.0f;

        int menubar_offset = 0;

        if (!iris->fullscreen) {
            menubar_offset += iris->menubar_height;
        }

        // GetBackgroundDrawList()->AddRectFilled(
        //     ImVec2(width - ts.x - offset.x - padding.x, menubar_offset + offset.y - padding.y),
        //     ImVec2(width - offset.x + padding.x, menubar_offset + ts.y + offset.y + padding.y),
        //     GetColorU32(GetStyleColorVec4(ImGuiCol_WindowBg)), 8.0f
        // );

        GetBackgroundDrawList(GetMainViewport())->AddText(
            ImVec2(width - ts.x - offset.x, menubar_offset + offset.y),
            GetColorU32(GetStyleColorVec4(ImGuiCol_Text)),
            ICON_MS_PAUSE
        );
    }

    handle_animations(ChuckStation2);

    // Rendering
    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();

    const bool main_is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

    iris->main_window_data.ClearValue.color.float32[0] = 0.0f;
    iris->main_window_data.ClearValue.color.float32[1] = 0.0f;
    iris->main_window_data.ClearValue.color.float32[2] = 0.0f;
    iris->main_window_data.ClearValue.color.float32[3] = 1.0f;

    if (!main_is_minimized) {
        if (!imgui::render_frame(iris, draw_data)) {
            printf("ChuckStation2: Failed to render ImGui frame\n");
        }
    }

    iris->frames++;
}

chuckstation2::instance* create() {
    return new chuckstation2::instance();
}

bool init(chuckstation2::instance* iris, int argc, const char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "ChuckStation2: Failed to init SDL \'%s\'\n", SDL_GetError());

        return false;
    }

    // Create and check window
    iris->main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

    // Init preferences path
    if (std::filesystem::exists("portable") || std::filesystem::exists("portable.txt")) {
        iris->pref_path = "./";
    } else {
        char* pref = SDL_GetPrefPath("Allkern", "ChuckStation2");

        iris->pref_path = std::string(pref);

        SDL_free(pref);
    }

    if (!chuckstation2::emu::init(ChuckStation2)) {
        fprintf(stderr, "ChuckStation2: Failed to initialize emulator state\n");

        return false;
    }

    if (!chuckstation2::settings::init(iris, argc, argv)) {
        fprintf(stderr, "ChuckStation2: Failed to initialize settings\n");

        return false;
    }

    iris->window = SDL_CreateWindow(
        CS2_TITLE,
        iris->window_width, iris->window_height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN
    );

    if (!iris->window) {
        printf("ChuckStation2: Failed to create SDL window \'%s\'\n", SDL_GetError());

        return false;
    }

    if (!chuckstation2::vulkan::init(iris, iris->vulkan_enable_validation_layers)) {
        fprintf(stderr, "ChuckStation2: Failed to initialize Vulkan\n");

        return false;
    }

    if (!chuckstation2::imgui::init(ChuckStation2)) {
        fprintf(stderr, "ChuckStation2: Failed to initialize ImGui\n");

        return false;
    }

    if (!chuckstation2::platform::init(ChuckStation2)) {
        fprintf(stderr, "ChuckStation2: Failed to initialize platform\n");

        return false;
    }

    if (!chuckstation2::audio::init(ChuckStation2)) {
        fprintf(stderr, "ChuckStation2: Failed to initialize audio\n");

        return false;
    }

    if (!chuckstation2::render::init(ChuckStation2)) {
        fprintf(stderr, "ChuckStation2: Failed to initialize render state\n");

        return false;
    }

    if (!chuckstation2::input::init(ChuckStation2)) {
        fprintf(stderr, "ChuckStation2: Failed to initialize input\n");

        return false;
    }

    for (const std::string& s : iris->shader_passes_pending)
        shaders::push(iris, s);

    iris->shader_passes_pending.clear();

    // Sadly we need to start a frame here to measure menubar height
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    iris->menubar_height = ImGui::GetFrameHeight();

    ImGui::EndFrame();

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    SDL_SetWindowSize(iris->window, iris->window_width, iris->window_height + get_menubar_height(ChuckStation2));
    SDL_ShowWindow(iris->window);

    return true;
}

SDL_AppResult update(chuckstation2::instance* iris) {
    if (iris->double_click_counter) {
        iris->double_click_counter--;
    }

    if (iris->pause) {
        iris->step_out = false;
        iris->step_over = false;

        if (iris->step) {
            ps2_step_ee(iris->ps2);

            iris->step = false;
        }

        chuckstation2::update_window(ChuckStation2);

        return SDL_APP_CONTINUE;
    }

    // Execute until VBlank
    while (!ps2_gs_is_vblank(iris->ps2->gs)) {
        do_cycle(ChuckStation2);

        if (iris->pause) {
            chuckstation2::update_window(ChuckStation2);

            return SDL_APP_CONTINUE;
        }
    }

    // Draw frame
    chuckstation2::update_window(ChuckStation2);
    
    // Execute until vblank is over
    while (ps2_gs_is_vblank(iris->ps2->gs)) {
        do_cycle(ChuckStation2);

        if (iris->pause) {
            chuckstation2::update_window(ChuckStation2);

            return SDL_APP_CONTINUE;
        }
    }

    // float p = ((float)iris->ps2->ee->eenull_counter / (float)(4920115)) * 100.0f;

    // printf("ee: Time spent idling: %ld cycles (%.2f%%) INTC reads: %d CSR reads: %d (%.1f fps)\n", iris->ps2->ee->eenull_counter, p, iris->ps2->ee->intc_reads, iris->ps2->ee->csr_reads, 1.0f / ImGui::GetIO().DeltaTime);

    iris->ps2->ee->eenull_counter = 0;
    iris->ps2->ee->intc_reads = 0;
    iris->ps2->ee->csr_reads = 0;

    return SDL_APP_CONTINUE;
}

SDL_AppResult handle_events(chuckstation2::instance* iris, SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);

    switch (event->type) {
        case SDL_EVENT_QUIT: {
            return SDL_APP_SUCCESS;
        } break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (ImGui::GetIO().WantCaptureMouse) {
                break;
            }

            if (event->button.button == SDL_BUTTON_LEFT && event->button.windowID == SDL_GetWindowID(iris->window)) {
                if (iris->double_click_counter) {
                    if ((SDL_GetTicks() - iris->double_click_counter) > iris->double_click_interval) {
                        iris->double_click_counter = SDL_GetTicks();
                    } else {
                        iris->fullscreen = !iris->fullscreen;

                        SDL_SetWindowFullscreen(iris->window, iris->fullscreen);
                    }
                } else {
                    iris->double_click_counter = SDL_GetTicks();
                }
            }
        } break;

        case SDL_EVENT_GAMEPAD_ADDED: {
            SDL_Gamepad* gamepad = SDL_OpenGamepad(event->gdevice.which);

            if (!gamepad) {
                SDL_Log("Failed to open gamepad ID %u: %s", (unsigned int) event->gdevice.which, SDL_GetError());
            }

            if (iris->ds[0] && ((iris->input_devices[0] == nullptr) || (iris->input_devices[0]->get_type() == 0))) {
                if (iris->input_devices[0]) delete iris->input_devices[0];

                iris->input_devices[0] = new chuckstation2::gamepad_device(event->gdevice.which);
                iris->input_devices[0]->set_slot(0);

                if (iris->input_map[0] <= 1) {
                    iris->input_map[0] = 1;
                }

                push_info(iris, "\'" + std::string(SDL_GetGamepadName(gamepad)) + "\' connected to slot 1");
            } else if (iris->ds[1] && ((iris->input_devices[1] == nullptr) || (iris->input_devices[1]->get_type() == 0))) {
                if (iris->input_devices[1]) delete iris->input_devices[1];

                iris->input_devices[1] = new chuckstation2::gamepad_device(event->gdevice.which);
                iris->input_devices[1]->set_slot(1);

                if (iris->input_map[1] <= 1) {
                    iris->input_map[1] = 1;
                }

                push_info(iris, "\'" + std::string(SDL_GetGamepadName(gamepad)) + "\' connected to slot 2");
            } else {
                push_info(iris, "\'" + std::string(SDL_GetGamepadName(gamepad)) + "\' connected");
            }

            iris->gamepads[event->gdevice.which] = gamepad;
        } break;

        case SDL_EVENT_GAMEPAD_REMOVED: {
            SDL_Gamepad* gamepad = iris->gamepads[event->gdevice.which];

            for (int i = 0; i < 2; i++) {
                if (iris->input_devices[i] && iris->input_devices[i]->get_type() == 1) {
                    chuckstation2::gamepad_device* gp = static_cast<chuckstation2::gamepad_device*>(iris->input_devices[i]);

                    if (gp->get_id() == event->gdevice.which) {
                        delete iris->input_devices[i];
                        iris->input_devices[i] = new chuckstation2::keyboard_device();

                        if (iris->input_map[i] <= 1) {
                            iris->input_map[i] = 0;
                        }

                        push_info(iris, "\'" + std::string(SDL_GetGamepadName(gamepad)) + "\' in slot " + std::to_string(i + 1) + " disconnected");
                    }
                }
            }

            if (gamepad) {
                SDL_CloseGamepad(gamepad);

                iris->gamepads.erase(event->gdevice.which);
            }
        } break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            if (event->window.windowID == SDL_GetWindowID(iris->window)) {
                return SDL_APP_SUCCESS;
            }
        } break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        case SDL_EVENT_KEY_UP: {
            iris->last_input_event_read = false;
            iris->last_input_event = input::sdl_event_to_input_event(event);

            if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
                iris->last_input_event_value = fabs(event->gaxis.value / 32767.0f);
            } else {
                iris->last_input_event_value = 1.0f;
            }

            if (iris->input_devices[0]) iris->input_devices[0]->handle_event(iris, event);
            if (iris->input_devices[1]) iris->input_devices[1]->handle_event(iris, event);
        } break;

        case SDL_EVENT_KEY_DOWN: {
            input::handle_keydown_event(iris, event);
        } break;

        case SDL_EVENT_DROP_BEGIN: {
            iris->drop_file_active = true;
            iris->drop_file_alpha = 0.0f;
            iris->drop_file_alpha_delta = 1.0f / 10.0f;
            iris->drop_file_alpha_target = 1.0f;
        } break;
        
        case SDL_EVENT_DROP_COMPLETE: {
            iris->drop_file_active = true;
            iris->drop_file_alpha = iris->drop_file_alpha_target;
            iris->drop_file_alpha_delta = -(1.0f / 10.0f);
            iris->drop_file_alpha_target = 0.0f;
        } break;

        case SDL_EVENT_DROP_FILE: {
            if (!event->drop.data)
                break;

            std::string path(event->drop.data);

            std::filesystem::path p(path);

            if (std::filesystem::is_regular_file(p)) {
                if (open_file(iris, path)) {
                    push_info(iris, "Failed to open file: " + path);
                } else {
                    add_recent(iris, path, RECENT_TYPE_PS2);
                }
            } else {
                if (emu::load_arcade(iris, path)) {
                    add_recent(iris, path, RECENT_TYPE_ARCADE);
                } else {
                    push_info(iris, "Failed to boot arcade: " + path);
                }
            }

            // Maybe not needed anymore?
            // SDL_free(event->drop.data);
        } break;
    }

    return SDL_APP_CONTINUE;
}

int get_menubar_height(chuckstation2::instance* iris) {
    if (iris->show_status_bar) {
        return iris->menubar_height * 2;
    }

    return iris->menubar_height;
}

void destroy(chuckstation2::instance* iris) {
    for (int i = 0; i < 2; i++) {
        if (iris->input_devices[i]) {
            delete iris->input_devices[i];
            iris->input_devices[i] = nullptr;
        }
    }

    if (iris->imgui_enable_viewports) {
        iris->show_ee_control = false;
        iris->show_ee_state = false;
        iris->show_ee_logs = false;
        iris->show_ee_interrupts = false;
        iris->show_ee_dmac = false;
        iris->show_iop_control = false;
        iris->show_iop_state = false;
        iris->show_iop_logs = false;
        iris->show_iop_interrupts = false;
        iris->show_iop_modules = false;
        iris->show_iop_dma = false;
        iris->show_gs_debugger = false;
        iris->show_spu2_debugger = false;
        iris->show_memory_viewer = false;
        iris->show_memory_search = false;
        iris->show_vu_disassembler = false;
        iris->show_breakpoints = false;
        iris->show_threads = false;
        iris->show_sysmem_logs = false;
        iris->show_imgui_demo = false;
        iris->show_overlay = false;
    }

    if (iris->window) SDL_HideWindow(iris->window);

    chuckstation2::imgui::cleanup(ChuckStation2);
    chuckstation2::audio::close(ChuckStation2);
    chuckstation2::settings::close(ChuckStation2);
    chuckstation2::render::destroy(ChuckStation2);
    chuckstation2::vulkan::cleanup(ChuckStation2);
    chuckstation2::platform::destroy(ChuckStation2);
    chuckstation2::emu::destroy(ChuckStation2);

    if (iris->window) SDL_DestroyWindow(iris->window);

    SDL_Quit();

    delete ChuckStation2;
}

}