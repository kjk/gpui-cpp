#include "Story.h"

struct InputStory {
    LineInput field = {};
    StoryToolbarState toolbar;

    bool seeded = false;

    static El* Render(InputStory* self, Ctx* cx);
};

static void FocusField(InputStory* self, Ctx* cx, const ClickEvent*) {
    self->field.focused = true;
    Notify(cx);
}

enum {
};

static El* FieldBox(Ctx* cx, const char* text, const char* prefix,
                    const char* suffix, bool disabled) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
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
        field->Child(StoryTxt(cx, Str(prefix), 13, th.mutedFg));
    }
    field->Child(
        StoryTxt(cx, Str(text), 13, disabled ? th.mutedFg : th.foreground)
            ->Grow());
    if (suffix) {
        field->Child(StoryTxt(cx, Str(suffix), 13, th.mutedFg));
    }
    return field;
}

El* InputStory::Render(InputStory* self, Ctx* cx) {
    Arena* a = cx->a;
    if (!self->seeded) {
        self->seeded = true;
        strncpy_s(self->field.placeholder, "Type something…", _TRUNCATE);
        strncpy_s(self->field.buf, "Hello GPUI", _TRUNCATE);
        self->field.len = (int)strlen(self->field.buf);
    }
    if (self->field.focused) {
        cx->win->input = &self->field;
    }
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* def =
        StorySection(cx, "Default", "Capture and validate short-form text.");
    StorySectionAdd(def, component::Input::New(cx, StrL("name"), &self->field)
                             ->OnFocus(Listen(cx, &FocusField))
                             ->Label(StrL("Display name"))
                             ->IntoEl());
    page->Child(def);

    El* states = StorySection(cx, "States", nullptr);
    El* stateCol = Div(a)->FlexCol()->Gap(8);
    stateCol->Child(FieldBox(cx, "Read-only value", nullptr, nullptr, false));
    stateCol->Child(FieldBox(cx, "Disabled value", nullptr, nullptr, true));
    StorySectionAdd(states, stateCol);
    page->Child(states);

    El* align = StorySection(cx, "Alignment", nullptr);
    El* alignCol = Div(a)->FlexCol()->Gap(8);
    alignCol->Child(FieldBox(cx, "Start aligned", nullptr, nullptr, false));
    alignCol->Child(FieldBox(cx, "Center aligned", nullptr, nullptr, false));
    alignCol->Child(FieldBox(cx, "End aligned", nullptr, nullptr, false));
    StorySectionAdd(align, alignCol);
    page->Child(align);

    El* affix = StorySection(cx, "Prefix and suffix", nullptr);
    El* affixCol = Div(a)->FlexCol()->Gap(8);
    affixCol->Child(FieldBox(cx, "gpui-component", "https://", nullptr, false));
    affixCol->Child(FieldBox(cx, "42", nullptr, "px", false));
    StorySectionAdd(affix, affixCol);
    page->Child(affix);
    return page;
}

STORY_PAGE(StoryInput, InputStory);
