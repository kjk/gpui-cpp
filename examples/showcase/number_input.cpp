#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static bool ParseNum(const char* s, int* out) {
    if (!s || !s[0]) {
        return false;
    }
    int n = 0;
    const char* p = s;
    if (*p == '-' || *p == '+') {
        p++;
    }
    if (!*p) {
        return false;
    }
    while (*p) {
        if (*p < '0' || *p > '9') {
            return false;
        }
        n = n * 10 + (*p - '0');
        p++;
    }
    *out = atoi(s);
    return true;
}

static void FocusNum(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->input.focused = true;
    Notify(cx);
}

static void StepNum(ShowcaseApp* app, Ctx* cx, const ClickEvent*,
                    intptr_t delta) {
    int n = 0;
    if (!ParseNum(InputCStr(&app->input), &n)) {
        n = 0;
    }
    n += (int)delta;
    InputSetValue(&app->input, fmt("%d", n));
    Notify(cx);
}

El* ShowcaseNumberInput(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    int dummy = 0;
    bool valid = ParseNum(InputCStr(&app->input), &dummy);
    El* controls = Div(a)->FlexCol()->W(24)->Shrink0();
    controls->Child(
        ScButton(cx, StrL("inc"))
            ->OnClick(Listen(cx, &StepNum, 1))
            ->Flex1()
            ->W(24)
            ->ItemsCenter()
            ->JustifyCenter()
            ->Bg(Rgb(0, 0, 0))
            ->HoverBg(Rgb(0x40, 0x40, 0x40))
            ->Child(TextEl(a, StrL("+"))->Font(12)->Fg(Rgb(0xff, 0xff, 0xff))));
    controls->Child(
        ScButton(cx, StrL("dec"))
            ->OnClick(Listen(cx, &StepNum, -1))
            ->Flex1()
            ->W(24)
            ->ItemsCenter()
            ->JustifyCenter()
            ->Bg(Rgb(0, 0, 0))
            ->HoverBg(Rgb(0x40, 0x40, 0x40))
            ->Child(TextEl(a, StrL("−"))->Font(12)->Fg(Rgb(0xff, 0xff, 0xff))));

    return Div(a)
        ->FlexCol()
        ->W(200)
        ->Gap(4)
        ->Child(
            TextEl(a, StrL("Quantity"))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17)))
        ->Child(NumberInput::New(cx, StrL("quantity"))
                    ->FlexRow()
                    ->W(kFill)
                    ->H(28)
                    ->ItemsCenter()
                    ->Border(1, valid ? Rgb(0x17, 0x17, 0x17)
                                      : Rgb(0x73, 0x73, 0x73))
                    ->Child(InputBase::New(cx, StrL("number-field"), true)
                                ->OnClick(Listen(cx, &FocusNum))
                                ->FocusId(0)
                                ->Flex1()
                                ->H(28)
                                ->PadX(8)
                                ->ItemsCenter()
                                ->Child(Input::New(cx, &app->input)))
                    ->Child(controls))
        ->Child(TextEl(a, valid ? StrL("Step: 1") : StrL("Enter a number"))
                    ->Font(12)
                    ->Fg(Rgb(0x73, 0x73, 0x73)));
}

SHOWCASE_PAGE(CompNumberInput, ShowcaseNumberInput);
