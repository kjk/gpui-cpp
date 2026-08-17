#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickColor = 300,
    ClickHex = 301,
    ClickSwatch = 302
};

static const uint32_t kSwatches[] = {0xdc2626, 0xd97706, 0x16a34a, 0x2563eb,
                                     0x7c3aed};

static Rgba FromHex(uint32_t h) {
    return Rgb((uint8_t)((h >> 16) & 0xff), (uint8_t)((h >> 8) & 0xff),
               (uint8_t)(h & 0xff));
}

static void WriteHex(ShowcaseApp* app, uint32_t hex) {
    _snprintf_s(app->hexIn.buf, _TRUNCATE, "#%06X", hex & 0xffffff);
    app->hexIn.len = (int)strlen(app->hexIn.buf);
}

static void SetHexBuf(ShowcaseApp* app) {
    WriteHex(app, app->colorHex);
}

static uint32_t DisplayedColor(ShowcaseApp* app) {
    if (app->colorOpen && app->hoverId >= ClickSwatch &&
        app->hoverId < ClickSwatch + 5) {
        return kSwatches[app->hoverId - ClickSwatch] & 0xffffff;
    }
    return app->colorHex & 0xffffff;
}

El* ShowcaseColorPicker(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    uint32_t shown = DisplayedColor(app);
    if (!app->hexIn.focused) {
        WriteHex(app, shown);
    }
    El* trigger = Div(a)
                      ->Id(StrL("color-trigger"))
                      ->H(28)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->Gap(8)
                      ->Border(1, Rgb(0x17, 0x17, 0x17))
                      ->Bg(Rgb(0xff, 0xff, 0xff))
                      ->Click(ClickColor)
                      ->FocusId(ClickColor)
                      ->Child(Div(a)
                                  ->W(14)
                                  ->H(14)
                                  ->Bg(FromHex(shown))
                                  ->Border(1, Rgb(0x17, 0x17, 0x17)))
                      ->Child(TextEl(a, Str(app->hexIn.buf, app->hexIn.len))
                                  ->Font(12)
                                  ->Fg(Rgb(0x17, 0x17, 0x17)))
                      ->Child(Div(a)->Grow())
                      ->Child(TextEl(a, app->colorOpen ? StrL("⌃") : StrL("⌄"))
                                  ->Font(12)
                                  ->Fg(Rgb(0x17, 0x17, 0x17)));
    El* pop = nullptr;
    if (app->colorOpen) {
        pop = Div(a)
                  ->FlexCol()
                  ->W(220)
                  ->Pad(8)
                  ->Gap(8)
                  ->Border(1, Rgb(0x17, 0x17, 0x17))
                  ->Bg(Rgb(0xff, 0xff, 0xff));
        El* sw = Div(a)->FlexRow()->Gap(4);
        for (int i = 0; i < 5; i++) {
            bool on = (app->colorHex & 0xffffff) == kSwatches[i];
            sw->Child(ColorSwatch::New(cx, DupFmt(cx, "swatch-%d", i),
                                       ClickSwatch + i)
                          ->W(24)
                          ->H(24)
                          ->Bg(FromHex(kSwatches[i]))
                          ->Border(1, on ? Rgb(0x17, 0x17, 0x17)
                                         : Rgb(0xff, 0xff, 0xff)));
        }
        pop->Child(sw);
        pop->Child(InputBase::New(cx, StrL("color-hex-input"), ClickHex)
                       ->W(204)
                       ->H(28)
                       ->PadX(8)
                       ->ItemsCenter()
                       ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
                       ->Child(Input::New(cx, &app->hexIn)));
    }
    El* root = ColorPicker::New(cx, StrL("example-color-picker"))
                   ->W(220)
                   ->Child(trigger);
    return Popup::New(cx, StrL("example-color-picker-popup"), root)
        ->Content(pop)
        ->IntoEl();
}

void ShowcaseColorPickerClick(ShowcaseApp* app, int id) {
    if (id == ClickColor) {
        app->colorOpen = !app->colorOpen;
        app->hexIn.focused = false;
    } else if (id == ClickHex) {
        app->hexIn.focused = true;
        app->input.focused = false;
    } else if (id >= ClickSwatch && id < ClickSwatch + 5) {
        app->colorHex = kSwatches[id - ClickSwatch];
        SetHexBuf(app);
        app->colorOpen = false;
        app->hexIn.focused = false;
    }
}

SHOWCASE_PAGE(CompColorPicker, ShowcaseColorPicker, ShowcaseColorPickerClick);
