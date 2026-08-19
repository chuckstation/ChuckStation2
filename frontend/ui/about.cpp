#include <vector>
#include <string>
#include <cctype>

#include "chuckstation2.hpp"
#include "config.hpp"

#include "res/IconsMaterialSymbols.h"

namespace chuckstation2 {

void show_about_window(chuckstation2::instance* iris) {
    using namespace ImGui;

    static ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable && !GetIO().ConfigViewportsNoDecoration)
        flags |= ImGuiWindowFlags_NoTitleBar;

    if (Begin("About", &iris->show_about_window, flags)) {
        if (BeginChild("##iconchild", ImVec2(100.0, 220.0), ImGuiChildFlags_AutoResizeY)) {
            Image((ImTextureID)(intptr_t)iris->iris_icon.descriptor_set, ImVec2(100.0, 100.0));
        } EndChild(); SameLine(0.0, 10.0);

        if (BeginChild("##textchild", ImVec2(350.0, 0.0))) {
            PushFont(iris->font_heading);
            Text(CS2_TITLE);
            PopFont();

            Separator();

            Text("Experimental PlayStation 2 emulator");
            Text("");
            Text("Available at "); SameLine(0.0, 0.0);
            TextLinkOpenURL("https://github.com/chuckstation/ChuckStation2", "https://github.com/chuckstation/ChuckStation2");
            Text("");
            TextWrapped(
                "This emulator is based on https://github.com/allkern/iris"
            );
            Text("");
            Text("Please file any issues to "); SameLine(0.0, 0.0);
            TextLinkOpenURL("our GitHub issues page", "https://github.com/chuckstation/ChuckStation2/issues"); SameLine(0.0, 0.0);
            Text(".");
        } EndChild();
    } End();
}

}