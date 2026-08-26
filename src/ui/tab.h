/* Themed tabs — crates/ui/src/tab

   Rust splits this in two: `Tab` is one tab and `TabBar` is the strip that
   holds them and hands each one the variant, the size and its index.
   `component::Tabs` is that strip; a tab is a `TabItem` in its list rather
   than an element of its own, because a tab here carries no children beyond
   its label and its icon. */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// TabVariant. Five looks over the same behaviour: a folder-style tab, an
// outlined one, a pill, a segmented control and an underline.
enum class TabVariant : uint8_t {
    Tab,
    Outline,
    Pill,
    Segmented,
    Underline
};

// The tables from `impl TabVariant`, which are what the look actually is.
// They are functions rather than constants because every one of them is a
// match on the variant and the size.
float TabHeight(TabVariant v, UiSize size);
float TabInnerHeight(TabVariant v, UiSize size);
// inner_paddings: the padding either side of the label. Underline has none —
// its gap comes from the bar instead.
float TabPadX(TabVariant v, UiSize size);
// inner_margins: only Underline has any, and only top and bottom.
float TabMarginTop(TabVariant v, UiSize size);
float TabMarginBottom(TabVariant v, UiSize size);
// The gap between two tabs, which the bar owns.
float TabBarGap(TabVariant v, UiSize size);
// The bar's own padding, which only Segmented has.
float TabBarPadX(TabVariant v, UiSize size);
// A rounded bar: Segmented only.
float TabBarRadius(TabVariant v, UiSize size, float radius, float radiusLg);
float TabRadius(TabVariant v, UiSize size, float radius, float radiusLg);
float TabInnerRadius(TabVariant v, UiSize size, float radius, float radiusLg);

struct TabItem {
    Str label = {};
    Str ariaLabel = {};
    IconName icon = IconName::None;
    bool disabled = false;
    // Tab::flex_1(): the tab shares the bar with its siblings instead of
    // taking the width of its own label. Two of them split a segmented bar
    // evenly, which is what the colour picker's Palette/HSLA pair does.
    bool flex1 = false;
};

struct Tabs {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    // As many tabs as the caller adds; the builder is on the frame arena, so
    // the items grow into it.
    ArenaVec<TabItem> items;
    int selected = 0;
    TabVariant variant = TabVariant::Tab;
    UiSize size = UiSize::Medium;
    // TabBar::max_width: the width a label gives way at, ellipsizing inside
    // it. 0 is Rust's `None` — the label decides the width and never shrinks.
    float maxWidth = 0;
    // The bar's own width. kAuto is Rust's default — the tabs decide.
    float width = kAuto;
    El* prefix = nullptr;
    El* suffix = nullptr;
    // TabBar::menu: the overflow button after the strip. Its dropdown lists
    // every tab by name, checked on the selected one and disabled where the
    // tab is, so a bar too narrow to show them all can still reach them.
    bool menu = false;
    Listener onChange;

    static Tabs* New(Ctx* cx);
    static Tabs* New(Ctx* cx, Str id);
    Tabs* Tab(Str label);
    Tabs* Tab(Str label, IconName icon, bool disabled = false);
    // `Tab::new().flex_1()`, applied to the tab just added.
    Tabs* Flex1();
    // `Tab::aria_label`, applied to the tab most recently added.
    Tabs* AriaLabel(Str label);
    Tabs* Disabled(int ix, bool v = true);
    Tabs* Selected(int i);
    Tabs* OnChange(Listener fn);
    Tabs* Variant(TabVariant v);
    Tabs* Outline();
    Tabs* Pill();
    Tabs* Segmented();
    Tabs* Underline();
    Tabs* Size(UiSize v);
    Tabs* MaxWidth(float v);
    // TabBar has no width of its own in Rust: it is as wide as its tabs
    // unless the caller says `.w_full()` or `.w_64()`. A bar that fills is
    // also what makes `Tab::flex_1()` mean anything, since there is no free
    // space to share otherwise.
    Tabs* W(float v);
    Tabs* WFill();
    Tabs* Prefix(El* e);
    Tabs* Suffix(El* e);
    Tabs* Menu(bool v = true);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
