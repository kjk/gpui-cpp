#include "Story.h"

// One ResizableState per group, the way the Rust story makes one per
// ResizablePanelGroup: the sizes the drags leave belong to the state, not to
// the elements, which are rebuilt every frame.
struct ResizableStory {
    bool showLeft = true;
    bool useFlexNone = true;
    float leftSize = 200;
    float rightSize = 200;
    Entity<component::ResizableState> nested = {};
    Entity<component::ResizableState> nestedTop = {};
    Entity<component::ResizableState> grow = {};
    Entity<component::ResizableState> flex = {};
    Entity<component::ResizableState> prog = {};
    bool seeded = false;

    static El* Render(ResizableStory* self, Ctx* cx);
};

static void ToggleLeft(ResizableStory* self, Ctx* cx, const ClickEvent*) {
    self->showLeft = !self->showLeft;
    Notify(cx);
}
static void ToggleFlexNone(ResizableStory* self, Ctx* cx, const ClickEvent*) {
    self->useFlexNone = !self->useFlexNone;
    Notify(cx);
}
static void SetPanelSize(ResizableStory* self, Ctx* cx, const ClickEvent*,
                         intptr_t packed) {
    // The low bit picks the panel, the rest is the new size.
    float size = (float)(packed >> 1);
    if (packed & 1) {
        self->rightSize = size;
    } else {
        self->leftSize = size;
    }
    // resize_panel_at_handle from an action rather than a drag, which is what
    // the Rust story's four buttons do: the group settles the rest.
    component::ResizableState* st = self->prog.Get(cx);
    if (st && st->sizes.len == 3) {
        float container = st->bounds.w;
        if (packed & 1) {
            ResizablePanelResize(st->sizes.els, st->mins.els, st->maxs.els,
                                 st->sizes.len, 1,
                                 container - size - st->sizes[0], container);
        } else {
            ResizablePanelResize(st->sizes.els, st->mins.els, st->maxs.els,
                                 st->sizes.len, 0, size, container);
        }
    }
    Notify(cx);
}

// panel_box(): p_4 over the whole panel.
static El* PanelBox(Ctx* cx, const char* text) {
    Arena* a = cx->a;
    return Div(a)->W(kFill)->H(kFill)->Pad(16)->Child(
        StoryTxt(cx, Str(text), 16, cx->theme().foreground)->Wrap());
}

static El* Frame(Ctx* cx, float h) {
    Arena* a = cx->a;
    return Div(a)->FlexCol()->W(kFill)->H(h)->Border(1, cx->theme().border);
}

El* ResizableStory::Render(ResizableStory* self, Ctx* cx) {
    Arena* a = cx->a;
    if (!self->seeded) {
        self->seeded = true;
        for (Entity<component::ResizableState>* e :
             {&self->nested, &self->nestedTop, &self->grow, &self->flex,
              &self->prog}) {
            *e = EntityNewState<component::ResizableState>(cx->app);
        }
    }
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill)->ItemsCenter();

    El* nested = StorySection(cx, "Nested Panels",
                              "Combines horizontal and vertical splits with "
                              "constrained panel sizes.");
    StorySectionBody(nested)->W(kFill);
    El* topRow =
        component::Resizable::New(cx, StrL("rz-nested-top"), self->nestedTop)
            ->H(264)
            ->Panel(PanelBox(cx, "Left (120px .. 300px)"), 150, 120, 300)
            ->Grow(PanelBox(cx, "Center"))
            ->Panel(PanelBox(cx, "Right"), 300)
            ->IntoEl();
    El* nestedBox =
        component::Resizable::New(cx, StrL("rz-nested"), self->nested,
                                  Axis::Vertical)
            ->H(600)
            ->Panel(topRow, 264)
            ->Grow(PanelBox(cx, "Center"))
            ->Panel(PanelBox(cx, "Bottom (80px .. 150px)"), 80, 80, 150)
            ->IntoEl();
    StorySectionAdd(nested, Frame(cx, 600)->Child(nestedBox));
    page->Child(nested);

    El* grow = StorySection(cx, "Growing Panel",
                            "A flexible panel absorbs the space left by a "
                            "constrained neighbor.");
    StorySectionBody(grow)->W(kFill);
    El* growRow = component::Resizable::New(cx, StrL("rz-grow"), self->grow)
                      ->Panel(PanelBox(cx, "Left 2"), 200)
                      ->Grow(PanelBox(cx, "Right (Grow)"))
                      ->IntoEl();
    StorySectionAdd(grow, Frame(cx, 400)->Child(growRow));
    page->Child(grow);

    El* flex = StorySection(cx, "Flex Behavior",
                            "Compare fixed and growing panels while toggling "
                            "visibility.");
    StorySectionBody(flex)->W(kFill);
    El* flexCol = Div(a)->FlexCol()->W(kFill)->Gap(8);
    El* flexBtns = StoryToolbarGroup(cx);
    flexBtns->Child(
        component::Button::New(cx, StrL("toggle-left"))
            ->Outline()
            ->Label(self->showLeft ? StrL("Hide Left") : StrL("Show Left"))
            ->OnClick(Listen(cx, &ToggleLeft))
            ->IntoEl());
    flexBtns->Child(component::Button::New(cx, StrL("toggle-flex-none"))
                        ->Outline()
                        ->Label(self->useFlexNone ? StrL("Use flex_none: ON")
                                                  : StrL("Use flex_none: OFF"))
                        ->OnClick(Listen(cx, &ToggleFlexNone))
                        ->IntoEl());
    El* flexBtnRow = Div(a)->FlexRow()->W(kFill)->Gap(8);
    flexBtnRow->Child(flexBtns);
    flexCol->Child(flexBtnRow);
    component::Resizable* flexGroup =
        component::Resizable::New(cx, StrL("rz-flex"), self->flex);
    if (self->showLeft) {
        flexGroup->Panel(PanelBox(cx, "Left"), 200);
    }
    flexGroup->Grow(PanelBox(cx, "Center"));
    if (self->useFlexNone) {
        flexGroup->Panel(PanelBox(cx, "Right"), 280);
    } else {
        flexGroup->Grow(PanelBox(cx, "Right"));
    }
    flexCol->Child(Frame(cx, 200)->Child(flexGroup->IntoEl()));
    StorySectionAdd(flex, flexCol);
    page->Child(flex);

    El* prog = StorySection(cx, "Programmatic Resize",
                            "Panel sizes can be changed by actions as well as "
                            "dragging.");
    StorySectionBody(prog)->W(kFill);
    El* progCol = Div(a)->FlexCol()->W(kFill)->Gap(8);
    Listener setSize = Listen(cx, &SetPanelSize);
    struct SizeBtn {
        const char* id;
        const char* label;
        int packed;
    };
    static const SizeBtn kSizeBtns[] = {
        {"compact-left", "Compact left \xE2\x86\x92 100", 100 << 1},
        {"reset-left", "Reset left \xE2\x86\x92 200", 200 << 1},
        {"compact-right", "Compact right \xE2\x86\x92 80", (80 << 1) | 1},
        {"reset-right", "Reset right \xE2\x86\x92 200", (200 << 1) | 1},
    };
    El* progBtns = Div(a)->FlexRow()->W(kFill)->Gap(8)->FlexWrap();
    for (size_t i = 0; i < sizeof(kSizeBtns) / sizeof(kSizeBtns[0]); i++) {
        progBtns->Child(component::Button::New(cx, Str(kSizeBtns[i].id))
                            ->Outline()
                            ->Label(Str(kSizeBtns[i].label))
                            ->OnClick(ListenerArg(setSize, kSizeBtns[i].packed))
                            ->IntoEl());
    }
    progCol->Child(progBtns);
    El* progRow = component::Resizable::New(cx, StrL("rz-prog"), self->prog)
                      ->Panel(PanelBox(cx, "Left"), self->leftSize)
                      ->Panel(PanelBox(cx, "Center"), 300)
                      ->Panel(PanelBox(cx, "Right"), self->rightSize)
                      ->IntoEl();
    progCol->Child(Frame(cx, 200)->Child(progRow));
    StorySectionAdd(prog, progCol);
    page->Child(prog);
    return page;
}

STORY_PAGE(StoryResizable, ResizableStory);
