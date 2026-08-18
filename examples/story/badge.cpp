#include "Story.h"

struct BadgeStory {
    StoryToolbarState toolbar;

    static El* Render(BadgeStory* self, Ctx* cx);
};

static El* Face(Ctx* cx, BadgeStory* self, const char* initials) {
    return component::Avatar::New(cx)
        ->Initials(Str(initials))
        ->WithSize(self->toolbar.size)
        ->IntoEl();
}

static El* FaceLarge(Ctx* cx, BadgeStory*, const char* initials) {
    return component::Avatar::New(cx)
        ->Initials(Str(initials))
        ->WithSize(UiSize::Large)
        ->IntoEl();
}

static El* FaceSmall(Ctx* cx, BadgeStory*, const char* initials) {
    return component::Avatar::New(cx)
        ->Initials(Str(initials))
        ->WithSize(UiSize::Small)
        ->IntoEl();
}

El* BadgeStory::Render(BadgeStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* icons = StorySection(cx, "Icon", nullptr);
    El* iconRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    iconRow->Child(
        component::Badge::New(cx)
            ->Count(3)
            ->WithSize(self->toolbar.size)
            ->Child(IconEl(a, IconName::Bell, UiIconPx(self->toolbar.size)))
            ->IntoEl());
    iconRow->Child(
        component::Badge::New(cx)
            ->Count(103)
            ->WithSize(self->toolbar.size)
            ->Child(IconEl(a, IconName::Inbox, UiIconPx(self->toolbar.size)))
            ->IntoEl());
    StorySectionAdd(icons, iconRow);
    page->Child(icons);

    El* counts = StorySection(cx, "Count", nullptr);
    El* countRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    countRow->Child(component::Badge::New(cx)
                        ->Count(3)
                        ->WithSize(self->toolbar.size)
                        ->Child(Face(cx, self, "JL"))
                        ->IntoEl());
    countRow->Child(component::Badge::New(cx)
                        ->Count(103)
                        ->WithSize(self->toolbar.size)
                        ->Child(Face(cx, self, "HU"))
                        ->IntoEl());
    StorySectionAdd(counts, countRow);
    page->Child(counts);

    El* ic = StorySection(cx, "Badge icon", nullptr);
    El* icRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    icRow->Child(component::Badge::New(cx)
                     ->Icon(IconName::Check)
                     ->Color(th.cyan)
                     ->WithSize(self->toolbar.size)
                     ->Child(Face(cx, self, "JL"))
                     ->IntoEl());
    icRow->Child(component::Badge::New(cx)
                     ->Icon(IconName::Star)
                     ->Color(th.yellow)
                     ->WithSize(self->toolbar.size)
                     ->Child(Face(cx, self, "TW"))
                     ->IntoEl());
    StorySectionAdd(ic, icRow);
    page->Child(ic);

    El* dots = StorySection(cx, "Dot", nullptr);
    StorySectionAdd(dots, component::Badge::New(cx)
                              ->Dot()
                              ->Count(1)
                              ->WithSize(self->toolbar.size)
                              ->Child(Face(cx, self, "JL"))
                              ->IntoEl());
    page->Child(dots);

    El* color = StorySection(cx, "Color", nullptr);
    El* colorRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    colorRow->Child(component::Badge::New(cx)
                        ->Count(3)
                        ->Color(th.blue)
                        ->WithSize(self->toolbar.size)
                        ->Child(Face(cx, self, "JL"))
                        ->IntoEl());
    colorRow->Child(component::Badge::New(cx)
                        ->Dot()
                        ->Color(th.green)
                        ->Count(1)
                        ->WithSize(self->toolbar.size)
                        ->Child(Face(cx, self, "JL"))
                        ->IntoEl());
    StorySectionAdd(color, colorRow);
    page->Child(color);

    El* nest = StorySection(cx, "Nested", nullptr);
    El* nestRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    nestRow->Child(component::Badge::New(cx)
                       ->Count(212)
                       ->WithSize(UiSize::Large)
                       ->Child(component::Badge::New(cx)
                                   ->Icon(IconName::Check)
                                   ->Color(th.cyan)
                                   ->WithSize(self->toolbar.size)
                                   ->Child(Face(cx, self, "JL"))
                                   ->IntoEl())
                       ->IntoEl());
    nestRow->Child(component::Badge::New(cx)
                       ->Count(2)
                       ->Color(th.green)
                       ->WithSize(UiSize::Large)
                       ->Child(component::Badge::New(cx)
                                   ->Icon(IconName::Star)
                                   ->Color(th.yellow)
                                   ->WithSize(self->toolbar.size)
                                   ->Child(FaceLarge(cx, self, "TW"))
                                   ->IntoEl())
                       ->IntoEl());
    nestRow->Child(component::Badge::New(cx)
                       ->Count(3)
                       ->Color(th.green)
                       ->WithSize(self->toolbar.size)
                       ->Child(component::Badge::New(cx)
                                   ->Icon(IconName::Asterisk)
                                   ->Color(th.green)
                                   ->WithSize(self->toolbar.size)
                                   ->Child(Face(cx, self, "AB"))
                                   ->IntoEl())
                       ->IntoEl());
    nestRow->Child(component::Badge::New(cx)
                       ->Dot()
                       ->WithSize(self->toolbar.size)
                       ->Child(component::Badge::New(cx)
                                   ->Icon(IconName::Sun)
                                   ->Color(th.red)
                                   ->WithSize(self->toolbar.size)
                                   ->Child(FaceSmall(cx, self, "CD"))
                                   ->IntoEl())
                       ->IntoEl());
    StorySectionAdd(nest, nestRow);
    page->Child(nest);
    return page;
}

STORY_PAGE(StoryBadge, BadgeStory);
