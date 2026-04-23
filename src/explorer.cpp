#include "explorer.h"
#include "roblox.h"
#include "vendor/imgui/imgui.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace explorer {

static constexpr int MAX_DEPTH    = 6;
static constexpr int MAX_CHILDREN = 64;
static constexpr int MAX_NODES    = 1024;

struct Node {
    uintptr_t         inst;
    std::string       name;
    std::vector<Node> children;
};

static Node      s_root;
static bool      s_built      = false;
static int       s_nodeCount  = 0;
static uint32_t  s_selected   = 0;

static uintptr_t ModuleBase() {
    static uintptr_t cached = 0;
    if (!cached) cached = reinterpret_cast<uintptr_t>(GetModuleHandleA("RobloxApp_client.exe"));
    return cached;
}

static void BuildNode(Node& node, int depth) {
    if (depth >= MAX_DEPTH || s_nodeCount >= MAX_NODES) return;

    uint32_t ptrs[MAX_CHILDREN];
    uint32_t n = GetChildren(static_cast<uint32_t>(node.inst), ptrs, MAX_CHILDREN);
    node.children.reserve(n);

    for (uint32_t i = 0; i < n && s_nodeCount < MAX_NODES; i++) {
        if (!ptrs[i]) continue;
        Node child;
        child.inst = ptrs[i];
        char nm[64] = {};
        ReadString(ptrs[i] + offsets::Name, nm, sizeof(nm));
        child.name = nm[0] ? nm : "Instance";
        s_nodeCount++;
        BuildNode(child, depth + 1);
        node.children.push_back(std::move(child));
    }
}

void Rebuild() {
    uintptr_t modBase = ModuleBase();
    if (!modBase) return;

    uintptr_t dm = GetDataModel(modBase);
    if (!dm) return;

    s_root.inst = dm;
    s_root.name = "Game";
    s_root.children.clear();
    s_nodeCount = 0;
    BuildNode(s_root, 0);
    s_built = true;
}

static void DrawNode(const Node& node) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (static_cast<uint32_t>(node.inst) == s_selected) flags |= ImGuiTreeNodeFlags_Selected;

    bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(node.inst)),
                                  flags, "%s", node.name.c_str());

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        s_selected = static_cast<uint32_t>(node.inst);

    if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(0) && !node.name.empty())
        ImGui::SetClipboardText(node.name.c_str());

    if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
        for (const auto& child : node.children)
            DrawNode(child);
        ImGui::TreePop();
    }
}

uint32_t GetSelected() { return s_selected; }

void Draw() {
    static DWORD s_lastRebuild = 0;
    DWORD now = GetTickCount();
    if (!s_built || (now - s_lastRebuild) > 2000) {
        Rebuild();
        s_lastRebuild = now;
    }

    if (!s_built) {
        ImGui::TextUnformatted("Waiting for game...");
        return;
    }

    ImGui::TextDisabled("Click to select  |  Double-click to copy  |  %d nodes", s_nodeCount);
    ImGui::Separator();

    ImGui::BeginChild("##explorer_tree", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    DrawNode(s_root);
    ImGui::EndChild();
}

} // namespace explorer
