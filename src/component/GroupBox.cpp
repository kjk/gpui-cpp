#include "component/GroupBox.h"

namespace gpui {

namespace component {

GroupBox* GroupBox::New(Ctx* cx, Str title) {
    Arena* a = cx->a;
    GroupBox* g = ArenaNew<GroupBox>(a);
    g->a = a;
    g->cx = cx;
    g->title = title;
    return g;
}
GroupBox* GroupBox::Child(El* e) {
    child = e;
    return this;
}
GroupBox* GroupBox::Outline() {
    outline = true;
    filled = false;
    return this;
}
GroupBox* GroupBox::Filled(bool v) {
    filled = v;
    return this;
}
GroupBox* GroupBox::TitleSemibold(bool v) {
    titleSemibold = v;
    return this;
}
GroupBox* GroupBox::TitlePadX(float px) {
    titlePadX = px;
    return this;
}
GroupBox* GroupBox::ContentBg(Rgba c) {
    contentBg = c;
    hasContentBg = true;
    return this;
}
GroupBox* GroupBox::ContentRadius(float px) {
    contentRadius = px;
    return this;
}
GroupBox* GroupBox::ContentPad(float px) {
    contentPad = px;
    return this;
}
GroupBox* GroupBox::ContentBorder(float px) {
    contentBorder = px;
    return this;
}

El* GroupBox::IntoEl() {
    const Theme& th = cx->theme();
    // The title sits outside the surface: only the content container takes
    // the fill, the border and the padding. Normal has none of the three, so
    // it spaces title and content further apart (gap_4 against gap_3).
    bool padded = filled || outline;
    El* box = Div(a)->FlexCol()->W(kFill)->Gap(padded ? 12.f : 16.f);
    if (title.s && title.len > 0) {
        El* text = TextEl(a, title)->Font(14)->Fg(th.mutedFg)->LineHeight(1.f);
        if (titleSemibold) {
            text->Semibold();
        }
        box->Child(titlePadX > 0 ? Div(a)->PadX(titlePadX)->Child(text) : text);
    }
    El* content = Div(a)->FlexCol()->W(kFill)->Gap(16)->Fg(th.groupBoxFg);
    content->Radius(contentRadius >= 0 ? contentRadius : th.radius);
    if (filled) {
        content->Bg(th.groupBox);
    }
    if (outline) {
        content->Border(1, th.border);
    }
    if (padded) {
        content->Pad(16);
    }
    if (hasContentBg) {
        content->Bg(contentBg);
    }
    if (contentPad >= 0) {
        content->Pad(contentPad);
    }
    if (contentBorder >= 0) {
        content->Border(contentBorder, th.border);
    }
    if (child) {
        content->Child(child);
    }
    box->Child(content);
    return box;
}

} // namespace component
} // namespace gpui
