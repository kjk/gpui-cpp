#include "Story.h"

// The Rust story (crates/story/src/dock_story.rs) fills its dock with panels
// that each render a different widget; ours fills it with labelled boxes, so
// the page is about the dock and not about what is inside it.
struct DockPanelData {
    const char* title;
    const char* body;
};

static DockPanelData kPanels[] = {
    {"Explorer",
     "The left Dock. Drag its inner edge to resize it, or use the "
     "toggle button in the tab bar to close it."},
    {"Outline", "A second panel in the left Dock's tab group."},
    {"main.cpp",
     "Drag a tab onto another group to merge it, or onto an edge "
     "of one to split that group."},
    {"README.md",
     "The centre item is a split of two tab groups. The handle "
     "between them resizes both."},
    {"Preview", "The right half of the centre split."},
    {"Terminal",
     "The bottom Dock keeps its tab bar when it is closed, so "
     "there is still something to click."},
    {"Problems", "A second panel in the bottom Dock."},
    {"Properties", "The right Dock."},
};

const int kNPanels = (int)(sizeof(kPanels) / sizeof(kPanels[0]));

struct DockStory {
    Entity<DockState> dock = {};
    bool seeded = false;
    // What the last DockEvent said, shown under the area.
    Str message = {};
    bool locked = false;

    static El* Render(DockStory* self, Ctx* cx);
    static void OnDockEvent(DockStory* self, Ctx* cx, const DockEvent* ev);
    static void OnToggleLock(DockStory* self, Ctx* cx, const ClickEvent* ev);
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
    if (self->message.s) {
        row->Child(StoryTxt(cx, self->message, 13, th.mutedFg));
    }
    StorySectionAdd(section, row);
    page->Child(section);
    return page;
}

STORY_PAGE(StoryDock, DockStory);
