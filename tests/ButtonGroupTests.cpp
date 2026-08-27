/* crates/ui/src/button/button_group.rs: selection callback behavior. */

#include "Test.h"

struct ButtonGroupHarness {
    int calls = 0;
    int count = 0;
    int selected[80] = {};

    static El* Render(ButtonGroupHarness*, Ctx* cx) {
        return Div(cx->a);
    }

    static void OnChange(ButtonGroupHarness* self, Ctx*,
                         const component::ButtonGroupEvent* ev) {
        self->calls++;
        self->count = std::min(ev->count, 80);
        for (int i = 0; i < self->count; i++) {
            self->selected[i] = ev->selected[i];
        }
    }
};

static bool SameButtonColor(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static int ButtonChildCount(const El* root) {
    int count = 0;
    for (const El* child = root ? root->first : nullptr; child;
         child = child->next) {
        count++;
    }
    return count;
}

static const AccessibilityNode* ButtonAt(const Vec<AccessibilityNode>& nodes,
                                         int wanted) {
    for (int i = 0; i < nodes.len; i++) {
        if (nodes[i].info.role != AccessibilityRole::Button) {
            continue;
        }
        if (wanted-- == 0) {
            return &nodes[i];
        }
    }
    return nullptr;
}

static void SelectionEventsAreOrderedAndNotWordSized() {
    App app;
    component::Init(&app);
    Window* win = new Window();
    win->app = &app;
    Arena* arena = ArenaNew();
    Entity<ButtonGroupHarness> harness = EntityNew<ButtonGroupHarness>(&app);
    Ctx cx{&app, win, arena, harness.id};

    component::ButtonGroup* group =
        component::ButtonGroup::New(&cx, StrL("wide-group"))
            ->Multiple(true)
            ->OnClick(Listen(&cx, &ButtonGroupHarness::OnChange));
    for (int i = 0; i < 70; i++) {
        Str id = StrDup(arena, fmt("button-%d", i));
        group->Child(component::Button::New(&cx, id)
                         ->Label(id)
                         ->Selected(i == 1 || i == 65));
    }
    El* root = group->IntoEl();
    IdsCollect(root);
    AccessibilityCollect(root, &win->accessibility);

    const AccessibilityNode* button69 = ButtonAt(win->accessibility, 69);
    utassert(button69 != nullptr);
    if (button69) {
        utassert(WindowAccessibilityPerform(
            win, button69->id, AccessibilityAction::Default));
    }
    ButtonGroupHarness* state = harness.Get(&app);
    utassert(state && state->calls == 1);
    utassert(state && state->count == 3);
    if (state && state->count == 3) {
        utassert(state->selected[0] == 1);
        utassert(state->selected[1] == 65);
        utassert(state->selected[2] == 69);
    }

    win->accessibility.Reset();
    WindowKeyedFree(win);
    delete win;
    ArenaDelete(arena);
    EntityDropAll(&app);
}

static void SourceButtonVariantsRoundingAndIconsRemainConcrete() {
    App app;
    component::Init(&app);
    Window* win = new Window();
    win->app = &app;
    Arena* arena = ArenaNew();
    Ctx cx{&app, win, arena, {}};
    const Theme& theme = ThemeNow(&app);

    component::ButtonCustomVariant base =
        component::ButtonCustomVariant::New(&app);
    component::ButtonCustomVariant custom =
        base.Color(theme.magenta)
            .Foreground(theme.magenta)
            .Hover(RgbaOpacity(theme.magenta, 0.1f))
            .Active(RgbaOpacity(theme.magenta, 0.2f))
            .Shadow();
    utassert(base.color.a == 0 && !base.shadow);
    utassert(custom.shadow && SameButtonColor(custom.foreground, theme.magenta));

    component::Button* button =
        component::Button::New(&cx, StrL("custom-button"))
            ->Custom(custom)
            ->Rounded(component::ButtonRounded::Large)
            ->Label(StrL("Custom"));
    El* rendered = button->IntoEl();
    utassert(button->variant == component::ButtonVariant::Custom);
    utassertnear(rendered->style.corners.tl, theme.radius * 2.f);
    utassert(SameButtonColor(
        rendered->style.bg.color,
        RgbaMixOklab(theme.magenta, Rgba8(0, 0, 0, 0), 0.2f)));
    component::ButtonVariants::Primary(button);
    utassert(button->variant == component::ButtonVariant::Primary &&
             !button->hasCustom);
    utassert(component::ButtonVariantIsGhost(
        component::ButtonVariant::Ghost));
    utassert(component::ButtonVariantIsLink(component::ButtonVariant::Link));
    utassert(component::ButtonVariantIsText(component::ButtonVariant::Text));

    component::Button* grouped[2] = {
        component::Button::New(&cx, StrL("grouped-one")),
        component::Button::New(&cx, StrL("grouped-two")),
    };
    component::ButtonGroup* sourceGroup =
        component::ButtonGroup::New(&cx, StrL("source-group"))
            ->Children(grouped, 2)
            ->Layout(Axis::Vertical)
            ->Custom(custom);
    utassert(sourceGroup->children.len == 2 && sourceGroup->vertical);
    utassert(sourceGroup->variant == component::ButtonVariant::Custom);
    component::DropdownButton* dropdown =
        component::DropdownButton::New(&cx, StrL("source-dropdown"))
            ->Success()
            ->Custom(custom);
    utassert(dropdown->variant == component::ButtonVariant::Custom);

    El* extra = Div(arena)->W(7)->H(7);
    component::ButtonIcon* icon = component::ButtonIcon::New(
        &cx, component::Icon::New(&cx, IconName::Check)->Color(theme.green));
    El* content = component::Button::New(&cx, StrL("icon-and-child"))
                      ->Icon(icon)
                      ->Label(StrL("Both"))
                      ->Extra(extra)
                      ->IntoEl();
    utassert(ButtonChildCount(content) == 3);
    utassert(content->first && content->first->next &&
             content->first->next->next == extra);
    utassert(icon->variant == component::ButtonIconVariant::Icon &&
             !icon->IsSpinner() && !icon->IsProgress());

    El* textLoading = component::Button::New(&cx, StrL("text-loading"))
                          ->Label(StrL("Waiting"))
                          ->Loading(true)
                          ->IntoEl();
    utassert(ButtonChildCount(textLoading) == 1);
    component::ButtonIcon* spinner = component::ButtonIcon::New(
        &cx, component::Spinner::New(&cx));
    component::ButtonIcon* progress = component::ButtonIcon::New(
        &cx, component::ProgressCircle::New(&cx)->Value(75));
    utassert(spinner->IsSpinner() && !spinner->IsProgress());
    utassert(progress->IsProgress() && !progress->IsSpinner());
    utassert(spinner->Loading(true)->WithSize(UiSize::Small)->IntoEl());
    utassert(progress->Loading(true)->Size(18)->IntoEl());

    WindowKeyedFree(win);
    delete win;
    ArenaDelete(arena);
    EntityDropAll(&app);
    AppGlobalClear(&app);
}

struct ToggleGroupHarness {
    int calls = 0;
    int count = 0;
    bool checked[8] = {};

    static El* Render(ToggleGroupHarness*, Ctx* cx) { return Div(cx->a); }
    static void OnChange(ToggleGroupHarness* self, Ctx*,
                         const component::ToggleGroupEvent* event) {
        self->calls++;
        self->count = std::min(event->count, 8);
        for (int i = 0; i < self->count; i++) {
            self->checked[i] = event->checked[i];
        }
    }
};

static void SourceToggleAndSegmentedGroupKeepStateAndGeometry() {
    App app;
    component::Init(&app);
    Window* win = new Window();
    win->app = &app;
    Arena* arena = ArenaNew();
    Entity<ToggleGroupHarness> harness = EntityNew<ToggleGroupHarness>(&app);
    Ctx cx{&app, win, arena, harness.id};
    const Theme& theme = ThemeNow(&app);

    component::Toggle* checked =
        component::Toggle::New(&cx, StrL("checked"))
            ->Label(StrL("Bold"))
            ->Icon(IconName::Check)
            ->Checked(true)
            ->Outline()
            ->WithSize(UiSize::Small);
    El* checkedEl = checked->IntoEl();
    utassert(checked->variant == component::ToggleVariant::Outline);
    utassertnear(checkedEl->style.height, 24.f);
    utassertnear(checkedEl->style.minW, 24.f);
    utassert(SameButtonColor(checkedEl->refine.bg.color,
                             theme.tokens.accent.color));
    utassert(ButtonChildCount(checkedEl) == 2);

    component::ToggleGroup* group =
        component::ToggleGroup::New(&cx, StrL("source-toggle-group"))
            ->Segmented()
            ->Outline()
            ->WithSize(UiSize::Medium)
            ->OnClick(Listen(&cx, &ToggleGroupHarness::OnChange))
            ->Child(component::Toggle::New(&cx, StrL("left"))->Checked(true))
            ->Child(component::Toggle::New(&cx, StrL("right")));
    El* root = group->IntoEl();
    utassert(ButtonChildCount(root) == 1);
    El* row = root->first;
    utassert(ButtonChildCount(row) == 2);
    utassertnear(row->first->style.corners.tr, 0.f);
    utassertnear(row->first->next->style.corners.tl, 0.f);
    utassertnear(row->first->next->style.borderL, 0.f);

    IdsCollect(root);
    AccessibilityCollect(root, &win->accessibility);
    const AccessibilityNode* second = ButtonAt(win->accessibility, 1);
    utassert(second != nullptr);
    if (second) {
        utassert(WindowAccessibilityPerform(
            win, second->id, AccessibilityAction::Default));
    }
    ToggleGroupHarness* state = harness.Get(&app);
    utassert(state && state->calls == 1 && state->count == 2);
    utassert(state && state->checked[0] && state->checked[1]);

    win->accessibility.Reset();
    WindowKeyedFree(win);
    delete win;
    ArenaDelete(arena);
    EntityDropAll(&app);
    AppGlobalClear(&app);
}

static void ButtonGroupsAssignSourceCornersWithoutAWrapperClip() {
    App app;
    component::Init(&app);
    Window* win = new Window();
    win->app = &app;
    Arena* arena = ArenaNew();
    Ctx cx{&app, win, arena, {}};
    El* root = component::ButtonGroup::New(&cx, StrL("corners"))
                   ->Outline()
                   ->Child(component::Button::New(&cx, StrL("one"))
                               ->Label(StrL("One")))
                   ->Child(component::Button::New(&cx, StrL("two"))
                               ->Label(StrL("Two")))
                   ->IntoEl();
    utassert(!root->style.hasCorners);
    utassert(ButtonChildCount(root) == 2);
    utassertnear(root->first->style.corners.tr, 0.f);
    utassertnear(root->first->style.corners.tl,
                 ThemeNow(&app).radius);
    utassertnear(root->first->next->style.corners.tl, 0.f);
    utassertnear(root->first->next->style.corners.tr,
                 ThemeNow(&app).radius);

    WindowKeyedFree(win);
    delete win;
    ArenaDelete(arena);
    EntityDropAll(&app);
    AppGlobalClear(&app);
}

void TestButtonGroup() {
    TestSuite("button_group");
    SelectionEventsAreOrderedAndNotWordSized();
    SourceButtonVariantsRoundingAndIconsRemainConcrete();
    SourceToggleAndSegmentedGroupKeepStateAndGeometry();
    ButtonGroupsAssignSourceCornersWithoutAWrapperClip();
}
