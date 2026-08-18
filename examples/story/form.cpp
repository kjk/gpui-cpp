#include "Story.h"

// Options rows this page adds to the toolbar.
enum {
    FormOptHorizontal = ToolbarOptHorizontal,
    FormOptColumns = ToolbarOptColumns,
};

struct FormStory {
    LineInput name = {};
    LineInput email = {};
    bool subscribe = false;
    bool futureEvents = false;
    bool horizontal = false;
    bool twoColumns = false;
    StoryToolbarState toolbar;
    bool seeded = false;

    static El* Render(FormStory* self, Ctx* cx);
};

static void FormToolbarAct(FormStory* self, Ctx* cx, const ClickEvent*,
                           intptr_t act) {
    if (act == FormOptHorizontal) {
        self->horizontal = !self->horizontal;
    } else if (act == FormOptColumns) {
        self->twoColumns = !self->twoColumns;
    } else {
        StoryToolbarApply(&self->toolbar, nullptr, (int)act);
    }
    Notify(cx);
}

static void FocusName(FormStory* self, Ctx* cx, const ClickEvent*) {
    self->name.focused = true;
    self->email.focused = false;
    Notify(cx);
}
static void FocusEmail(FormStory* self, Ctx* cx, const ClickEvent*) {
    self->email.focused = true;
    self->name.focused = false;
    Notify(cx);
}
static void ToggleSubscribe(FormStory* self, Ctx* cx, const ClickEvent*) {
    self->subscribe = !self->subscribe;
    Notify(cx);
}
static void ToggleFuture(FormStory* self, Ctx* cx, const ClickEvent*) {
    self->futureEvents = !self->futureEvents;
    Notify(cx);
}

El* FormStory::Render(FormStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        strncpy_s(self->name.buf, "Jason Lee", _TRUNCATE);
        self->name.len = (int)strlen(self->name.buf);
        strncpy_s(self->email.placeholder, "Enter text here...", _TRUNCATE);
    }
    if (self->name.focused) {
        cx->win->input = &self->name;
    } else if (self->email.focused) {
        cx->win->input = &self->email;
    }

    El* page = Div(a)->FlexCol()->Gap(12)->Pad(16)->W(kFill);
    StoryToolbarOpt opts[2] = {
        {"Horizontal", self->horizontal, FormOptHorizontal},
        {"Multiple columns", self->twoColumns, FormOptColumns},
    };
    page->Child(
        StoryToolbarOptions(cx, self, opts, 2, Listen(cx, &FormToolbarAct)));
    page->Child(component::Separator::Horizontal(cx)->IntoEl());

    // Name carries a title select inside the input, like Rust's
    // `Input::prefix(Select::appearance(false))`.
    El* prefix =
        Div(a)
            ->FlexRow()
            ->W(90)
            ->PadL(10)
            ->Gap(4)
            ->ItemsCenter()
            ->JustifyBetween()
            ->Child(StoryTxt(cx, StrL("Mr."), 14, th.foreground))
            ->Child(IconEl(a, IconName::ChevronDown, 14)->Fg(th.mutedFg));

    component::Form* form =
        component::Form::New(cx)
            ->Horizontal(self->horizontal)
            ->Columns(self->twoColumns ? 2 : 1)
            ->Field(StrL("Name"),
                    component::Input::New(cx, StrL("form-name"), &self->name)
                        ->Prefix(prefix)
                        ->OnFocus(Listen(cx, &FocusName))
                        ->IntoEl())
            ->Field(StrL("Email"),
                    component::Input::New(cx, StrL("form-email"), &self->email)
                        ->OnFocus(Listen(cx, &FocusEmail))
                        ->IntoEl())
            ->Required()
            ->Field(StrL("Bio"),
                    component::Textarea::New(
                        cx, StrL("form-bio"),
                        "Hello \xe4\xb8\x96\xe7\x95\x8c\xef\xbc\x8cthis is "
                        "GPUI component.")
                        ->Rows(5)
                        ->IntoEl())
            ->Description(StrL("Use at most 100 words to describe yourself."))
            ->Field(Str{}, StoryTxt(cx,
                                    StrL("This is a full width form "
                                         "field."),
                                    14, th.foreground))
            ->SpanAll()
            ->Field(StrL("Please select your birthday"),
                    component::DatePicker::New(cx)
                        ->Day(0)
                        ->Placeholder(StrL("Select date"))
                        ->IntoEl())
            ->Description(
                StrL("Select your birthday, we will send you a gift."))
            ->Field(Str{}, component::Switch::New(cx, StrL("subscribe"))
                               ->Label(StrL("Subscribe our newsletter"))
                               ->Checked(self->subscribe)
                               ->OnClick(Listen(cx, &ToggleSubscribe))
                               ->IntoEl())
            ->Field(Str{}, component::ColorPicker::New(cx)
                               ->WithSize(UiSize::Small)
                               ->Label(StrL("Theme color"))
                               ->IntoEl())
            ->Field(Str{}, component::Checkbox::New(cx, StrL("future-events"))
                               ->Label(StrL("Use this color for future "
                                            "events"))
                               ->Checked(self->futureEvents)
                               ->OnClick(Listen(cx, &ToggleFuture))
                               ->IntoEl());
    page->Child(form->IntoEl());
    return page;
}

STORY_PAGE(StoryForm, FormStory);
