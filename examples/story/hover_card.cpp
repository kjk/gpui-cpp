#include "Story.h"

struct HoverCardStory {
    static El* Render(HoverCardStory* self, Ctx* cx);
};

// A card is open while its trigger is hovered, which is what a HoverCard is
// for. The window already tracks the hovered element, so the page reads that
// rather than keeping a register of its own.
static bool Hovered(Ctx* cx, Str id) {
    return cx->win->hoverId == HashClickId(id);
}

// Gives a trigger a click id, so the runtime can report it as hovered. The
// card opens on hover alone; there is nothing to click.
static El* Trig(El* e, Str id) {
    int cid = HashClickId(id);
    return e->Id(id)->Click(cid);
}

static El* Card(Ctx* cx, const char* title, const char* body) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* card = Div(a)
                   ->W(280)
                   ->Pad(12)
                   ->FlexCol()
                   ->Gap(4)
                   ->Border(1, th.border)
                   ->Bg(th.background)
                   ->Radius(th.radius);
    card->Child(StoryTxt(cx, Str(title), 14, th.foreground)->Semibold());
    card->Child(StoryTxt(cx, Str(body), 12, th.mutedFg)->Wrap()->MaxW(260));
    return card;
}

// The Position cards carry no heading in Rust, just the one line.
static El* PlainCard(Ctx* cx, Str body) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)
        ->Pad(12)
        ->Border(1, th.border)
        ->Bg(th.background)
        ->Radius(th.radius)
        ->Child(StoryTxt(cx, body, 13, th.foreground));
}

El* HoverCardStory::Render(HoverCardStory*, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(
        cx, "Default",
        "Shows supporting information without changing the current view.");
    bool defOpen = Hovered(cx, StrL("hc-default"));
    El* defTrig = Trig(StoryTxt(cx, StrL("Hover over me"), 13, th.primary),
                       StrL("hc-default"));
    StorySectionAdd(def,
                    component::HoverCard::New(cx, StrL("hc-default-card"))
                        ->Trigger(defTrig)
                        ->Content(defOpen ? Card(cx, "This is a hover card",
                                                 "You can display rich content "
                                                 "when hovering over a "
                                                 "trigger element.")
                                          : nullptr)
                        ->Open(defOpen)
                        ->IntoEl());
    page->Child(def);

    El* rich = StorySection(
        cx, "Rich Content",
        "Cards can contain avatars, typography, and structured details.");
    El* richRow = Div(a)->FlexRow()->ItemsCenter()->Gap(4);
    richRow->Child(StoryTxt(cx, StrL("Hover over"), 16, th.foreground));
    bool richOpen = Hovered(cx, StrL("hc-rich"));
    El* link = Trig(StoryTxt(cx, StrL("@huacnlee"), 16, th.blue)->Underline(),
                    StrL("hc-rich"));
    El* profile = nullptr;
    if (richOpen) {
        profile = Div(a)
                      ->FlexRow()
                      ->Gap(12)
                      ->W(280)
                      ->Pad(12)
                      ->Border(1, th.border)
                      ->Bg(th.background)
                      ->Radius(th.radius);
        profile->Child(component::Avatar::New(cx)
                           ->Initials(StrL("JL"))
                           ->Size(40)
                           ->IntoEl());
        El* info = Div(a)->FlexCol()->Gap(2);
        info->Child(StoryTxt(cx, StrL("Jason Lee"), 14, th.foreground)
                        ->Semibold());
        info->Child(StoryTxt(cx, StrL("@huacnlee"), 13, th.blue));
        info->Child(StoryTxt(cx, StrL("The author of GPUI Component."), 12,
                             th.mutedFg));
        profile->Child(info);
    }
    richRow->Child(component::HoverCard::New(cx)
                       ->Trigger(link)
                       ->Content(profile)
                       ->Open(richOpen)
                       ->IntoEl());
    richRow
        ->Child(StoryTxt(cx, StrL("to see their profile"), 16, th.foreground));
    StorySectionAdd(rich, richRow);
    page->Child(rich);

    El* timing = StorySection(
        cx, "Timing",
        "Open and close delays can match the interaction context.");
    El* timingRow = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    timingRow->Child(component::HoverCard::New(cx)
                         ->Trigger(component::Button::New(cx, StrL("fast"))
                                       ->Label(StrL("Fast Open (200ms)"))
                                       ->Outline()
                                       ->IntoEl())
                         ->Content(Hovered(cx, StrL("fast"))
                                       ? Card(cx, "Fast open",
                                              "This hover card opens after "
                                              "200ms")
                                       : nullptr)
                         ->Open(Hovered(cx, StrL("fast")))
                         ->IntoEl());
    timingRow->Child(component::HoverCard::New(cx)
                         ->Trigger(component::Button::New(cx, StrL("slow"))
                                       ->Label(StrL("Slow Open (1000ms)"))
                                       ->Outline()
                                       ->IntoEl())
                         ->Content(Hovered(cx, StrL("slow"))
                                       ? Card(cx, "Slow open",
                                              "This hover card opens after "
                                              "1000ms")
                                       : nullptr)
                         ->Open(Hovered(cx, StrL("slow")))
                         ->IntoEl());
    StorySectionAdd(timing, timingRow);
    page->Child(timing);

    El* pos = StorySection(cx, "Position",
                           "Content can anchor to each side of its trigger.");
    El* posCol = Div(a)->FlexCol()->Gap(16)->ItemsCenter()->JustifyCenter();
    struct AnchorBtn {
        const char* id;
        const char* label;
        component::HoverCardAnchor anchor;
    };
    static const AnchorBtn kAnchors[2][3] = {
        {{"tl", "Top Left", component::HoverCardAnchor::TopLeft},
         {"tc", "Top Center", component::HoverCardAnchor::TopCenter},
         {"tr", "Top Right", component::HoverCardAnchor::TopRight}},
        {{"bl", "Bottom Left", component::HoverCardAnchor::BottomLeft},
         {"bc", "Bottom Center", component::HoverCardAnchor::BottomCenter},
         {"br", "Bottom Right", component::HoverCardAnchor::BottomRight}},
    };
    for (int r = 0; r < 2; r++) {
        El* row = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
        for (int i = 0; i < 3; i++) {
            Str id = Str(kAnchors[r][i].id);
            bool on = Hovered(cx, id);
            row->Child(
                component::HoverCard::New(cx, id)
                    ->Anchor(kAnchors[r][i].anchor)
                    ->Trigger(component::Button::New(cx, id)
                                  ->Label(Str(kAnchors[r][i].label))
                                  ->Outline()
                                  ->IntoEl())
                    ->Content(
                        on ? PlainCard(cx, StoryFmt(cx, "Positioned at %s",
                                                    kAnchors[r][i].label))
                           : nullptr)
                    ->Open(on)
                    ->IntoEl());
        }
        posCol->Child(row);
    }
    StorySectionAdd(pos, posCol);
    page->Child(pos);
    return page;
}

STORY_PAGE(StoryHoverCard, HoverCardStory);
