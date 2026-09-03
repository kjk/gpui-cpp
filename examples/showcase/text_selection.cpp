/* The text-selection page —
   crates/base/examples/showcase/components/text_selection.rs

   The page used to carry its own selectable text element: an Element impl
   that laid a StyledText out, registered a hitbox with a TextSelectionHandle
   and painted the projected ranges itself. `SelectableText` is that element,
   promoted into gpui-base, so the page is four runs sharing one document and
   a footer that shows what the window says is selected. */

#include "Showcase.h"
#include "palette.h"
#include "gpui.h"

using namespace gpui;

static const char* kHeading = "Text selection across renderers";
static const char* kProductParagraph =
    "Selection should feel like a natural part of reading a product brief. "
    "Start in this paragraph, continue into the next renderer, and GPUI "
    "preserves the document order while every frame supplies fresh geometry "
    "for the same stable selection handle.";
static const char* kImplementationParagraph =
    "This second paragraph is deliberately long enough to wrap in the "
    "showcase. Drag across the boundary to see one continuous highlight, then "
    "use the platform copy shortcut to confirm that the copied result follows "
    "the visible reading order rather than renderer ownership.";
static const char* kInternationalParagraph =
    "International text should remain predictable when a line mixes café, "
    "déjà vu, Kraków, naïve, and résumé. Resize the window or drag across "
    "several wrapped lines; UTF-8 byte ranges still map back to the correct "
    "glyphs without splitting a character.";

static void OnSelClear(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    // `TextSelection::clear(window, cx)`: the window owns the selection, so
    // all the page drops is the copy of it that its footer shows.
    WindowSelectionClear(cx->win);
    app->selActive = false;
    app->selText[0] = 0;
    Notify(cx);
}

El* ShowcaseTextSelection(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    ExamplePalette palette = PaletteActive();

    // One run per paragraph, in reading order, each joining the window's
    // selection. Rust hands each a TextSelectionHandle of its own out of the
    // page's array; the handles are the document here as well.
    El* doc = Div(a)->FlexCol()->Gap(12)->Pad(16);
    doc->Child(SelectableText::New(cx, StrL("selection-heading"), Str(kHeading))
                   ->TextStyle(18, RgbaHex(palette.foreground))
                   ->Semibold()
                   ->DocumentOrder(0)
                   ->IntoEl()
                   ->MaxW(560));
    struct Para {
        const char* id;
        const char* text;
    };
    const Para paragraphs[] = {
        {"selection-product", kProductParagraph},
        {"selection-implementation", kImplementationParagraph},
        {"selection-international", kInternationalParagraph},
    };
    for (int i = 0; i < 3; i++) {
        doc->Child(SelectableText::New(cx, Str(paragraphs[i].id),
                                       Str(paragraphs[i].text))
                       ->TextStyle(14, RgbaHex(palette.mutedForeground))
                       ->DocumentOrder((uint64_t)(i + 1))
                       ->IntoEl()
                       ->MaxW(560));
    }

    // The footer is `TextSelection::selected_text(window, cx)` — what the
    // window would copy, not a reconstruction of the page's own offsets.
    bool active = app->selActive && app->selText[0] != 0;
    El* preview = Div(a)->Flex1()->ClipY();
    if (active) {
        preview->Child(ScTxt(cx, StrDup(a, Str(app->selText)), 12,
                             RgbaHex(palette.mutedForeground))
                           ->Wrap()
                           ->MaxW(560));
    } else {
        preview->Child(ScTxt(cx,
                             StrL("Drag across any paragraphs to select text."),
                             12, RgbaHex(palette.mutedForeground)));
    }

    El* footer =
        Div(a)
            ->H(150)
            ->Shrink0()
            ->Pad(12)
            ->FlexCol()
            ->Gap(8)
            ->ItemsStart()
            ->Bg(RgbaHex(palette.hover))
            ->Border(1, RgbaHex(palette.Resolve(0xe5e5e5)))
            ->Child(
                ScTxt(cx,
                      active ? StrL("Selection active") : StrL("No selection"),
                      12, RgbaHex(palette.foreground))
                    ->Semibold())
            ->Child(preview)
            ->Child(Button::New(cx, StrL("clear-text-selection"), false,
                                Listen(cx, &OnSelClear))
                        ->H(28)
                        ->PadX(8)
                        ->Shrink0()
                        ->ItemsCenter()
                        ->JustifyCenter()
                        ->Border(1, RgbaHex(palette.foreground))
                        ->Child(TextEl(a, StrL("Clear selection"))
                                    ->Font(12)
                                    ->Fg(RgbaHex(palette.foreground))));

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
