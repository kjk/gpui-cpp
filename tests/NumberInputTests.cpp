/* Ported from crates/base/src/number_input.rs step_value.
 *
 * Rust's own mod tests there is all #[gpui::test]: they drive step buttons
 * through a window to check that a press never moves focus off the editor.
 * step_value itself is pure, and it is where the behavior worth pinning is —
 * the precision it keeps, the range it clamps into, and its refusal to report
 * a step that did not move. */

#include "Test.h"

static Str Stepped(const char* value, StepAction action, double step) {
    static char out[64];
    out[0] = 0;
    if (!NumberStepValue(Str(value), action, step, false, 0, false, 0, out,
                         (int)sizeof(out))) {
        return {};
    }
    return Str(out);
}

static bool Same(Str got, const char* want) {
    return got.s && StrEqI(got, Str(want));
}

static void AStepKeepsTheTextsPrecision() {
    // The integer stays an integer, and the two decimals stay two.
    utassert(Same(Stepped("1", StepAction::Increment, 1), "2"));
    utassert(Same(Stepped("1.50", StepAction::Increment, 1), "2.50"));
    utassert(Same(Stepped("1.5", StepAction::Decrement, 1), "0.5"));
    // A fractional step widens an integer to the step's own precision.
    utassert(Same(Stepped("1", StepAction::Increment, 0.01), "1.01"));
    utassert(Same(Stepped("1234.56", StepAction::Decrement, 0.01), "1234.55"));
}

static void TextThatIsNotANumberStepsFromZero() {
    // Rust's `current.unwrap_or(0.)`, and with no current there is nothing to
    // compare against, so the step always reports.
    utassert(Same(Stepped("", StepAction::Increment, 1), "1"));
    utassert(Same(Stepped("abc", StepAction::Decrement, 1), "-1"));
    // Trailing junk is not a number: parse::<f64>() refuses it.
    utassert(Same(Stepped("12px", StepAction::Increment, 1), "1"));
}

static void TheRangeClampsAndWidens() {
    char out[64];
    utassert(NumberStepValue(StrL("9"), StepAction::Increment, 5, false, 0,
                             true, 10.5, out, (int)sizeof(out)));
    // Clamped to the max, whose own precision widens the result.
    utassert(StrEqI(Str(out), StrL("10.5")));

    utassert(NumberStepValue(StrL("1"), StepAction::Decrement, 5, true, 0.25,
                             false, 0, out, (int)sizeof(out)));
    utassert(StrEqI(Str(out), StrL("0.25")));
}

static void AStepThatDoesNotMoveIsNoStep() {
    char out[64];
    // Already at the max: the clamp puts it back where it started.
    utassert(!NumberStepValue(StrL("10"), StepAction::Increment, 1, false, 0,
                              true, 10, out, (int)sizeof(out)));
    utassert(!NumberStepValue(StrL("0"), StepAction::Decrement, 1, true, 0,
                              false, 0, out, (int)sizeof(out)));
    // A zero step moves nothing in either direction.
    utassert(!NumberStepValue(StrL("3"), StepAction::Increment, 0, false, 0,
                              false, 0, out, (int)sizeof(out)));
}

static El* FindNamed(El* root, const char* name) {
    if (!root) {
        return nullptr;
    }
    if (root->id.s && StrEqI(root->id, Str(name))) {
        return root;
    }
    for (El* c = root->first; c; c = c->next) {
        if (El* hit = FindNamed(c, name)) {
            return hit;
        }
    }
    return nullptr;
}

// `InputBase::new(("number-input", state.entity_id()))` wrapping
// `Button::new("decrement")` and `Button::new("increment")`: the step buttons
// are named only among their siblings, and the frame is what tells one
// spinbutton's from another's. The port spelled the caller's id into each
// button instead; now the frame carries it and the fold does the rest.
static void TwoSpinbuttonsHaveTwoIncrements() {
    App app;
    Window* win = new Window();
    win->app = &app;
    Arena* a = ArenaNew();
    Ctx cx = {};
    cx.app = &app;
    cx.win = win;
    cx.a = a;

    InputState one;
    InputState two;
    El* page = Div(a);
    page->Child(component::NumberInput::New(&cx, StrL("first"), &one)
                    ->IntoEl());
    page->Child(component::NumberInput::New(&cx, StrL("second"), &two)
                    ->IntoEl());
    IdsCollect(page);

    El* incOne = FindNamed(page->first, "increment");
    El* incTwo = FindNamed(page->first->next, "increment");
    utassert(incOne && incTwo);
    utassert(incOne->clickId != 0 && incTwo->clickId != 0);
    utassert(incOne->clickId != incTwo->clickId);
    // And the two buttons of one frame are not each other.
    El* decOne = FindNamed(page->first, "decrement");
    utassert(decOne && decOne->clickId != incOne->clickId);

    ArenaDelete(a);
    delete win;
    EntityDropAll(&app);
}

void TestNumberInput() {
    TestSuite("number_input");
    TwoSpinbuttonsHaveTwoIncrements();
    AStepKeepsTheTextsPrecision();
    TextThatIsNotANumberStepsFromZero();
    TheRangeClampsAndWidens();
    AStepThatDoesNotMoveIsNoStep();
}
