#include "base/switch.h"

namespace gpui {

SwitchStyles& SwitchStyles::Checked(const StateStyle& style) {
    StateStyleRefine(&checked, style);
    return *this;
}
SwitchStyles& SwitchStyles::Disabled(const StateStyle& style) {
    StateStyleRefine(&disabled, style);
    return *this;
}
SwitchTrackStyles& SwitchTrackStyles::Checked(const StateStyle& style) {
    StateStyleRefine(&checked, style);
    return *this;
}
SwitchTrackStyles& SwitchTrackStyles::Disabled(const StateStyle& style) {
    StateStyleRefine(&disabled, style);
    return *this;
}
SwitchThumbStyles& SwitchThumbStyles::Checked(const StateStyle& style) {
    StateStyleRefine(&checked, style);
    return *this;
}
SwitchThumbStyles& SwitchThumbStyles::Disabled(const StateStyle& style) {
    StateStyleRefine(&disabled, style);
    return *this;
}

static StateStyle ResolveSwitchStyle(bool checked, bool disabled,
                                     const StateStyle* checkedStyle,
                                     const StateStyle* disabledStyle,
                                     const StateStyle* instance) {
    StateStyle base = instance ? *instance : StateStyle{};
    const StateStyle* states[2] = {checked ? checkedStyle : nullptr,
                                   disabled ? disabledStyle : nullptr};
    return StateStyleResolve(base, states, 2);
}

El* Switch::New(Ctx* cx, Str id, bool checked, bool disabled,
                Listener onChange, const SwitchStyles* styles,
                const StateStyle* instance, Str accessibilityLabel,
                int tabIndex, bool tabStop, FocusHandle focus) {
    Arena* a = cx->a;
    El* e = Div(a)
                ->PathClick(id)
                ->Role(AccessibilityRole::Switch)
                ->AriaToggled(checked ? AccessibilityToggled::True
                                      : AccessibilityToggled::False)
                ->AriaDisabled(disabled);
    if (accessibilityLabel.s) {
        e->AriaLabel(accessibilityLabel);
    }
    if (styles || instance) {
        StateStyle resolved = ResolveSwitchStyle(
            checked, disabled, styles ? &styles->checked : nullptr,
            styles ? &styles->disabled : nullptr, instance);
        ElRefine(e, resolved);
    }
    if (disabled) {
        // Rust takes the left-button press and stops it here so an enclosing
        // row does not activate when its disabled switch is pressed.
        return e->StopMouseDown();
    }
    if (focus.IsValid()) {
        e->TrackFocus(focus);
    } else {
        e->PathId(id);
    }
    e->TabIndex(tabIndex)->TabStop(tabStop);
    if (onChange.IsValid()) {
        e->OnClick(ListenerFill(onChange, !checked));
    }
    return e;
}

El* SwitchTrack::New(Ctx* cx, Str id, bool checked, bool disabled,
                     const SwitchTrackStyles* styles,
                     const StateStyle* instance) {
    Arena* a = cx->a;
    El* e = Div(a)->Id(id);
    if (styles || instance) {
        StateStyle resolved = ResolveSwitchStyle(
            checked, disabled, styles ? &styles->checked : nullptr,
            styles ? &styles->disabled : nullptr, instance);
        ElRefine(e, resolved);
    }
    return e;
}

El* SwitchThumb::New(Ctx* cx, bool checked, bool disabled,
                     const SwitchThumbStyles* styles,
                     const StateStyle* instance) {
    Arena* a = cx->a;
    El* e = Div(a);
    if (styles || instance) {
        StateStyle resolved = ResolveSwitchStyle(
            checked, disabled, styles ? &styles->checked : nullptr,
            styles ? &styles->disabled : nullptr, instance);
        ElRefine(e, resolved);
    }
    return e;
}
} // namespace gpui
