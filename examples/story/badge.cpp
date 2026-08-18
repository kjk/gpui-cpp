#include "Story.h"

struct BadgeStory {
    StoryToolbarState toolbar;

    static El* Render(BadgeStory* self, Ctx* cx);
    static void Click(BadgeStory* self, Ctx* cx, int id);
};

static El* Face(Ctx* cx, BadgeStory* self, const char* initials) {
    Arena* a = cx->a;
    return component::Avatar::New(cx)
        ->Initials(Str(initials))
        ->WithSize(self->toolbar.size)
        ->IntoEl();
}

static El* FaceLarge(Ctx* cx, BadgeStory* self, const char* initials) {
    Arena* a = cx->a;
    return component::Avatar::New(cx)
        ->Initials(Str(initials))
        ->WithSize(UiSize::Large)
        ->IntoEl();
}

static El* FaceSmall(Ctx* cx, BadgeStory* self, const char* initials) {
    Arena* a = cx->a;
    return component::Avatar::New(cx)
        ->Initials(Str(initials))
        ->WithSize(UiSize::Small)
        ->IntoEl();
}

El* BadgeStory::Render(BadgeStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, &self->toolbar));

    El* icons = StorySection(cx, "Icon", nullptr);
    El* iconRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    iconRow->Child(
        component::Badge::New(cx)
            ->Count(3)
            ->WithSize(self->toolbar.size)
            ->Child(IconEl(a, IconName::Bell, UiSizePx(self->toolbar.size)))
            ->IntoEl());
    iconRow->Child(
        component::Badge::New(cx)
            ->Count(103)
            ->WithSize(self->toolbar.size)
            ->Child(IconEl(a, IconName::Inbox, UiSizePx(self->toolbar.size)))
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
                     ->Color(Rgb(0x22, 0xd3, 0xee))
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
                                   ->Color(Rgb(0x22, 0xd3, 0xee))
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

void BadgeStory::Click(BadgeStory* self, Ctx* cx, int id) {
    if (StoryToolbarClick(&self->toolbar, id)) {
        return;
    }
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryBadge, BadgeStory);
