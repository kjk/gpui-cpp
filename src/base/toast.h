/* Unstyled toast — crates/base/src/toast.rs */

#include "gpui/gpui.h"

namespace gpui {

// Where a toast is in its life. Rust's ToastTransitionStatus.
enum class ToastStatus : uint8_t {
    // Animating in. Its timeout has not started counting yet.
    Starting,
    // Up and readable. This is the only state the timeout runs in.
    Present,
    // Animating out, on its way to being dropped.
    Ending
};

struct ToastEntry {
    int id = 0;
    ToastStatus status = ToastStatus::Starting;
    // Rust's Option<Duration>: a toast with none stays until it is dismissed.
    bool hasTimeout = false;
    int timeoutRemainingMs = 0;
    // transition_elapsed: how long the current animation has run.
    int elapsedMs = 0;
};

// How many a stack holds. Rust's VecDeque grows; a screen's worth is the real
// bound, and a toast past that would be off it anyway.
const int kToastStackCap = 16;

// ToastMotion::sonner(): the numbers the shadcn/Sonner toaster stacks by.
const int kToastTransitionMs = 400;
const int kToastExitMs = 200;
// The visible sliver of each toast under the front one, the gap between them
// once the stack opens, how much narrower each layer behind is, and how many
// of them show at all.
const float kToastCollapsedPeek = 14.f;
const float kToastExpandedGap = 14.f;
const float kToastCollapsedScaleStep = 0.05f;
const int kToastCollapsedVisible = 3;

struct ToastStackState {
    ToastEntry entries[kToastStackCap] = {};
    int n = 0;
    // How long the in and out animations run.
    int transitionMs = kToastTransitionMs;
    int exitMs = kToastExitMs;
    // is_expanded: a stack the pointer is over, or one that holds the focus,
    // opens out into a list.
    bool hovered = false;
    bool focused = false;

    bool IsExpanded() const { return hovered || focused; }
};

// stack_geometry: where each toast sits, closed and open, and how tall the
// stack is either way. `heights` runs oldest first, so the last one is the
// front. A stack anchored at the bottom of the window grows upwards, which
// flips every offset.
//
// `collapsedOffsets` and `expandedOffsets` are written for each toast; either
// may be null. Answers the collapsed height and writes the expanded one when
// `expandedHeight` is not null.
float ToastStackGeometry(const float* heights, int n, float peek, float gap,
                         bool anchoredBottom, float* collapsedOffsets,
                         float* expandedOffsets, float* expandedHeight);

// Push a toast. `timeoutMs` of 0 means it stays until dismissed, which is
// Rust's None. Answers false when the stack is full.
bool ToastPush(ToastStackState* s, int id, int timeoutMs);

// Drop one by id, wherever it is in its life.
bool ToastRemove(ToastStackState* s, int id);

// advance. Moves every toast on by `deltaMs` and drops the ones that finished
// leaving. `paused` freezes the timeouts and nothing else: Rust guards only
// the Present arm with it, so a toast still finishes animating in and out
// while the pointer rests on the stack — it just never times out under it.
//
// Answers whether anything changed, which is Rust's `changed`.
bool ToastAdvance(ToastStackState* s, int deltaMs, bool paused);

struct Toast {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
