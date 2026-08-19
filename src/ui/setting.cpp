#include "ui/setting.h"
#include "ui/button.h"
#include "ui/input.h"

namespace gpui {

namespace component {

// to_lowercase().contains(q): the query against one string, ignoring case.
static bool ContainsCI(Str hay, Str needle) {
    if (needle.len <= 0) {
        return true;
    }
    if (!hay.s || hay.len < needle.len) {
        return false;
    }
    for (int i = 0; i + needle.len <= hay.len; i++) {
        int j = 0;
        while (j < needle.len) {
            char a = hay.s[i + j];
            char b = needle.s[j];
            if (a >= 'A' && a <= 'Z') {
                a = (char)(a + 32);
            }
            if (b >= 'A' && b <= 'Z') {
                b = (char)(b + 32);
            }
            if (a != b) {
                break;
            }
            j++;
        }
        if (j == needle.len) {
            return true;
        }
    }
    return false;
}

bool SettingItemMatches(const SettingItem* it, Str query) {
    if (query.len <= 0) {
        return true;
    }
    if (ContainsCI(it->title, query) || ContainsCI(it->description, query)) {
        return true;
    }
    for (int i = 0; i < it->nKeywords; i++) {
        if (ContainsCI(it->keywords[i], query)) {
            return true;
        }
    }
    return false;
}

bool SettingGroupMatches(const SettingGroup* g, Str query) {
    if (query.len <= 0) {
        return true;
    }
    // A group is shown when anything in it is: Rust drops a group whose
    // filtered items came out empty.
    for (int i = 0; i < g->n; i++) {
        if (SettingItemMatches(&g->items[i], query)) {
            return true;
        }
    }
    return false;
}

bool SettingPageMatches(const SettingPage* p, Str query) {
    if (query.len <= 0) {
        return true;
    }
    for (int i = 0; i < p->n; i++) {
        if (SettingGroupMatches(&p->groups[i], query)) {
            return true;
        }
    }
    return false;
}

void SettingsState::OnPageClick(SettingsState* self, Ctx* cx, const ClickEvent*,
                                intptr_t page) {
    self->page = (int)page;
    self->group = -1;
    Notify(cx);
}

void SettingsState::OnGroupClick(SettingsState* self, Ctx* cx,
                                 const ClickEvent*, intptr_t packed) {
    self->page = (int)(packed / 64);
    self->group = (int)(packed % 64);
    Notify(cx);
}

Settings* Settings::New(Ctx* cx, Str id, Entity<SettingsState> state) {
    Arena* a = cx->a;
    Settings* s = ArenaNew<Settings>(a);
    s->a = a;
    s->cx = cx;
    s->id = id;
    s->state = state;
    return s;
}

Settings* Settings::Page(Str title, IconName icon, Str description) {
    if (n < kMaxSettingPages) {
        pages[n].title = title;
        pages[n].icon = icon;
        pages[n].description = description;
        n++;
    }
    return this;
}

Settings* Settings::Group(Str title, Str description) {
    if (n == 0) {
        Page(StrL("Settings"));
    }
    SettingPage& p = pages[n - 1];
    if (p.n < kMaxSettingGroups) {
        p.groups[p.n].title = title;
        p.groups[p.n].description = description;
        p.n++;
    }
    return this;
}

Settings* Settings::Item(Str title, Str description, El* control) {
    if (n == 0) {
        Group({});
    }
    SettingPage& p = pages[n - 1];
    if (p.n == 0) {
        Group({});
    }
    SettingGroup& g = p.groups[p.n - 1];
    if (g.n < kMaxSettingItems) {
        g.items[g.n] = {};
        g.items[g.n].title = title;
        g.items[g.n].description = description;
        g.items[g.n].control = control;
        g.n++;
    }
    return this;
}

// The item last added, which is what every modifier below reads.
static SettingItem* LastItem(Settings* s) {
    if (s->n == 0) {
        return nullptr;
    }
    SettingPage& p = s->pages[s->n - 1];
    if (p.n == 0) {
        return nullptr;
    }
    SettingGroup& g = p.groups[p.n - 1];
    return g.n > 0 ? &g.items[g.n - 1] : nullptr;
}

Settings* Settings::Keywords(Str a1, Str a2, Str a3) {
    SettingItem* it = LastItem(this);
    if (it) {
        it->nKeywords = 0;
        if (a1.s) {
            it->keywords[it->nKeywords++] = a1;
        }
        if (a2.s) {
            it->keywords[it->nKeywords++] = a2;
        }
        if (a3.s) {
            it->keywords[it->nKeywords++] = a3;
        }
    }
    return this;
}

Settings* Settings::Disabled(bool v) {
    SettingItem* it = LastItem(this);
    if (it) {
        it->disabled = v;
    }
    return this;
}

Settings* Settings::Resettable(bool dirty, Listener onReset) {
    SettingItem* it = LastItem(this);
    if (it) {
        it->dirty = dirty;
        it->onReset = onReset;
    }
    return this;
}

Settings* Settings::Layout(Axis axis) {
    SettingItem* it = LastItem(this);
    if (it) {
        it->layout = axis;
    }
    return this;
}

Settings* Settings::Searchable(InputState* s, Listener onFocus) {
    search = s;
    onSearchFocus = onFocus;
    return this;
}
Settings* Settings::SidebarWidth(float v) {
    sidebarWidth = v;
    return this;
}
Settings* Settings::H(float v) {
    h = v;
    return this;
}
Settings* Settings::Bordered(bool v) {
    bordered = v;
    return this;
}

// One row: the title and description on the left, the field on the right —
// or under it, when the item asked for a vertical layout.
static El* RenderItem(Ctx* cx, const SettingItem& it, Str id, bool first) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* line = Div(a)->W(kFill)->PadX(16)->PadY(12)->Gap(16);
    if (it.layout == Axis::Horizontal) {
        line->FlexRow()->ItemsCenter()->JustifyBetween();
    } else {
        line->FlexCol();
    }
    if (!first) {
        line->BorderT(1, th.border);
    }
    El* text = Div(a)->FlexCol()->Grow()->Gap(4);
    text->Child(TextEl(a, it.title)
                    ->Font(16)
                    ->Fg(it.disabled ? th.mutedFg : th.foreground));
    if (it.description.s) {
        text->Child(
            TextEl(a, it.description)->Font(14)->Fg(th.mutedFg)->Wrap());
    }
    line->Child(text);
    El* right = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    if (it.control) {
        right->Child(it.control);
    }
    // The reset button, which is only there once the item has been changed.
    if (it.dirty && it.onReset.IsValid()) {
        // Rust's reset button carries an Undo2 icon; the nearest one this
        // tree has is the arrow that points back.
        right->Child(Button::New(cx, StrDup(a, fmt("%s-reset", id)))
                         ->Icon(IconName::ArrowLeft)
                         ->Ghost()
                         ->WithSize(UiSize::XSmall)
                         ->OnClick(it.onReset)
                         ->IntoEl());
    }
    line->Child(right);
    return line;
}

El* Settings::IntoEl() {
    const Theme& th = cx->theme();
    SettingsState* st = state.Get(cx);
    Str query = search ? InputValue(search) : Str{};

    El* row = Div(a)->FlexRow()->W(kFill)->H(h)->ItemsStart();

    // The sidebar: the search field, then a row per page, with the groups of
    // the open page under it.
    El* side = Div(a)
                   ->FlexCol()
                   ->W(sidebarWidth)
                   ->H(kFill)
                   ->Pad(8)
                   ->Gap(4)
                   ->BorderR(1, th.border);
    if (search) {
        side->Child(Input::New(cx, StrDup(a, fmt("%s-search", id)), search)
                        ->Prefix(Div(a)->PadL(10)->Child(
                            IconEl(a, IconName::Search, 16)->Fg(th.mutedFg)))
                        ->WithSize(UiSize::Small)
                        ->OnFocus(onSearchFocus)
                        ->IntoEl());
    }
    int selected = st ? st->page : 0;
    for (int i = 0; i < n; i++) {
        const SettingPage& p = pages[i];
        if (!SettingPageMatches(&p, query)) {
            continue;
        }
        bool active = i == selected;
        El* item = Div(a)
                       ->FlexRow()
                       ->W(kFill)
                       ->H(32)
                       ->PadX(8)
                       ->Gap(8)
                       ->ItemsCenter()
                       ->Radius(th.radius)
                       ->HoverBg(th.muted);
        if (active) {
            item->Bg(th.accent);
        }
        if (p.icon != IconName::None) {
            item->Child(IconEl(a, p.icon, 16)->Fg(th.foreground));
        }
        item->Child(Div(a)->Grow()->ClipY()->Child(TextEl(a, p.title)
                                                       ->Font(16)
                                                       ->Fg(th.foreground)
                                                       ->MaxW(sidebarWidth - 80)
                                                       ->Truncate()));
        if (p.n > 0) {
            item->Child(
                IconEl(a,
                       active ? IconName::ChevronDown : IconName::ChevronRight,
                       16)
                    ->Fg(th.mutedFg));
        }
        BindClick(item, StrDup(a, fmt("%s-page-%d", id, i)),
                  ListenTo(state, &SettingsState::OnPageClick, (intptr_t)i));
        side->Child(item);
        // click_to_open: the open page lists its groups under it, and each
        // one jumps to that part of the page.
        if (!active) {
            continue;
        }
        for (int g = 0; g < p.n; g++) {
            if (!SettingGroupMatches(&p.groups[g], query)) {
                continue;
            }
            El* sub = Div(a)
                          ->FlexRow()
                          ->W(kFill)
                          ->H(32)
                          ->PadL(28)
                          ->ItemsCenter()
                          ->Radius(th.radius)
                          ->HoverBg(th.muted);
            if (st && st->group == g) {
                sub->Bg(RgbaOpacity(th.accent, 0.6f));
            }
            sub->Child(
                TextEl(a, p.groups[g].title)->Font(16)->Fg(th.foreground));
            BindClick(sub, StrDup(a, fmt("%s-group-%d-%d", id, i, g)),
                      ListenTo(state, &SettingsState::OnGroupClick,
                               (intptr_t)(i * 64 + g)));
            side->Child(sub);
        }
    }
    row->Child(side);

    // The page: its title, then a card per group.
    El* pane = Div(a)->FlexCol()->Grow()->H(kFill)->ClipY();
    if (selected >= 0 && selected < n) {
        const SettingPage& p = pages[selected];
        El* head =
            Div(a)->FlexCol()->W(kFill)->PadX(16)->PadY(12)->Gap(4)->BorderB(
                1, th.border);
        head->Child(
            TextEl(a, p.title)->Font(20)->Semibold()->Fg(th.foreground));
        if (p.description.s) {
            head->Child(
                TextEl(a, p.description)->Font(14)->Fg(th.mutedFg)->Wrap());
        }
        pane->Child(head);

        El* body = Div(a)->FlexCol()->W(kFill)->Pad(16)->Gap(8);
        for (int g = 0; g < p.n; g++) {
            const SettingGroup& grp = p.groups[g];
            if (!SettingGroupMatches(&grp, query)) {
                continue;
            }
            if (grp.title.s) {
                body->Child(
                    TextEl(a, grp.title)->Font(14)->Fg(th.mutedFg)->PadY(4));
            }
            El* card = Div(a)->FlexCol()->W(kFill)->Radius(th.radiusLg);
            if (bordered) {
                card->Border(1, th.border);
            }
            int shown = 0;
            for (int i = 0; i < grp.n; i++) {
                const SettingItem& it = grp.items[i];
                if (!SettingItemMatches(&it, query)) {
                    continue;
                }
                card->Child(RenderItem(
                    cx, it, StrDup(a, fmt("%s-%d-%d-%d", id, selected, g, i)),
                    shown == 0));
                shown++;
            }
            body->Child(card);
        }
        pane->Child(body);
    }
    row->Child(pane);
    return row;
}

} // namespace component
} // namespace gpui
