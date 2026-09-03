#ifndef GPUI_SRC_UI_ATTACHMENT_H_
#define GPUI_SRC_UI_ATTACHMENT_H_
/* Themed attachment card — crates/ui/src/attachment.rs */

#include "ui/sizing.h"
#include "ui/shimmer.h"

namespace gpui {

namespace component {

// The lifecycle status of an attachment.
enum class AttachmentStatus : uint8_t {
    // Selected and waiting to be uploaded.
    Pending,
    // Currently being uploaded.
    Uploading,
    // Uploaded and being processed.
    Processing,
    // Failed to upload or process.
    Failed,
    // Ready.
    Complete
};

inline bool AttachmentStatusIsPending(AttachmentStatus s) {
    return s == AttachmentStatus::Pending;
}
inline bool AttachmentStatusIsUploading(AttachmentStatus s) {
    return s == AttachmentStatus::Uploading;
}
inline bool AttachmentStatusIsProcessing(AttachmentStatus s) {
    return s == AttachmentStatus::Processing;
}
inline bool AttachmentStatusIsFailed(AttachmentStatus s) {
    return s == AttachmentStatus::Failed;
}
inline bool AttachmentStatusIsComplete(AttachmentStatus s) {
    return s == AttachmentStatus::Complete;
}
inline bool AttachmentStatusIsInProgress(AttachmentStatus s) {
    return s == AttachmentStatus::Uploading ||
           s == AttachmentStatus::Processing;
}

// The media slot for an attachment. Add an icon or another element as a child
// for an icon-style preview; use Src when the attachment has an image.
struct AttachmentMedia {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    UiSize size = UiSize::Medium;
    bool hasSize = false;
    AttachmentStatus status = AttachmentStatus::Complete;
    Axis axis = Axis::Horizontal;
    Str source = {};
    bool hasSource = false;
    Style style = {};
    uint32_t styleSet = 0;

    static AttachmentMedia* New(Ctx* cx);
    AttachmentMedia* Src(Str source);
    // Centered content above the preview, which is not dimmed while loading.
    AttachmentMedia* Overlay(El* overlay);
    AttachmentMedia* Child(El* e);
    AttachmentMedia* WithSize(UiSize value);
    AttachmentMedia* Refine(const Style& s, uint32_t fields);
    // Fills the slot in from the attachment that owns it.
    AttachmentMedia* Layout(UiSize value, AttachmentStatus st, Axis ax);
    El* IntoEl();
};

// A single-line attachment title.
struct AttachmentTitle {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str text = {};
    AttachmentStatus status = AttachmentStatus::Complete;
    bool hasStatus = false;
    ShimmerStyle shimmerStyle = {};
    bool hasShimmerStyle = false;
    Style style = {};
    uint32_t styleSet = 0;

    static AttachmentTitle* New(Ctx* cx, Str text);
    AttachmentTitle* Status(AttachmentStatus value);
    AttachmentTitle* WithShimmerStyle(const ShimmerStyle& value);
    AttachmentTitle* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

// A single-line attachment description or status message.
struct AttachmentDescription {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str text = {};
    AttachmentStatus status = AttachmentStatus::Complete;
    bool hasStatus = false;
    Style style = {};
    uint32_t styleSet = 0;

    static AttachmentDescription* New(Ctx* cx, Str text);
    AttachmentDescription* Status(AttachmentStatus value);
    AttachmentDescription* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

// One entry of an AttachmentContent: a typed title or description, which
// inherits the card's lifecycle status, or an arbitrary element.
struct AttachmentContentChild {
    AttachmentTitle* title = nullptr;
    AttachmentDescription* description = nullptr;
    El* element = nullptr;
};

// The metadata slot for an attachment.
struct AttachmentContent {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<AttachmentContentChild> children;
    bool verticalLayout = false;
    Style style = {};
    uint32_t styleSet = 0;

    static AttachmentContent* New(Ctx* cx);
    AttachmentContent* Title(AttachmentTitle* value);
    AttachmentContent* Description(AttachmentDescription* value);
    AttachmentContent* Child(El* e);
    AttachmentContent* Refine(const Style& s, uint32_t fields);
    AttachmentContent* Layout(Axis axis, AttachmentStatus status);
    El* IntoEl();
};

// A composition slot for attachment actions. Add existing Button or other
// controls as children; a separate attachment-specific action wrapper is
// intentionally unnecessary.
struct AttachmentActions {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    bool verticalLayout = false;
    Style style = {};
    uint32_t styleSet = 0;

    static AttachmentActions* New(Ctx* cx);
    AttachmentActions* Child(El* e);
    AttachmentActions* Refine(const Style& s, uint32_t fields);
    AttachmentActions* LayoutForAxis(Axis axis);
    El* IntoEl();
};

// A file or image attachment composed from media, content and action slots.
struct Attachment {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    bool hasId = false;
    Style style = {};
    uint32_t styleSet = 0;
    AttachmentStatus status = AttachmentStatus::Complete;
    UiSize size = UiSize::Medium;
    Axis axis = Axis::Horizontal;
    AttachmentMedia* media = nullptr;
    AttachmentContent* content = nullptr;
    AttachmentActions* actions = nullptr;
    Listener onClick;

    static Attachment* New(Ctx* cx);
    // A stable identity for the whole-card click layer.
    Attachment* Id(Str value);
    // Make the whole card clickable. The click layer is painted below the
    // actions slot, so action buttons stay independently clickable; click
    // state needs a stable identity, so this takes effect only with Id.
    Attachment* OnClick(Listener handler);
    Attachment* Status(AttachmentStatus value);
    Attachment* WithAxis(Axis value);
    Attachment* Media(AttachmentMedia* value);
    Attachment* Content(AttachmentContent* value);
    Attachment* Actions(AttachmentActions* value);
    Attachment* WithSize(UiSize value);
    Attachment* Refine(const Style& s, uint32_t fields);
    // The pass that hands the card's size, axis and status to its slots.
    void LayoutSlots();
    El* IntoEl();
};

// A horizontally scrollable row of attachments.
//
// GPUI keeps the row's scroll offset in the element's own retained state; a
// scroll offset is view-owned here, so a group that must actually scroll is
// given one and reports where the wheel left it.
struct AttachmentGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    ArenaVec<El*> children;
    float scrollX = 0;
    Listener onScroll;
    Style style = {};
    uint32_t styleSet = 0;

    static AttachmentGroup* New(Ctx* cx, Str id);
    AttachmentGroup* Child(El* e);
    AttachmentGroup* ScrollX(float value);
    AttachmentGroup* OnScroll(Listener fn);
    AttachmentGroup* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

// attachment_size_style: the gap, text size and padding one card size takes.
El* AttachmentSizeStyle(El* element, UiSize size, bool hasMedia,
                        bool hasContent);

} // namespace component
} // namespace gpui
#endif // GPUI_SRC_UI_ATTACHMENT_H_
