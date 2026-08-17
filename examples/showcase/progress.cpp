#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

El* ShowcaseProgress(ShowcaseApp* app, Arena* a) {
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
                                ->Fg(Rgb(0x17, 0x17, 0x17)))
                    ->Child(TextEl(a, StrL("68%"))
                                ->Font(12)
                                ->Fg(Rgb(0x17, 0x17, 0x17))))
        ->Child(
            Progress::New(a, StrL("example-progress"))
                ->Child(ProgressTrack::New(a)
                            ->W(256)
                            ->H(7)
                            ->Border(1, Rgb(0x17, 0x17, 0x17))
                            ->Child(ProgressIndicator::New(a)->W(177)->H(7)->Bg(
                                Rgb(0x17, 0x17, 0x17)))))
        ->Child(Div(a)
                    ->FlexRow()
                    ->JustifyBetween()
                    ->W(kFill)
                    ->Child(TextEl(a, StrL("Optimizing bundle"))
                                ->Font(14)
                                ->Fg(Rgb(0x73, 0x73, 0x73)))
                    ->Child(TextEl(a, StrL("32%"))
                                ->Font(14)
                                ->Fg(Rgb(0x73, 0x73, 0x73))))
        ->Child(
            Progress::New(a, StrL("example-progress-secondary"))
                ->Child(ProgressTrack::New(a)
                            ->W(256)
                            ->H(6)
                            ->Border(1, Rgb(0xa3, 0xa3, 0xa3))
                            ->Child(ProgressIndicator::New(a)->W(83)->H(6)->Bg(
                                Rgb(0x73, 0x73, 0x73)))));
}

void ShowcaseProgressClick(ShowcaseApp* app, int id) {
    (void)app;
    (void)id;
}

SHOWCASE_PAGE(CompProgress, ShowcaseProgress, ShowcaseProgressClick);
