#include "Story.h"

// crates/story has no dock page at the pinned SHA: the dock is what the
// gallery itself is built out of, and every story is a Panel inside one. This
// page is a demo of its own, with labelled boxes for panels so it is about
// the dock and not about what is inside it.
struct DockPanelData {
    // Panel::panel_name(), which is what a saved layout stores. The Rust
    // story's panels are all "StoryContainer"; each of these is its own kind,
    // so a layout read back knows which is which.
    const char* name;
    const char* title;
    const char* body;
};

static DockPanelData kPanels[] = {
    {"ExplorerPanel", "Explorer",
     "The left Dock. Drag its inner edge to resize it, or use the "
     "toggle button in the tab bar to close it."},
    {"OutlinePanel", "Outline", "A second panel in the left Dock's tab group."},
    {"EditorPanel", "main.cpp",
     "Drag a tab onto another group to merge it, or onto an edge "
     "of one to split that group."},
    {"ReadmePanel", "README.md",
     "The centre item is a split of two tab groups. The handle "
     "between them resizes both."},
    {"PreviewPanel", "Preview", "The right half of the centre split."},
    {"TerminalPanel", "Terminal",
     "The bottom Dock keeps its tab bar when it is closed, so "
     "there is still something to click."},
    {"ProblemsPanel", "Problems", "A second panel in the bottom Dock."},
    {"PropertiesPanel", "Properties", "The right Dock."},
};

const int kNPanels = (int)(sizeof(kPanels) / sizeof(kPanels[0]));

struct DockStory {
    Entity<DockState> dock = {};
    bool seeded = false;
    // What the last DockEvent said, shown under the area.
    Str message = {};
    bool locked = false;

    // The layout as JSON, which is what DockArea::dump answers with — the
    // story keeps it in hand rather than on disk.
    Str saved = {};
    Arena* savedArena = nullptr;

    static El* Render(DockStory* self, Ctx* cx);
    static void OnDockEvent(DockStory* self, Ctx* cx, const DockEvent* ev);
    static void OnToggleLock(DockStory* self, Ctx* cx, const ClickEvent* ev);
    static void OnSaveLayout(DockStory* self, Ctx* cx, const ClickEvent* ev);
    static void OnLoadLayout(DockStory* self, Ctx* cx, const ClickEvent* ev);
    static void OnLoadStale(DockStory* self, Ctx* cx, const ClickEvent* ev);
};

static El* RenderPanel(Ctx* cx, void* data) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    const DockPanelData* d = (const DockPanelData*)data;
    El* box = Div(a)->FlexCol()->Gap(8)->Pad(12)->W(kFill);
    box->Child(StoryTxt(cx, Str(d->title), 14, th.foreground));
    box->Child(StoryTxt(cx, Str(d->body), 13, th.mutedFg)->Wrap());
    return box;
}

void DockStory::OnDockEvent(DockStory* self, Ctx* cx, const DockEvent*) {
    if (self->message.s) {
        StrFree(self->message);
    }
    self->message = StrDup(StrL("Layout changed"));
    Notify(cx);
}

void DockStory::OnToggleLock(DockStory* self, Ctx* cx, const ClickEvent*) {
    self->locked = !self->locked;
    DockState* s = self->dock.Get(cx);
    if (s) {
        s->locked = self->locked;
    }
    Notify(cx);
}

// DockArea::add_panel: the layout the story opens with.
static void Seed(DockStory* self, Ctx* cx) {
    self->dock = EntityNewState<DockState>(cx->app);
    DockState* s = self->dock.Get(cx);
    if (!s) {
        return;
    }
    int panel[kNPanels];
    for (int i = 0; i < kNPanels; i++) {
        DockPanelDef def;
        def.name = Str(kPanels[i].name);
        def.title = Str(kPanels[i].title);
        def.render = RenderPanel;
        def.data = &kPanels[i];
        panel[i] = DockAddPanelDef(s, def);
    }

    int leftTabs = DockNewTabs(s);
    DockTabsAdd(s, leftTabs, panel[0]);
    DockTabsAdd(s, leftTabs, panel[1]);
    s->left.node = leftTabs;
    s->left.size = 180;

    int centerLeft = DockNewTabs(s);
    DockTabsAdd(s, centerLeft, panel[2]);
    DockTabsAdd(s, centerLeft, panel[3]);
    int centerRight = DockNewTabs(s);
    DockTabsAdd(s, centerRight, panel[4]);
    int split = DockNewSplit(s, Axis::Horizontal);
    DockSplitAdd(s, split, centerLeft, 320);
    DockSplitAdd(s, split, centerRight, 220);
    s->center = split;

    int bottomTabs = DockNewTabs(s);
    DockTabsAdd(s, bottomTabs, panel[5]);
    DockTabsAdd(s, bottomTabs, panel[6]);
    s->bottom.node = bottomTabs;
    s->bottom.size = 140;

    int rightTabs = DockNewTabs(s);
    DockTabsAdd(s, rightTabs, panel[7]);
    s->right.node = rightTabs;
    s->right.size = 180;
}

// DockArea::dump: the tree as JSON.
void DockStory::OnSaveLayout(DockStory* self, Ctx* cx, const ClickEvent*) {
    DockState* s = self->dock.Get(cx);
    if (!s) {
        return;
    }
    DockAreaState state;
    DockDump(s, &state);
    StrBuilder sb;
    DockAreaStateWrite(&state, &sb);
    if (self->saved.s) {
        StrFree(self->saved);
    }
    self->saved = sb.TakeStr();
    if (self->message.s) {
        StrFree(self->message);
    }
    self->message = StrDup(StrL("Layout saved"));
    Notify(cx);
}

// DockArea::load, with the panels matched by name.
static void LoadFrom(DockStory* self, Ctx* cx, Str json) {
    DockState* s = self->dock.Get(cx);
    if (!s || !json.s) {
        return;
    }
    // The strings the loaded panels keep have to outlive the frame, so the
    // story holds the arena the state is parsed into.
    if (self->savedArena) {
        ArenaDelete(self->savedArena);
    }
    self->savedArena = ArenaNew();
    DockAreaState state;
    if (!DockAreaStateParse(self->savedArena, json, &state)) {
        return;
    }
    DockLoad(s, &state, self->savedArena, component::DockInvalidPanelRender);
    if (self->message.s) {
        StrFree(self->message);
    }
    self->message = StrDup(StrL("Layout loaded"));
    Notify(cx);
}

void DockStory::OnLoadLayout(DockStory* self, Ctx* cx, const ClickEvent*) {
    LoadFrom(self, cx, self->saved);
}

// A layout written by an older build, naming a panel this one does not have:
// the dock keeps its shape and says which type is missing, which is what
// Rust's InvalidPanel is for.
void DockStory::OnLoadStale(DockStory* self, Ctx* cx, const ClickEvent*) {
    LoadFrom(self, cx,
             StrL("{\"center\":{\"panel_name\":\"TabPanel\",\"children\":["
                  "{\"panel_name\":\"EditorPanel\",\"children\":[],"
                  "\"info\":{\"panel\":null}},"
                  "{\"panel_name\":\"GitGraphPanel\",\"children\":[],"
                  "\"info\":{\"panel\":null}}],"
                  "\"info\":{\"tabs\":{\"active_index\":1}}}}"));
}

El* DockStory::Render(DockStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        Seed(self, cx);
        DockState* s = self->dock.Get(cx);
        if (s) {
            s->onEvent = Listen(cx, &DockStory::OnDockEvent);
        }
    }
    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);

    El* section =
        StorySection(cx, "Dock",
                     "A centre item with a Dock on the left, the right and "
                     "the bottom. Tabs move between groups by dragging.");
    El* box = Div(a)
                  ->FlexCol()
                  ->W(kFill)
                  ->H(520)
                  ->Border(1, th.border)
                  ->Radius(th.radius);
    box->Child(component::DockArea::New(cx, StrL("dock"), self->dock)
                   ->IntoEl());
    StorySectionAdd(section, box);

    El* row = Div(a)->FlexRow()->Gap(12)->ItemsCenter()->W(kFill);
    row->Child(
        component::Button::New(cx, StrL("dock-lock"))
            ->Label(self->locked ? StrL("Unlock layout") : StrL("Lock layout"))
            ->Compact()
            ->OnClick(Listen(cx, &DockStory::OnToggleLock))
            ->IntoEl());
    row->Child(component::Button::New(cx, StrL("dock-save"))
                   ->Label(StrL("Save layout"))
                   ->Compact()
                   ->OnClick(Listen(cx, &DockStory::OnSaveLayout))
                   ->IntoEl());
    row->Child(component::Button::New(cx, StrL("dock-load"))
                   ->Label(StrL("Load layout"))
                   ->Compact()
                   ->Disabled(!self->saved.s)
                   ->OnClick(Listen(cx, &DockStory::OnLoadLayout))
                   ->IntoEl());
    row->Child(component::Button::New(cx, StrL("dock-stale"))
                   ->Label(StrL("Load stale layout"))
                   ->Compact()
                   ->OnClick(Listen(cx, &DockStory::OnLoadStale))
                   ->IntoEl());
    if (self->message.s) {
        row->Child(StoryTxt(cx, self->message, 13, th.mutedFg));
    }
    StorySectionAdd(section, row);
    page->Child(section);
    return page;
}

STORY_PAGE(StoryDock, DockStory);
