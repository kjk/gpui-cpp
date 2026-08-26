/* The theme registry — crates/ui/src/theme/{registry.rs, schema.rs, color.rs}

   Upstream's palette is data, not code: `default-theme.json` names sixty-odd
   tokens, every other theme file names a subset of them, and `apply_config`
   resolves what a file leaves out from what it sets — `muted.foreground`
   falls back to the muted surface blended with 70% of the foreground, a
   button's hover to the input colour mixed toward transparent, and so on down
   a chain a hundred entries long.

   This is that reader. `ThemeConfig` is one theme out of a file, held as the
   parsed document plus its header; `ThemeConfigResolve` walks the chain into a
   `Theme`; and the registry is the table of them a picker lists. The colour
   grammar is `color.rs`: a hex string, or a shadcn name with an optional scale
   and an optional percentage — `neutral-200`, `white`, `red-500/40`.

   What is deliberately not here: the highlight styles
   (our colouring is a scanner, not tree-sitter), and the directory watcher —
   Rust reloads themes when the folder changes, and nothing in this tree
   watches a folder. */

#include "base/json.h"
#include "base/theme_tokens.h"

namespace gpui {

// ─── the shadcn scales, generated into theme_data.cpp ────────────────────

const int kNumShadcnColumns = 11;

// One hue and the eleven scales it carries, as 0xRRGGBB.
struct ShadcnScale {
    const char* name;
    uint32_t hex[kNumShadcnColumns];
};

extern const ShadcnScale kShadcnScales[];
extern const int kNumShadcnScales;
extern const int kShadcnScaleNums[kNumShadcnColumns];
// stone, which no ColorName can reach; the colour picker's palette grid is
// the only thing that names it.
extern const uint32_t kShadcnStone[kNumShadcnColumns];
extern const uint32_t kShadcnBlack;
extern const uint32_t kShadcnWhite;
// default-theme.json verbatim: the theme set the registry starts with.
extern const char* const kDefaultThemeJson;

// try_parse_color. `#rgb`, `#rgba`, `#rrggbb` and `#rrggbbaa`, or a colour
// name with an optional `-scale` and an optional `/percent`. A scale that no
// hue carries falls back to 500, the way `ColorName::scale` does. False for
// anything else, which is what leaves a token on its fallback.
bool ThemeParseColor(Str s, Rgba* out);

// try_parse_background. Everything `ThemeParseColor` takes, and CSS two-stop
// `linear-gradient(...)` besides: an angle in degrees or one of the eight
// `to ..` directions, then two colour stops with optional percentages.
//
//     linear-gradient(180deg, #1E293B, #0F172A)
//     linear-gradient(to right, red-500 25%, blue-600 75%)
//
// A gradient's `color` is its first stop, which is what `try_parse_theme_color`
// keeps for the flat palette field beside the renderable token.
bool ThemeParseBackground(Str s, Background* out);

// ─── one theme out of a theme file ───────────────────────────────────────

// ThemeConfig. The colours stay as the parsed `colors` object rather than as
// fields: a token's fallback is usually another token, so they can only be
// read in the order `apply_config` reads them, and a table of Options would
// be the same document twice.
struct ThemeConfig {
    Str name = {};
    Str author = {};
    Str url = {};
    ThemeMode mode = ThemeMode::Light;
    bool isDefault = false;
    // The `colors` object, or null for a theme that only sets metrics.
    const JsonValue* colors = nullptr;
    // The metrics, or a non-positive number for one the file leaves out.
    float fontSize = 0;
    float radius = -1;
    float radiusLg = -1;
};

// `is_explicit` in the theme viewer: whether the file names this token itself
// rather than leaving it to the fallback chain. `key` is the one schema.rs
// reads it from, and the aliases the resolver takes count — Rust's own set is
// built from the config struct the file was read into, not from its text.
bool ThemeConfigNames(const ThemeConfig* cfg, const char* key);

// ThemeColor::apply_config: every token of `out` is read from the config,
// falling back to `base` — Rust's `ThemeColor::light()` / `::dark()` — or to
// whatever the chain says when the config has neither. `base` is the palette
// a file is measured against, never the live one, so applying two themes in a
// row cannot compound.
void ThemeConfigResolve(Theme* out, const ThemeConfig* cfg, const Theme& base);

// ─── the registry ────────────────────────────────────────────────────────

// The default themes, from the embedded `default-theme.json`. Idempotent, and
// every other entry point calls it, so an application never has to.
void ThemeRegistryInit(App* app);

// Theme::sync_base: replace Base's application-owned theme with the active
// styled projection. Component initialization installs this as the callback
// for every public theme mutation.
void ThemeSyncBase(App* app);

// load_themes_from_str: every theme in a ThemeSet document joins the table
// under its own name. A name already taken is skipped, the way Rust's is.
// Returns how many were added.
int ThemeRegistryLoadStr(App* app, Str json);

// Rust's `reload()` over its `themes_dir`: every `*.json` in `dir`, with an
// unparseable one skipped rather than fatal. Returns how many themes were
// added. A relative path is resolved against the asset roots.
int ThemeRegistryLoadDir(App* app, Str dir);

// `Theme::apply_semantic_config_str`: a theme written in the semantic
// vocabulary — `{"tokens": {"colors": {..}, "radius": {..}, ..}}` — resolved
// over the palette in force for that mode and applied to it. Every field is
// optional and what a document leaves out stays as it was. False for a
// document that is not one. `out` takes the resolved set, which is what Rust
// hands back for application-owned UI to read.
bool ThemeApplySemanticConfigStr(App* app, ThemeMode mode, Str json,
                                 SemanticThemeTokens* out = nullptr);
// The resolve half on its own, over tokens the caller already has: what
// `SemanticThemeConfig::apply_to` does. Exposed for the tests, which drive it
// without installing a theme.
bool ThemeSemanticConfigApply(const JsonValue* doc, SemanticThemeTokens* io);

// sorted_themes: the defaults first, then light before dark, then by name
// folded to lower case.
int ThemeRegistryCount(const App* app);
const ThemeConfig* ThemeRegistryAt(const App* app, int ix);
const ThemeConfig* ThemeRegistryFind(const App* app, Str name);
// The name of the theme installed for each mode, which is what a picker ticks.
Str ThemeRegistryActive(const App* app, ThemeMode mode);

// Theme::apply_config: the config is resolved against its mode's default
// palette and becomes the light or the dark theme. Switching to that mode
// then paints with it. Answers false for a config that is not in the table.
bool ThemeRegistryApply(App* app, const ThemeConfig* cfg);
bool ThemeRegistryApply(App* app, Str name);

// Puts both modes back on the palettes the tree was built with.
void ThemeRegistryReset(App* app);

void ThemeRegistryFree(App* app);

} // namespace gpui
