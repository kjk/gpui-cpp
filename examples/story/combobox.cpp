#include "Story.h"

struct ComboboxStory {
    int selectIx = 0;
    bool comboOpen = false;
    LineInput search = {};

    bool seeded = false;

    static El* Render(ComboboxStory* self, Ctx* cx);
    static void OnKey(ComboboxStory* self, Ctx* cx, const KeyEvent* ev);
};

static void ToggleCombo(ComboboxStory* self, Ctx* cx, const ClickEvent*) {
    self->comboOpen = !self->comboOpen;
}
static void PickCombo(ComboboxStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t i) {
    self->selectIx = i;
    self->comboOpen = false;
}

static El* Combo(Ctx* cx, ComboboxStory* self, const char* id) {
    Arena* a = cx->a;
    const char* opts[] = {"GPUI", "React", "SwiftUI", "Vue"};
    component::Combobox* cb = component::Combobox::New(cx, Str(id))
                                  ->Selected(Str(opts[self->selectIx]))
                                  ->Open(self->comboOpen)
                                  ->Query(&self->search)
                                  ->OnToggle(Listen(cx, &ToggleCombo))
                                  ->OnChange(Listen(cx, &PickCombo));
    cb->Option(StrL("GPUI"))
        ->Option(StrL("React"))
        ->Option(StrL("SwiftUI"))
        ->Option(StrL("Vue"));
    return cb->IntoEl();
}

El* ComboboxStory::Render(ComboboxStory* self, Ctx* cx) {
    Arena* a = cx->a;
    if (!self->seeded) {
        self->seeded = true;
        strncpy_s(self->search.placeholder, "Search frameworks…", _TRUNCATE);
    }
    if (self->search.focused) {
        cx->win->input = &self->search;
    }
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(
        cx, "Default",
        "Autocomplete input and command palette with a list of suggestions.");
    StorySectionAdd(def, Combo(cx, self, "frameworks"));
    page->Child(def);

    El* multi = StorySection(cx, "Multiple", nullptr);
    StorySectionAdd(multi, Combo(cx, self, "multi"));
    page->Child(multi);

    El* groups = StorySection(cx, "Groups", nullptr);
    StorySectionAdd(groups, Combo(cx, self, "groups"));
    page->Child(groups);

    El* icons = StorySection(cx, "Icons", nullptr);
    StorySectionAdd(icons, Combo(cx, self, "icons"));
    page->Child(icons);

    El* footer = StorySection(cx, "Footer", nullptr);
    StorySectionAdd(footer, Combo(cx, self, "footer"));
    page->Child(footer);
    return page;
}

// Esc closes what this page has open, like an overlay dismiss.
void ComboboxStory::OnKey(ComboboxStory* self, Ctx* cx, const KeyEvent* ev) {
    if (ev->vk != VK_ESCAPE) {
        return;
    }
    self->comboOpen = false;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryCombobox, ComboboxStory);
