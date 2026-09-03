#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

El* ShowcaseProgress(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    return Div(a)
        ->FlexCol()
        ->W(256)
        ->Gap(8)
        ->Child(Div(a)
                    ->FlexRow()
                    ->JustifyBetween()
                    ->W(kFill)
                    ->Child(TextEl(a, StrL("Uploading assets"))
                                ->Font(12)
                                ->Fg(ExampleRgb(0x171717)))
                    ->Child(TextEl(a, StrL("68%"))
                                ->Font(12)
                                ->Fg(ExampleRgb(0x171717))))
        ->Child(Progress::New(cx, StrL("example-progress"), 68)
                    ->AriaLabel(StrL("Uploading assets"))
                    ->Child(ProgressTrack::New(cx)
                                ->W(256)
                                ->H(7)
                                ->Border(1, ExampleRgb(0x171717))
                                ->Child(ProgressIndicator::New(cx)
                                            ->W(177)
                                            ->H(7)
                                            ->Bg(ExampleRgb(0x171717)))))
        ->Child(Div(a)
                    ->FlexRow()
                    ->JustifyBetween()
                    ->W(kFill)
                    ->Child(TextEl(a, StrL("Optimizing bundle"))
                                ->Font(14)
                                ->Fg(ExampleRgb(0x737373)))
                    ->Child(TextEl(a, StrL("32%"))
                                ->Font(14)
                                ->Fg(ExampleRgb(0x737373))))
        ->Child(
            Progress::New(cx, StrL("example-progress-secondary"), 32)
                ->AriaLabel(StrL("Optimizing bundle"))
                ->Child(ProgressTrack::New(cx)
                            ->W(256)
                            ->H(6)
                            ->Border(1, ExampleRgb(0xa3a3a3))
                            ->Child(ProgressIndicator::New(cx)->W(83)->H(6)->Bg(
                                ExampleRgb(0x737373)))));
}

SHOWCASE_PAGE(CompProgress, ShowcaseProgress);
