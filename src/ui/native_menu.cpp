#include "ui/native_menu.h"
#include "gpui/platform.h"

namespace gpui {

namespace component {

NativeMenu* NativeMenu::New(Ctx* cx) {
    Arena* a = cx->a;
    NativeMenu* m = ArenaNew<NativeMenu>(a);
    m->a = a;
    m->cx = cx;
    return m;
}

static NativeMenuItem* PushItem(NativeMenu* m) {
    if (m->n >= kNativeMenuMaxItems) {
        return nullptr;
    }
    return &m->items[m->n++];
}

NativeMenu* NativeMenu::Menu(Str label, intptr_t id) {
    return MenuWithDisabled(label, false, id);
}
NativeMenu* NativeMenu::MenuWithDisabled(Str label, bool disabled,
                                         intptr_t id) {
    NativeMenuItem* it = PushItem(this);
    if (it) {
        it->kind = NativeMenuItemKind::Item;
        it->label = label;
        it->disabled = disabled;
        it->id = id;
    }
    return this;
}
NativeMenu* NativeMenu::MenuWithCheck(Str label, bool checked, intptr_t id) {
    NativeMenuItem* it = PushItem(this);
    if (it) {
        it->kind = NativeMenuItemKind::Item;
        it->label = label;
        it->checked = checked;
        it->id = id;
    }
    return this;
}
NativeMenu* NativeMenu::MenuWithIcon(Str label, IconName icon, intptr_t id) {
    NativeMenuItem* it = PushItem(this);
    if (it) {
        it->kind = NativeMenuItemKind::Item;
        it->label = label;
        it->icon = icon;
        it->id = id;
    }
    return this;
}
NativeMenu* NativeMenu::Separator() {
    NativeMenuItem* it = PushItem(this);
    if (it) {
        it->kind = NativeMenuItemKind::Separator;
    }
    return this;
}
NativeMenu* NativeMenu::Submenu(Str label, NativeMenu* menu) {
    NativeMenuItem* it = PushItem(this);
    if (it) {
        it->kind = NativeMenuItemKind::Submenu;
        it->label = label;
        it->submenu = menu;
    }
    return this;
}
NativeMenu* NativeMenu::OnSelect(Listener l) {
    onSelect = l;
    return this;
}

int NativeMenuSelectable(const NativeMenu* m, const NativeMenuItem** out,
                         int cap) {
    if (!m) {
        return 0;
    }
    int n = 0;
    for (int i = 0; i < m->n; i++) {
        const NativeMenuItem& it = m->items[i];
        if (it.kind == NativeMenuItemKind::Separator) {
            continue;
        }
        if (it.kind == NativeMenuItemKind::Submenu) {
            // A submenu row reports nothing itself; the rows under it are
            // numbered where they are built, which is right after it.
            n += NativeMenuSelectable(it.submenu, out ? out + n : nullptr,
                                      cap - n);
            continue;
        }
        // A greyed row cannot be chosen, so it is given no id at all.
        if (it.disabled) {
            continue;
        }
        if (out && n < cap) {
            out[n] = &it;
        }
        n++;
    }
    return n;
}

// The rows as the platform takes them: the same tree, with every row that can
// be chosen numbered by its place in the selectable order, so the id the OS
// answers with is an index back into that table.
static PlatMenuItem* ToPlat(Arena* a, const NativeMenu* m, int* nextId) {
    if (!m || m->n == 0) {
        return nullptr;
    }
    auto* out = (PlatMenuItem*)a->Push((uint64_t)m->n * sizeof(PlatMenuItem),
                                       alignof(PlatMenuItem), true);
    for (int i = 0; i < m->n; i++) {
        const NativeMenuItem& it = m->items[i];
        PlatMenuItem& p = out[i];
        p.label = StrDup(a, it.label).s;
        p.disabled = it.disabled;
        p.checked = it.checked;
        if (it.kind == NativeMenuItemKind::Separator) {
            p.separator = true;
            continue;
        }
        if (it.kind == NativeMenuItemKind::Submenu) {
            p.submenu = ToPlat(a, it.submenu, nextId);
            p.submenuN = it.submenu ? it.submenu->n : 0;
            continue;
        }
        if (!it.disabled) {
            p.id = (*nextId)++;
        }
    }
    return out;
}

bool NativeMenu::Show(float x, float y) {
    if (n == 0 || !PlatHasMenu()) {
        return false;
    }
    int nextId = 1;
    PlatMenuItem* plat = ToPlat(a, this, &nextId);
    bool dark = cx->themeMode() == ThemeMode::Dark;
    // The OS runs its own tracking loop, so this comes back once the menu is
    // gone and the answer is in hand.
    int chosen = PlatShowMenu(cx->win, plat, n, x, y, dark);
    if (chosen <= 0) {
        return true;
    }
    const NativeMenuItem* table[kNativeMenuMaxItems * 4] = {};
    int count = NativeMenuSelectable(this, table, kNativeMenuMaxItems * 4);
    if (chosen > count || !table[chosen - 1]) {
        return true;
    }
    ClickEvent ev = {};
    ListenerCall(cx->app, cx->win,
                 ListenerFill(onSelect, table[chosen - 1]->id), &ev);
    return true;
}

PopupMenu* NativeMenu::IntoPopupMenu(Str id) const {
    PopupMenu* menu = PopupMenu::New(cx, id);
    for (int i = 0; i < n; i++) {
        const NativeMenuItem& it = items[i];
        if (it.kind == NativeMenuItemKind::Separator) {
            menu->Separator();
            continue;
        }
        if (it.kind == NativeMenuItemKind::Submenu) {
            menu->Submenu(it.label,
                          it.submenu ? it.submenu->IntoPopupMenu(id) : nullptr);
            menu->Disabled(it.disabled);
            continue;
        }
        if (it.checked) {
            menu->MenuWithCheck(it.label, it.checked);
        } else {
            menu->Menu(it.label, it.icon);
        }
        menu->Disabled(it.disabled);
    }
    return menu;
}

} // namespace component
} // namespace gpui
