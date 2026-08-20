/* Ported from crates/base/src/input/base/state.rs, mod tests, plus the two
 * cases in movement.rs's neighbourhood that are pure logic.
 *
 * Every Rust case there is a `#[gpui::test]` built on `TestAppContext` and a
 * `VisualTestContext`: it opens a window, paints it, and then asserts. The
 * ones ported here are the ones whose assertion does not need the window —
 * the text, the selection, and the undo history. Their scroll-offset and
 * `last_layout` halves are dropped, and so are the ones that only assert on
 * something this port does not have (number stepping, decorations, soft
 * wrap).
 *
 * The engine takes `App*` and `Window*` because it pauses a caret and asks
 * for a repaint; both are optional, so a test drives it with nulls. */

#include "Test.h"

static bool Is(Str got, const char* want) {
    int n = (int)strlen(want);
    return got.len == n && (n == 0 || memcmp(got.s, want, (size_t)n) == 0);
}

static bool ValueIs(const InputState& s, const char* want) {
    return Is(InputValue(&s), want);
}

static bool RangeIs(const InputState& s, int start, int end) {
    return s.selectedRange.start == start && s.selectedRange.end == end;
}

// The user typing, which is what goes through the same path a key press does.
static void Type(InputState* s, const char* text) {
    InputReplaceTextInRange(s, nullptr, nullptr, nullptr, Str(text));
}

static void Act(InputState* s, InputAction action) {
    InputPerform(s, nullptr, nullptr, action, false);
}

// The input method staging a candidate: replace_and_mark_text_in_range with
// no range, which is what each keystroke of a composition does.
static void Mark(InputState* s, const char* text) {
    InputReplaceAndMarkText(s, nullptr, nullptr, nullptr, Str(text), nullptr);
}

static bool MarkIs(const InputState& s, int start, int end) {
    Selection m = {};
    if (!InputMarkedRange(&s, &m)) {
        return start < 0;
    }
    return m.start == start && m.end == end;
}

static void SingleLineRemovesNewlines() {
    InputState s;
    InputSetValue(&s, StrL("default\nvalue"));
    utassert(ValueIs(s, "defaultvalue"));

    InputSetValue(&s, StrL("first\nsecond\r\nthird\rfourth"));
    utassert(ValueIs(s, "firstsecondthirdfourth"));

    InputSetValue(&s, Str{});
    utassert(ValueIs(s, ""));

    // A textarea keeps them.
    InputState multi;
    multi.kind = InputKind::Textarea;
    InputSetValue(&multi, StrL("first\nsecond"));
    utassert(ValueIs(multi, "first\nsecond"));
}

// set_value parks a single-line caret at the end (matching an HTML <input>)
// and a multi-line one at 0..0. The scroll half of the Rust case needs a
// painted window.
static void SetValueCaretAtEnd() {
    InputState s;
    InputSetValue(&s, StrL("https://example.com/v1/users"));
    utassert(RangeIs(s, 28, 28));

    InputState multi;
    multi.kind = InputKind::Textarea;
    InputSetValue(&multi, StrL("one\ntwo"));
    utassert(RangeIs(multi, 0, 0));
}

// replace_all does the same to the selection, but stays in the history.
static void ReplaceAllPreservesUndoHistory() {
    InputState s;
    InputSetValue(&s, StrL("hello"));
    InputReplaceAll(&s, nullptr, nullptr, StrL("world!"));
    utassert(ValueIs(s, "world!"));
    utassert(RangeIs(s, 6, 6));

    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, "hello"));

    // set_value, by contrast, clears the history: there is nothing to undo.
    InputSetValue(&s, StrL("fresh"));
    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, "fresh"));
}

static void SetSelectedRange() {
    InputState s;
    InputSetValue(&s, StrL("hello world"));

    InputSetSelectedRange(&s, nullptr, nullptr, 0, 5);
    utassert(RangeIs(s, 0, 5));
    utassert(Is(InputSelectedValue(&s), "hello"));

    InputSetSelectedRange(&s, nullptr, nullptr, 6, 11);
    utassert(Is(InputSelectedValue(&s), "world"));

    // clamped + collapsed
    InputSetSelectedRange(&s, nullptr, nullptr, 100, 100);
    utassert(RangeIs(s, 11, 11));
}

static void SetSelectedRangeClipsToUtf8Boundaries() {
    InputState s;
    InputSetValue(&s, StrL("éx"));

    // A non-empty range grows out to character boundaries...
    InputSetSelectedRange(&s, nullptr, nullptr, 0, 1);
    utassert(RangeIs(s, 0, 2));

    // ...an empty one clips back to the boundary before it.
    InputSetSelectedRange(&s, nullptr, nullptr, 1, 1);
    utassert(RangeIs(s, 0, 0));
}

static void AdjacentTypingCoalescesIntoOneUndo() {
    InputState s;
    Type(&s, "a");
    Type(&s, "b");
    Type(&s, "c");
    utassert(ValueIs(s, "abc"));

    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, ""));
    Act(&s, InputAction::Redo);
    utassert(ValueIs(s, "abc"));
}

// A cursor move ends the typing session, so the two runs undo separately.
static void CursorMovementSplitsTyping() {
    InputState s;
    Type(&s, "ab");
    Act(&s, InputAction::MoveToStart);
    Type(&s, "X");
    utassert(ValueIs(s, "Xab"));

    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, "ab"));
    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, ""));
}

static void BackwardAndForwardDeletesDoNotCoalesce() {
    InputState s;
    InputSetValue(&s, StrL("abcd"));
    InputSetSelectedRange(&s, nullptr, nullptr, 2, 2);

    Act(&s, InputAction::Backspace); // "acd"
    Act(&s, InputAction::Delete);    // "ad"
    utassert(ValueIs(s, "ad"));

    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, "acd"));
    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, "abcd"));
}

// Repeated deletes in the same direction do coalesce.
static void DirectionalCharacterDeletesCoalesce() {
    InputState s;
    InputSetValue(&s, StrL("abcd"));
    Act(&s, InputAction::Backspace);
    Act(&s, InputAction::Backspace);
    utassert(ValueIs(s, "ab"));

    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, "abcd"));
}

static void SelectedReplacementIsAtomic() {
    InputState s;
    InputSetValue(&s, StrL("hello world"));
    InputSetSelectedRange(&s, nullptr, nullptr, 0, 5);
    Type(&s, "bye");
    utassert(ValueIs(s, "bye world"));

    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, "hello world"));
    utassert(RangeIs(s, 0, 5));
}

static void ForwardDeleteRestoresCursor() {
    InputState s;
    InputSetValue(&s, StrL("abc"));
    InputSetSelectedRange(&s, nullptr, nullptr, 1, 1);
    Act(&s, InputAction::Delete);
    utassert(ValueIs(s, "ac"));

    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, "abc"));
    // The caret goes back to where it was, in front of what was deleted.
    utassert(RangeIs(s, 1, 1));
}

static void NoopEditPreservesRedo() {
    InputState s;
    Type(&s, "abc");
    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, ""));

    // Deleting at the start of an empty field changes nothing.
    Act(&s, InputAction::Backspace);
    Act(&s, InputAction::Redo);
    utassert(ValueIs(s, "abc"));
}

static void MaskedRedoRestoresActualCursor() {
    InputState s;
    InputSetMaskPattern(&s, MaskPatternNew(StrL("(999)999-9999")));
    Type(&s, "1234567890");
    utassert(ValueIs(s, "(123)456-7890"));

    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, ""));
    Act(&s, InputAction::Redo);
    utassert(ValueIs(s, "(123)456-7890"));
    // The caret is at the end of the masked text, not of what was typed.
    utassert(InputCursor(&s) == 13);
}

static void WordMovement() {
    InputState s;
    InputSetValue(&s, StrL("hello brave world"));

    Act(&s, InputAction::MoveToStart);
    Act(&s, InputAction::MoveToNextWord);
    utassert(InputCursor(&s) == 5);
    Act(&s, InputAction::MoveToNextWord);
    utassert(InputCursor(&s) == 11);

    Act(&s, InputAction::MoveToEnd);
    Act(&s, InputAction::MoveToPreviousWord);
    utassert(InputCursor(&s) == 12);
    Act(&s, InputAction::MoveToPreviousWord);
    utassert(InputCursor(&s) == 6);
}

static void DeleteToWordAndLineBoundaries() {
    InputState s;
    InputSetValue(&s, StrL("hello brave world"));
    Act(&s, InputAction::DeleteToPreviousWordStart);
    utassert(ValueIs(s, "hello brave "));

    InputSetValue(&s, StrL("hello brave world"));
    InputSetSelectedRange(&s, nullptr, nullptr, 6, 6);
    Act(&s, InputAction::DeleteToNextWordEnd);
    utassert(ValueIs(s, "hello  world"));

    InputSetValue(&s, StrL("hello world"));
    InputSetSelectedRange(&s, nullptr, nullptr, 5, 5);
    Act(&s, InputAction::DeleteToBeginningOfLine);
    utassert(ValueIs(s, " world"));

    InputSetValue(&s, StrL("hello world"));
    InputSetSelectedRange(&s, nullptr, nullptr, 5, 5);
    Act(&s, InputAction::DeleteToEndOfLine);
    utassert(ValueIs(s, "hello"));
}

// A single-line field's line is the whole document; a textarea's is not.
static void LineBoundaries() {
    InputState one;
    InputSetValue(&one, StrL("hello world"));
    InputSetSelectedRange(&one, nullptr, nullptr, 4, 4);
    utassert(InputStartOfLine(&one) == 0);
    utassert(InputEndOfLine(&one) == 11);

    InputState many;
    many.kind = InputKind::Textarea;
    InputSetValue(&many, StrL("one\ntwo\nthree"));
    InputSetSelectedRange(&many, nullptr, nullptr, 5, 5);
    utassert(InputStartOfLine(&many) == 4);
    utassert(InputEndOfLine(&many) == 7);

    // Down keeps the column, and coming back up returns to it even after
    // passing through a shorter line.
    InputSetSelectedRange(&many, nullptr, nullptr, 12, 12); // "three", col 4
    Act(&many, InputAction::MoveUp);
    utassert(InputCursor(&many) == 7); // "two" is shorter, so its end
    Act(&many, InputAction::MoveDown);
    utassert(InputCursor(&many) == 12);
}

// Boundaries step whole characters, not bytes.
static void BoundariesStepCharacters() {
    InputState s;
    InputSetValue(&s, StrL("a中b"));
    utassert(InputNextBoundary(&s, 0) == 1);
    utassert(InputNextBoundary(&s, 1) == 4);
    utassert(InputPreviousBoundary(&s, 4) == 1);
    utassert(InputPreviousBoundary(&s, 1) == 0);

    Act(&s, InputAction::MoveToEnd);
    Act(&s, InputAction::Backspace);
    utassert(ValueIs(s, "a中"));
    Act(&s, InputAction::Backspace);
    utassert(ValueIs(s, "a"));
}

static void SelectionFollowsTheDragDirection() {
    InputState s;
    InputSetValue(&s, StrL("hello world"));
    InputMoveTo(&s, nullptr, nullptr, 5);

    Act(&s, InputAction::SelectRight);
    utassert(RangeIs(s, 5, 6));
    utassert(InputCursor(&s) == 6);

    // Back past the anchor: the live end flips to the other side.
    Act(&s, InputAction::SelectLeft);
    Act(&s, InputAction::SelectLeft);
    utassert(RangeIs(s, 4, 5));
    utassert(InputCursor(&s) == 4);
}

// select_word / select_line, which is what a double and a triple click take.
static void SelectWordAndLine() {
    InputState s;
    s.kind = InputKind::Textarea;
    InputSetValue(&s, StrL("hello brave\nnew world"));

    InputSelectWord(&s, nullptr, nullptr, 7);
    utassert(Is(InputSelectedValue(&s), "brave"));

    InputSelectLine(&s, nullptr, nullptr, 14);
    utassert(Is(InputSelectedValue(&s), "new world"));
}

// The word a double click took stays whole while the drag goes on.
static void DraggingCannotEatIntoTheSelectedWord() {
    InputState s;
    InputSetValue(&s, StrL("hello brave world"));
    InputSelectWord(&s, nullptr, nullptr, 7);
    utassert(RangeIs(s, 6, 11));

    InputSelectTo(&s, nullptr, nullptr, 8);
    utassert(RangeIs(s, 6, 11));

    InputSelectTo(&s, nullptr, nullptr, 15);
    utassert(RangeIs(s, 6, 15));
}

// readonly and disabled reject what the user does, not what the program does.
static void ReadonlyRejectsUserEditsOnly() {
    InputState s;
    InputSetValue(&s, StrL("hello"));
    s.readonly = true;

    Type(&s, "X");
    utassert(ValueIs(s, "hello"));
    Act(&s, InputAction::Backspace);
    utassert(ValueIs(s, "hello"));
    // Typing and the input method go through the same handler, so a readonly
    // field refuses a composition as flatly as it refuses a keystroke.
    Mark(&s, "\xE3\x81\x82"); // U+3042 HIRAGANA A
    utassert(ValueIs(s, "hello"));
    utassert(MarkIs(s, -1, -1));

    InputSetValue(&s, StrL("set anyway"));
    utassert(ValueIs(s, "set anyway"));
    InputInsert(&s, nullptr, nullptr, StrL("!"));
    utassert(ValueIs(s, "set anyway!"));
}

// Enter is a submit in a single-line field and a newline in a textarea,
// unless the textarea submits on Enter — then only Shift+Enter breaks a line.
static void EnterInsertsANewlineOnlyWhereItShould() {
    InputState one;
    utassert(!InputPerform(&one, nullptr, nullptr, InputAction::Enter, false));
    utassert(ValueIs(one, ""));

    InputState many;
    many.kind = InputKind::Textarea;
    utassert(InputPerform(&many, nullptr, nullptr, InputAction::Enter, false));
    utassert(ValueIs(many, "\n"));

    InputState chat;
    chat.kind = InputKind::Textarea;
    chat.submitOnEnter = true;
    utassert(!InputPerform(&chat, nullptr, nullptr, InputAction::Enter, false));
    utassert(ValueIs(chat, ""));
    utassert(InputPerform(&chat, nullptr, nullptr, InputAction::Enter, true));
    utassert(ValueIs(chat, "\n"));
}

// A mask rejects a character it has no room for and reformats as it fills.
static void MaskFormatsWhileTyping() {
    InputState s;
    InputSetMaskPattern(&s, MaskPatternNew(StrL("(999)999-9999")));
    // The cue comes from the pattern.
    utassert(Is(s.placeholder, "(___)___-____"));

    Type(&s, "1");
    utassert(ValueIs(s, "(1"));
    // A separator only appears once something follows it, so the ")" is
    // not written until the fourth digit arrives.
    Type(&s, "23");
    utassert(ValueIs(s, "(123"));
    Type(&s, "4567890");
    utassert(ValueIs(s, "(123)456-7890"));

    // A letter has no token to land on, so the edit is rejected.
    Type(&s, "A");
    utassert(ValueIs(s, "(123)456-7890"));
}

// The keymap state.rs::init installs, off macOS.
static void ActionForKey() {
    InputState s;
    utassert(InputActionForKey(&s, KeyLeft, false, false, false) ==
             InputAction::MoveLeft);
    utassert(InputActionForKey(&s, KeyLeft, true, false, false) ==
             InputAction::SelectLeft);
    utassert(InputActionForKey(&s, KeyLeft, false, true, false) ==
             InputAction::MoveToPreviousWord);
    utassert(InputActionForKey(&s, KeyRight, true, true, false) ==
             InputAction::SelectToNextWordEnd);
    utassert(InputActionForKey(&s, KeyHome, false, false, false) ==
             InputAction::MoveHome);
    utassert(InputActionForKey(&s, KeyHome, true, false, false) ==
             InputAction::SelectToStartOfLine);
    utassert(InputActionForKey(&s, KeyHome, false, true, false) ==
             InputAction::MoveToStart);
    utassert(InputActionForKey(&s, KeyBack, false, true, false) ==
             InputAction::DeleteToPreviousWordStart);
    utassert(InputActionForKey(&s, KeyDelete, false, false, false) ==
             InputAction::Delete);
    utassert(InputActionForKey(&s, KeyA, false, true, false) ==
             InputAction::SelectAll);
    utassert(InputActionForKey(&s, KeyZ, false, true, false) ==
             InputAction::Undo);
    utassert(InputActionForKey(&s, KeyZ, true, true, false) ==
             InputAction::Redo);
    utassert(InputActionForKey(&s, KeyY, false, true, false) ==
             InputAction::Redo);
    // Without the modifier a letter is text, not an action.
    utassert(InputActionForKey(&s, KeyA, false, false, false) ==
             InputAction::None);
}

// mode.rs LayoutMode: rows, and the clamp an auto-growing one applies.
static void LayoutModeRowsClamp() {
    LayoutMode plain;
    plain.rows = 5;
    utassert(LayoutModeRows(plain) == 5);
    utassert(LayoutModeMinRows(plain) == 1);

    LayoutMode grow;
    grow.kind = LayoutModeKind::AutoGrow;
    grow.minRows = 2;
    grow.maxRows = 5;
    grow.rows = 2;
    utassert(LayoutModeRows(grow) == 2);
    utassert(LayoutModeMinRows(grow) == 2);

    LayoutModeSetRows(&grow, 4);
    utassert(LayoutModeRows(grow) == 4);
    LayoutModeSetRows(&grow, 1);
    utassert(LayoutModeRows(grow) == 2);
    LayoutModeSetRows(&grow, 10);
    utassert(LayoutModeRows(grow) == 5);
}

// kind.rs: the kind decides whether an input is multi-line, not the row count.
static void KindDoesNotFollowTheRowCount() {
    InputState s;
    s.kind = InputKind::Textarea;
    s.mode.kind = LayoutModeKind::AutoGrow;
    s.mode.minRows = 1;
    s.mode.maxRows = 1;
    utassert(InputIsMultiLine(&s));

    InputState one;
    one.mode.rows = 4;
    utassert(InputIsSingleLine(&one));
}

// A field twenty lines tall inside a box that shows five of them.
static void SeedScroll(InputState* s) {
    s->lastLineH = 20;
    s->viewH = 100;
    s->viewW = 200;
    s->contentH = 400;
    s->contentW = 600;
}

static void ScrollToBringsTheCaretIntoView() {
    InputState s;
    SeedScroll(&s);
    // A caret inside the box moves nothing.
    InputScrollToCaret(&s, 0, 40, InputMoveDir::None);
    utassertnear(s.scrollY, 0.f);

    // Past the bottom: the line comes in with a line's clearance under it.
    InputScrollToCaret(&s, 0, 200, InputMoveDir::None);
    utassertnear(s.scrollY, 140.f);

    // Back above the top: a line's clearance over it.
    InputScrollToCaret(&s, 0, 100, InputMoveDir::None);
    utassertnear(s.scrollY, 80.f);
}

static void AVerticalWalkDoesNotFightItself() {
    InputState s;
    SeedScroll(&s);
    s.scrollY = 140;
    // Rust clamps the answer by the direction the caret went: a move up is
    // never answered by scrolling down...
    InputScrollToCaret(&s, 0, 300, InputMoveDir::Up);
    utassertnear(s.scrollY, 140.f);
    // ...and a move down is never answered by scrolling up.
    InputScrollToCaret(&s, 0, 40, InputMoveDir::Down);
    utassertnear(s.scrollY, 140.f);
}

static void TheOffsetStaysInsideTheContent() {
    InputState s;
    SeedScroll(&s);
    // The last line cannot pull the view past the end of the text.
    InputScrollToCaret(&s, 0, 10000, InputMoveDir::None);
    utassertnear(s.scrollY, 300.f);
    // Nor can the first pull it above the start.
    InputScrollToCaret(&s, 0, 0, InputMoveDir::None);
    utassertnear(s.scrollY, 0.f);
}

static void ASidewaysCaretPullsTheRunAcross() {
    InputState s;
    SeedScroll(&s);
    // Past the right edge, with the margin Rust keeps.
    InputScrollToCaret(&s, 400, 0, InputMoveDir::None);
    utassertnear(s.scrollX, 205.f);
    // And back to the left edge.
    InputScrollToCaret(&s, 100, 0, InputMoveDir::None);
    utassertnear(s.scrollX, 95.f);
    // Never past the end of the run.
    InputScrollToCaret(&s, 100000, 0, InputMoveDir::None);
    utassertnear(s.scrollX, 400.f);
}

static void TheNumberKeysStepTheField() {
    StepAction action = StepAction::Decrement;
    utassert(NumberStepForKey(KeyUp, &action));
    utassert(action == StepAction::Increment);
    utassert(NumberStepForKey(KeyDown, &action));
    utassert(action == StepAction::Decrement);
    // Anything else is the field's own.
    utassert(!NumberStepForKey(KeyLeft, &action));
    utassert(!NumberStepForKey(KeyReturn, &action));
}


// replace_and_mark_text_in_range: each candidate stands in for the last, and
// the range that is marked is what the next one replaces. The Rust case is
// `undo_with_ime_input`, typed the way a pinyin IME types 你.
static void ACompositionReplacesItselfUntilItCommits() {
    InputState s;
    Type(&s, "prefix ");
    Mark(&s, "n");
    utassert(ValueIs(s, "prefix n"));
    utassert(MarkIs(s, 7, 8));
    Mark(&s, "ni");
    utassert(ValueIs(s, "prefix ni"));
    utassert(MarkIs(s, 7, 9));
    Mark(&s, "\xE4\xBD\xA0"); // U+4F60, three bytes
    utassert(ValueIs(s, "prefix \xE4\xBD\xA0"));
    utassert(MarkIs(s, 7, 10));
    // The caret sits at the end of the marked run while it is being composed.
    utassert(RangeIs(s, 10, 10));

    InputUnmarkText(&s, nullptr, nullptr);
    utassert(MarkIs(s, -1, -1));
    Type(&s, " suffix");
    utassert(ValueIs(s, "prefix \xE4\xBD\xA0 suffix"));
}

// The whole composition is one undo step: the candidates were staging posts,
// not edits the user made.
static void ACompositionUndoesAsOneThing() {
    InputState s;
    Type(&s, "prefix ");
    Mark(&s, "n");
    Mark(&s, "ni");
    Mark(&s, "\xE4\xBD\xA0");
    InputUnmarkText(&s, nullptr, nullptr);
    Type(&s, " suffix");
    utassert(ValueIs(s, "prefix \xE4\xBD\xA0 suffix"));

    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, "prefix \xE4\xBD\xA0"));
    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, "prefix "));
    Act(&s, InputAction::Undo);
    utassert(ValueIs(s, ""));
}

// An empty insert is the composition being abandoned: the staged text goes,
// the caret goes back where it started, and nothing is left marked.
static void AnAbandonedCompositionLeavesNothingBehind() {
    InputState s;
    Type(&s, "ab");
    Mark(&s, "ni");
    utassert(ValueIs(s, "abni"));
    Mark(&s, "");
    utassert(ValueIs(s, "ab"));
    utassert(RangeIs(s, 2, 2));
    utassert(MarkIs(s, -1, -1));
}

// A commit that names no range of its own replaces the marked text rather
// than the selection — which is how the platform hands over a result string.
static void ACommitReplacesWhatWasMarked() {
    InputState s;
    Type(&s, "ab");
    Mark(&s, "ni");
    Type(&s, "\xE4\xBD\xA0");
    utassert(ValueIs(s, "ab\xE4\xBD\xA0"));
    utassert(MarkIs(s, -1, -1));
}

void TestInputState() {
    TestSuite("input_state");
    SingleLineRemovesNewlines();
    SetValueCaretAtEnd();
    ReplaceAllPreservesUndoHistory();
    SetSelectedRange();
    SetSelectedRangeClipsToUtf8Boundaries();
    AdjacentTypingCoalescesIntoOneUndo();
    CursorMovementSplitsTyping();
    BackwardAndForwardDeletesDoNotCoalesce();
    DirectionalCharacterDeletesCoalesce();
    SelectedReplacementIsAtomic();
    ForwardDeleteRestoresCursor();
    NoopEditPreservesRedo();
    MaskedRedoRestoresActualCursor();
    WordMovement();
    DeleteToWordAndLineBoundaries();
    LineBoundaries();
    BoundariesStepCharacters();
    SelectionFollowsTheDragDirection();
    SelectWordAndLine();
    DraggingCannotEatIntoTheSelectedWord();
    ReadonlyRejectsUserEditsOnly();
    ACompositionReplacesItselfUntilItCommits();
    ACompositionUndoesAsOneThing();
    AnAbandonedCompositionLeavesNothingBehind();
    ACommitReplacesWhatWasMarked();
    EnterInsertsANewlineOnlyWhereItShould();
    MaskFormatsWhileTyping();
    ActionForKey();
    LayoutModeRowsClamp();
    KindDoesNotFollowTheRowCount();
    ScrollToBringsTheCaretIntoView();
    AVerticalWalkDoesNotFightItself();
    TheOffsetStaysInsideTheContent();
    ASidewaysCaretPullsTheRunAcross();
    TheNumberKeysStepTheField();
}
