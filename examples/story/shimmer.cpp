#include "Story.h"

// crates/story/src/stories/shimmer_story.rs

// section(..).max_w(rems(42.5)) — 680 at the 16px root.
static const float kShimmerSectionMaxW = 680;

struct ShimmerStory {
    int replayCount = 0;

    static void OnReplay(ShimmerStory* self, Ctx* cx, const ClickEvent*) {
        self->replayCount++;
        Notify(cx);
    }

    static El* Render(ShimmerStory* self, Ctx* cx);
};

static El* ShimmerSection(Ctx* cx, const char* title, const char* desc,
                          float gap) {
    El* section = StorySection(cx, title, desc);
    StorySectionBody(section)->FlexCol()->Gap(gap)->MaxW(kShimmerSectionMaxW);
    return section;
}

El* ShimmerStory::Render(ShimmerStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow(cx->app);
    component::ShimmerStyle shared = component::ShimmerStyle::New()
                                         .Duration(3000)
                                         .HighlightColor(th.primary)
                                         .Spread(0.42f);

    El* page = Div(a)->FlexCol()->W(kFill)->Gap(16);

    El* def = ShimmerSection(
        cx, "Default",
        "A readable, theme-aware highlight crosses the existing text.", 8);
    StorySectionAdd(def, component::ShimmerText::New(cx, StrL("Thinking…"))
                             ->IntoEl());
    StorySectionAdd(def, component::ShimmerText::New(
                             cx, StrL("Searching the current project…"))
                             ->Fg(th.mutedFg)
                             ->IntoEl());
    page->Child(def);

    El* color = ShimmerSection(
        cx, "Color", "Highlight colors come from semantic theme roles.", 8);
    StorySectionAdd(color, component::ShimmerText::New(
                               cx, StrL("Automatic theme-aware highlight"))
                               ->IntoEl());
    StorySectionAdd(color,
                    component::ShimmerText::New(cx, StrL("Primary highlight"))
                        ->HighlightColor(th.primary)
                        ->IntoEl());
    StorySectionAdd(color,
                    component::ShimmerText::New(cx, StrL("Success highlight"))
                        ->HighlightColor(th.success)
                        ->IntoEl());
    page->Child(color);

    El* duration = ShimmerSection(
        cx, "Duration", "Each duration controls one complete sweep.", 8);
    StorySectionAdd(duration, component::ShimmerText::New(
                                  cx, StrL("Quick sweep · 1 second"))
                                  ->Duration(1000)
                                  ->IntoEl());
    StorySectionAdd(duration, component::ShimmerText::New(
                                  cx, StrL("Default sweep · 2 seconds"))
                                  ->Duration(2000)
                                  ->IntoEl());
    StorySectionAdd(duration, component::ShimmerText::New(
                                  cx, StrL("Relaxed sweep · 4 seconds"))
                                  ->Duration(4000)
                                  ->IntoEl());
    page->Child(duration);

    El* spread = ShimmerSection(
        cx, "Spread", "Spread is a relative or absolute highlight half-width.",
        8);
    StorySectionAdd(
        spread, component::ShimmerText::New(cx, StrL("Narrow highlight · 0.12"))
                    ->Spread(0.12f)
                    ->IntoEl());
    StorySectionAdd(spread, component::ShimmerText::New(
                                cx, StrL("Default highlight · 0.30"))
                                ->Spread(0.30f)
                                ->IntoEl());
    StorySectionAdd(
        spread, component::ShimmerText::New(cx, StrL("Wide highlight · 0.60"))
                    ->Spread(0.60f)
                    ->IntoEl());
    StorySectionAdd(
        spread, component::ShimmerText::New(cx, StrL("Fixed highlight · 48px"))
                    ->Spread(component::ShimmerSpread::Absolute(48))
                    ->IntoEl());
    page->Child(spread);

    El* direction = ShimmerSection(
        cx, "Direction",
        "Reverse changes movement without replacing text or layout.", 8);
    StorySectionAdd(direction, component::ShimmerText::New(
                                   cx, StrL("Forward · left to right"))
                                   ->IntoEl());
    StorySectionAdd(direction, component::ShimmerText::New(
                                   cx, StrL("Reverse · right to left"))
                                   ->Reverse(true)
                                   ->IntoEl());
    page->Child(direction);

    El* once = ShimmerSection(
        cx, "Play once",
        "A stable explicit identity controls one-shot playback and replay.",
        12);
    StorySectionAdd(once, component::ShimmerText::New(
                              cx, StrL("A single sweep completes and then "
                                       "stops"))
                              ->Id(StoryFmt(cx, "shimmer-single-sweep-%d",
                                            self->replayCount))
                              ->Once(true)
                              ->IntoEl());
    StorySectionAdd(once, component::Button::New(cx, StrL("shimmer-replay"))
                              ->Label(StrL("Replay"))
                              ->OnClick(Listen(cx, &ShimmerStory::OnReplay))
                              ->IntoEl());
    page->Child(once);

    El* reusable = ShimmerSection(
        cx, "Reusable style",
        "One ShimmerStyle can be shared by independent status labels.", 8);
    StorySectionAdd(reusable, component::ShimmerText::New(
                                  cx, StrL("Analyzing source files…"))
                                  ->WithShimmerStyle(shared)
                                  ->IntoEl());
    StorySectionAdd(
        reusable, component::ShimmerText::New(cx, StrL("Preparing a response…"))
                      ->WithShimmerStyle(shared)
                      ->IntoEl());
    page->Child(reusable);

    El* typography = ShimmerSection(
        cx, "Typography and wrapping",
        "Text inherits typography, color, and the surrounding layout.", 12);
    StorySectionAdd(typography, component::ShimmerText::New(
                                    cx, StrL("Compact supporting status"))
                                    ->Fg(th.mutedFg)
                                    ->IntoEl()
                                    ->Font(14));
    StorySectionAdd(typography,
                    component::ShimmerText::New(cx, StrL("Prominent status"))
                        ->IntoEl()
                        ->Font(18)
                        ->Semibold());
    StorySectionAdd(
        typography,
        Div(a)->FlexRow()->ItemsCenter()->W(384)->MaxW(kFill)->MinW(0)->Child(
            component::ShimmerText::New(
                cx, StrL("Long loading messages remain readable as "
                         "the surrounding region becomes narrower."))
                ->IntoEl()));
    page->Child(typography);

    El* marker = ShimmerSection(
        cx, "Marker",
        "Marker applies the same reusable style to its text content.", 0);
    StorySectionAdd(
        marker, component::Marker::New(cx)
                    ->Loading(true)
                    ->WithLoadingStyle(component::MarkerLoadingStyle::Shimmer)
                    ->WithShimmerStyle(shared)
                    ->Content(component::MarkerContent::New(cx)
                                  ->Text(StrL("Generating an answer…")))
                    ->IntoEl());
    page->Child(marker);

    El* attachment = ShimmerSection(
        cx, "Attachment",
        "An uploading or processing title can share its loading style.", 0);
    StorySectionAdd(
        attachment,
        component::Attachment::New(cx)
            ->Status(component::AttachmentStatus::Processing)
            ->Content(component::AttachmentContent::New(cx)
                          ->Title(component::AttachmentTitle::New(
                                      cx, StrL("meeting-transcript.pdf"))
                                      ->WithShimmerStyle(shared))
                          ->Description(component::AttachmentDescription::New(
                              cx, StrL("Extracting key discussion points"))))
            ->IntoEl());
    page->Child(attachment);
    return page;
}

STORY_PAGE(StoryShimmer, ShimmerStory);
