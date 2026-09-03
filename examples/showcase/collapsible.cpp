#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void OnCollapsible(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    (void)app;
    app->collapsibleOpen = !app->collapsibleOpen;
    Notify(cx);
}

static El* RepoRow(Ctx* cx, const char* name) {
    Arena* a = cx->a;
    return Div(a)
        ->W(kFill)
        ->H(28)
        ->PadX(8)
        ->ItemsCenter()
        ->Border(1, ExampleRgb(0xd4d4d4))
        ->Child(TextEl(a, Str(name))->Font(12)->Fg(ExampleRgb(0x171717)));
}

El* ShowcaseCollapsible(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    bool open = app->collapsibleOpen;
    return Collapsible::New(cx)
        ->FlexCol()
        ->Open(open)
        ->Child(Div(a)
                    ->W(256)
                    ->FlexRow()
                    ->ItemsCenter()
                    ->JustifyBetween()
                    ->Child(TextEl(a, StrL("@gpui/base · 3 repositories"))
                                ->Font(12)
                                ->Fg(ExampleRgb(0x171717)))
                    ->Child(Button::New(cx, StrL("collapsible-trigger"), false,
                                        Listen(cx, &OnCollapsible))
                                ->W(28)
                                ->H(28)
                                ->ItemsCenter()
                                ->JustifyCenter()
                                ->Border(1, ExampleRgb(0xd4d4d4))
                                ->HoverBg(ExampleRgb(0xf5f5f5))
                                ->Child(TextEl(a, open ? StrL("−") : StrL("+"))
                                            ->Font(12)
                                            ->Fg(ExampleRgb(0x171717)))))
        ->Child(Div(a)->PadT(8)->W(256)->Child(RepoRow(cx, "gpui-component")))
        ->Content(Div(a)
                      ->PadT(8)
                      ->W(256)
                      ->FlexCol()
                      ->Gap(8)
                      ->Child(RepoRow(cx, "gpui-base"))
                      ->Child(RepoRow(cx, "gpui-storybook")))
        ->IntoEl()
        ->W(256);
}

SHOWCASE_PAGE(CompCollapsible, ShowcaseCollapsible);
