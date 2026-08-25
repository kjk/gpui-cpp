#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static const char* kParas[] = {
    "Text selection across renderers",
    "Selection should feel like a natural part of reading a product brief. "
    "Start in this paragraph, continue into the next renderer, and GPUI "
    "preserves the document order while every frame supplies fresh geometry "
    "for the same stable selection handle.",
    "This second paragraph is deliberately long enough to wrap in the "
    "showcase. Drag across the boundary to see one continuous highlight, then "
    "use the platform copy shortcut to confirm that the copied result follows "
    "the visible reading order rather than renderer ownership.",
    "International text should remain predictable when a line mixes café, déjà "
    "vu, Kraków, naïve, and résumé. Resize the window or drag across several "
    "wrapped lines; UTF-8 byte ranges still map back to the correct glyphs "
    "without splitting a character.",
};

static int ParaLen(int i) {
    return (int)strlen(kParas[i]);
}

static void CopySelected(ShowcaseApp* app, char* out, int cap) {
    if (cap <= 0) {
        return;
    }
    out[0] = 0;
    if (app->selA < 0 || app->selB < 0) {
        return;
    }
    int a = app->selA;
    int b = app->selB;
    if (a > b) {
        int t = a;
        a = b;
        b = t;
    }
    int n = 0;
    int pos = 0;
    for (int i = 0; i < 4 && n < cap - 1; i++) {
        int plen = ParaLen(i);
        int lo = a > pos ? a : pos;
        int hi = b < pos + plen ? b : pos + plen;
        if (lo < hi) {
            int take = hi - lo;
            if (n + take > cap - 1) {
                take = cap - 1 - n;
            }
            memcpy(out + n, kParas[i] + (lo - pos), (size_t)take);
            n += take;
        }
        pos += plen;
        if (i < 3 && a <= pos && b > pos && n < cap - 1) {
            out[n++] = '\n';
        }
        pos += 1;
    }
    out[n] = 0;
}

static void OnSelClear(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    (void)app;
    app->selA = -1;
    app->selB = -1;
    Notify(cx);
}

El* ShowcaseTextSelection(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    int a0 = app->selA;
    int b0 = app->selB;
    if (a0 > b0) {
        int t = a0;
        a0 = b0;
        b0 = t;
    }
    El* doc = Div(a)->FlexCol()->Gap(12)->Pad(16);
    int pos = 0;
    for (int i = 0; i < 4; i++) {
        int plen = ParaLen(i);
        El* p = TextSelection::New(cx, DupFmt(cx, "para-%d", i), 0)->W(kFill);
        float size = i == 0 ? 18.f : 14.f;
        El* t = ScTxt(cx, Str(kParas[i]), size, i == 0 ? ScInk() : ScGray())
                    ->Wrap()
                    ->MaxW(560)
                    ->Click(HashClickId(DupFmt(cx, "para-%d", i)));
        if (i == 0) {
            t->Semibold();
        }
        if (a0 >= 0 && b0 > a0) {
            int lo = a0 > pos ? a0 : pos;
            int hi = b0 < pos + plen ? b0 : pos + plen;
            if (lo < hi) {
                t->selLo = lo - pos;
                t->selHi = hi - pos;
            }
        }
        p->Child(t);
        doc->Child(p);
        pos += plen + 1;
    }

    bool active = app->selA >= 0 && app->selA != app->selB;
    El* preview = Div(a)->Flex1()->ClipY();
    if (active) {
        char picked[1024];
        CopySelected(app, picked, (int)sizeof(picked));
        preview->Child(
            ScTxt(cx, DupA(cx, picked), 12, ScGray())->Wrap()->MaxW(560));
    } else {
        preview->Child(ScTxt(cx,
                             StrL("Drag across any paragraphs to select text."),
                             12, ScGray()));
    }

    El* footer = Div(a)
                     ->H(150)
                     ->Shrink0()
                     ->Pad(12)
                     ->FlexCol()
                     ->Gap(8)
                     ->ItemsStart()
                     ->Bg(ScHover())
                     ->Border(1, ScLine())
                     ->Child(ScTxt(cx,
                                   active ? StrL("Selection active")
                                          : StrL("No selection"),
                                   12, ScInk())
                                 ->Semibold())
                     ->Child(preview)
                     ->Child(Button::New(cx, StrL("clear-text-selection"),
                                         false, Listen(cx, &OnSelClear))
                                 ->H(28)
                                 ->PadX(8)
                                 ->Shrink0()
                                 ->ItemsCenter()
                                 ->JustifyCenter()
                                 ->Border(1, Rgb(0x17, 0x17, 0x17))
                                 ->Child(TextEl(a, StrL("Clear selection"))
                                             ->Font(12)
                                             ->Fg(Rgb(0x17, 0x17, 0x17))));

    return Div(a)
        ->FlexCol()
        ->W(620)
        ->MaxW(620)
        ->H(520)
        ->MaxH(520)
        ->Gap(12)
        ->Child(Div(a)->Flex1()->ClipY()->ScrollY(0)->FlexCol()->Child(doc))
        ->Child(footer);
}

SHOWCASE_PAGE(CompTextSelection, ShowcaseTextSelection);
