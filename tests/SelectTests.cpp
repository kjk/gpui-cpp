/* Ported from crates/base/src/select.rs.
 *
 * Rust binds up, down, enter and escape in the select's key context and hangs
 * an on_action off each. Every one of those handlers is a few lines of rules
 * over `open` and `disabled`; SelectActionForKey is that table, and this pins
 * it. The focus transfer each handler also does needs a pair of focus handles
 * a select does not have here. */

#include "Test.h"

static void ArrowsOpenAClosedSelect() {
    utassert(SelectActionForKey(KeyDown, false, false) == SelectAction::Open);
    utassert(SelectActionForKey(KeyUp, false, false) == SelectAction::Open);
    // Once open the root is done with them: Rust has focused the content by
    // then, so the list takes the arrow.
    utassert(SelectActionForKey(KeyDown, true, false) == SelectAction::None);
    utassert(SelectActionForKey(KeyUp, true, false) == SelectAction::None);
}

static void EnterOpensThenConfirms() {
    utassert(SelectActionForKey(KeyReturn, false, false) == SelectAction::Open);
    utassert(SelectActionForKey(KeyReturn, true, false) ==
             SelectAction::Confirm);
}

static void EscapeOnlyCountsWhileOpen() {
    utassert(SelectActionForKey(KeyEscape, true, false) ==
             SelectAction::Dismiss);
    // Closed, Rust propagates it so whatever encloses the select can use it.
    utassert(SelectActionForKey(KeyEscape, false, false) == SelectAction::None);
}

static void ADisabledSelectAnswersToNothing() {
    utassert(SelectActionForKey(KeyDown, false, true) == SelectAction::None);
    utassert(SelectActionForKey(KeyUp, true, true) == SelectAction::None);
    utassert(SelectActionForKey(KeyReturn, true, true) == SelectAction::None);
    utassert(SelectActionForKey(KeyEscape, true, true) == SelectAction::None);
}

static void OtherKeysAreNotTheSelects() {
    utassert(SelectActionForKey(KeyTab, true, false) == SelectAction::None);
    utassert(SelectActionForKey(KeySpace, true, false) == SelectAction::None);
    utassert(SelectActionForKey(KeyBack, false, false) == SelectAction::None);
}

void TestSelect() {
    TestSuite("select");
    ArrowsOpenAClosedSelect();
    EnterOpensThenConfirms();
    EscapeOnlyCountsWhileOpen();
    ADisabledSelectAnswersToNothing();
    OtherKeysAreNotTheSelects();
}
