cmake_minimum_required(VERSION 3.21)
project(ChuckStation2 LANGUAGES C CXX)

# ============================================================
# Cross-platform configuration: MacOS, Windows, Linux, Android
# ============================================================

# --- Platform detection ---
if (ANDROID)
    message(STATUS "Building for Android (API ${ANDROID_API}, ABI ${ANDROID_ABI})")
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    message(STATUS "Building for macOS")
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    message(STATUS "Building for Windows")
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    message(STATUS "Building for Linux")
else()
    message(WARNING "Unknown platform: ${CMAKE_SYSTEM_NAME}")
endif()

# --- Platform-specific settings ---
if (APPLE AND NOT ANDROID)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    set(OSX_ICON ${CMAKE_CURRENT_SOURCE_DIR}/res/ChuckStation2.icns)
    set_source_files_properties(${OSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

if (WIN32 AND NOT ANDROID)
    set(WIN_ICON ${CMAKE_CURRENT_SOURCE_DIR}/res/ChuckStation2.rc)
endif()

# --- Static linking defaults ---
set(SDL_STATIC ON)
set(ASMJIT_STATIC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- Version from Git ---
find_package(Git QUIET)

if (GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty --match "0.*"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if (NOT GIT_VERSION_STRING)
        set(GIT_VERSION_STRING "unknown-version")
    endif()
else()
    set(GIT_VERSION_STRING "git-not-found")
endif()

message(STATUS "ChuckStation2 Version: ${GIT_VERSION_STRING}")

# --- LTO support ---
if (NOT ANDROID AND NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)
endif()

set(BUILD_SHARED_LIBS OFF)

# ============================================================
# Target definition
# ============================================================

if (ANDROID)
    # On Android we build a shared library loaded by JNI
    add_library(ChuckStation2 SHARED)
else()
    add_executable(ChuckStation2 MACOSX_BUNDLE ${WIN_ICON} ${OSX_ICON})
endif()

# --- Compile definitions ---
target_compile_definitions(ChuckStation2 PUBLIC
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    IMGUI_IMPL_VULKAN_USE_VOLK
)

# Suppress deprecation warnings
if (NOT MSVC)
    target_compile_options(ChuckStation2 PUBLIC "-Wno-deprecated-declarations")
endif()

# --- Platform-specific compile definitions ---
if (ANDROID)
    target_compile_definitions(ChuckStation2 PUBLIC __ANDROID__=1)
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    if (NOT ANDROID OR ANDROID_ABI STREQUAL "x86_64")
        target_compile_options(ChuckStation2 PRIVATE -D_EE_USE_INTRINSICS -mssse3 -msse4.1)
    endif()
endif()

# ARM NEON for Android ARM64 and Apple Silicon
if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    target_compile_options(ChuckStation2 PRIVATE -D_EE_USE_INTRINSICS)
endif()

# ============================================================
# Dependencies (git submodules)
# ============================================================

set(PARALLEL_GS_STANDALONE ON CACHE BOOL "" FORCE)
add_subdirectory(deps/asmjit EXCLUDE_FROM_ALL)
add_subdirectory(deps/tomlplusplus EXCLUDE_FROM_ALL)
add_subdirectory(deps/libdeflate EXCLUDE_FROM_ALL)
add_subdirectory(deps/parallel-gs EXCLUDE_FROM_ALL)
add_subdirectory(deps/libchdr EXCLUDE_FROM_ALL)
add_subdirectory(deps/SDL EXCLUDE_FROM_ALL)

# ============================================================
# Vulkan platform extensions
# ============================================================

if (X11_API AND NOT ANDROID)
    target_compile_definitions(granite-volk PUBLIC VK_USE_PLATFORM_XLIB_KHR)
endif()
if (WAYLAND_API AND NOT ANDROID)
    target_compile_definitions(granite-volk PUBLIC VK_USE_PLATFORM_WAYLAND_KHR)
endif()
if (WIN32 AND NOT ANDROID)
    target_compile_definitions(granite-volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
endif()
if (ANDROID)
    target_compile_definitions(granite-volk PUBLIC VK_USE_PLATFORM_ANDROID_KHR)
endif()
if (APPLE AND NOT ANDROID)
    target_compile_definitions(granite-volk PUBLIC VK_USE_PLATFORM_MACOS_MVK)
endif()

# ============================================================
# LTO enablement
# ============================================================

if (NOT ANDROID AND NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
    if (LTO_SUPPORTED)
        message(STATUS "IPO/LTO enabled")
        set_property(TARGET ChuckStation2 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "IPO/LTO not supported: ${LTO_ERROR}")
    endif()
endif()

set_property(TARGET ChuckStation2 PROPERTY CXX_STANDARD 20)
add_definitions("-D_CS2_VERSION=${GIT_VERSION_STRING}")

# ============================================================
# Source files
# ============================================================

target_sources(ChuckStation2 PRIVATE
    main.cpp
    frontend/audio.cpp
    frontend/handlers.cpp
    frontend/vulkan.cpp
    frontend/imgui.cpp
    frontend/emu.cpp
    frontend/render.cpp
    frontend/shaders.cpp
    frontend/input.cpp
    frontend/chuckstation2.cpp
    frontend/elf.cpp
    frontend/notifications.cpp
    frontend/settings.cpp
    frontend/ui/about.cpp
    frontend/ui/bios_setting.cpp
    frontend/ui/breakpoints.cpp
    frontend/ui/control.cpp
    frontend/ui/dma.cpp
    frontend/ui/gs.cpp
    frontend/ui/intc.cpp
    frontend/ui/logs.cpp
    frontend/ui/memory.cpp
    frontend/ui/memory_card_tool.cpp
    frontend/ui/memory_search.cpp
    frontend/ui/menubar.cpp
    frontend/ui/modules.cpp
    frontend/ui/overlay.cpp
    frontend/ui/pad.cpp
    frontend/ui/settings.cpp
    frontend/ui/spu2.cpp
    frontend/ui/state.cpp
    frontend/ui/statusbar.cpp
    frontend/ui/symbols.cpp
    frontend/ui/threads.cpp
    frontend/ui/vu_disassembly.cpp
    # Core PS2 emulation
    src/ps2.c
    src/ps2_elf.c
    src/ps2_iso9660.c
    src/queue.c
    src/rom.c
    src/md5.c
    src/list.c
    src/scheduler.c
    # Devices
    src/dev/ds.c
    src/dev/guncon.c
    src/dev/mcd.c
    src/dev/mtap.c
    src/dev/ps1_mcd.c
    # Emotion Engine
    src/ee/ee_cached.cpp
    src/ee/bus.c
    src/ee/dmac.c
    src/ee/ee_dis.c
    src/ee/gif.c
    src/ee/intc.c
    src/ee/timers.c
    src/ee/vif.c
    src/ee/vu_cached.cpp
    src/ee/vu_dis.c
    # Graphics Synthesizer
    src/gs/gs.c
    src/gs/renderer/null.cpp
    src/gs/renderer/renderer.cpp
    src/gs/renderer/hardware.cpp
    # IOP
    src/iop/bus.c
    src/iop/cdvd.c
    src/iop/disc.c
    src/iop/dma.c
    src/iop/fw.c
    src/iop/intc.c
    src/iop/iop.c
    src/iop/iop_dis.c
    src/iop/iop_export.c
    src/iop/rpc.c
    src/iop/sio2.c
    src/iop/spu2.c
    src/iop/timers.c
    src/iop/usb.c
    src/iop/disc/bin.c
    src/iop/disc/cue.c
    src/iop/disc/chd.c
    src/iop/disc/ciso.c
    src/iop/disc/iso.c
    src/iop/hle/ioman.cpp
    src/iop/hle/loadcore.c
    src/iop/hle/sysmem.c
    # IPU
    src/ipu/chromtable.cpp
    src/ipu/codedblockpattern.cpp
    src/ipu/dct_coeff.cpp
    src/ipu/dct_coeff_table0.cpp
    src/ipu/dct_coeff_table1.cpp
    src/ipu/ipu.cpp
    src/ipu/ipu_fifo.cpp
    src/ipu/lumtable.cpp
    src/ipu/mac_addr_inc.cpp
    src/ipu/mac_b_pic.cpp
    src/ipu/mac_i_pic.cpp
    src/ipu/mac_p_pic.cpp
    src/ipu/motioncode.cpp
    src/ipu/vlc_table.cpp
    # Shared subsystems
    src/shared/bios.c
    src/shared/dev9.c
    src/shared/ram.c
    src/shared/sbus.c
    src/shared/sif.c
    src/shared/speed.c
    src/shared/speed/ata.c
    src/shared/speed/eeprom.c
    src/shared/speed/flash.c
    # S14x (PSX)
    src/s14x/nand.c
    src/s14x/syscon.c
    src/s14x/sram.c
    src/s14x/link.c
    src/s14x/ioboard.c
    src/s14x/aiboard.c
    # UI libraries
    deps/imgui/imgui.cpp
    deps/imgui/imgui_demo.cpp
    deps/imgui/imgui_draw.cpp
    deps/imgui/imgui_tables.cpp
    deps/imgui/imgui_widgets.cpp
    deps/imgui/backends/imgui_impl_sdl3.cpp
    deps/imgui/backends/imgui_impl_vulkan.cpp
    deps/implot/implot_demo.cpp
    deps/implot/implot_items.cpp
    deps/implot/implot.cpp
    deps/lz4/lib/lz4.c
)

# ============================================================
# Include directories
# ============================================================

target_include_directories(ChuckStation2 PRIVATE
    deps/asmjit
    deps/imgui
    deps/imgui/backends
    deps/implot
    deps/SDL/include
    deps/incbin
    deps/parallel-gs
    deps/libchdr/include
    deps/lz4/lib
    deps/stb
    deps/portable-file-dialogs
    frontend
    src
    res
)

# ============================================================
# Link libraries
# ============================================================

target_link_libraries(ChuckStation2 PUBLIC
    asmjit::asmjit
    libdeflate::libdeflate_static
    tomlplusplus::tomlplusplus
    SDL3::SDL3-static
    parallel-gs
    chdr-static
)

# ============================================================
# Platform-specific sources and linking
# ============================================================

if (ANDROID)
    # Android: JNI bridge, log library, EGL, Vulkan
    target_sources(ChuckStation2 PRIVATE
        frontend/platform/android.cpp
    )
    target_link_libraries(ChuckStation2 PUBLIC
        android
        log
        EGL
        vulkan
    )
    # Link SDL3 dynamically on Android (static SDL with Android is problematic)
    target_link_libraries(ChuckStation2 PUBLIC SDL3::SDL3)

elseif (APPLE AND NOT ANDROID)
    # macOS: Cocoa framework, Vulkan via MoltenVK
    target_sources(ChuckStation2 PRIVATE
        frontend/platform/macos.cpp
    )
    find_package(Vulkan COMPONENTS MoltenVK)
    find_library(MOLTENVK_LIBRARY
        NAMES libMoltenVK.dylib
        PATHS ENV VULKAN_SDK
        PATH_SUFFIXES /lib
    )
    target_link_libraries(ChuckStation2 PUBLIC
        Vulkan::Vulkan
        ${MOLTENVK_LIBRARY}
        "-framework Cocoa"
        "-framework QuartzCore"
    )

elseif (WIN32 AND NOT ANDROID)
    # Windows: DWM for window styling, Vulkan
    target_sources(ChuckStation2 PRIVATE
        frontend/platform/windows.cpp
    )
    target_link_libraries(ChuckStation2 PRIVATE
        dwmapi
    )

elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    # Linux: X11/Wayland, pthread, dl
    target_sources(ChuckStation2 PRIVATE
        frontend/platform/linux.cpp
    )
    target_link_libraries(ChuckStation2 PUBLIC
        pthread
        dl
    )

else()
    # Fallback stub for unknown platforms
    target_sources(ChuckStation2 PRIVATE
        frontend/platform/stub.cpp
    )
endif()

# ============================================================
# Target properties
# ============================================================

set_target_properties(ChuckStation2 PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    MACOSX_BUNDLE_ICON_FILE ChuckStation2.icns
    XCODE_GENERATE_SCHEME TRUE
    XCODE_ATTRIBUTE_BUNDLE_IDENTIFIER "com.chuckstation.ChuckStation2"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.chuckstation.ChuckStation2"
    OUTPUT_NAME ChuckStation2
)

if (ANDROID)
    set_target_properties(ChuckStation2 PROPERTIES
        MACOSX_BUNDLE FALSE
    )
endif()

# Set default startup project in Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "ChuckStation2")

# ============================================================
# macOS installation (app bundle + Vulkan dylibs)
# ============================================================

if (CMAKE_SYSTEM_NAME MATCHES "Darwin" AND NOT ANDROID)
    install(TARGETS ChuckStation2
        BUNDLE DESTINATION ./install COMPONENT Runtime
        RUNTIME DESTINATION ./install/bin COMPONENT Runtime
    )

    set(BUNDLE_PATH "${CMAKE_BINARY_DIR}/ChuckStation2.app")
    file(GLOB VULKAN_DYLIBS $ENV{VULKAN_SDK}/lib/libvulkan.*.dylib)
    set(VULKAN_DYLIBS ${VULKAN_DYLIBS} $ENV{VULKAN_SDK}/lib/libMoltenVK.dylib)

    install(CODE "
        execute_process(COMMAND echo \"Preparing to bundle ChuckStation2...\")
        execute_process(COMMAND echo \"Bundle path: ${BUNDLE_PATH}\")
        execute_process(COMMAND echo \"Vulkan dylibs: ${VULKAN_DYLIBS}\")
        execute_process(COMMAND echo \"Adding local RPATH...\")
        execute_process(
            COMMAND install_name_tool -add_rpath
            \"@executable_path/../Frameworks\"
            \"${BUNDLE_PATH}/Contents/MacOS/ChuckStation2\"
        )
        execute_process(COMMAND echo \"Creating ICD JSON directory...\")
        make_directory(${BUNDLE_PATH}/Contents/Resources/vulkan/icd.d)
        execute_process(COMMAND echo \"Creating Frameworks directory...\")
        make_directory(${BUNDLE_PATH}/Contents/Frameworks)
        execute_process(COMMAND echo \"Copying files...\")
        file(COPY
            ${CMAKE_CURRENT_SOURCE_DIR}/MoltenVK_icd.json
            DESTINATION ${BUNDLE_PATH}/Contents/Resources/vulkan/icd.d/
        )
        file(COPY
            ${VULKAN_DYLIBS}
            DESTINATION ${BUNDLE_PATH}/Contents/Frameworks/
        )
        execute_process(COMMAND echo \"Signing app bundle...\")
        execute_process(COMMAND
            codesign --force --deep --sign - ${BUNDLE_PATH}
            RESULT_VARIABLE codesign_result
            OUTPUT_VARIABLE codesign_output
        )
        if (codesign_result)
            execute_process(COMMAND echo \"${codesign_output}\")
        endif()
    ")
    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif()

# ============================================================
# Linux installation (AppImage)
# ============================================================

if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND NOT ANDROID)
    install(TARGETS ChuckStation2
        RUNTIME DESTINATION ./install/bin COMPONENT Runtime
    )
    install(CODE
        "include(${CMAKE_CURRENT_SOURCE_DIR}/AppImage.cmake)
        make_appimage(
            EXE \"${CMAKE_CURRENT_SOURCE_DIR}/build/ChuckStation2\"
            NAME \"ChuckStation2\"
            ICON \"${CMAKE_CURRENT_SOURCE_DIR}/res/ChuckStation2.png\"
            DIR_ICON \"${CMAKE_CURRENT_SOURCE_DIR}/res/ChuckStation2.png\"
            OUTPUT_NAME \"${CMAKE_CURRENT_SOURCE_DIR}/build/ChuckStation2-${GIT_VERSION_STRING}.AppImage\"
        )
        "
        COMPONENT Runtime
    )
endif()

# ============================================================
# Windows installation
# ============================================================

if (WIN32 AND NOT ANDROID)
    install(TARGETS ChuckStation2
        RUNTIME DESTINATION ./install COMPONENT Runtime
    )
endif()
