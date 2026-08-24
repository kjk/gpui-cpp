#include "Story.h"

struct ColorPickerStory {
    // The committed colour, mirrored out of the picker's own
    // ColorPickerState so the preview card beside it can show what is picked.
    // Rust's story owns the Entity<ColorPickerState> and reads it the same
    // way; here the widget keeps it keyed by its id.
    uint32_t shown = 0x6366f1;
    bool seeded = false;
    StoryToolbarState toolbar;
    static El* Render(ColorPickerStory* self, Ctx* cx);
};

// ColorPickerEvent::Change: the picker hands over what it committed.
static void SetColor(ColorPickerStory* self, Ctx* cx, const ClickEvent*,
                     intptr_t hex) {
    self->shown = (uint32_t)hex;
    Notify(cx);
}

El* ColorPickerStory::Render(ColorPickerStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    // Seeded with indigo_500, the way the Rust story seeds its state.
    Entity<ColorPickerState> st =
        component::ColorPickerStateFor(cx, StrL("accent-color"));
    if (ColorPickerState* s = st.Get(cx)) {
        if (!self->seeded) {
            self->seeded = true;
            ColorPickerSetValue(s, 0x6366f1);
        }
        // What the card shows follows the picker, preview and all.
        uint32_t hex = 0;
        if (ColorPickerShown(s, &hex)) {
            self->shown = hex;
        }
    }
    uint32_t shown = self->shown;
    Rgba color = RgbaHex(shown);

    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill)->ItemsCenter();
    page->Child(StoryToolbar(cx, self));

    El* sec = StorySection(cx, "Theme Color",
                           "Select a color and preview the resulting value.");
    StorySectionBody(sec)->W(440);
    El* card = Div(a)
                   ->FlexCol()
                   ->W(kFill)
                   ->Gap(16)
                   ->Pad(16)
                   ->Radius(th.radiusLg)
                   ->Border(1, th.border);

    El* head =
        Div(a)->FlexRow()->W(kFill)->Gap(16)->ItemsCenter()->JustifyBetween();
    El* text = Div(a)->FlexCol()->Gap(4);
    text->Child(StoryTxt(cx, StrL("Accent color"), 16, th.foreground)
                    ->Medium());
    text->Child(StoryTxt(cx, StrL("Used for primary actions and highlights."),
                         14, th.mutedFg));
    head->Child(text);
    head->Child(component::ColorPicker::New(cx, StrL("accent-color"))
                    ->OnChange(Listen(cx, &SetColor))
                    ->IntoEl());
    card->Child(head);

    // The preview: the color over a muted footer naming its hex.
    El* preview = Div(a)
                      ->FlexCol()
                      ->W(kFill)
                      ->ClipY()
                      ->Radius(th.radiusLg)
                      ->Border(1, th.border);
    preview->Child(Div(a)->W(kFill)->H(96)->Bg(color));
    El* foot = Div(a)
                   ->FlexRow()
                   ->W(kFill)
                   ->PadX(12)
                   ->PadY(8)
                   ->ItemsCenter()
                   ->JustifyBetween()
                   ->Bg(th.tokens.muted);
    foot->Child(StoryTxt(cx, StrL("Selected color"), 14, th.mutedFg));
    foot->Child(
        // `color.to_hex()`, which is the Hsla round trip and not the digits
        // the palette entry was written with.
        StoryTxt(cx, RgbaToHex(cx->a, RgbaHex(shown & 0xffffffu)), 16,
                 th.foreground)
            ->Mono()
            ->Medium());
    preview->Child(foot);
    card->Child(preview);

    StorySectionAdd(sec, card);
    page->Child(sec);
    return page;
}

STORY_PAGE(StoryColorPicker, ColorPickerStory);
