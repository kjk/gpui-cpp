/* Persisted panel registry — crates/base/src/dock/registry.rs */

#include "base/dock_state.h"

namespace gpui {

// The source keeps a weak DockArea entity. DockState is the retained entity
// in the compatibility runtime, so it is the stable area handle here.
struct PanelBuildContext {
    Entity<DockState> dockArea = {};
    const PanelState* state = nullptr;
    const PanelInfo* info = nullptr;
};

using PanelRegistryBuild = DockPanelDef (*)(const PanelBuildContext* context,
                                            Window* win, App* app,
                                            void* data);

struct PanelRegistryEntry {
    Str name = {};
    PanelRegistryBuild build = nullptr;
    void* data = nullptr;
};

struct PanelRegistry {
    Arena* arena = nullptr;
    Vec<PanelRegistryEntry> items;

    PanelRegistry();
    ~PanelRegistry();
    void Register(Str panelName, PanelRegistryBuild build, void* data);
    bool BuildPanel(Str panelName, const PanelBuildContext* context,
                    Window* win, App* app, DockPanelDef* out) const;
};

PanelRegistry* PanelRegistryGlobal(App* app);
void register_panel(App* app, Str panelName, PanelRegistryBuild build,
                    void* data = nullptr);

} // namespace gpui
