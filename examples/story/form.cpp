#include "Story.h"

struct FormStory {
    LineInput field = {};
    bool switchOn = true;
    StoryToolbarState toolbar;

    bool seeded = false;

    static El* Render(FormStory* self, Ctx* cx);
    static void Click(FormStory* self, Ctx* cx, int id);
};

static void SetFormSwitch(FormStory* self, bool v) {
    self->switchOn = v;
}

El* FormStory::Render(FormStory* self, Ctx* cx) {
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
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, &self->toolbar));
    El* sec =
        StorySection(cx, "Default", "Building forms with labeled fields.");
    StorySectionAdd(
        sec,
        component::Form::New(cx)
            ->Field(StrL("Name"),
                    component::Input::New(cx, StrL("form-name"), &self->field)
                        ->IntoEl())
            ->Field(StrL("Email"),
                    component::Input::New(cx, StrL("form-email"), &self->field)
                        ->IntoEl())
            ->Field(StrL("Notify"), component::Switch::New(cx, StrL("form-sw"))
                                        ->Checked(self->switchOn)
                                        ->OnClick(MkFunc1(&SetFormSwitch, self))
                                        ->IntoEl())
            ->IntoEl());
    page->Child(sec);
    return page;
}

void FormStory::Click(FormStory* self, Ctx* cx, int id) {
    if (StoryToolbarClick(&self->toolbar, id)) {
        return;
    }
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryForm, FormStory);
