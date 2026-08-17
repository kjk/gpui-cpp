#include "Story.h"

static El* Face(Arena* a, StoryApp* app, const char* initials) {
    return component::Avatar::New(a)
        ->Initials(Str(initials))
        ->WithSize(app->size)
        ->IntoEl();
}

static El* FaceLarge(Arena* a, StoryApp* app, const char* initials) {
    return component::Avatar::New(a)
        ->Initials(Str(initials))
        ->WithSize(UiSize::Large)
        ->IntoEl();
}

static El* FaceSmall(Arena* a, StoryApp* app, const char* initials) {
    (void)app;
    return component::Avatar::New(a)
        ->Initials(Str(initials))
        ->WithSize(UiSize::Small)
        ->IntoEl();
}

El* BadgeRender(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* icons = StorySection(a, "Icon", nullptr);
    El* iconRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    iconRow->Child(component::Badge::New(a)
                       ->Count(3)
                       ->WithSize(app->size)
                       ->Child(IconEl(a, IconName::Bell, UiSizePx(app->size)))
                       ->IntoEl());
    iconRow->Child(component::Badge::New(a)
                       ->Count(103)
                       ->WithSize(app->size)
                       ->Child(IconEl(a, IconName::Inbox, UiSizePx(app->size)))
                       ->IntoEl());
    StorySectionAdd(icons, iconRow);
    page->Child(icons);

    El* counts = StorySection(a, "Count", nullptr);
    El* countRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    countRow->Child(component::Badge::New(a)
                        ->Count(3)
                        ->WithSize(app->size)
                        ->Child(Face(a, app, "JL"))
                        ->IntoEl());
    countRow->Child(component::Badge::New(a)
                        ->Count(103)
                        ->WithSize(app->size)
                        ->Child(Face(a, app, "HU"))
                        ->IntoEl());
    StorySectionAdd(counts, countRow);
    page->Child(counts);

    El* ic = StorySection(a, "Badge icon", nullptr);
    El* icRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    icRow->Child(component::Badge::New(a)
                     ->Icon(IconName::Check)
                     ->Color(Rgb(0x22, 0xd3, 0xee))
                     ->WithSize(app->size)
                     ->Child(Face(a, app, "JL"))
                     ->IntoEl());
    icRow->Child(component::Badge::New(a)
                     ->Icon(IconName::Star)
                     ->Color(th.yellow)
                     ->WithSize(app->size)
                     ->Child(Face(a, app, "TW"))
                     ->IntoEl());
    StorySectionAdd(ic, icRow);
    page->Child(ic);

    El* dots = StorySection(a, "Dot", nullptr);
    StorySectionAdd(dots, component::Badge::New(a)
                              ->Dot()
                              ->Count(1)
                              ->WithSize(app->size)
                              ->Child(Face(a, app, "JL"))
                              ->IntoEl());
    page->Child(dots);

    El* color = StorySection(a, "Color", nullptr);
    El* colorRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    colorRow->Child(component::Badge::New(a)
                        ->Count(3)
                        ->Color(th.blue)
                        ->WithSize(app->size)
                        ->Child(Face(a, app, "JL"))
                        ->IntoEl());
    colorRow->Child(component::Badge::New(a)
                        ->Dot()
                        ->Color(th.green)
                        ->Count(1)
                        ->WithSize(app->size)
                        ->Child(Face(a, app, "JL"))
                        ->IntoEl());
    StorySectionAdd(color, colorRow);
    page->Child(color);

    El* nest = StorySection(a, "Nested", nullptr);
    El* nestRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    nestRow->Child(component::Badge::New(a)
                       ->Count(212)
                       ->WithSize(UiSize::Large)
                       ->Child(component::Badge::New(a)
                                   ->Icon(IconName::Check)
                                   ->Color(Rgb(0x22, 0xd3, 0xee))
                                   ->WithSize(app->size)
                                   ->Child(Face(a, app, "JL"))
                                   ->IntoEl())
                       ->IntoEl());
    nestRow->Child(component::Badge::New(a)
                       ->Count(2)
                       ->Color(th.green)
                       ->WithSize(UiSize::Large)
                       ->Child(component::Badge::New(a)
                                   ->Icon(IconName::Star)
                                   ->Color(th.yellow)
                                   ->WithSize(app->size)
                                   ->Child(FaceLarge(a, app, "TW"))
                                   ->IntoEl())
                       ->IntoEl());
    nestRow->Child(component::Badge::New(a)
                       ->Count(3)
                       ->Color(th.green)
                       ->WithSize(app->size)
                       ->Child(component::Badge::New(a)
                                   ->Icon(IconName::Asterisk)
                                   ->Color(th.green)
                                   ->WithSize(app->size)
                                   ->Child(Face(a, app, "AB"))
                                   ->IntoEl())
                       ->IntoEl());
    nestRow->Child(component::Badge::New(a)
                       ->Dot()
                       ->WithSize(app->size)
                       ->Child(component::Badge::New(a)
                                   ->Icon(IconName::Sun)
                                   ->Color(th.red)
                                   ->WithSize(app->size)
                                   ->Child(FaceSmall(a, app, "CD"))
                                   ->IntoEl())
                       ->IntoEl());
    StorySectionAdd(nest, nestRow);
    page->Child(nest);
    return page;
}

void BadgeClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryBadge, BadgeRender, BadgeClick);
