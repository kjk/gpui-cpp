#include "Story.h"

// badge_story.rs hangs every badge off a github avatar URL. There is no
// socket here, so each is a name and the avatar falls back to its initials.
struct BadgeStory {
    StoryToolbarState toolbar;

    static El* Render(BadgeStory* self, Ctx* cx);
};

static El* Face(Ctx* cx, UiSize size, const char* name) {
    return component::Avatar::New(cx)
        ->Name(Str(name))
        ->WithSize(size)
        ->IntoEl();
}

// section(title).w_128() — or .w(px(480.)) for Dot.
static El* BadgeSection(Ctx* cx, const char* title, float w) {
    El* sec = StorySection(cx, title, nullptr);
    StorySectionBody(sec)->W(w);
    return sec;
}

El* BadgeStory::Render(BadgeStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    UiSize size = self->toolbar.size;
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* icons = BadgeSection(cx, "Icon", 512);
    StorySectionAdd(icons,
                    component::Badge::New(cx)
                        ->WithSize(size)
                        ->Count(3)
                        ->Child(IconEl(a, IconName::Bell, UiIconPx(size)))
                        ->IntoEl());
    StorySectionAdd(icons,
                    component::Badge::New(cx)
                        ->WithSize(size)
                        ->Count(103)
                        ->Child(IconEl(a, IconName::Inbox, UiIconPx(size)))
                        ->IntoEl());
    page->Child(icons);

    El* counts = BadgeSection(cx, "Count", 512);
    StorySectionAdd(counts, component::Badge::New(cx)
                                ->WithSize(size)
                                ->Count(3)
                                ->Child(Face(cx, size, "Jason Lee"))
                                ->IntoEl());
    StorySectionAdd(counts, component::Badge::New(cx)
                                ->WithSize(size)
                                ->Count(103)
                                ->Child(Face(cx, size, "huacnlee"))
                                ->IntoEl());
    page->Child(counts);

    El* ic = BadgeSection(cx, "Badge icon", 512);
    StorySectionAdd(ic, component::Badge::New(cx)
                            ->WithSize(size)
                            ->Icon(IconName::Check)
                            ->Color(th.cyan)
                            ->Child(Face(cx, size, "Jason Lee"))
                            ->IntoEl());
    StorySectionAdd(ic, component::Badge::New(cx)
                            ->WithSize(size)
                            ->Icon(IconName::Star)
                            ->Color(th.yellow)
                            ->Child(Face(cx, size, "Tim Wang"))
                            ->IntoEl());
    page->Child(ic);

    El* dots = BadgeSection(cx, "Dot", 480);
    StorySectionAdd(dots, component::Badge::New(cx)
                              ->WithSize(size)
                              ->Dot()
                              ->Count(1)
                              ->Child(Face(cx, size, "Jason Lee"))
                              ->IntoEl());
    page->Child(dots);

    El* color = BadgeSection(cx, "Color", 512);
    StorySectionAdd(color, component::Badge::New(cx)
                               ->WithSize(size)
                               ->Count(3)
                               ->Color(th.blue)
                               ->Child(Face(cx, size, "Jason Lee"))
                               ->IntoEl());
    StorySectionAdd(color, component::Badge::New(cx)
                               ->WithSize(size)
                               ->Dot()
                               ->Color(th.green)
                               ->Count(1)
                               ->Child(Face(cx, size, "Jason Lee"))
                               ->IntoEl());
    page->Child(color);

    // .large() after .with_size(self.size): the outer chip is Large whatever
    // the toolbar says, on all but the third and fourth.
    El* nest = BadgeSection(cx, "Nested", 512);
    StorySectionAdd(nest, component::Badge::New(cx)
                              ->WithSize(UiSize::Large)
                              ->Count(212)
                              ->Child(component::Badge::New(cx)
                                          ->WithSize(size)
                                          ->Icon(IconName::Check)
                                          ->Color(th.cyan)
                                          ->Child(Face(cx, size, "Jason Lee"))
                                          ->IntoEl())
                              ->IntoEl());
    StorySectionAdd(nest,
                    component::Badge::New(cx)
                        ->WithSize(UiSize::Large)
                        ->Count(2)
                        ->Color(th.green)
                        ->Child(component::Badge::New(cx)
                                    ->WithSize(size)
                                    ->Icon(IconName::Star)
                                    ->Color(th.yellow)
                                    ->Child(Face(cx, UiSize::Large, "Tim Wang"))
                                    ->IntoEl())
                        ->IntoEl());
    StorySectionAdd(nest, component::Badge::New(cx)
                              ->WithSize(size)
                              ->Count(3)
                              ->Color(th.green)
                              ->Child(component::Badge::New(cx)
                                          ->WithSize(size)
                                          ->Icon(IconName::Asterisk)
                                          ->Color(th.green)
                                          ->Child(Face(cx, size, "Alice Brown"))
                                          ->IntoEl())
                              ->IntoEl());
    StorySectionAdd(nest,
                    component::Badge::New(cx)
                        ->WithSize(size)
                        ->Dot()
                        ->Child(component::Badge::New(cx)
                                    ->WithSize(size)
                                    ->Icon(IconName::Sun)
                                    ->Color(th.red)
                                    ->Child(Face(cx, UiSize::Small, "Chen Dan"))
                                    ->IntoEl())
                        ->IntoEl());
    page->Child(nest);
    return page;
}

STORY_PAGE(StoryBadge, BadgeStory);
