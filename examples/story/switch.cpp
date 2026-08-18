#include "Story.h"

struct SwitchStory {
    bool switches[8] = {true, false, true, true, false};
    StoryToolbarState toolbar;

    static El* Render(SwitchStory* self, Ctx* cx);
    static void Click(SwitchStory* self, Ctx* cx, int id);
};

static void SetSw0(SwitchStory* self, Ctx* cx, const ClickEvent*, intptr_t v) {
    self->switches[0] = v;
}
static void SetSw1(SwitchStory* self, Ctx* cx, const ClickEvent*, intptr_t v) {
    self->switches[1] = v;
}
static void SetSw3(SwitchStory* self, Ctx* cx, const ClickEvent*, intptr_t v) {
    self->switches[3] = v;
}
static void SetSw4(SwitchStory* self, Ctx* cx, const ClickEvent*, intptr_t v) {
    self->switches[4] = v;
}

static El* SwitchRow(Ctx* cx, SwitchStory* self, const char* title,
                     const char* desc, const char* id, int slot, Listener on) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* text = Div(a)->FlexCol()->Gap(4)->Grow();
    text->Child(StoryTxt(cx, Str(title), 14, th.foreground)->Semibold());
    text->Child(StoryTxt(cx, Str(desc), 13, th.mutedFg));
    return Div(a)
        ->FlexRow()
        ->W(kFill)
        ->ItemsCenter()
        ->JustifyBetween()
        ->Gap(24)
        ->Pad(16)
        ->Child(text)
        ->Child(component::Switch::New(cx, Str(id))
                    ->Checked(self->switches[slot])
                    ->WithSize(self->toolbar.size)
                    ->OnClick(on)
                    ->IntoEl());
}

El* SwitchStory::Render(SwitchStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, &self->toolbar));

    El* def = StorySection(cx, "Default",
                           "Switches work well in a compact settings list.");
    El* list =
        Div(a)->FlexCol()->W(512)->Border(1, th.border)->Radius(th.radius);
    list->Child(SwitchRow(cx, self, "Product updates",
                          "New features and release notes.", "switch1", 0,
                          Listen(cx, &SetSw0)));
    list->Child(component::Separator::Horizontal(cx)->IntoEl());
    list->Child(SwitchRow(cx, self, "Security alerts",
                          "Important activity on your account.", "switch2", 1,
                          Listen(cx, &SetSw1)));
    StorySectionAdd(def, list);
    page->Child(def);

    El* dis = StorySection(
        cx, "Disabled", "Unavailable switches preserve their current value.");
    El* disRow = Div(a)->FlexRow()->Gap(16)->ItemsCenter()->Wrap();
    disRow->Child(component::Switch::New(cx, StrL("switch3"))
                      ->Checked(self->switches[2])
                      ->Disabled(true)
                      ->WithSize(self->toolbar.size)
                      ->IntoEl());
    disRow->Child(component::Switch::New(cx, StrL("switch3_1"))
                      ->Label(StrL("Airplane mode"))
                      ->Checked(true)
                      ->Disabled(true)
                      ->WithSize(self->toolbar.size)
                      ->IntoEl());
    StorySectionAdd(dis, disRow);
    page->Child(dis);

    El* col = StorySection(cx, "Color",
                           "Semantic colors can reinforce the setting state.");
    El* colRow = Div(a)->FlexRow()->Gap(16)->ItemsCenter()->Wrap();
    colRow->Child(component::Switch::New(cx, StrL("switch4"))
                      ->Label(StrL("Success"))
                      ->Checked(self->switches[3])
                      ->Color(th.success)
                      ->WithSize(self->toolbar.size)
                      ->OnClick(Listen(cx, &SetSw3))
                      ->IntoEl());
    colRow->Child(component::Switch::New(cx, StrL("switch5"))
                      ->Label(StrL("Destructive"))
                      ->Checked(self->switches[4])
                      ->Color(th.danger)
                      ->WithSize(self->toolbar.size)
                      ->OnClick(Listen(cx, &SetSw4))
                      ->IntoEl());
    colRow->Child(component::Switch::New(cx, StrL("switch4_disabled"))
                      ->Label(StrL("Disabled"))
                      ->Checked(true)
                      ->Color(th.success)
                      ->Disabled(true)
                      ->WithSize(self->toolbar.size)
                      ->IntoEl());
    StorySectionAdd(col, colRow);
    page->Child(col);
    return page;
}

void SwitchStory::Click(SwitchStory* self, Ctx* cx, int id) {
    if (StoryToolbarClick(&self->toolbar, id)) {
        return;
    }
    (void)cx;
    (void)id;
}

STORY_PAGE(StorySwitch, SwitchStory);
