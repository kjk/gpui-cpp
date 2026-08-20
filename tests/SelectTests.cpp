/* Ported from crates/base/src/select.rs.
 *
 * Rust binds up, down, enter, secondary-enter and escape in the select's key
 * context and hangs an on_action off each. Every one of those handlers is a
 * few lines of rules over `open` and `disabled`; this walks the chord in
 * through the keymap and pins what comes out. The focus transfer each handler
 * also does needs a pair of focus handles a select does not have here — the
 * trigger and the query field are both under the element that declares the
 * context instead. */

#include "Test.h"

// The chord, resolved in the select's context, read as what the select does.
static SelectAction ForChord(const char* spec, bool open, bool disabled) {
    SelectInitKeys();
    KeyChord c = {};
    utassert(KeyChordParse(Str(spec), &c));
    uint32_t ctx = KeyContextOf(SelectContext());
    return SelectActionOf(KeymapMatch(c, &ctx, 1).action, open, disabled);
}

static void ArrowsOpenAClosedSelect() {
    utassert(ForChord("down", false, false) == SelectAction::Open);
    utassert(ForChord("up", false, false) == SelectAction::Open);
    // Once open the root is done with them: Rust has focused the content by
    // then, so the list takes the arrow.
    utassert(ForChord("down", true, false) == SelectAction::None);
    utassert(ForChord("up", true, false) == SelectAction::None);
}

static void EnterOpensThenConfirms() {
    // secondary-enter is Confirm { secondary: true } in Rust, which has no
    // payload to carry here — it is its own name and the same answer.
    utassert(ForChord("secondary-enter", true, false) == SelectAction::Confirm);
    utassert(ForChord("enter", false, false) == SelectAction::Open);
    utassert(ForChord("enter", true, false) == SelectAction::Confirm);
}

static void EscapeOnlyCountsWhileOpen() {
    utassert(ForChord("escape", true, false) == SelectAction::Dismiss);
    // Closed, Rust propagates it so whatever encloses the select can use it.
    utassert(ForChord("escape", false, false) == SelectAction::None);
}

static void ADisabledSelectAnswersToNothing() {
    utassert(ForChord("down", false, true) == SelectAction::None);
    utassert(ForChord("up", true, true) == SelectAction::None);
    utassert(ForChord("enter", true, true) == SelectAction::None);
    utassert(ForChord("escape", true, true) == SelectAction::None);
}

static void OtherKeysAreNotTheSelects() {
    utassert(ForChord("tab", true, false) == SelectAction::None);
    utassert(ForChord("space", true, false) == SelectAction::None);
    utassert(ForChord("backspace", false, false) == SelectAction::None);
}

void TestSelect() {
    TestSuite("select");
    ArrowsOpenAClosedSelect();
    EnterOpensThenConfirms();
    EscapeOnlyCountsWhileOpen();
    ADisabledSelectAnswersToNothing();
    OtherKeysAreNotTheSelects();
}
