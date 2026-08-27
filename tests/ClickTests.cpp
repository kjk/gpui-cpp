/* The rule GPUI's on_click follows, which crates/base's controls are all
 * written against: the click comes from the release, not the press.
 *
 * Interactivity holds the press as `pending_mouse_down` and fires the click
 * from its mouse-up handler, where the button has to match and the pointer
 * has to still be over the element that took the press. checkbox.rs's
 * `enter_and_space_each_emit_once` and the button tests both lean on it —
 * a reader who presses a control and slides off has not clicked it. */

#include "Test.h"

static void AReleaseOnTheElementThatTookThePressIsAClick() {
    utassert(ClickFromRelease(true, 7, MouseButton::Left, false, 7,
                              MouseButton::Left));
    // The page itself is an element too: press and release on nothing is the
    // outside click an overlay dismisses on.
    utassert(ClickFromRelease(true, 0, MouseButton::Left, false, 0,
                              MouseButton::Left));
}

static void APressThatSlidOffIsNoClick() {
    // Off the button and onto the page.
    utassert(!ClickFromRelease(true, 7, MouseButton::Left, false, 0,
                               MouseButton::Left));
    // Onto a different element.
    utassert(!ClickFromRelease(true, 7, MouseButton::Left, false, 8,
                               MouseButton::Left));
    // And the other way about: a press on the page that came up over a button
    // does not click the button.
    utassert(!ClickFromRelease(true, 0, MouseButton::Left, false, 7,
                               MouseButton::Left));
}

// GPUI checks `event.button == mouse_down.button`, so a chord — one button
// down, the other released — is not a click for either of them.
static void OnlyTheButtonThatWentDownMakesTheClick() {
    utassert(!ClickFromRelease(true, 7, MouseButton::Left, false, 7,
                               MouseButton::Right));
    utassert(!ClickFromRelease(true, 7, MouseButton::Right, false, 7,
                               MouseButton::Left));
    utassert(ClickFromRelease(true, 7, MouseButton::Right, false, 7,
                              MouseButton::Right));
}

// A drag takes the release: GPUI hands the up to the drop and the click never
// runs, so dropping a tab where it came from is not also a click on it.
static void ADragTakesTheReleaseFromTheClick() {
    utassert(!ClickFromRelease(true, 7, MouseButton::Left, true, 7,
                               MouseButton::Left));
}

// pending_mouse_down being None: the scrollbar, the inspector and a
// non-focusing press each take the press for themselves, and the release that
// follows is nobody's click.
static void AReleaseWithNoPressWaitingIsNothing() {
    utassert(!ClickFromRelease(false, 7, MouseButton::Left, false, 7,
                               MouseButton::Left));
    utassert(!ClickFromRelease(false, 0, MouseButton::Left, false, 0,
                               MouseButton::Left));
}

// The keyboard half of the same rule, which div.rs arms on the enter or space
// key down and fires from the key up. `enter_and_space_use_one_native_keyboard
// _click_each` in button.rs sends each keystroke as a down and an up and
// expects one click out of the pair.
static void TheReleaseOfEnterOrSpaceOnTheFocusedElementIsAClick() {
    utassert(ClickFromKeyRelease(true, 3, 3, KeyReturn, false));
    utassert(ClickFromKeyRelease(true, 3, 3, KeySpace, false));
    // Nothing armed the press — the focused field ate the space as text, or
    // the chord ran an action — so the release makes nothing.
    utassert(!ClickFromKeyRelease(false, 3, 3, KeyReturn, false));
}

// A key that does not activate anything, coming up mid-press: GPUI treats
// that as an unclean activation and drops the pending press.
static void AnotherKeyComingUpIsNoClick() {
    utassert(!ClickFromKeyRelease(true, 3, 3, KeyEscape, false));
    utassert(!ClickFromKeyRelease(true, 3, 3, KeyTab, false));
}

// `!stroke.modifiers.modified()`: Ctrl-Enter is a shortcut, not the button
// being pressed.
static void AModifierMakesItAShortcutRatherThanAnActivation() {
    utassert(!ClickFromKeyRelease(true, 3, 3, KeyReturn, true));
    utassert(!ClickFromKeyRelease(true, 3, 3, KeySpace, true));
}

// The focus generation, which is what GPUI stamps the pending press with
// rather than the focus handle: focus that moved between the two halves — even
// away and back to the same element — leaves the release with no click to
// make.
static void FocusThatMovedBetweenTheHalvesTakesTheClick() {
    utassert(!ClickFromKeyRelease(true, 3, 4, KeyReturn, false));
    utassert(!ClickFromKeyRelease(true, 4, 3, KeySpace, false));
}

namespace {
struct MouseDownRecorder {
    int calls = 0;

    static void OnDown(MouseDownRecorder* self, Ctx*,
                       const MouseDownEvent*) {
        self->calls++;
    }
};
} // namespace

// Tab::render and a disabled Toggle install a left-button mouse-down handler
// that only calls cx.stop_propagation(). The port represents that handler on
// the hitbox so it does not need to invent a widget-owned callback entity.
static void AControlCanOwnThePressWithoutACallback() {
    App app;
    Window* win = new Window();
    win->app = &app;
    Entity<MouseDownRecorder> recorder =
        EntityNewState<MouseDownRecorder>(&app);

    HitRect outer = {};
    outer.bounds = {0, 0, 100, 100};
    outer.onMouseDown = ListenTo(recorder, &MouseDownRecorder::OnDown);
    win->paint.hits.Append(outer);

    HitRect inner = {};
    inner.bounds = {10, 10, 50, 50};
    inner.parent = 0;
    inner.stopMouseDown = true;
    win->paint.hits.Append(inner);

    PlatformInput press = {};
    press.kind = PlatformInputKind::MouseDown;
    press.mouseDown.button = MouseButton::Left;
    press.mouseDown.x = 20;
    press.mouseDown.y = 20;
    WindowDispatchInput(win, &press);
    utassert(recorder.Get(&app)->calls == 0);

    // Rust registers the barrier for MouseButton::Left, not every pointer
    // button. A right press still bubbles to the enclosing element.
    press.mouseDown.button = MouseButton::Right;
    WindowDispatchInput(win, &press);
    utassert(recorder.Get(&app)->calls == 1);

    win->paint.hits.Reset();
    delete win;
    EntityDropAll(&app);
}

// CheckboxState::activated: mixed follows unchecked rather than cycling
// through a third value.
static void CheckboxActivationProducesTheControlledNextState() {
    utassert(CheckboxActivated(CheckboxState::Unchecked) ==
             CheckboxState::Checked);
    utassert(CheckboxActivated(CheckboxState::Indeterminate) ==
             CheckboxState::Checked);
    utassert(CheckboxActivated(CheckboxState::Checked) ==
             CheckboxState::Unchecked);
}

// Switch::on_change receives the next controlled bool, and a disabled switch
// owns the press so an enclosing settings row cannot activate instead.
static void SwitchActivationProducesTheControlledNextValue() {
    Arena* a = ArenaNew();
    Ctx cx = {};
    cx.a = a;
    Listener change = {};
    change.fn = (void*)1;
    El* off = Switch::New(&cx, StrL("off"), false, false, change);
    El* on = Switch::New(&cx, StrL("on"), true, false, change);
    El* disabled = Switch::New(&cx, StrL("disabled"), false, true,
                               change);
    utassert(off->listener.IsValid() && off->listener.hasArg &&
             off->listener.arg == 1);
    utassert(on->listener.IsValid() && on->listener.hasArg &&
             on->listener.arg == 0);
    utassert(!disabled->listener.IsValid() && disabled->stopMouseDown);
    ArenaDelete(a);
}

void TestClick() {
    TestSuite("click");
    AReleaseOnTheElementThatTookThePressIsAClick();
    APressThatSlidOffIsNoClick();
    OnlyTheButtonThatWentDownMakesTheClick();
    ADragTakesTheReleaseFromTheClick();
    AReleaseWithNoPressWaitingIsNothing();
    TheReleaseOfEnterOrSpaceOnTheFocusedElementIsAClick();
    AnotherKeyComingUpIsNoClick();
    AModifierMakesItAShortcutRatherThanAnActivation();
    FocusThatMovedBetweenTheHalvesTakesTheClick();
    AControlCanOwnThePressWithoutACallback();
    CheckboxActivationProducesTheControlledNextState();
    SwitchActivationProducesTheControlledNextValue();
}
