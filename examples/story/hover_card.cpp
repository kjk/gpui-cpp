#include "Story.h"

struct HoverCardStory {
    static El* Render(HoverCardStory* self, Ctx* cx);
    static void Click(HoverCardStory* self, Ctx* cx, int id);
};

// The window's hover id doubles as "which card is open" on this page.
static void ToggleCard(HoverCardStory*, Ctx* cx, const ClickEvent*,
                       intptr_t which) {
    cx->win->hoverId = cx->win->hoverId == (int)which ? 0 : (int)which;
    Notify(cx);
}

static El* Card(Ctx* cx, const char* title, const char* body) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
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

El* HoverCardStory::Render(HoverCardStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(
        cx, "Default",
        "Shows supporting information without changing the current view.");
    El* defTrig = StoryTxt(cx, StrL("Hover over me"), 13, th.primary);
    defTrig->OnClick(Listen(cx, &ToggleCard, 1));
    StorySectionAdd(def,
                    component::HoverCard::New(cx)
                        ->Trigger(defTrig)
                        ->Content(cx->win->hoverId == 1
                                      ? Card(cx, "This is a hover card",
                                             "You can display rich content "
                                             "when hovering over a "
                                             "trigger element.")
                                      : nullptr)
                        ->Open(cx->win->hoverId == 1)
                        ->IntoEl());
    page->Child(def);

    El* rich = StorySection(
        cx, "Rich Content",
        "Cards can contain avatars, typography, and structured details.");
    El* richRow = Div(a)->FlexRow()->ItemsCenter()->Gap(4);
    richRow->Child(StoryTxt(cx, StrL("Hover over"), 13, th.foreground));
    El* link = StoryTxt(cx, StrL("@huacnlee"), 13, th.blue);
    link->OnClick(Listen(cx, &ToggleCard, 2));
    El* profile = nullptr;
    if (cx->win->hoverId == 2) {
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
                       ->Open(cx->win->hoverId == 2)
                       ->IntoEl());
    richRow
        ->Child(StoryTxt(cx, StrL("to see their profile"), 13, th.foreground));
    StorySectionAdd(rich, richRow);
    page->Child(rich);

    El* timing = StorySection(
        cx, "Timing",
        "Open and close delays can match the interaction context.");
    StorySectionAdd(timing, component::Button::New(cx, StrL("fast"))
                                ->Label(StrL("Fast Open (200ms)"))
                                ->Outline()
                                ->IntoEl()
                                ->OnClick(Listen(cx, &ToggleCard, 3)));
    if (cx->win->hoverId == 3) {
        StorySectionAdd(timing, Card(cx, "Fast open",
                                     "This hover card opens after 200ms."));
    }
    page->Child(timing);

    El* pos = StorySection(cx, "Position", nullptr);
    StorySectionAdd(pos, component::Button::New(cx, StrL("pos"))
                             ->Label(StrL("Hover for position"))
                             ->Outline()
                             ->IntoEl()
                             ->OnClick(Listen(cx, &ToggleCard, 4)));
    if (cx->win->hoverId == 4) {
        StorySectionAdd(
            pos, Card(cx, "Positioned card", "Shown relative to the trigger."));
    }
    page->Child(pos);
    return page;
}

void HoverCardStory::Click(HoverCardStory* self, Ctx* cx, int id) {
    (void)self;
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryHoverCard, HoverCardStory);
