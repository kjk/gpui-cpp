#include "Story.h"

struct ProgressStory {
    // The story's value starts at 25 and its toolbar steps it.
    float value = 25;
    StoryToolbarState toolbar;

    static El* Render(ProgressStory* self, Ctx* cx);
};

El* ProgressStory::Render(ProgressStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* upload = StorySection(
        cx, "Upload", "Pair progress with a clear label, value, and status.");
    El* card = Div(a)
                   ->FlexCol()
                   ->Gap(12)
                   ->W(400)
                   ->Pad(16)
                   ->Border(1, th.border)
                   ->Radius(th.radius);
    El* head = Div(a)->FlexRow()->W(kFill)->JustifyBetween()->ItemsCenter();
    head->Child(
        StoryTxt(cx, StrL("Uploading design-assets.zip"), 14, th.foreground)
            ->Semibold());
    head->Child(
        StoryTxt(cx, StoryFmt(cx, "%.0f%%", self->value), 13, th.mutedFg));
    card->Child(head);
    card->Child(
        component::Progress::New(cx)->Value(self->value)->W(kFill)->IntoEl());
    El* foot = Div(a)->FlexRow()->W(kFill)->JustifyBetween()->ItemsCenter();
    foot->Child(StoryTxt(cx, StrL("24.8 MB of 96 MB"), 12, th.mutedFg));
    foot->Child(StoryTxt(cx, StrL("About 1 min left"), 12, th.mutedFg));
    card->Child(foot);
    StorySectionAdd(upload, card);
    page->Child(upload);

    El* circ = StorySection(
        cx, "Circular", "Use a compact radial indicator for focused tasks.");
    El* circBox = Div(a)
                      ->FlexRow()
                      ->W(400)
                      ->ItemsCenter()
                      ->Gap(20)
                      ->Pad(16)
                      ->Radius(th.radius)
                      ->Bg(RgbaOpacity(th.muted, 0.4f));
    circBox->Child(component::ProgressCircle::New(cx)
                       ->Value(self->value)
                       ->Size(80)
                       ->IntoEl());
    El* circText = Div(a)->FlexCol()->Gap(4);
    circText->Child(StoryTxt(cx, StrL("Analyzing project"), 14, th.foreground)
                        ->Semibold());
    circText->Child(StoryTxt(cx, StrL("Scanning components and dependencies."),
                             13, th.mutedFg));
    circBox->Child(circText);
    StorySectionAdd(circ, circBox);
    page->Child(circ);
    return page;
}

STORY_PAGE(StoryProgress, ProgressStory);
