#include "Story.h"

struct StatusBarStory {
    static El* Render(StatusBarStory* self, Ctx* cx);
    static void Click(StatusBarStory* self, Ctx* cx, int id);
};

El* StatusBarStory::Render(StatusBarStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* editor = StorySection(
        cx, "Editor",
        "Places repository state on the left and document state on the right.");
    El* ed = Div(a)
                 ->FlexRow()
                 ->W(kFill)
                 ->H(28)
                 ->PadX(8)
                 ->ItemsCenter()
                 ->JustifyBetween()
                 ->Bg(th.titleBar)
                 ->BorderT(1, th.border);
    El* left = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    left->Child(component::Button::New(cx, StrL("branch"))
                    ->Ghost()
                    ->Icon(IconName::Folder)
                    ->Label(StrL("main"))
                    ->Tooltip(StrL("Git branch"))
                    ->Compact()
                    ->IntoEl());
    left->Child(component::Separator::Vertical(cx)->IntoEl());
    left->Child(IconEl(a, IconName::CircleCheck, 12)->Fg(th.green));
    left->Child(StoryTxt(cx, StrL("0"), 12, th.foreground));
    left->Child(IconEl(a, IconName::Info, 12)->Fg(th.blue));
    left->Child(StoryTxt(cx, StrL("2"), 12, th.foreground));
    El* right = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    right->Child(StoryTxt(cx, StrL("Ln 12, Col 34"), 12, th.mutedFg));
    right->Child(StoryTxt(cx, StrL("UTF-8"), 12, th.mutedFg));
    right->Child(StoryTxt(cx, StrL("Rust"), 12, th.mutedFg));
    ed->Child(left)->Child(right);
    StorySectionAdd(editor, ed);
    page->Child(editor);

    El* appSec = StorySection(
        cx, "Application",
        "Combines connectivity, progress, save state, and notifications.");
    El* bar = Div(a)
                  ->FlexRow()
                  ->W(kFill)
                  ->H(28)
                  ->PadX(8)
                  ->ItemsCenter()
                  ->JustifyBetween()
                  ->Bg(th.titleBar)
                  ->BorderT(1, th.border);
    El* aLeft = Div(a)->FlexRow()->Gap(6)->ItemsCenter();
    aLeft->Child(IconEl(a, IconName::Check, 12)->Fg(th.foreground));
    aLeft->Child(StoryTxt(cx, StrL("Connected"), 12, th.foreground));
    El* aMid = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    aMid->Child(component::ProgressCircle::New(cx)
                    ->Value(45)
                    ->Size(16)
                    ->Label(false)
                    ->IntoEl());
    aMid->Child(StoryTxt(cx, StrL("Syncing…"), 12, th.foreground));
    El* aRight = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    aRight->Child(StoryTxt(cx, StrL("All changes saved"), 12, th.mutedFg));
    aRight->Child(component::Button::New(cx, StrL("notifications"))
                      ->Ghost()
                      ->Icon(IconName::Bell)
                      ->Label(StrL("3"))
                      ->Compact()
                      ->IntoEl());
    bar->Child(aLeft)->Child(aMid)->Child(aRight);
    StorySectionAdd(appSec, bar);
    page->Child(appSec);

    El* align = StorySection(
        cx, "Alignment",
        "Center content adapts when either side is empty or populated.");
    El* alignCol = Div(a)->FlexCol()->Gap(12)->W(kFill);
    alignCol->Child(component::StatusBar::New(cx)
                        ->Left(StrL("Center only → start-aligned"))
                        ->IntoEl());
    alignCol->Child(component::StatusBar::New(cx)
                        ->Left(StrL("Left"))
                        ->Right(StrL("Center → end (only left)"))
                        ->IntoEl());
    alignCol->Child(component::StatusBar::New(cx)
                        ->Left(StrL("Center → start (only right)"))
                        ->Right(StrL("Right"))
                        ->IntoEl());
    StorySectionAdd(align, alignCol);
    page->Child(align);
    return page;
}

void StatusBarStory::Click(StatusBarStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryStatusBar, StatusBarStory);
