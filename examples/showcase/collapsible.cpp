#include "Showcase.h"
#include "ui/Button.h"
#include "ui/Collapsible.h"

enum {
    ClickCollapsible = 290
};

static El* RepoRow(Arena* a, const char* name) {
    return Div(a)
        ->W(kFill)
        ->H(28)
        ->PadX(8)
        ->ItemsCenter()
        ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
        ->Child(TextEl(a, Str(name))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17)));
}

El* ShowcaseCollapsible(ShowcaseApp* app, Arena* a) {
    bool open = app->collapsibleOpen;
    return Collapsible::New(a)
        ->Open(open)
        ->Child(Div(a)
                    ->W(256)
                    ->FlexRow()
                    ->ItemsCenter()
                    ->JustifyBetween()
                    ->Child(TextEl(a, StrL("@gpui/base · 3 repositories"))
                                ->Font(12)
                                ->Fg(Rgb(0x17, 0x17, 0x17)))
                    ->Child(Button::New(a, StrL("collapsible-trigger"),
                                        ClickCollapsible)
                                ->W(28)
                                ->H(28)
                                ->ItemsCenter()
                                ->JustifyCenter()
                                ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
                                ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
                                ->Child(TextEl(a, open ? StrL("−") : StrL("+"))
                                            ->Font(12)
                                            ->Fg(Rgb(0x17, 0x17, 0x17)))))
        ->Child(Div(a)->PadT(8)->W(256)->Child(RepoRow(a, "gpui-component")))
        ->Content(Div(a)
                      ->PadT(8)
                      ->W(256)
                      ->FlexCol()
                      ->Gap(8)
                      ->Child(RepoRow(a, "gpui-base"))
                      ->Child(RepoRow(a, "gpui-storybook")))
        ->IntoEl()
        ->W(256);
}

void ShowcaseCollapsibleClick(ShowcaseApp* app, int id) {
    if (id == ClickCollapsible) {
        app->collapsibleOpen = !app->collapsibleOpen;
    }
}

SHOWCASE_PAGE(CompCollapsible, ShowcaseCollapsible, ShowcaseCollapsibleClick);
