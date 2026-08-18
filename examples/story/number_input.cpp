#include "Story.h"

struct NumberInputStory {
    LineInput field = {};
    bool seeded = false;

    static El* Render(NumberInputStory* self, Ctx* cx);
};

static void IncNum(NumberInputStory* self, Ctx* cx, const ClickEvent*) {
    int v = 0;
    sscanf_s(self->field.buf, "%d", &v);
    v++;
    _snprintf_s(self->field.buf, _TRUNCATE, "%d", v);
    self->field.len = (int)strlen(self->field.buf);
}
static void DecNum(NumberInputStory* self, Ctx* cx, const ClickEvent*) {
    int v = 0;
    sscanf_s(self->field.buf, "%d", &v);
    v--;
    _snprintf_s(self->field.buf, _TRUNCATE, "%d", v);
    self->field.len = (int)strlen(self->field.buf);
}

El* NumberInputStory::Render(NumberInputStory* self, Ctx* cx) {
    Arena* a = cx->a;
    if (!self->seeded) {
        self->seeded = true;
        strncpy_s(self->field.buf, "12", _TRUNCATE);
        self->field.len = (int)strlen(self->field.buf);
    }
    if (self->field.focused) {
        cx->win->input = &self->field;
    }
    if (self->field.len == 0 || self->field.buf[0] < '0' ||
        self->field.buf[0] > '9') {
        strncpy_s(self->field.buf, "12", _TRUNCATE);
        self->field.len = 2;
    }
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        cx, "Default", "Numeric input with increment and decrement controls.");
    StorySectionAdd(sec, component::NumberInput::New(cx, &self->field)
                             ->OnInc(Listen(cx, &IncNum))
                             ->OnDec(Listen(cx, &DecNum))
                             ->IntoEl());
    page->Child(sec);

    El* dis = StorySection(cx, "Disabled", nullptr);
    StorySectionAdd(dis, component::NumberInput::New(cx, &self->field)
                             ->IntoEl());
    page->Child(dis);

    El* suf = StorySection(cx, "Suffix", nullptr);
    El* row = Div(a)->FlexRow()->ItemsCenter()->Gap(8);
    row->Child(component::NumberInput::New(cx, &self->field)
                   ->OnInc(Listen(cx, &IncNum))
                   ->OnDec(Listen(cx, &DecNum))
                   ->IntoEl());
    row->Child(StoryTxt(cx, StrL("px"), 13, ThemeNow().mutedFg));
    StorySectionAdd(suf, row);
    page->Child(suf);
    return page;
}

STORY_PAGE(StoryNumberInput, NumberInputStory);
