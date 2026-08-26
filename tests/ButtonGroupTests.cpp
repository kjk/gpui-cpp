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

void TestButtonGroup() {
    TestSuite("button_group");
    SelectionEventsAreOrderedAndNotWordSized();
}
