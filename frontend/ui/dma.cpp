#include <vector>
#include <string>
#include <cctype>

#include "chuckstation2.hpp"

#include "res/IconsMaterialSymbols.h"

namespace chuckstation2 {

void show_ee_dmac(chuckstation2::instance* iris) {
    using namespace ImGui;

    if (imgui::BeginEx("EE DMAC", &iris->show_ee_dmac)) {

    } End();
}

void show_iop_dma(chuckstation2::instance* iris) {
    using namespace ImGui;

    if (imgui::BeginEx("IOP DMA", &iris->show_iop_dma)) {

    } End();
}

}