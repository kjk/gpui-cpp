#include "Story.h"

struct ResizableStory {
    bool showLeft = true;
    bool useFlexNone = true;
    float leftSize = 200;
    float rightSize = 200;

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
    Notify(cx);
}

// panel_box(): p_4 over the whole panel.
static El* PanelBox(Ctx* cx, const char* text) {
    Arena* a = cx->a;
    return Div(a)->W(kFill)->H(kFill)->Pad(16)->Child(
        StoryTxt(cx, Str(text), 16, cx->theme().foreground)->Wrap());
}

static El* Divider(Ctx* cx, bool vertical) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return vertical ? Div(a)->W(1)->H(kFill)->Bg(th.border)
                    : Div(a)->H(1)->W(kFill)->Bg(th.border);
}

static El* Frame(Ctx* cx, float h) {
    Arena* a = cx->a;
    return Div(a)->FlexCol()->W(kFill)->H(h)->Border(1, cx->theme().border);
}

El* ResizableStory::Render(ResizableStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill)->ItemsCenter();

    El* nested = StorySection(cx, "Nested Panels",
                              "Combines horizontal and vertical splits with "
                              "constrained panel sizes.");
    El* nestedBox = Frame(cx, 600);
    El* topRow = Div(a)->FlexRow()->W(kFill)->H(264);
    topRow->Child(
        Div(a)->W(150)->H(kFill)->Child(PanelBox(cx, "Left (120px .. 300px)")));
    topRow->Child(Divider(cx, true));
    topRow->Child(Div(a)->Grow()->H(kFill)->Child(PanelBox(cx, "Center")));
    topRow->Child(Divider(cx, true));
    topRow->Child(Div(a)->W(300)->H(kFill)->Child(PanelBox(cx, "Right")));
    nestedBox->Child(topRow);
    nestedBox->Child(Divider(cx, false));
    nestedBox->Child(Div(a)->W(kFill)->Grow()->Child(PanelBox(cx, "Center")));
    nestedBox->Child(Divider(cx, false));
    nestedBox->Child(
        Div(a)->W(kFill)->H(80)->Child(PanelBox(cx, "Bottom (80px .. 150px)")));
    StorySectionAdd(nested, nestedBox);
    page->Child(nested);

    El* grow = StorySection(cx, "Growing Panel",
                            "A flexible panel absorbs the space left by a "
                            "constrained neighbor.");
    El* growBox = Frame(cx, 400);
    El* growRow = Div(a)->FlexRow()->W(kFill)->H(kFill);
    growRow->Child(Div(a)->W(200)->H(kFill)->Child(PanelBox(cx, "Left 2")));
    growRow->Child(Divider(cx, true));
    growRow
        ->Child(Div(a)->Grow()->H(kFill)->Child(PanelBox(cx, "Right (Grow)")));
    growBox->Child(growRow);
    StorySectionAdd(grow, growBox);
    page->Child(grow);

    El* flex = StorySection(cx, "Flex Behavior",
                            "Compare fixed and growing panels while toggling "
                            "visibility.");
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
    El* flexBox = Frame(cx, 200);
    El* flexRow = Div(a)->FlexRow()->W(kFill)->H(kFill);
    if (self->showLeft) {
        flexRow->Child(Div(a)->W(200)->H(kFill)->Child(PanelBox(cx, "Left")));
        flexRow->Child(Divider(cx, true));
    }
    flexRow->Child(Div(a)->Grow()->H(kFill)->Child(PanelBox(cx, "Center")));
    flexRow->Child(Divider(cx, true));
    El* flexRight = Div(a)->H(kFill)->Child(PanelBox(cx, "Right"));
    if (self->useFlexNone) {
        flexRight->W(280);
    } else {
        flexRight->Grow();
    }
    flexRow->Child(flexRight);
    flexBox->Child(flexRow);
    flexCol->Child(flexBox);
    StorySectionAdd(flex, flexCol);
    page->Child(flex);

    El* prog = StorySection(cx, "Programmatic Resize",
                            "Panel sizes can be changed by actions as well as "
                            "dragging.");
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
    El* progBox = Frame(cx, 200);
    El* progRow = Div(a)->FlexRow()->W(kFill)->H(kFill);
    progRow->Child(
        Div(a)->W(self->leftSize)->H(kFill)->Child(PanelBox(cx, "Left")));
    progRow->Child(Divider(cx, true));
    progRow
        ->Child(Div(a)->Grow()->H(kFill)->Child(PanelBox(cx, "Center (grow)")));
    progRow->Child(Divider(cx, true));
    progRow->Child(
        Div(a)->W(self->rightSize)->H(kFill)->Child(PanelBox(cx, "Right")));
    progBox->Child(progRow);
    progCol->Child(progBox);
    StorySectionAdd(prog, progCol);
    page->Child(prog);
    return page;
}

STORY_PAGE(StoryResizable, ResizableStory);
