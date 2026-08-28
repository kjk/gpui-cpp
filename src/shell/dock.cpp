#include "shell/dock.h"

#include "shell/scope.h"

namespace gpui::shell {

struct ScriptPanelRegistration {
    Str name = {};
    ShellPanelScript script = {};
};

struct ScriptPanelData {
    App* app = nullptr;
    ScriptPanelRegistration* registration = nullptr;
    Entity<ScriptView> view = {};
    FocusHandle focus = {};
    Str serialized = {};
    bool serializedIsJson = false;
};

struct ScriptPanelManager {
    Vec<ScriptPanelRegistration*> registrations;
    Vec<ScriptPanelData*> panels;

    ~ScriptPanelManager() {
        for (int i = 0; i < panels.len; i++) {
            StrFree(panels[i]->serialized);
            delete panels[i];
        }
        for (int i = 0; i < registrations.len; i++) {
            ScriptPanelRegistration* registration = registrations[i];
            if (registration->script.release)
                registration->script.release(registration->script.data);
            delete registration;
        }
        panels.Reset();
        registrations.Reset();
    }
};

struct PanelNameTable {
    Arena* arena = ArenaNew();
    Mutex mutex;
    Vec<Str> names;

    ~PanelNameTable() {
        names.Reset();
        ArenaDelete(arena);
    }
};

static PanelNameTable& Names() {
    static PanelNameTable table;
    return table;
}

static Str InternPanelName(Str name) {
    PanelNameTable& table = Names();
    table.mutex.Lock();
    for (int i = 0; i < table.names.len; i++) {
        if (StrEq(table.names[i], name)) {
            Str found = table.names[i];
            table.mutex.Unlock();
            return found;
        }
    }
    Str interned = StrDup(table.arena, name);
    table.names.Append(interned);
    table.mutex.Unlock();
    return interned;
}

Str ShellPanelName(Str application, Str panel) {
    StrBuilder name;
    name.Append(StrL("shell:"));
    name.Append(application);
    name.AppendChar('/');
    name.Append(panel);
    Str value = name.TakeStr();
    Str interned = InternPanelName(value);
    StrFree(value);
    return interned;
}

static Str QualifiedPanelName(Str name) {
    if (StrStartsWith(name, "shell:")) return InternPanelName(name);
    StrBuilder qualified;
    qualified.Append(StrL("shell:"));
    qualified.Append(name);
    Str value = qualified.TakeStr();
    Str interned = InternPanelName(value);
    StrFree(value);
    return interned;
}

static ScriptPanelManager* PanelManager(App* app) {
    return AppGlobalEnsure<ScriptPanelManager>(app);
}

static ScriptPanelData* NewPanelData(App* app,
                                     ScriptPanelRegistration* registration,
                                     Entity<ScriptView> view) {
    ScriptPanelManager* manager = PanelManager(app);
    if (!manager) return nullptr;
    auto* panel = new ScriptPanelData();
    panel->app = app;
    panel->registration = registration;
    panel->view = view;
    panel->focus = FocusHandleNew(app);
    manager->panels.Append(panel);
    return panel;
}

static El* RenderPanel(Ctx* cx, void* data) {
    auto* panel = (ScriptPanelData*)data;
    El* root = Div(cx->a)->TrackFocus(panel ? panel->focus : FocusHandle{})
                   ->SizeFull();
    if (panel && panel->view.IsValid())
        root->Child(EntityRender(cx->app, cx->win, cx->a, panel->view.id));
    return root;
}

static void DumpPanel(void* data, PanelStateNode* out) {
    auto* panel = (ScriptPanelData*)data;
    if (!panel || !out) return;
    if (!panel->view.IsValid()) {
        out->info = panel->serialized;
        out->infoIsJson = panel->serializedIsJson;
        return;
    }
    ShellPanelScript* script = panel->registration
                                   ? &panel->registration->script
                                   : nullptr;
    if (!script || !script->serialize) return;
    StrBuilder encoded;
    if (!script->serialize(panel->view, panel->app, script->data, &encoded))
        return;
    Str json = encoded.TakeStr();
    Arena* check = ArenaNew();
    bool valid = JsonParse(check, json) != nullptr;
    ArenaDelete(check);
    if (!valid) {
        StrFree(json);
        return;
    }
    StrFree(panel->serialized);
    panel->serialized = json;
    panel->serializedIsJson = true;
    out->info = panel->serialized;
    out->infoIsJson = true;
}

static DockPanelDef PanelDef(ScriptPanelData* panel) {
    DockPanelDef def;
    if (!panel) return def;
    ShellPanelScript* script = panel->registration
                                   ? &panel->registration->script
                                   : nullptr;
    def.name = panel->registration ? panel->registration->name
                                   : StrL("shell:panel");
    def.title = def.name;
    def.render = RenderPanel;
    def.dump = DumpPanel;
    def.data = panel;
    def.closable = script ? script->closable : true;
    def.visible = script ? script->visible : true;
    def.canZoom = script ? script->zoomable : true;
    def.zoomable = def.canZoom ? DockPanelControl::Menu
                              : DockPanelControl::No;
    return def;
}

DockPanelDef ScriptPanelNew(App* app, Str name, Entity<ScriptView> view,
                            const ShellPanelScript* script) {
    auto* registration = new ScriptPanelRegistration();
    registration->name = QualifiedPanelName(name);
    if (script) registration->script = *script;
    ScriptPanelManager* manager = PanelManager(app);
    if (!manager) {
        delete registration;
        return {};
    }
    manager->registrations.Append(registration);
    return PanelDef(NewPanelData(app, registration, view));
}

static DockPanelDef BuildRegisteredPanel(const PanelBuildContext* context,
                                         Window* window, App* app,
                                         void* data) {
    auto* registration = (ScriptPanelRegistration*)data;
    Entity<ScriptView> view = {};
    if (registration && registration->script.build)
        view = registration->script.build(window, app,
                                           registration->script.data);
    ScriptPanelData* panel = NewPanelData(app, registration, view);
    if (!panel) return {};
    if (context && context->state && context->state->info) {
        if (view.IsValid() && registration->script.deserialize) {
            registration->script.deserialize(
                view, context->state->info, window, app,
                registration->script.data);
        } else if (!view.IsValid()) {
            panel->serialized = StrDup(context->state->info);
            panel->serializedIsJson = context->state->infoIsJson;
        }
    }
    return PanelDef(panel);
}

Str ShellRegisterPanel(App* app, Str application, Str panel,
                       const ShellPanelScript& script) {
    ScriptPanelManager* manager = PanelManager(app);
    if (!manager) return {};
    Str name = ShellPanelName(application, panel);
    for (int i = 0; i < manager->registrations.len; i++) {
        ScriptPanelRegistration* registration = manager->registrations[i];
        if (!StrEq(registration->name, name)) continue;
        if (registration->script.release)
            registration->script.release(registration->script.data);
        registration->script = script;
        register_panel(app, name, BuildRegisteredPanel, registration);
        return name;
    }
    auto* registration = new ScriptPanelRegistration();
    registration->name = name;
    registration->script = script;
    manager->registrations.Append(registration);
    register_panel(app, name, BuildRegisteredPanel, registration);
    return name;
}

static CallScopeGuard EnterLayout(Ctx* cx) {
    return ScopeEnter(cx ? cx->win : nullptr, cx ? cx->app : nullptr,
                      ScopePhase::Layout, ScopeCurrentView(),
                      ScopeCurrentPolicy(), ScopeCurrentRuntime(),
                      ScopeCurrentApplication());
}

static El* SkinTabBar(Ctx* cx, void* data, const DockTabGroup* group) {
    auto* skin = (ScriptDockSkin*)data;
    CallScopeGuard guard = EnterLayout(cx);
    return skin && skin->chrome.tabBar
               ? skin->chrome.tabBar(cx, skin->chrome.data, group)
               : nullptr;
}

static El* SkinDropIndicator(Ctx* cx, void* data, Bounds bounds) {
    auto* skin = (ScriptDockSkin*)data;
    CallScopeGuard guard = EnterLayout(cx);
    return skin && skin->chrome.dropIndicator
               ? skin->chrome.dropIndicator(cx, skin->chrome.data, bounds)
               : nullptr;
}

static El* SkinDock(Ctx* cx, void* data, const DockCtx* dock, El* content) {
    auto* skin = (ScriptDockSkin*)data;
    CallScopeGuard guard = EnterLayout(cx);
    return skin && skin->chrome.dock
               ? skin->chrome.dock(cx, skin->chrome.data, dock, content)
               : content;
}

ScriptDockSkin::ScriptDockSkin() {
    renderer.data = this;
    renderer.tabBar = SkinTabBar;
    renderer.dropIndicator = SkinDropIndicator;
    renderer.dock = SkinDock;
}

ScriptDockSkin::ScriptDockSkin(ShellDockChrome value) : ScriptDockSkin() {
    chrome = value;
}

const DockRenderer* ScriptDockSkin::Renderer() {
    renderer.data = this;
    return &renderer;
}

static const char* PlacementName(DockPlacement placement) {
    switch (placement) {
        case DockPlacement::Left:
            return "left";
        case DockPlacement::Right:
            return "right";
        case DockPlacement::Bottom:
            return "bottom";
        default:
            return "center";
    }
}

void ShellTabGroupData(const DockTabGroup* group, StrBuilder* out) {
    JsonWriter json;
    json.out = out;
    DockState* state = group && group->cx ? group->state.Get(group->cx)
                                          : nullptr;
    int active = group ? DockGroupActiveIx(group) : -1;
    json.BeginObject();
    json.Number("node", group ? group->node : -1);
    json.Number("active_index", active);
    json.Bool("zoomed", state && state->zoomPanel >= 0 &&
                            DockNodeOfPanel(state, state->zoomPanel) ==
                                group->node);
    json.Bool("collapsed", group && group->collapsed);
    json.Bool("locked", !state || DockNodeLocked(state, group->node));
    json.Bool("draggable", state && DockNodeDraggable(state, group->node));
    json.Bool("droppable", state && DockNodeDroppable(state, group->node));
    json.Bool("closable", state && !DockIsLastPanel(state, group->node));
    json.BeginArray("tabs");
    int count = group ? DockGroupCount(group) : 0;
    for (int i = 0; i < count; i++) {
        const DockPanelDef* panel = DockGroupPanel(group, i);
        if (!panel) continue;
        json.BeginObject();
        json.Number("index", i);
        json.String("name", panel->name);
        json.Number("id", (double)panel->id.AsU64());
        json.Bool("active", i == active);
        json.Bool("visible", panel->visible);
        json.Bool("closable", panel->closable);
        json.Bool("zoomable", panel->canZoom);
        json.EndObject();
    }
    json.EndArray();
    json.EndObject();
}

void ShellDockData(const DockCtx* dock, StrBuilder* out) {
    JsonWriter json;
    json.out = out;
    json.BeginObject();
    json.String("placement",
                Str(PlacementName(dock ? dock->placement
                                       : DockPlacement::Center)));
    json.Number("size", dock ? dock->size : 0);
    json.Bool("open", dock && dock->open);
    json.Bool("collapsible", dock && dock->collapsible);
    json.EndObject();
}

void ShellTileData(const TileContext* tile, const DockState* dock,
                   StrBuilder* out) {
    const TileItem* item = tile ? tile->Item() : nullptr;
    const DockPanelDef* panel =
        item && dock && item->panel >= 0 && item->panel < dock->panels.len
            ? &dock->panels[item->panel]
            : nullptr;
    JsonWriter json;
    json.out = out;
    json.BeginObject();
    json.Number("node", tile ? (double)tile->node.AsU64() : 0);
    json.BeginObject("panel");
    json.String("name", panel ? panel->name : Str{});
    json.Number("id", panel ? (double)panel->id.AsU64() : 0);
    json.Bool("visible", panel && panel->visible);
    json.EndObject();
    Bounds bounds = item ? item->bounds : Bounds{};
    json.BeginObject("bounds");
    json.Number("x", bounds.x);
    json.Number("y", bounds.y);
    json.Number("width", bounds.w);
    json.Number("height", bounds.h);
    json.EndObject();
    json.Number("z_index", item ? item->zIndex : 0);
    json.Bool("moving", tile && tile->state &&
                            tile->state->dragging == tile->ix);
    json.Bool("resizing", tile && tile->state &&
                              tile->state->resizing == tile->ix);
    json.Bool("closable", panel && panel->closable);
    json.Bool("zoomed", tile && tile->state && item &&
                            tile->state->zoomedPanel == item->panel);
    json.Bool("zoomable", panel && panel->canZoom);
    json.EndObject();
}

} // namespace gpui::shell
