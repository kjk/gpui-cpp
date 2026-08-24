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
    utassert(Same(out.style.bg.color, kInstance));
}

// an_active_state_overrides_the_instance_style.
static void AnActiveStateOverridesTheInstance() {
    StateStyle instance;
    instance.Bg(kInstance);
    StateStyle selected;
    selected.Bg(kSelected);
    const StateStyle* states[] = {&selected};
    StateStyle out = StateStyleResolve(instance, states, 1);
    utassert(Same(out.style.bg.color, kSelected));
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
    utassert(Same(out.style.bg.color, kDisabled));
}

// states_only_override_the_fields_they_set.
static void AStateOnlyOverridesWhatItNames() {
    StateStyle instance;
    instance.Bg(kInstance).Radius(8);
    StateStyle disabled;
    disabled.Bg(kDisabled);
    const StateStyle* states[] = {&disabled};
    StateStyle out = StateStyleResolve(instance, states, 1);
    utassert(Same(out.style.bg.color, kDisabled));
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
    utassert(Same(StateStyleResolve(instance, enabled, 1).style.bg.color,
                  kInstance));

    const StateStyle* off[] = {&disabled};
    utassert(
        Same(StateStyleResolve(instance, off, 1).style.bg.color, kDisabled));

    // And with no instance style of its own, the state is all there is.
    StateStyle bare;
    utassert(Same(StateStyleResolve(bare, off, 1).style.bg.color, kDisabled));
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
    utassert(Same(resolve(true, false).style.bg.color, kSelected));
    utassert(Same(resolve(true, true).style.bg.color, kDisabled));
    utassert(Same(resolve(false, true).style.bg.color, kDisabled));
}

// refine_style: what the resolved style does to the element it is put on, and
// nothing beyond it. It lands at layout rather than where the call sits, so
// the element carries the refinement until then.
static void AResolvedStyleGoesOntoTheElement() {
    Arena* a = ArenaNew();
    StateStyle s;
    s.Bg(kSelected).Border(2, kDisabled);
    El* e = Div(a)->Radius(4);
    ElRefine(e, s);
    utassert(e->refineSet == s.set);
    StyleApplyFields(&e->style, e->refine, e->refineSet);
    utassert(e->style.hasBg && Same(e->style.bg.color, kSelected));
    utassertnear(e->style.border, 2.f);
    utassert(Same(e->style.borderColor, kDisabled));
    // The radius was never named, so the element keeps its own.
    utassertnear(e->style.radius, 4.f);
    ArenaDelete(a);
}

// The order resolve_style promises: the instance style is underneath, and a
// semantic state wins over it — including the part of the instance style the
// caller chained on *after* the primitive handed the element back, which is
// the only order a builder can offer.
static void AStateWinsOverWhatIsChainedAfterIt() {
    Arena* a = ArenaNew();
    StateStyle s;
    s.Bg(kDisabled);
    El* e = ElRefine(Div(a), s)->Bg(kSelected)->Radius(6);
    utassert(Same(e->style.bg.color, kSelected));
    StyleApplyFields(&e->style, e->refine, e->refineSet);
    utassert(Same(e->style.bg.color, kDisabled));
    utassertnear(e->style.radius, 6.f);
    ArenaDelete(a);
}

// The three fills a box can paint, and which one the pointer picks. GPUI
// refines the hovered style and then the active one over the base, so a box
// that is both hovered and held paints its pressed fill.
static void TheHeldBoxPaintsItsPressedFill() {
    utassert(BoxFillFor(true, true, 7, 7, 7) == BoxFill::Active);
    utassert(BoxFillFor(true, true, 7, 0, 7) == BoxFill::Hover);
    utassert(BoxFillFor(true, true, 7, 7, 0) == BoxFill::Active);
    utassert(BoxFillFor(true, true, 7, 0, 0) == BoxFill::Base);
}

// A press that slides off keeps the pressed fill and loses the hover: the
// window holds `activeId` from the press to the release, where `hoverId`
// follows the pointer.
static void APressThatSlidOffStaysPressed() {
    utassert(BoxFillFor(true, true, 7, 7, 8) == BoxFill::Active);
    // And the box it slid onto is neither: nothing else can be held.
    utassert(BoxFillFor(true, true, 8, 7, 8) == BoxFill::Hover);
}

// Both states need a click id of their own, the way the hover always has:
// without one the box matches the 0 that means nothing is hovered or held.
static void ABoxWithNoClickIdIsNeitherHoveredNorHeld() {
    utassert(BoxFillFor(true, true, 0, 0, 0) == BoxFill::Base);
}

// A box that never named a pressed fill falls through to the hover it did
// name, which is every element in the tree that predates `activeBg`.
static void AFillThatWasNeverNamedIsNotPainted() {
    utassert(BoxFillFor(false, true, 7, 7, 7) == BoxFill::Hover);
    utassert(BoxFillFor(false, false, 7, 7, 7) == BoxFill::Base);
    utassert(BoxFillFor(true, false, 7, 7, 7) == BoxFill::Active);
}

// StyleRefinement::refine over the pressed fill, which is what lets a
// control state name one.
static void AStateCanNameThePressedFill() {
    StateStyle instance;
    instance.Bg(kInstance);
    StateStyle over;
    over.ActiveBg(kSelected);
    const StateStyle* states[1] = {&over};
    StateStyle out = StateStyleResolve(instance, states, 1);
    utassert(out.Has(StateFieldActiveBg));
    utassert(Same(out.style.activeBg.color, kSelected));
    // And it says nothing about the fill underneath it.
    utassert(Same(out.style.bg.color, kInstance));
}

void TestStateStyle() {
    TestSuite("state_style");
    TheHeldBoxPaintsItsPressedFill();
    APressThatSlidOffStaysPressed();
    ABoxWithNoClickIdIsNeitherHoveredNorHeld();
    AFillThatWasNeverNamedIsNotPainted();
    AStateCanNameThePressedFill();
    TheInstanceStyleIsTheBaseline();
    AnActiveStateOverridesTheInstance();
    LaterStatesOverrideEarlierOnes();
    AStateOnlyOverridesWhatItNames();
    ADisabledStyleOnlyAppliesWhileDisabled();
    SelectedAndDisabledFollowTheSharedPriority();
    AResolvedStyleGoesOntoTheElement();
    AStateWinsOverWhatIsChainedAfterIt();
}
