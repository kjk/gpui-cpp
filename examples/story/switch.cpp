#include "Story.h"

static void SetSw0(StoryApp* app, bool v) {
    app->switches[0] = v;
}
static void SetSw1(StoryApp* app, bool v) {
    app->switches[1] = v;
}
static void SetSw3(StoryApp* app, bool v) {
    app->switches[3] = v;
}
static void SetSw4(StoryApp* app, bool v) {
    app->switches[4] = v;
}

static El* SwitchRow(Arena* a, StoryApp* app, const char* title,
                     const char* desc, const char* id, int slot,
                     Func1<bool> on) {
    const Theme& th = ThemeNow();
    El* text = Div(a)->FlexCol()->Gap(4)->Grow();
    text->Child(StoryTxt(a, Str(title), 14, th.foreground)->Semibold());
    text->Child(StoryTxt(a, Str(desc), 13, th.mutedFg));
    return Div(a)
        ->FlexRow()
        ->W(kFill)
        ->ItemsCenter()
        ->JustifyBetween()
        ->Gap(24)
        ->Pad(16)
        ->Child(text)
        ->Child(component::Switch::New(a, Str(id))
                    ->Checked(app->switches[slot])
                    ->WithSize(app->size)
                    ->OnClick(on)
                    ->IntoEl());
}

El* SwitchRender(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* def = StorySection(a, "Default",
                           "Switches work well in a compact settings list.");
    El* list =
        Div(a)->FlexCol()->W(kFill)->Border(1, th.border)->Radius(th.radius);
    list->Child(SwitchRow(a, app, "Product updates",
                          "New features and release notes.", "switch1", 0,
                          MkFunc1(&SetSw0, app)));
    list->Child(component::Separator::Horizontal(a)->IntoEl());
    list->Child(SwitchRow(a, app, "Security alerts",
                          "Important activity on your account.", "switch2", 1,
                          MkFunc1(&SetSw1, app)));
    StorySectionAdd(def, list);
    page->Child(def);

    El* dis = StorySection(
        a, "Disabled", "Unavailable switches preserve their current value.");
    El* disCol = Div(a)->FlexCol()->Gap(12);
    disCol->Child(component::Switch::New(a, StrL("switch3"))
                      ->Checked(app->switches[2])
                      ->Disabled(true)
                      ->WithSize(app->size)
                      ->IntoEl());
    disCol->Child(component::Switch::New(a, StrL("switch3_1"))
                      ->Label(StrL("Airplane mode"))
                      ->Checked(true)
                      ->Disabled(true)
                      ->WithSize(app->size)
                      ->IntoEl());
    StorySectionAdd(dis, disCol);
    page->Child(dis);

    El* col = StorySection(a, "Color",
                           "Semantic colors can reinforce the setting state.");
    El* colCol = Div(a)->FlexCol()->Gap(12);
    colCol->Child(component::Switch::New(a, StrL("switch4"))
                      ->Label(StrL("Success"))
                      ->Checked(app->switches[3])
                      ->Color(th.success)
                      ->WithSize(app->size)
                      ->OnClick(MkFunc1(&SetSw3, app))
                      ->IntoEl());
    colCol->Child(component::Switch::New(a, StrL("switch5"))
                      ->Label(StrL("Destructive"))
                      ->Checked(app->switches[4])
                      ->Color(th.danger)
                      ->WithSize(app->size)
                      ->OnClick(MkFunc1(&SetSw4, app))
                      ->IntoEl());
    colCol->Child(component::Switch::New(a, StrL("switch4_disabled"))
                      ->Label(StrL("Disabled"))
                      ->Checked(true)
                      ->Color(th.success)
                      ->Disabled(true)
                      ->WithSize(app->size)
                      ->IntoEl());
    StorySectionAdd(col, colCol);
    page->Child(col);
    return page;
}

void SwitchClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySwitch, SwitchRender, SwitchClick);
