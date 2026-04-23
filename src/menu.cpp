#include "menu.h"
#include "executor.h"
#include "explorer.h"
#include "properties.h"
#include "vendor/imgui/imgui.h"
#include "vendor/TextEditor.h"
#include <string>

namespace menu {

static TextEditor s_editor;
static bool s_editorInit = false;

static void DrawESPTab(Config& cfg) {
    ImGui::Checkbox("Box", &cfg.box);
    if (cfg.box) {
        ImGui::SameLine();
        ImGui::ColorEdit3("##box1", cfg.colBox, ImGuiColorEditFlags_NoInputs);
        ImGui::Combo("Style", &cfg.boxStyle, BOX_STYLES, BOX_STYLE_COUNT);
        ImGui::Checkbox("Box Gradient", &cfg.boxGradient);
        if (cfg.boxGradient) {
            ImGui::SameLine();
            ImGui::ColorEdit3("##box2", cfg.colBox2, ImGuiColorEditFlags_NoInputs);
        }
        ImGui::Checkbox("Filled", &cfg.boxFilled);
        if (cfg.boxFilled) ImGui::SliderFloat("Opacity", &cfg.fillAlpha, 0.05f, 0.8f);
    }
    ImGui::Separator();
    ImGui::Checkbox("Health Bar", &cfg.health);
    if (cfg.health) {
        ImGui::Checkbox("HP Gradient", &cfg.hpGradient);
        if (cfg.hpGradient) {
            ImGui::ColorEdit3("Full HP",  cfg.colHP1, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine();
            ImGui::ColorEdit3("Empty HP", cfg.colHP2, ImGuiColorEditFlags_NoInputs);
        }
    }
    ImGui::Separator();
    ImGui::Checkbox("Name", &cfg.name);
    if (cfg.name) { ImGui::SameLine(); ImGui::ColorEdit3("##name", cfg.colName, ImGuiColorEditFlags_NoInputs); }
    ImGui::Checkbox("Distance", &cfg.distance);
    if (cfg.distance) { ImGui::SameLine(); ImGui::ColorEdit3("##dist", cfg.colDist, ImGuiColorEditFlags_NoInputs); }
    ImGui::Separator();
    ImGui::Checkbox("Off-screen Arrows", &cfg.offscreen);
    if (cfg.offscreen) {
        ImGui::SameLine();
        ImGui::ColorEdit3("##ofs", cfg.colOfs, ImGuiColorEditFlags_NoInputs);
        ImGui::SliderFloat("Arrow Size", &cfg.arrowScale, 5.f, 30.f);
    }
    ImGui::Separator();
    ImGui::Checkbox("Snaplines", &cfg.snaplines);
    if (cfg.snaplines) { ImGui::SameLine(); ImGui::ColorEdit3("##snap", cfg.colSnap, ImGuiColorEditFlags_NoInputs); }
    ImGui::Separator();

    static const char* status = nullptr;
    if (ImGui::Button("Save"))  status = cfg.Save() ? "Saved!"  : "Save failed!";
    ImGui::SameLine();
    if (ImGui::Button("Load"))  status = cfg.Load() ? "Loaded!" : "Load failed!";
    ImGui::SameLine();
    if (ImGui::Button("Reset")) { cfg = Config{}; cfg.showMenu = true; status = "Reset!"; }
    if (status) { ImGui::SameLine(); ImGui::TextUnformatted(status); }
}

static void DrawExecutorTab() {
    if (!s_editorInit) {
        s_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
        s_editor.SetText("print(\"hello from executor\")");
        s_editorInit = true;
    }

    float editorH = ImGui::GetContentRegionAvail().y - 130.f;
    if (editorH < 80.f) editorH = 80.f;
    s_editor.Render("##src", ImVec2(-1, editorH), true);

    bool ctrl = ImGui::GetIO().KeyCtrl;
    bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    if ((ImGui::Button("Execute") || (ctrl && enter)))
        executor::QueueScript(s_editor.GetText());
    ImGui::SameLine();
    if (ImGui::Button("Clear Script")) s_editor.SetText("");
    ImGui::SameLine();
    if (ImGui::Button("Clear Log"))    executor::ClearLog();

    ImGui::Separator();
    ImGui::TextUnformatted("Log:");
    ImGui::BeginChild("##log", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (auto& e : executor::SnapshotLog()) {
        if (e.isError) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 120, 120, 255));
        ImGui::TextUnformatted(e.text.c_str());
        if (e.isError) ImGui::PopStyleColor();
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.f)
        ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
}

void Draw(Config& cfg) {
    if (!cfg.showMenu) return;
    ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);
    ImGui::Begin("2009tool", &cfg.showMenu);
    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("ESP"))      { DrawESPTab(cfg);    ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Executor")) { DrawExecutorTab();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Explorer")) {
            float w = ImGui::GetContentRegionAvail().x;
            ImGui::BeginChild("##exp_pane", ImVec2(w * 0.45f, 0), false);
            explorer::Draw();
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("##prop_pane", ImVec2(0, 0), false);
            properties::Draw(explorer::GetSelected());
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

} // namespace menu
