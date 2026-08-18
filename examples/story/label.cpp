#include "Story.h"

struct LabelStory {
    bool labelMasked = false;
    static El* Render(LabelStory* self, Ctx* cx);
    static void Click(LabelStory* self, Ctx* cx, int id);
};

enum {
    ClickLabelMask = 2500
};

El* LabelStory::Render(LabelStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def =
        StorySection(cx, "Default",
                     "Present primary text with optional supporting context.");
    El* defCol = Div(a)->FlexCol()->Gap(16)->W(320);
    defCol->Child(component::Label::New(cx, StrL("Account details"))->IntoEl());
    defCol->Child(component::Label::New(cx, StrL("Company address"))
                      ->Secondary(StrL("Optional"))
                      ->IntoEl());
    defCol->Child(component::Label::New(cx, StrL("Workspace owner"))
                      ->Secondary(StrL("Administrator"))
                      ->Semibold()
                      ->IntoEl());
    StorySectionAdd(def, defCol);
    page->Child(def);

    El* hi = StorySection(cx, "Highlighting",
                          "Find matching text across Latin and CJK content.");
    El* hiBox = Div(a)
                    ->FlexCol()
                    ->Gap(12)
                    ->W(320)
                    ->Pad(16)
                    ->Border(1, th.border)
                    ->Radius(th.radius);
    hiBox->Child(component::Label::New(cx, StrL("Design system documentation"))
                     ->IntoEl());
    hiBox->Child(component::Label::New(cx, StrL("AAA中文BB"))->IntoEl());
    StorySectionAdd(hi, hiBox);
    page->Child(hi);

    El* lay = StorySection(cx, "Layout",
                           "Labels support alignment and natural wrapping.");
    El* layCol = Div(a)->FlexCol()->Gap(16)->W(320);
    El* align =
        Div(a)->FlexCol()->Gap(8)->Pad(16)->W(kFill)->Radius(th.radius)->Bg(
            RgbaOpacity(th.muted, 0.4f));
    align->Child(StoryTxt(cx, StrL("Start aligned"), 14, th.foreground));
    align->Child(Div(a)->W(kFill)->ItemsCenter()->JustifyCenter()->Child(
        StoryTxt(cx, StrL("Center aligned"), 14, th.foreground)));
    align->Child(Div(a)->W(kFill)->JustifyEnd()->Child(
        StoryTxt(cx, StrL("End aligned"), 14, th.foreground)));
    layCol->Child(align);
    layCol->Child(
        StoryTxt(cx,
                 StrL("Long labels wrap cleanly inside constrained layouts."),
                 14, th.foreground)
            ->Wrap()
            ->MaxW(220));
    StorySectionAdd(lay, layCol);
    page->Child(lay);

    El* mask = StorySection(cx, "Masked",
                            "Reveal or conceal sensitive values in place.");
    El* maskRow = Div(a)
                      ->FlexRow()
                      ->W(320)
                      ->ItemsCenter()
                      ->JustifyBetween()
                      ->Pad(16)
                      ->Border(1, th.border)
                      ->Radius(th.radius);
    El* bal = Div(a)->FlexCol()->Gap(4);
    bal->Child(StoryTxt(cx, StrL("Available balance"), 12, th.mutedFg));
    bal->Child(component::Label::New(cx, StrL("$9,182.10"))
                   ->Masked(self->labelMasked)
                   ->Semibold()
                   ->Font(24)
                   ->IntoEl());
    maskRow->Child(bal);
    maskRow->Child(component::Button::New(cx, StrL("btn-mask"))
                       ->Ghost()
                       ->Icon(self->labelMasked ? IconName::Eye : IconName::Eye)
                       ->IntoEl()
                       ->Click(ClickLabelMask));
    StorySectionAdd(mask, maskRow);
    page->Child(mask);
    return page;
}

void LabelStory::Click(LabelStory* self, Ctx* cx, int id) {
    (void)cx;
    if (id == ClickLabelMask) {
        self->labelMasked = !self->labelMasked;
    }
}

STORY_PAGE(StoryLabel, LabelStory);
