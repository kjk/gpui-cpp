#include "base/state_style.h"

namespace gpui {

StateStyle& StateStyle::Bg(Rgba c) {
    style.bg = c;
    style.hasBg = true;
    set |= StateFieldBg;
    return *this;
}
StateStyle& StateStyle::Fg(Rgba c) {
    style.color = c;
    style.hasColor = true;
    set |= StateFieldFg;
    return *this;
}
StateStyle& StateStyle::Border(float w, Rgba c) {
    style.border = w;
    style.borderColor = c;
    set |= StateFieldBorder;
    return *this;
}
StateStyle& StateStyle::Radius(float v) {
    style.radius = v;
    set |= StateFieldRadius;
    return *this;
}
StateStyle& StateStyle::HoverBg(Rgba c) {
    style.hoverBg = c;
    style.hasHoverBg = true;
    set |= StateFieldHoverBg;
    return *this;
}
StateStyle& StateStyle::HoverFg(Rgba c) {
    style.hoverFg = c;
    style.hasHoverFg = true;
    set |= StateFieldHoverFg;
    return *this;
}

void StateStyleRefine(StateStyle* into, const StateStyle& over) {
    if (over.Has(StateFieldBg)) {
        into->Bg(over.style.bg);
    }
    if (over.Has(StateFieldFg)) {
        into->Fg(over.style.color);
    }
    if (over.Has(StateFieldBorder)) {
        into->Border(over.style.border, over.style.borderColor);
    }
    if (over.Has(StateFieldRadius)) {
        into->Radius(over.style.radius);
    }
    if (over.Has(StateFieldHoverBg)) {
        into->HoverBg(over.style.hoverBg);
    }
    if (over.Has(StateFieldHoverFg)) {
        into->HoverFg(over.style.hoverFg);
    }
}

StateStyle StateStyleResolve(const StateStyle& instance,
                             const StateStyle* const* states, int n) {
    StateStyle out;
    StateStyleRefine(&out, instance);
    for (int i = 0; i < n; i++) {
        if (states[i]) {
            StateStyleRefine(&out, *states[i]);
        }
    }
    return out;
}

El* ElRefine(El* e, const StateStyle& s) {
    if (!e) {
        return e;
    }
    if (s.Has(StateFieldBg)) {
        e->Bg(s.style.bg);
    }
    if (s.Has(StateFieldFg)) {
        e->Fg(s.style.color);
    }
    if (s.Has(StateFieldBorder)) {
        e->Border(s.style.border, s.style.borderColor);
    }
    if (s.Has(StateFieldRadius)) {
        e->Radius(s.style.radius);
    }
    if (s.Has(StateFieldHoverBg)) {
        e->HoverBg(s.style.hoverBg);
    }
    if (s.Has(StateFieldHoverFg)) {
        e->HoverFg(s.style.hoverFg);
    }
    return e;
}

} // namespace gpui
