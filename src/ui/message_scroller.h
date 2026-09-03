#ifndef GPUI_SRC_UI_MESSAGE_SCROLLER_H_
#define GPUI_SRC_UI_MESSAGE_SCROLLER_H_
/* Themed message scroller — crates/ui/src/message_scroller.rs */

#include "ui/sizing.h"
#include "ui/scroll.h"
#include "base/virtual_list.h"
#include "base/motion.h"

namespace gpui {

namespace component {

struct Button;

// LIST_OVERDRAW / JUMP_BUTTON_TRANSITION / BOTTOM_FADE_TRANSITION.
const float kMessageScrollerOverdraw = 400.f;
const float kMessageScrollerJumpTransitionMs = 200.f;
const float kMessageScrollerBottomFadeTransitionMs = 200.f;
// What a row that has not been laid out yet is assumed to be tall. GPUI's
// `list` measures a row the first time it is scrolled near; nothing here can
// measure while the tree is being built, so a row starts at this height and
// is corrected from the box layout gave it on the next frame.
const float kMessageScrollerEstimatedRowHeight = 64.f;

// The entity-owned scrolling state for a MessageScroller.
//
// The state owns only the virtual-list bookkeeping; message data stays with
// the caller and is read by the row renderer passed to MessageScroller::New.
//
// Where this differs from `ListState`: GPUI measures a row during layout and
// caches its height by item. This tree has no measure-during-layout seam, so
// the state keeps the box the previous frame gave each row — reported through
// El::BoundsOut, one frame late, the way every other measured thing here is —
// and a row nothing has laid out yet stands in at the estimate above. The
// consequences are that the first frame of a list is laid out from estimates,
// and that a row whose content grows is right on the frame after it grew.
struct MessageScrollerState {
    VirtualListScrollHandle handle;
    // The height of each row, or 0 for one that has not been laid out.
    Vec<float> heights;
    // Where the last frame put each row. The scroller hands the row wrapper
    // a pointer into this, so it must not move while a frame is being built.
    Vec<Bounds> probes;
    // FollowMode::Tail: the list sticks to the newest row until the reader
    // scrolls away from it.
    bool followTail = true;

    ~MessageScrollerState();

    // ListState::new(item_count, Top, LIST_OVERDRAW) plus tail following.
    static void Init(MessageScrollerState* self, int itemCount);

    // The current number of rows known by the virtual list.
    int ItemCount() const;
    // Whether the reader has scrolled away from the latest content.
    bool IsScrolledUp() const;
    // Whether the list is actively following its tail.
    bool IsFollowingTail() const;

    // Reset the list to `itemCount` rows.
    void Reset(Ctx* cx, int itemCount);
    // Replace [start, end) with `count` new rows. Answers false when the
    // range is outside the current list, leaving the state unchanged.
    bool Splice(Ctx* cx, int start, int end, int count);
    // Append `count` rows to the end of the list.
    bool Append(Ctx* cx, int count);
    // Prepend `count` rows while preserving the current scroll anchor.
    bool Prepend(Ctx* cx, int count);
    // Mark every row for remeasurement.
    void Remeasure(Ctx* cx);
    // Mark [start, end) for remeasurement. False when the range is outside.
    bool RemeasureItems(Ctx* cx, int start, int end);
    // Scroll to the row at `index`, if it exists.
    bool ScrollToItem(Ctx* cx, int index);
    // Resume tail following and scroll to the latest row.
    void ScrollToEnd(Ctx* cx);

    // The two listeners the scroller binds to this entity: where the wheel
    // and the scrollbar leave the offset, and the jump button's press.
    static void OnScroll(MessageScrollerState* self, Ctx* cx,
                         const ScrollEvent* ev);
    static void OnJumpToLatest(MessageScrollerState* self, Ctx* cx,
                               const ClickEvent* ev);

    bool ValidRange(int start, int end) const;
};

// The row builder. Rust's is a boxed closure; the C++ seam is a function and
// the environment it reads, which is what every other row builder here takes.
using MessageScrollerRowFn = El* (*)(void* user, Ctx* cx, int index);
// with_jump_button_renderer: the fully configured Button, for a caller that
// wants to change its variant, size, icon or tooltip without replacing the
// scroll action.
using MessageScrollerButtonFn = void (*)(Button* button);

// A virtualized message list with an optional scrollbar and jump-to-latest UI.
struct MessageScroller {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<MessageScrollerState> state = {};
    MessageScrollerRowFn renderer = nullptr;
    void* user = nullptr;
    // The viewport height. GPUI's `list` fills whatever box it is given;
    // the virtual list here is told how tall its viewport is, so a scroller
    // in a flexible box is handed the height that box resolved to.
    float h = 0;
    Style style = {};
    uint32_t styleSet = 0;
    Style contentStyle = {};
    uint32_t contentStyleSet = 0;
    Style listStyle = {};
    uint32_t listStyleSet = 0;
    Style rowStyle = {};
    uint32_t rowStyleSet = 0;
    Style jumpButtonStyle = {};
    uint32_t jumpButtonStyleSet = 0;
    MessageScrollerButtonFn jumpButtonRenderer = nullptr;
    float jumpButtonTransitionMs = kMessageScrollerJumpTransitionMs;
    Rgba bottomFade = {};
    bool hasBottomFade = false;
    bool scrollbar = true;
    bool jumpButton = true;
    Str jumpButtonLabel = StrL("Jump to latest");

    static MessageScroller* New(Ctx* cx, Str id,
                                Entity<MessageScrollerState> state,
                                MessageScrollerRowFn renderer, void* user);
    // The viewport height, in DIPs.
    MessageScroller* H(float px);
    MessageScroller* Scrollbar(bool value);
    MessageScroller* JumpButton(bool value);
    MessageScroller* WithJumpButtonLabel(Str label);
    MessageScroller* WithContentStyle(const Style& s, uint32_t fields);
    MessageScroller* WithListStyle(const Style& s, uint32_t fields);
    MessageScroller* WithRowStyle(const Style& s, uint32_t fields);
    MessageScroller* WithJumpButtonStyle(const Style& s, uint32_t fields);
    MessageScroller* WithJumpButtonRenderer(MessageScrollerButtonFn fn);
    // How long the built-in jump button takes to enter or leave. A zero
    // duration disables its transition; reduced motion always adopts the
    // final state at once.
    MessageScroller* WithJumpButtonTransition(float ms);
    // Fade the transcript's bottom edge into `color`. The fade shows only
    // while the reader is away from the live edge — at the bottom nothing is
    // clipped — and sits under the jump button.
    MessageScroller* WithBottomFade(Rgba color);
    MessageScroller* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
#endif // GPUI_SRC_UI_MESSAGE_SCROLLER_H_
