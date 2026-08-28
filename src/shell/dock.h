#ifndef GPUI_SHELL_DOCK_H_
#define GPUI_SHELL_DOCK_H_

#include "base/dock_registry.h"
#include "base/tiles.h"
#include "shell/view.h"

namespace gpui::shell {

using ShellPanelBuildFn = Entity<ScriptView> (*)(Window* window, App* app,
                                                  void* data);
using ShellPanelSerializeFn = bool (*)(Entity<ScriptView> view, App* app,
                                       void* data, StrBuilder* out);
using ShellPanelDeserializeFn = void (*)(Entity<ScriptView> view, Str json,
                                         Window* window, App* app,
                                         void* data);

struct ShellPanelScript {
    void* data = nullptr;
    ShellPanelBuildFn build = nullptr;
    ShellPanelSerializeFn serialize = nullptr;
    ShellPanelDeserializeFn deserialize = nullptr;
    void (*release)(void* data) = nullptr;
    bool closable = true;
    bool zoomable = true;
    bool visible = true;
};

// Stable, process-wide `shell:<application>/<panel>` names. The intentionally
// interned allocation is bounded by the number of panel contributions.
Str ShellPanelName(Str application, Str panel);

DockPanelDef ScriptPanelNew(App* app, Str name, Entity<ScriptView> view,
                            const ShellPanelScript* script = nullptr);
Str ShellRegisterPanel(App* app, Str application, Str panel,
                       const ShellPanelScript& script);

// Script-owned appearance over Base's dock behavior. Hooks run in a nested
// Layout scope and may return null to preserve Base's no-chrome default.
struct ShellDockChrome {
    void* data = nullptr;
    El* (*tabBar)(Ctx* cx, void* data, const DockTabGroup* group) = nullptr;
    El* (*dropIndicator)(Ctx* cx, void* data, Bounds bounds) = nullptr;
    El* (*dock)(Ctx* cx, void* data, const DockCtx* dock,
                El* content) = nullptr;
};

struct ScriptDockSkin {
    ShellDockChrome chrome = {};
    DockRenderer renderer = {};

    ScriptDockSkin();
    explicit ScriptDockSkin(ShellDockChrome value);
    const DockRenderer* Renderer();
};

void ShellTabGroupData(const DockTabGroup* group, StrBuilder* out);
void ShellDockData(const DockCtx* dock, StrBuilder* out);
void ShellTileData(const TileContext* tile, const DockState* dock,
                   StrBuilder* out);

} // namespace gpui::shell

#endif // GPUI_SHELL_DOCK_H_
