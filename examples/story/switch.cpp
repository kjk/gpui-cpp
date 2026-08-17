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

static El* SwitchRow(Ctx* cx, StoryApp* app, const char* title,
                     const char* desc, const char* id, int slot,
                     Func1<bool> on) {
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
                    ->Checked(app->switches[slot])
                    ->WithSize(app->size)
                    ->OnClick(on)
                    ->IntoEl());
}

El* SwitchRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, app));

    El* def = StorySection(cx, "Default",
                           "Switches work well in a compact settings list.");
    El* list =
        Div(a)->FlexCol()->W(512)->Border(1, th.border)->Radius(th.radius);
    list->Child(SwitchRow(cx, app, "Product updates",
                          "New features and release notes.", "switch1", 0,
                          MkFunc1(&SetSw0, app)));
    list->Child(component::Separator::Horizontal(cx)->IntoEl());
    list->Child(SwitchRow(cx, app, "Security alerts",
                          "Important activity on your account.", "switch2", 1,
                          MkFunc1(&SetSw1, app)));
    StorySectionAdd(def, list);
    page->Child(def);

    El* dis = StorySection(
        cx, "Disabled", "Unavailable switches preserve their current value.");
    El* disRow = Div(a)->FlexRow()->Gap(16)->ItemsCenter()->Wrap();
    disRow->Child(component::Switch::New(cx, StrL("switch3"))
                      ->Checked(app->switches[2])
                      ->Disabled(true)
                      ->WithSize(app->size)
                      ->IntoEl());
    disRow->Child(component::Switch::New(cx, StrL("switch3_1"))
                      ->Label(StrL("Airplane mode"))
                      ->Checked(true)
                      ->Disabled(true)
                      ->WithSize(app->size)
                      ->IntoEl());
    StorySectionAdd(dis, disRow);
    page->Child(dis);

    El* col = StorySection(cx, "Color",
                           "Semantic colors can reinforce the setting state.");
    El* colRow = Div(a)->FlexRow()->Gap(16)->ItemsCenter()->Wrap();
    colRow->Child(component::Switch::New(cx, StrL("switch4"))
                      ->Label(StrL("Success"))
                      ->Checked(app->switches[3])
                      ->Color(th.success)
                      ->WithSize(app->size)
                      ->OnClick(MkFunc1(&SetSw3, app))
                      ->IntoEl());
    colRow->Child(component::Switch::New(cx, StrL("switch5"))
                      ->Label(StrL("Destructive"))
                      ->Checked(app->switches[4])
                      ->Color(th.danger)
                      ->WithSize(app->size)
                      ->OnClick(MkFunc1(&SetSw4, app))
                      ->IntoEl());
    colRow->Child(component::Switch::New(cx, StrL("switch4_disabled"))
                      ->Label(StrL("Disabled"))
                      ->Checked(true)
                      ->Color(th.success)
                      ->Disabled(true)
                      ->WithSize(app->size)
                      ->IntoEl());
    StorySectionAdd(col, colRow);
    page->Child(col);
    return page;
}

void SwitchClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySwitch, SwitchRender, SwitchClick);
