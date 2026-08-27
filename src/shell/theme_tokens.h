#ifndef GPUI_SHELL_THEME_TOKENS_H_
#define GPUI_SHELL_THEME_TOKENS_H_

#include "base/theme_tokens.h"

namespace gpui {
struct App;

namespace shell {

void ThemeTokensSync(const App* app);
bool ThemeTokenColor(Str name, Hsla* out);
bool ThemeTokenSpacing(Str name, float* out);
bool ThemeTokenRadius(Str name, float* out);
SeqStrings ThemeColorTokenNames();
SeqStrings ThemeSpacingTokenNames();
SeqStrings ThemeRadiusTokenNames();

} // namespace shell
} // namespace gpui
#endif // GPUI_SHELL_THEME_TOKENS_H_
