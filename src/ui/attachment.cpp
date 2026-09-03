#include "ui/attachment.h"

namespace gpui {

namespace component {

AttachmentMedia* AttachmentMedia::New(Ctx* cx) {
    Arena* a = cx->a;
    AttachmentMedia* s = ArenaNew<AttachmentMedia>(a);
    s->a = a;
    s->cx = cx;
    return s;
}

AttachmentMedia* AttachmentMedia::Src(Str value) {
    source = value;
    hasSource = true;
    return this;
}

AttachmentMedia* AttachmentMedia::Overlay(El* overlay) {
    if (overlay) {
        children.Append(a, Div(a)
                               ->Absolute()
                               ->Left(0)
                               ->Top(0)
                               ->Right(0)
                               ->Bottom(0)
                               ->Flex()
                               ->ItemsCenter()
                               ->JustifyCenter()
                               ->Child(overlay));
    }
    return this;
}

AttachmentMedia* AttachmentMedia::Child(El* e) {
    if (e) {
        children.Append(a, e);
    }
    return this;
}

AttachmentMedia* AttachmentMedia::WithSize(UiSize value) {
    size = value;
    hasSize = true;
    return this;
}

AttachmentMedia* AttachmentMedia::Refine(const Style& s, uint32_t fields) {
    StyleApplyFields(&style, s, fields);
    styleSet |= fields;
    return this;
}

AttachmentMedia* AttachmentMedia::Layout(UiSize value, AttachmentStatus st,
                                         Axis ax) {
    if (!hasSize) {
        size = value;
        hasSize = true;
    }
    status = st;
    axis = ax;
    return this;
}

// size_7 / size_8 / size_10 / size_12, the icon-preview square per card size.
static float AttachmentMediaBox(UiSize size) {
    switch (size) {
        case UiSize::Size:
            return size.pixels;
        case UiSize::XSmall:
            return 28;
        case UiSize::Small:
            return 32;
        case UiSize::Large:
            return 48;
        default:
            return 40;
    }
}

El* AttachmentMedia::IntoEl() {
    const Theme& th = ThemeNow(cx->app);
    // radius_tokens().sm / .md, which are half the theme radius and the
    // theme radius itself.
    float radius = size == UiSize::XSmall ? th.radius / 2.f : th.radius;
    bool failedMedia = AttachmentStatusIsFailed(status) && !hasSource;
    bool dimmedImage = hasSource && !(status == AttachmentStatus::Pending ||
                                      status == AttachmentStatus::Complete);

    El* box = Div(a)
                  ->Flex()
                  ->Shrink0()
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->ClipX()
                  ->ClipY();
    if (axis == Axis::Horizontal) {
        float px = AttachmentMediaBox(size);
        box->W(px)->H(px);
    } else {
        box->W(kFill)->Aspect(1.f);
    }
    box->Radius(radius);
    box->Bg(failedMedia ? RgbaOpacity(th.danger, 0.1f) : th.muted);
    box->Fg(failedMedia ? th.danger : th.foreground);
    if (hasSource) {
        El* image = ImageEl(a, source)
                        ->Absolute()
                        ->Left(0)
                        ->Top(0)
                        ->Right(0)
                        ->Bottom(0)
                        ->SizeFull();
        if (dimmedImage) {
            image->Opacity(0.6f);
        }
        box->Child(image);
    }
    for (int i = 0; i < children.len; i++) {
        box->Child(children[i]);
    }
    if (styleSet) {
        box->Refine(style, styleSet);
    }
    return box;
}

AttachmentTitle* AttachmentTitle::New(Ctx* cx, Str text) {
    Arena* a = cx->a;
    AttachmentTitle* s = ArenaNew<AttachmentTitle>(a);
    s->a = a;
    s->cx = cx;
    s->text = text;
    return s;
}

AttachmentTitle* AttachmentTitle::Status(AttachmentStatus value) {
    status = value;
    hasStatus = true;
    return this;
}

AttachmentTitle* AttachmentTitle::WithShimmerStyle(const ShimmerStyle& value) {
    shimmerStyle = value;
    hasShimmerStyle = true;
    return this;
}

AttachmentTitle* AttachmentTitle::Refine(const Style& s, uint32_t fields) {
    StyleApplyFields(&style, s, fields);
    styleSet |= fields;
    return this;
}

El* AttachmentTitle::IntoEl() {
    bool loading = hasStatus && AttachmentStatusIsInProgress(status);
    El* box = Div(a)->MaxW(kFill)->MinW(0)->Truncate()->Medium();
    if (loading) {
        ShimmerText* shimmer = ShimmerText::New(cx, text);
        if (hasShimmerStyle) {
            shimmer->WithShimmerStyle(shimmerStyle);
        }
        if (styleSet & StyleFieldColor) {
            shimmer->Fg(style.color);
        }
        box->Child(shimmer->IntoEl());
    } else {
        // El::Truncate is a property of the run, not of the box around it,
        // so the ellipsis goes on the text element rather than on the div
        // Rust puts `.truncate()` on.
        box->Child(TextEl(a, text)->Truncate()->MaxW(kFill));
    }
    if (styleSet) {
        box->Refine(style, styleSet);
    }
    return box;
}

AttachmentDescription* AttachmentDescription::New(Ctx* cx, Str text) {
    Arena* a = cx->a;
    AttachmentDescription* s = ArenaNew<AttachmentDescription>(a);
    s->a = a;
    s->cx = cx;
    s->text = text;
    return s;
}

AttachmentDescription* AttachmentDescription::Status(AttachmentStatus value) {
    status = value;
    hasStatus = true;
    return this;
}

AttachmentDescription* AttachmentDescription::Refine(const Style& s,
                                                     uint32_t fields) {
    StyleApplyFields(&style, s, fields);
    styleSet |= fields;
    return this;
}

El* AttachmentDescription::IntoEl() {
    const Theme& th = ThemeNow(cx->app);
    Rgba color = (hasStatus && AttachmentStatusIsFailed(status))
                     ? RgbaOpacity(th.danger, 0.8f)
                     : th.mutedFg;
    El* box = Div(a)
                  ->MaxW(kFill)
                  ->MinW(0)
                  ->Truncate()
                  ->Font(12)
                  ->LineHeight(1.25f)
                  ->Fg(color)
                  ->Child(TextEl(a, text)->Truncate()->MaxW(kFill));
    if (styleSet) {
        box->Refine(style, styleSet);
    }
    return box;
}

AttachmentContent* AttachmentContent::New(Ctx* cx) {
    Arena* a = cx->a;
    AttachmentContent* s = ArenaNew<AttachmentContent>(a);
    s->a = a;
    s->cx = cx;
    return s;
}

AttachmentContent* AttachmentContent::Title(AttachmentTitle* value) {
    if (value) {
        AttachmentContentChild child;
        child.title = value;
        children.Append(a, child);
    }
    return this;
}

AttachmentContent* AttachmentContent::Description(
    AttachmentDescription* value) {
    if (value) {
        AttachmentContentChild child;
        child.description = value;
        children.Append(a, child);
    }
    return this;
}

AttachmentContent* AttachmentContent::Child(El* e) {
    if (e) {
        AttachmentContentChild child;
        child.element = e;
        children.Append(a, child);
    }
    return this;
}

AttachmentContent* AttachmentContent::Refine(const Style& s, uint32_t fields) {
    StyleApplyFields(&style, s, fields);
    styleSet |= fields;
    return this;
}

AttachmentContent* AttachmentContent::Layout(Axis axis,
                                             AttachmentStatus status) {
    verticalLayout = axis == Axis::Vertical;
    for (int i = 0; i < children.len; i++) {
        AttachmentContentChild& child = children[i];
        if (child.title && !child.title->hasStatus) {
            child.title->Status(status);
        }
        if (child.description && !child.description->hasStatus) {
            child.description->Status(status);
        }
    }
    return this;
}

El* AttachmentContent::IntoEl() {
    El* column = Div(a)
                     ->FlexCol()
                     ->MaxW(kFill)
                     ->MinW(0)
                     // `flex_1()` upstream. `Grow(1)` — grow 1, shrink 1,
                     // basis *auto* — because this port's flex intrinsic
                     // sizing gives a `basis: 0` item no max-content
                     // contribution, so a content-sized card would collapse
                     // onto its `min_w_40`. With an auto basis the card is as
                     // wide as its metadata and the column still takes the
                     // slack, which is what the zero basis was for here.
                     ->Grow(1)
                     ->Gap(2)
                     ->LineHeight(1.25f);
    if (verticalLayout) {
        column->W(kFill)->PadX(4);
    }
    for (int i = 0; i < children.len; i++) {
        const AttachmentContentChild& child = children[i];
        if (child.title) {
            column->Child(child.title->IntoEl());
        } else if (child.description) {
            column->Child(child.description->IntoEl());
        } else {
            column->Child(child.element);
        }
    }
    if (styleSet) {
        column->Refine(style, styleSet);
    }
    return column;
}

AttachmentActions* AttachmentActions::New(Ctx* cx) {
    Arena* a = cx->a;
    AttachmentActions* s = ArenaNew<AttachmentActions>(a);
    s->a = a;
    s->cx = cx;
    return s;
}

AttachmentActions* AttachmentActions::Child(El* e) {
    if (e) {
        children.Append(a, e);
    }
    return this;
}

AttachmentActions* AttachmentActions::Refine(const Style& s, uint32_t fields) {
    StyleApplyFields(&style, s, fields);
    styleSet |= fields;
    return this;
}

AttachmentActions* AttachmentActions::LayoutForAxis(Axis axis) {
    verticalLayout = axis == Axis::Vertical;
    return this;
}

El* AttachmentActions::IntoEl() {
    El* row = Div(a)->Flex()->Shrink0()->ItemsCenter()->Gap(4);
    if (verticalLayout) {
        row->Absolute()->Top(12)->Right(12);
    }
    // The actions cluster owns its presses: an action, or the gap between
    // actions, must not also arm the whole-card click layer below.
    row->StopMouseDown();
    for (int i = 0; i < children.len; i++) {
        row->Child(children[i]);
    }
    if (styleSet) {
        row->Refine(style, styleSet);
    }
    return row;
}

El* AttachmentSizeStyle(El* element, UiSize size, bool hasMedia,
                        bool hasContent) {
    switch (size) {
        case UiSize::XSmall:
            element->Gap(6)->Font(12);
            if (hasContent) {
                element->PadX(6)->PadY(4);
            }
            if (hasMedia) {
                element->Pad(4);
            }
            break;
        case UiSize::Small:
            element->Gap(10)->Font(12);
            if (hasContent) {
                element->PadX(8)->PadY(6);
            }
            if (hasMedia) {
                element->Pad(6);
            }
            break;
        case UiSize::Large:
            element->Gap(12)->Font(16);
            if (hasContent) {
                element->PadX(16)->PadY(12);
            }
            if (hasMedia) {
                element->Pad(12);
            }
            break;
        case UiSize::Size:
            element->Gap(4)->Font(size.pixels * 0.875f);
            if (hasContent || hasMedia) {
                element->Pad(size.pixels * 0.25f);
            }
            break;
        default:
            element->Gap(8)->Font(14);
            if (hasContent) {
                element->PadX(10)->PadY(8);
            }
            if (hasMedia) {
                element->Pad(8);
            }
            break;
    }
    return element;
}

Attachment* Attachment::New(Ctx* cx) {
    Arena* a = cx->a;
    Attachment* s = ArenaNew<Attachment>(a);
    s->a = a;
    s->cx = cx;
    return s;
}

Attachment* Attachment::Id(Str value) {
    id = value;
    hasId = true;
    return this;
}

Attachment* Attachment::OnClick(Listener handler) {
    onClick = handler;
    return this;
}

Attachment* Attachment::Status(AttachmentStatus value) {
    status = value;
    return this;
}

Attachment* Attachment::WithAxis(Axis value) {
    axis = value;
    return this;
}

Attachment* Attachment::Media(AttachmentMedia* value) {
    media = value;
    return this;
}

Attachment* Attachment::Content(AttachmentContent* value) {
    content = value;
    return this;
}

Attachment* Attachment::Actions(AttachmentActions* value) {
    actions = value;
    return this;
}

Attachment* Attachment::WithSize(UiSize value) {
    size = value;
    return this;
}

Attachment* Attachment::Refine(const Style& s, uint32_t fields) {
    StyleApplyFields(&style, s, fields);
    styleSet |= fields;
    return this;
}

void Attachment::LayoutSlots() {
    if (media) {
        media->Layout(size, status, axis);
    }
    if (content) {
        content->Layout(axis, status);
    }
    if (actions) {
        actions->LayoutForAxis(axis);
    }
}

El* Attachment::IntoEl() {
    const Theme& th = ThemeNow(cx->app);
    bool hasMedia = media != nullptr;
    bool hasContent = content != nullptr;
    bool clickable = hasId && onClick.IsValid();

    LayoutSlots();

    El* card = Div(a)
                   ->Flex()
                   ->FlexNone()
                   ->MaxW(kFill)
                   ->MinW(0)
                   // radius_tokens().xl is twice the theme radius.
                   ->Radius(size == UiSize::XSmall ? th.radius * 2.f
                                                   : ThemeRadius2xl(th))
                   ->Border(1, AttachmentStatusIsFailed(status)
                                   ? RgbaOpacity(th.danger, 0.3f)
                                   : th.border)
                   ->Bg(th.tokens.background)
                   ->Fg(th.foreground)
                   ->LineHeight(1.25f);
    if (AttachmentStatusIsPending(status)) {
        card->Dashed();
    }
    if (clickable) {
        card->HoverBg(BackgroundOpacity(th.tokens.muted, 0.5f));
    }
    AttachmentSizeStyle(card, size, hasMedia, hasContent);
    if (axis == Axis::Horizontal) {
        card->MinW(160)->ItemsCenter();
    } else {
        // w(rems(7.5)) with metadata, w_24 without.
        card->W(hasContent ? 120.f : 96.f)->FlexCol()->ItemsStart();
    }
    if (media) {
        card->Child(media->IntoEl());
    }
    if (content) {
        card->Child(content->IntoEl());
    }
    if (clickable) {
        // The click layer is added before the actions slot, so the actions'
        // hitboxes stay on top and their buttons keep working.
        card->Child(Div(a)
                        ->PathClick(id)
                        ->Absolute()
                        ->Left(0)
                        ->Top(0)
                        ->Right(0)
                        ->Bottom(0)
                        ->OnClick(onClick));
    }
    if (actions) {
        card->Child(actions->IntoEl());
    }
    if (styleSet) {
        card->Refine(style, styleSet);
    }
    return card;
}

AttachmentGroup* AttachmentGroup::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    AttachmentGroup* s = ArenaNew<AttachmentGroup>(a);
    s->a = a;
    s->cx = cx;
    s->id = id;
    return s;
}

AttachmentGroup* AttachmentGroup::Child(El* e) {
    if (e) {
        children.Append(a, e);
    }
    return this;
}

AttachmentGroup* AttachmentGroup::ScrollX(float value) {
    scrollX = value;
    return this;
}

AttachmentGroup* AttachmentGroup::OnScroll(Listener fn) {
    onScroll = fn;
    return this;
}

AttachmentGroup* AttachmentGroup::Refine(const Style& s, uint32_t fields) {
    StyleApplyFields(&style, s, fields);
    styleSet |= fields;
    return this;
}

El* AttachmentGroup::IntoEl() {
    El* row = Div(a)
                  ->FlexRow()
                  ->ItemsCenter()
                  ->PathId(id)
                  ->W(kFill)
                  ->MinW(0)
                  ->Gap(12)
                  ->PadY(4)
                  ->ClipX()
                  ->ScrollX(scrollX)
                  ->ScrollFromPath()
                  // lock_scroll_axis: the row takes a horizontal-dominant
                  // wheel gesture and lets a vertical one reach the scroller
                  // around it.
                  ->ScrollMask(Axis::Horizontal);
    if (onScroll.IsValid()) {
        row->OnScroll(onScroll);
    }
    if (styleSet) {
        row->Refine(style, styleSet);
    }
    for (int i = 0; i < children.len; i++) {
        row->Child(children[i]);
    }
    return row;
}

} // namespace component
} // namespace gpui
