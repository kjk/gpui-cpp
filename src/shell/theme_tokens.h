#ifndef GPUI_SHELL_THEME_TOKENS_H_
#define GPUI_SHELL_THEME_TOKENS_H_

#include "base/theme_tokens.h"

namespace gpui {
struct App;

namespace shell {

// Refreshes the palette this thread resolves tokens against and answers the
// revision it now carries.
//
// The revision changes only when the tokens or the appearance do, which is what
// lets a script's `cx.theme()` be a cache read: the snapshot's JSON is built
// once per palette rather than once per component that asks for it, and a
// ScriptView rebuilds its description exactly when the palette under it moved.
uint32_t ThemeTokensSync(const App* app);
uint32_t ThemeTokensRevision();
bool ThemeTokenColor(Str name, Hsla* out);
bool ThemeTokenSpacing(Str name, float* out);
bool ThemeTokenRadius(Str name, float* out);
// The whole type scale, rather than one entry by name. The colour, spacing and
// radius lookups answer one token at a time because a description names them
// one at a time; nothing names a text style that way. The scale is read whole,
// to be reported back to a script.
bool ThemeTypographyTokens(TypographyTokens* out);
SeqStrings ThemeColorTokenNames();
SeqStrings ThemeSpacingTokenNames();
SeqStrings ThemeRadiusTokenNames();

} // namespace shell
} // namespace gpui
#endif // GPUI_SHELL_THEME_TOKENS_H_
