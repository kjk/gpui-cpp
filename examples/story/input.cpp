#include "Story.h"

enum {
    ClickStoryField = 2600
};

static El* FieldBox(Arena* a, const char* text, const char* prefix,
                    const char* suffix, bool disabled) {
    const Theme& th = ThemeNow();
    El* field = Div(a)
                    ->FlexRow()
                    ->H(28)
                    ->W(280)
                    ->PadX(8)
                    ->Gap(8)
                    ->ItemsCenter()
                    ->Border(1, th.border)
                    ->Radius(th.radius)
                    ->Bg(disabled ? th.muted : th.background);
    if (prefix) {
        field->Child(StoryTxt(a, Str(prefix), 13, th.mutedFg));
    }
    field->Child(
        StoryTxt(a, Str(text), 13, disabled ? th.mutedFg : th.foreground)
            ->Grow());
    if (suffix) {
        field->Child(StoryTxt(a, Str(suffix), 13, th.mutedFg));
    }
    return field;
}

El* InputRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* def =
        StorySection(a, "Default", "Capture and validate short-form text.");
    StorySectionAdd(def, component::Input::New(a, StrL("name"), &app->field)
                             ->Label(StrL("Display name"))
                             ->IntoEl());
    page->Child(def);

    El* states = StorySection(a, "States", nullptr);
    El* stateCol = Div(a)->FlexCol()->Gap(8);
    stateCol->Child(FieldBox(a, "Read-only value", nullptr, nullptr, false));
    stateCol->Child(FieldBox(a, "Disabled value", nullptr, nullptr, true));
    StorySectionAdd(states, stateCol);
    page->Child(states);

    El* align = StorySection(a, "Alignment", nullptr);
    El* alignCol = Div(a)->FlexCol()->Gap(8);
    alignCol->Child(FieldBox(a, "Start aligned", nullptr, nullptr, false));
    alignCol->Child(FieldBox(a, "Center aligned", nullptr, nullptr, false));
    alignCol->Child(FieldBox(a, "End aligned", nullptr, nullptr, false));
    StorySectionAdd(align, alignCol);
    page->Child(align);

    El* affix = StorySection(a, "Prefix and suffix", nullptr);
    El* affixCol = Div(a)->FlexCol()->Gap(8);
    affixCol->Child(FieldBox(a, "gpui-component", "https://", nullptr, false));
    affixCol->Child(FieldBox(a, "42", nullptr, "px", false));
    StorySectionAdd(affix, affixCol);
    page->Child(affix);
    return page;
}

void InputClick(StoryApp* app, int id) {
    if (id == ClickStoryField || id == HashClickId(StrL("name"))) {
        app->field.focused = true;
    }
}

STORY_PAGE(StoryInput, InputRender, InputClick);
