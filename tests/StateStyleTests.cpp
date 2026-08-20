/* Ported from crates/base/src/state_style.rs, and the priority tests a
 * control keeps for it in crates/base/src/button.rs.
 *
 * A state style is a partial style: it says something about the fields it
 * names and nothing about the rest. resolve_style lays them down in one fixed
 * order — the instance, then the value states, then disabled — so no control
 * gets to decide the order for itself. */

#include "Test.h"

static bool Same(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static const Rgba kInstance = Rgb(0x11, 0x22, 0x33);
static const Rgba kSelected = Rgb(0x44, 0x55, 0x66);
static const Rgba kDisabled = Rgb(0x77, 0x88, 0x99);

// instance_style_is_the_baseline_when_no_state_is_active.
static void TheInstanceStyleIsTheBaseline() {
    StateStyle instance;
    instance.Bg(kInstance);
    StateStyle out = StateStyleResolve(instance, nullptr, 0);
    utassert(out.Has(StateFieldBg));
    utassert(Same(out.style.bg, kInstance));
}

// an_active_state_overrides_the_instance_style.
static void AnActiveStateOverridesTheInstance() {
    StateStyle instance;
    instance.Bg(kInstance);
    StateStyle selected;
    selected.Bg(kSelected);
    const StateStyle* states[] = {&selected};
    StateStyle out = StateStyleResolve(instance, states, 1);
    utassert(Same(out.style.bg, kSelected));
}

// later_states_override_earlier_states: disabled is last, so it wins.
static void LaterStatesOverrideEarlierOnes() {
    StateStyle instance;
    instance.Bg(kInstance);
    StateStyle selected;
    selected.Bg(kSelected);
    StateStyle disabled;
    disabled.Bg(kDisabled);
    const StateStyle* states[] = {&selected, &disabled};
    StateStyle out = StateStyleResolve(instance, states, 2);
    utassert(Same(out.style.bg, kDisabled));
}

// states_only_override_the_fields_they_set.
static void AStateOnlyOverridesWhatItNames() {
    StateStyle instance;
    instance.Bg(kInstance).Radius(8);
    StateStyle disabled;
    disabled.Bg(kDisabled);
    const StateStyle* states[] = {&disabled};
    StateStyle out = StateStyleResolve(instance, states, 1);
    utassert(Same(out.style.bg, kDisabled));
    // The radius came from the instance and nothing said otherwise.
    utassert(out.Has(StateFieldRadius));
    utassertnear(out.style.radius, 8.f);
}

// button.rs disabled_style_applies_only_while_disabled_and_then_wins: a state
// that is not active is left out of the list rather than resolved to nothing.
static void ADisabledStyleOnlyAppliesWhileDisabled() {
    StateStyle instance;
    instance.Bg(kInstance);
    StateStyle disabled;
    disabled.Bg(kDisabled);

    const StateStyle* enabled[] = {nullptr};
    utassert(Same(StateStyleResolve(instance, enabled, 1).style.bg, kInstance));

    const StateStyle* off[] = {&disabled};
    utassert(Same(StateStyleResolve(instance, off, 1).style.bg, kDisabled));

    // And with no instance style of its own, the state is all there is.
    StateStyle bare;
    utassert(Same(StateStyleResolve(bare, off, 1).style.bg, kDisabled));
}

// button.rs selected_disabled_and_instance_styles_follow_the_shared_priority.
static void SelectedAndDisabledFollowTheSharedPriority() {
    StateStyle selected;
    selected.Bg(kSelected);
    StateStyle disabled;
    disabled.Bg(kDisabled);
    auto resolve = [&](bool isSelected, bool isDisabled) {
        const StateStyle* states[2] = {isSelected ? &selected : nullptr,
                                       isDisabled ? &disabled : nullptr};
        return StateStyleResolve(StateStyle{}, states, 2);
    };
    utassert(!resolve(false, false).Has(StateFieldBg));
    utassert(Same(resolve(true, false).style.bg, kSelected));
    utassert(Same(resolve(true, true).style.bg, kDisabled));
    utassert(Same(resolve(false, true).style.bg, kDisabled));
}

// refine_style: what the resolved style does to the element it is put on, and
// nothing beyond it.
static void AResolvedStyleGoesOntoTheElement() {
    Arena* a = ArenaNew();
    StateStyle s;
    s.Bg(kSelected).Border(2, kDisabled);
    El* e = Div(a)->Radius(4);
    ElRefine(e, s);
    utassert(e->style.hasBg && Same(e->style.bg, kSelected));
    utassertnear(e->style.border, 2.f);
    utassert(Same(e->style.borderColor, kDisabled));
    // The radius was never named, so the element keeps its own.
    utassertnear(e->style.radius, 4.f);
    ArenaDelete(a);
}

void TestStateStyle() {
    TestSuite("state_style");
    TheInstanceStyleIsTheBaseline();
    AnActiveStateOverridesTheInstance();
    LaterStatesOverrideEarlierOnes();
    AStateOnlyOverridesWhatItNames();
    ADisabledStyleOnlyAppliesWhileDisabled();
    SelectedAndDisabledFollowTheSharedPriority();
    AResolvedStyleGoesOntoTheElement();
}
