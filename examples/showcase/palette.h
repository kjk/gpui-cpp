/* The showcase's own palette — crates/base/examples/shared/palette.rs
 *
 * crates/base is the unstyled layer, so its examples supply the colours
 * themselves. Rust's shared palette answers a light and a dark set and maps
 * the literals the pages were written with onto whichever is active.
 *
 * This is the minimal version: the fields the pages here read, the two
 * palettes, and `Resolve`, which is what turns a light literal into its dark
 * counterpart. The full 123-line port — the window-appearance hook and the
 * Base theme projection — belongs to the palette package; when that lands,
 * this file is what it replaces.
 */

#ifndef GPUI_SHOWCASE_PALETTE_H_
#define GPUI_SHOWCASE_PALETTE_H_

#include "gpui.h"

struct ExamplePalette {
    uint32_t canvas;
    uint32_t surface;
    uint32_t elevated;
    uint32_t foreground;
    uint32_t mutedForeground;
    uint32_t subtleForeground;
    uint32_t border;
    uint32_t strongBorder;
    uint32_t hover;
    uint32_t selected;
    uint32_t accent;
    uint32_t accentForeground;

    static ExamplePalette ForDark(bool dark) {
        if (dark) {
            return ExamplePalette{0x0e0e0e, 0x171717, 0x262626, 0xffffff,
                                  0xa3a3a3, 0x737373, 0x404040, 0xd4d4d4,
                                  0x262626, 0x303030, 0xffffff, 0x171717};
        }
        return ExamplePalette{0xffffff, 0xffffff, 0xf5f5f5, 0x171717,
                              0x525252, 0x737373, 0xd4d4d4, 0x171717,
                              0xf5f5f5, 0xf0f0f0, 0x171717, 0xffffff};
    }

    // The light literal the pages are written with, in this palette.
    uint32_t Resolve(uint32_t lightColor) const {
        switch (lightColor) {
            case 0xffffff:
                return surface;
            case 0x171717:
            case 0x262626:
                return foreground;
            case 0x404040:
                return strongBorder;
            case 0x525252:
                return mutedForeground;
            case 0x737373:
            case 0xa3a3a3:
                return subtleForeground;
            case 0xd4d4d4:
            case 0xe5e5e5:
                return border;
            case 0xf0f0f0:
                return selected;
            case 0xf5f5f5:
                return hover;
            case 0x007fff:
                return canvas == 0x0e0e0e ? 0x79c0ff : 0x007fff;
            default:
                return lightColor;
        }
    }
};

// The showcase runs light; the dark palette is here because the pages that
// read it — the TextView page's style — have to say which they built.
inline ExamplePalette ExamplePaletteActive() {
    return ExamplePalette::ForDark(false);
}

inline gpui::Rgba ExampleRgb(uint32_t lightColor) {
    return gpui::RgbaHex(ExamplePaletteActive().Resolve(lightColor));
}

#endif // GPUI_SHOWCASE_PALETTE_H_
