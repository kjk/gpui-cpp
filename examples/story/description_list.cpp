#include "Story.h"

struct DescriptionListStory {
    StoryToolbarState toolbar;

    static El* Render(DescriptionListStory* self, Ctx* cx);
};

static El* Para(Ctx* cx, const char* s) {
    return StoryTxt(cx, StoryDup(cx, s), 14, cx->theme().foreground)->Wrap();
}

El* DescriptionListStory::Render(DescriptionListStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill)->ItemsCenter();
    page->Child(StoryToolbar(cx, self));

    // The description value is markdown in Rust: three paragraphs, with the
    // component names in bold and a link to gpui.rs.
    El* desc = Div(a)->FlexCol()->Gap(12)->W(kFill);
    El* first = Div(a)->FlexRow()->FlexWrap()->W(kFill);
    first->Child(StoryTxt(cx,
                          StrL("UI components for building fantastic desktop "
                               "application by using "),
                          14, th.foreground));
    first->Child(StoryTxt(cx, StrL("GPUI"), 14, th.primary)
                     ->BorderB(1, th.primary));
    first->Child(StoryTxt(cx, StrL("."), 14, th.foreground));
    desc->Child(first);
    El* second = Div(a)->FlexRow()->FlexWrap()->W(kFill);
    second->Child(
        StoryTxt(cx, StrL("Contains a lot of useful UI components, such as "),
                 14, th.foreground));
    const char* bolds[] = {"Button", "Input",  "Table",
                           "List",   "Select", "DatePicker"};
    for (int i = 0; i < 6; i++) {
        second
            ->Child(StoryTxt(cx, Str(bolds[i]), 14, th.foreground)->Semibold());
        second->Child(
            StoryTxt(cx, i < 5 ? StrL(", ") : StrL(" ..."), 14, th.foreground));
    }
    desc->Child(second);
    desc->Child(Para(cx,
                     "You can easily create your native desktop "
                     "application by using GPUI Component."));

    El* repo =
        StoryTxt(cx, StrL("https://github.com/longbridge/gpui-component"), 14,
                 th.primary)
            ->BorderB(1, th.primary)
            ->Wrap();

    El* list = component::DescriptionList::New(cx)
                   ->Columns(3)
                   ->WithSize(self->toolbar.size)
                   ->Item(StrL("Name"), StrL("GPUI Component"))
                   ->ItemEl(StrL("Description"), desc, 3)
                   ->Item(StrL("Version"), StrL("0.1.0"))
                   ->Item(StrL("License"), StrL("Apache-2.0"))
                   ->Item(StrL("Author"), StrL("Longbridge"))
                   ->Separator()
                   ->ItemEl(StrL("Repository"), repo, 2)
                   ->Item(StrL("Category"), StrL("UI, Desktop, Framework"))
                   ->Item(StrL("This is a long label for Platform"),
                          StrL("macOS, Windows, Linux"))
                   ->IntoEl();
    page->Child(Div(a)->W(720)->Child(list));
    return page;
}

STORY_PAGE(StoryDescriptionList, DescriptionListStory);
