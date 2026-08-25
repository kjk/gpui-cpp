#include "ui/i18n.h"
#include "ui/tab.h"
#include "base/motion.h"

namespace gpui {

namespace component {

// tab_bar.rs INDICATOR_SPRING: the period the indicator's slide is felt at.
// Rust keeps the box it is coming from in an epoch-stamped state; a spring
// here remembers where it had got to and how fast it was going, so what the
// bar has to keep is only the box it is going to.
static const float kTabIndicatorMotionMs = 250.f;

float TabHeight(TabVariant v, UiSize size) {
    bool under = v == TabVariant::Underline;
    switch (size) {
        case UiSize::XSmall:
            return under ? 26.f : 20.f;
        case UiSize::Small:
            return under ? 30.f : 24.f;
        case UiSize::Large:
            return under ? 44.f : 36.f;
        default:
            return under ? 36.f : 32.f;
    }
}

float TabInnerHeight(TabVariant v, UiSize size) {
    bool boxed = v == TabVariant::Tab || v == TabVariant::Outline ||
                 v == TabVariant::Pill;
    switch (size) {
        case UiSize::XSmall:
            return boxed ? 18.f : v == TabVariant::Segmented ? 16.f : 20.f;
        case UiSize::Small:
            return boxed ? 22.f : v == TabVariant::Segmented ? 18.f : 22.f;
        case UiSize::Large:
            return boxed ? 36.f : v == TabVariant::Segmented ? 28.f : 32.f;
        default:
            switch (v) {
                case TabVariant::Tab:
                    return 30.f;
                case TabVariant::Outline:
                case TabVariant::Pill:
                    return 26.f;
                case TabVariant::Segmented:
                    return 24.f;
                default:
                    return 26.f;
            }
    }
}

float TabPadX(TabVariant v, UiSize size) {
    // Underline has no padding of its own; the bar's gap does that job.
    if (v == TabVariant::Underline) {
        return 0;
    }
    switch (size) {
        case UiSize::XSmall:
            return 8.f;
        case UiSize::Small:
            return 10.f;
        case UiSize::Large:
            return 16.f;
        default:
            return 12.f;
    }
}

float TabMarginTop(TabVariant v, UiSize size) {
    if (v != TabVariant::Underline) {
        return 0;
    }
    switch (size) {
        case UiSize::XSmall:
            return 1.f;
        case UiSize::Small:
            return 2.f;
        case UiSize::Large:
            return 5.f;
        default:
            return 3.f;
    }
}

float TabMarginBottom(TabVariant v, UiSize size) {
    if (v != TabVariant::Underline) {
        return 0;
    }
    return TabMarginTop(v, size) + 1.f;
}

float TabBarGap(TabVariant v, UiSize size) {
    switch (v) {
        case TabVariant::Tab:
            return 0;
        case TabVariant::Pill:
            return 4.f;
        case TabVariant::Segmented:
            return 2.f;
        case TabVariant::Underline:
            // The same as the tab's own padding would have been, which is
            // what Rust says where it writes this out.
            switch (size) {
                case UiSize::XSmall:
                    return 10.f;
                case UiSize::Small:
                    return 12.f;
                case UiSize::Large:
                    return 20.f;
                default:
                    return 16.f;
            }
        default:
            // Outline takes the default gap.
            switch (size) {
                case UiSize::XSmall:
                case UiSize::Small:
                    return 8.f;
                case UiSize::Large:
                    return 16.f;
                default:
                    return 12.f;
            }
    }
}

float TabBarPadX(TabVariant v, UiSize size) {
    if (v != TabVariant::Segmented) {
        return 0;
    }
    switch (size) {
        case UiSize::XSmall:
            return 2.f;
        case UiSize::Small:
            return 3.f;
        default:
            return 4.f;
    }
}

float TabBarRadius(TabVariant v, UiSize size, float radius, float radiusLg) {
    if (v != TabVariant::Segmented) {
        return 0;
    }
    return (size == UiSize::XSmall || size == UiSize::Small) ? radius
                                                             : radiusLg;
}

float TabRadius(TabVariant v, UiSize size, float radius, float radiusLg) {
    if (v == TabVariant::Outline || v == TabVariant::Pill) {
        // Fully round: Rust asks for 99px and lets the height cap it.
        return 99.f;
    }
    return TabBarRadius(v, size, radius, radiusLg);
}

float TabInnerRadius(TabVariant v, UiSize size, float radius, float radiusLg) {
    if (v != TabVariant::Segmented) {
        return 0;
    }
    float outer = TabBarRadius(v, size, radius, radiusLg);
    float inset = size == UiSize::Large ? 3.f : 2.f;
    float r = outer - inset;
    return r > 0 ? r : 0;
}

Tabs* Tabs::New(Ctx* cx) {
    return New(cx, StrL("tabs"));
}
Tabs* Tabs::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    Tabs* t = ArenaNew<Tabs>(a);
    t->a = a;
    t->cx = cx;
    t->id = id;
    return t;
}
Tabs* Tabs::Tab(Str label) {
    return Tab(label, IconName::None, false);
}
Tabs* Tabs::Tab(Str label, IconName icon, bool disabled) {
    TabItem it;
    it.label = label;
    it.icon = icon;
    it.disabled = disabled;
    items.Append(a, it);
    return this;
}
Tabs* Tabs::W(float v) {
    width = v;
    return this;
}
Tabs* Tabs::WFill() {
    width = kFill;
    return this;
}
Tabs* Tabs::Flex1() {
    if (items.len > 0) {
        items[items.len - 1].flex1 = true;
    }
    return this;
}
Tabs* Tabs::Disabled(int ix, bool v) {
    if (ix >= 0 && ix < items.len) {
        items[ix].disabled = v;
    }
    return this;
}
Tabs* Tabs::Selected(int i) {
    selected = i;
    return this;
}
Tabs* Tabs::OnChange(Listener fn) {
    onChange = fn;
    return this;
}
Tabs* Tabs::Variant(TabVariant v) {
    variant = v;
    return this;
}
Tabs* Tabs::Outline() {
    return Variant(TabVariant::Outline);
}
Tabs* Tabs::Pill() {
    return Variant(TabVariant::Pill);
}
Tabs* Tabs::Segmented() {
    return Variant(TabVariant::Segmented);
}
Tabs* Tabs::Underline() {
    return Variant(TabVariant::Underline);
}
Tabs* Tabs::Size(UiSize v) {
    size = v;
    return this;
}
Tabs* Tabs::MaxWidth(float v) {
    maxWidth = v;
    return this;
}
Tabs* Tabs::Prefix(El* e) {
    prefix = e;
    return this;
}
Tabs* Tabs::Suffix(El* e) {
    suffix = e;
    return this;
}
Tabs* Tabs::Menu(bool v) {
    menu = v;
    return this;
}

// Nothing painted: `a == 0` is what says a style leaves this part alone,
// which is Rust's `theme.transparent`.
const Rgba kTabNone = {0, 0, 0, 0};

// TabStyle: the four states of a variant, as Rust's normal / hovered /
// selected / disabled return them. `bg` is the tab's own box, `innerBg` the
// box around the label — Segmented paints the selected tab there so the strip
// underneath keeps its own background.
struct TabStyle {
    Rgba fg = {};
    Rgba bg = kTabNone;
    Rgba innerBg = kTabNone;
    Rgba borderColor = kTabNone;
    float borderT = 0;
    float borderL = 0;
    float borderR = 0;
    float borderB = 0;
    bool shadow = false;
};

static TabStyle TabNormal(TabVariant v, const Theme& th) {
    TabStyle s;
    switch (v) {
        case TabVariant::Tab:
            s.fg = th.tabFg;
            s.borderL = s.borderR = 1;
            break;
        case TabVariant::Outline:
            s.fg = th.tabFg;
            // Edges::all(px(1.)): an outline tab is ringed, not underlined.
            s.borderT = s.borderL = s.borderR = s.borderB = 1;
            s.borderColor = th.border;
            break;
        case TabVariant::Pill:
            s.fg = th.foreground;
            break;
        case TabVariant::Segmented:
            s.fg = th.tabFg;
            break;
        case TabVariant::Underline:
            s.fg = th.tabFg;
            s.borderB = 2;
            break;
    }
    return s;
}

static TabStyle TabSelected(TabVariant v, const Theme& th) {
    TabStyle s = TabNormal(v, th);
    switch (v) {
        case TabVariant::Tab:
            s.fg = th.tabActiveFg;
            s.bg = th.tabActiveBg;
            s.borderColor = th.border;
            break;
        case TabVariant::Outline:
            s.fg = th.primary;
            s.borderColor = th.primary;
            break;
        case TabVariant::Pill:
            s.fg = th.primaryFg;
            s.bg = th.primary;
            break;
        case TabVariant::Segmented:
            s.fg = th.tabActiveFg;
            s.innerBg = th.background;
            s.shadow = true;
            break;
        case TabVariant::Underline:
            s.fg = th.tabActiveFg;
            s.borderColor = th.primary;
            break;
    }
    return s;
}

static TabStyle TabHovered(TabVariant v, bool selected, const Theme& th) {
    TabStyle s = TabNormal(v, th);
    switch (v) {
        case TabVariant::Tab:
            s.fg = th.tabActiveFg;
            break;
        case TabVariant::Outline:
            s.fg = th.secondaryFg;
            s.bg = th.secondaryHover;
            break;
        case TabVariant::Pill:
            s.fg = th.secondaryFg;
            s.bg = th.secondary;
            break;
        case TabVariant::Segmented:
            s.fg = th.tabActiveFg;
            s.innerBg = selected ? th.background : kTabNone;
            break;
        case TabVariant::Underline:
            s.fg = th.tabActiveFg;
            break;
    }
    return s;
}

static TabStyle TabDisabled(TabVariant v, bool selected, const Theme& th) {
    TabStyle s = TabNormal(v, th);
    s.fg = th.mutedFg;
    switch (v) {
        case TabVariant::Tab:
            s.borderColor = selected ? th.border : kTabNone;
            break;
        case TabVariant::Outline:
            s.borderColor = selected ? th.primary : th.border;
            break;
        case TabVariant::Pill:
            if (selected) {
                s.fg = RgbaOpacity(th.primaryFg, 0.5f);
                s.bg = RgbaOpacity(th.primary, 0.5f);
            }
            break;
        case TabVariant::Segmented:
            s.bg = th.tabBar;
            s.innerBg = selected ? th.background : kTabNone;
            break;
        case TabVariant::Underline:
            s.borderColor = selected ? th.border : kTabNone;
            break;
    }
    return s;
}

// TabBar::menu's "more" button: an xsmall ghost with a caret, whose dropdown
// is the tab list itself. The rows are the tabs in order, so the index the
// menu confirms is the index the bar's on_click wants.
static El* TabMenuButton(Tabs* tabs, const Theme&, float) {
    Ctx* cx = tabs->cx;
    Str menuId = StrDup(cx->a, fmt("%s-menu", tabs->id));
    Entity<PopupMenuState> st = PopupMenuStateFor(cx, menuId);
    if (PopupMenuState* s = st.Get(cx)) {
        s->onConfirm = tabs->onChange;
    }
    PopupMenu* menu = PopupMenu::New(cx, menuId, st)->Scrollable();
    int i = -1;
    for (const TabItem& it : tabs->items) {
        i++;
        // A tab with no label is an icon-only one; Rust puts the icon in the
        // row, and falls back to "Unnamed" when there is neither.
        if (it.label.s) {
            menu->MenuWithCheck(it.label, i == tabs->selected);
        } else if (it.icon != IconName::None) {
            menu->Element(IconEl(cx->a, it.icon, 16));
            menu->Checked(i == tabs->selected);
        } else {
            menu->MenuWithCheck(Tr("Dock.Unnamed"), i == tabs->selected);
        }
        if (it.disabled) {
            menu->Disabled(true);
        }
    }
    return DropdownMenu::New(cx, StrDup(cx->a, fmt("%s-more", tabs->id)))
        ->Trigger(Button::New(cx, StrDup(cx->a, fmt("%s-more-btn", tabs->id)))
                      ->Ghost()
                      ->WithSize(UiSize::XSmall)
                      ->DropdownCaret()
                      ->IntoEl())
        ->Menu(menu)
        ->AnchorRight()
        ->IntoEl();
}

El* Tabs::IntoEl() {
    const Theme& th = cx->theme();
    float h = TabHeight(variant, size);
    float innerH = TabInnerHeight(variant, size);
    float padX = TabPadX(variant, size);
    float gap = TabBarGap(variant, size);
    float barPadX = TabBarPadX(variant, size);
    float barRadius = TabBarRadius(variant, size, th.radius, th.radius * 1.5f);
    float radius = TabRadius(variant, size, th.radius, th.radius * 1.5f);
    float innerRadius =
        TabInnerRadius(variant, size, th.radius, th.radius * 1.5f);
    float font = UiFontPx(size);

    El* bar = gpui::Tabs::New(cx, id)
                  ->FlexRow()
                  ->ItemsCenter()
                  ->W(width)
                  ->H(h)
                  ->Radius(barRadius);
    if (variant == TabVariant::Tab) {
        bar->Bg(th.tokens.tabBar);
    } else if (variant == TabVariant::Segmented) {
        bar->Bg(th.tokens.tabBar);
    }
    if (barPadX > 0) {
        bar->PadX(barPadX);
    }
    // The strip's own bottom border, which the folder and underline looks
    // share; Rust draws it as an absolute child so the tabs sit over it.
    if (variant == TabVariant::Tab || variant == TabVariant::Underline) {
        bar->BorderB(1, th.border);
    }
    if (prefix) {
        bar->Child(prefix);
    }

    // h_flex().id("tabs").flex_1().overflow_x_hidden(): the strip gives way
    // before the bar does, so a suffix and the overflow menu keep their place
    // when there are more tabs than fit.
    El* strip = Div(a)->FlexRow()->ItemsCenter()->Flex1()->H(kFill)->ClipX();
    if (gap > 0) {
        strip->Gap(gap);
    }
    // Where the selected tab was last frame, and where the indicator has got
    // to on its way there. Both outlive the frame, so both are motion slots.
    auto* selBox = (Bounds*)MotionSlot(cx, MotionId(StrL("tab-sel"), id),
                                       (int)sizeof(Bounds));
    auto* stripBox = (Bounds*)MotionSlot(cx, MotionId(StrL("tab-strip"), id),
                                         (int)sizeof(Bounds));
    strip->BoundsOut(stripBox);
    // INDICATOR_SPRING: the selection moves again while the indicator is
    // still travelling — a run down a row of tabs — so it is sprung, and
    // slightly under critical so it arrives with a little of the overshoot
    // the eye reads as weight. A tenth of a pixel is arrived.
    Spring indSpring = SpringNew(kTabIndicatorMotionMs);
    indSpring.damping = 0.85f;
    indSpring.epsilon = 0.1f;
    float indX = 0;
    float indW = 0;
    bool sliding = false;
    if (selBox && selBox->w > 0) {
        indX = SpringValue(cx, MotionId(StrL("tab-ind-x"), id), selBox->x,
                           indSpring);
        indW = SpringValue(cx, MotionId(StrL("tab-ind-w"), id), selBox->w,
                           indSpring);
        // In flight: the tab it belongs to holds back its own selected look
        // while the indicator is on its way, which is what Rust does too.
        float dx = indX - selBox->x;
        float dw = indW - selBox->w;
        sliding = (dx < -0.5f || dx > 0.5f) || (dw < -0.5f || dw > 0.5f);
    }
    int i = -1;
    for (const TabItem& item : items) {
        i++;
        bool on = i == selected;
        TabStyle st = item.disabled      ? TabDisabled(variant, on, th)
                      : (on && !sliding) ? TabSelected(variant, th)
                                         : TabNormal(variant, th);
        if (on && sliding && !item.disabled) {
            // The label keeps the colour it is going to end up with; it is the
            // box behind it that the indicator is carrying.
            st.fg = TabSelected(variant, th).fg;
        }
        Str tabId = StrDup(a, fmt("%d", i));
        El* tab = gpui::Tab::New(
                      cx, tabId, item.disabled,
                      item.disabled ? Listener{} : ListenerArg(onChange, i))
                      ->FlexRow()
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->H(kFill)
                      ->Radius(radius);
        if (st.bg.a) {
            tab->Bg(st.bg);
        }
        // The first folder tab drops its left border, so the strip does not
        // open with a line down its edge.
        if (st.borderT > 0 && st.borderL > 0 && st.borderR > 0 &&
            st.borderB > 0) {
            // Edges::all: a ring rather than four rules, so the radius the
            // tab asked for is what the stroke follows.
            tab->Border(st.borderT, st.borderColor);
        } else {
            if (st.borderL > 0 && !(i == 0 && variant == TabVariant::Tab)) {
                tab->BorderL(st.borderL, st.borderColor);
            }
            if (st.borderR > 0) {
                tab->BorderR(st.borderR, st.borderColor);
            }
            if (st.borderB > 0) {
                tab->BorderB(st.borderB, st.borderColor);
            }
        }
        if (!item.disabled) {
            TabStyle hov = TabHovered(variant, on, th);
            if (hov.bg.a) {
                tab->HoverBg(hov.bg);
            }
            tab->HoverFg(hov.fg);
        }

        // The inner box is what carries the padding, the label and — for
        // Segmented — the background of the selected tab.
        El* inner = Div(a)
                        ->FlexRow()
                        ->ItemsCenter()
                        ->JustifyCenter()
                        ->H(innerH)
                        ->Radius(innerRadius);
        if (st.innerBg.a) {
            inner->Bg(st.innerBg);
        }
        // inner_margins: the underline's inner box stands off the border
        // above and below it. There is no margin here, so the tab pads the
        // box away from its own edges, which comes to the same thing.
        float marginTop = TabMarginTop(variant, size);
        if (marginTop > 0) {
            tab->PadT(marginTop)->PadB(TabMarginBottom(variant, size));
        }
        if (item.icon != IconName::None) {
            // An icon tab is square and exempt from max_width.
            inner->W(innerH * 1.25f)
                ->Child(IconEl(a, item.icon, UiIconPx(size))->Fg(st.fg));
        } else {
            if (padX > 0) {
                inner->PadX(padX);
            }
            El* label = TextEl(a, item.label)->Font(font)->Fg(st.fg);
            if (maxWidth > 0) {
                // Text takes its natural width, so capping a tab means
                // giving the label a box it is allowed to shrink inside.
                label->MaxW(maxWidth - padX * 2)->Truncate();
            }
            inner->Child(label);
        }
        // Tab::flex_1(). Upstream wraps every tab of a variant that has an
        // indicator — Segmented, Pill, Underline — in a
        // `div().flex_shrink_0().on_prepaint(..)` so it can measure it, and
        // the wrapper is what the bar lays out. The tab's own flex_1 then
        // only fills that wrapper, which is content-sized, so those three
        // variants never stretch however many tabs ask to. Reproduced rather
        // than fixed: the colour picker's Palette/HSLA pair and the tabs
        // story's "Filling Space" both look the way they do because of it.
        bool wrapped = variant == TabVariant::Segmented ||
                       variant == TabVariant::Pill ||
                       variant == TabVariant::Underline;
        if (item.flex1 && !wrapped) {
            tab->Flex1();
            inner->W(kFill);
        }
        tab->Child(inner);
        if (maxWidth > 0 && item.icon == IconName::None) {
            tab->MaxW(maxWidth);
        }
        if (on && selBox) {
            // The box the indicator is heading for, measured where it is.
            tab->BoundsOut(selBox);
        }
        strip->Child(tab);
    }
    if (sliding && stripBox) {
        // The indicator itself, which is whatever the selected tab would have
        // painted: the raised inner box for a folder or a segmented strip, the
        // filled pill, or the two-pixel rule under an underline tab.
        float x = indX - stripBox->x;
        El* ind = Div(a)->Absolute()->Left(x)->W(indW);
        switch (variant) {
            case TabVariant::Underline:
                ind->Bottom(0)->H(2)->Bg(th.tokens.primary);
                break;
            case TabVariant::Pill:
                ind->Top(0)->H(kFill)->Radius(99)->Bg(th.tokens.primary);
                break;
            case TabVariant::Segmented:
            case TabVariant::Tab:
                ind->Top((h - innerH) * 0.5f)
                    ->H(innerH)
                    ->Radius(innerRadius)
                    ->Bg(th.tokens.background);
                break;
            default:
                ind = nullptr;
                break;
        }
        if (ind) {
            strip->Child(ind);
        }
    }
    bar->Child(strip);
    if (menu) {
        bar->Child(TabMenuButton(this, th, font));
    }
    if (suffix) {
        bar->Child(suffix);
    }
    return bar;
}

} // namespace component
} // namespace gpui
