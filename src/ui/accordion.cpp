#include "ui/accordion.h"
#include "base/motion.h"
#include "ui/tag.h"

namespace gpui {

namespace component {

// accordion.rs: ANIMATION_DURATION, along ease_out_cubic.
static const float kAccordionMotionMs = 200.f;

Accordion* Accordion::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    Accordion* acc = ArenaNew<Accordion>(a);
    acc->a = a;
    acc->cx = cx;
    acc->id = id;
    return acc;
}

Accordion* Accordion::Multiple(bool v) {
    multiple = v;
    return this;
}
Accordion* Accordion::Bordered(bool v) {
    bordered = v;
    return this;
}
Accordion* Accordion::Disabled(bool v) {
    disabled = v;
    return this;
}
Accordion* Accordion::WithSize(UiSize s) {
    size = s;
    return this;
}
Accordion* Accordion::Item(Str title, Str body, bool open) {
    if (nItems < 8) {
        items[nItems].title = title;
        items[nItems].body = body;
        items[nItems].open = open;
        nItems++;
    }
    return this;
}
Accordion* Accordion::SettingsItem(Str title, Str body, bool open,
                                   IconName icon, Str tag) {
    if (nItems < 8) {
        items[nItems].title = title;
        items[nItems].body = body;
        items[nItems].open = open;
        items[nItems].icon = icon;
        items[nItems].tag = tag;
        items[nItems].settings = true;
        nItems++;
    }
    return this;
}
Accordion* Accordion::OnToggle(Listener fn) {
    onToggle = fn;
    return this;
}

El* Accordion::IntoEl() {
    const Theme& th = cx->theme();
    // Items paint tokens.accordion (= background); bordered turns the group
    // into one rounded card instead of a stack of separators.
    El* root =
        gpui::Accordion::New(cx, id)->FlexCol()->W(kFill)->Bg(th.background);
    if (bordered) {
        root->Border(1, th.border)->Radius(th.radiusLg)->ClipY();
    }
    for (int i = 0; i < nItems; i++) {
        float font = UiFontPx(size);
        El* trig = AccordionTrigger::New(cx, items[i].title, items[i].open,
                                         disabled, ListenerArg(onToggle, i));
        // AccordionTrigger: py_2 px_3 at Medium, font_medium.
        trig->FlexRow()->ItemsCenter()->JustifyBetween()->PadX(12)->PadY(8)->W(
            kFill);
        if (items[i].settings) {
            El* left = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Grow();
            if (items[i].icon != IconName::None) {
                left->Child(
                    Div(a)
                        ->W(32)
                        ->H(32)
                        ->Radius(8)
                        ->Bg(RgbaOpacity(th.secondary, 0.5f))
                        ->ItemsCenter()
                        ->JustifyCenter()
                        ->Shrink0()
                        ->Child(IconEl(a, items[i].icon, 16)->Fg(th.mutedFg)));
            }
            left->Child(TextEl(a, items[i].title)
                            ->Font(font)
                            ->Fg(th.foreground)
                            ->Semibold());
            if (items[i].tag.s) {
                left->Child(Tag::New(cx, items[i].tag)
                                ->Success()
                                ->Outline()
                                ->WithSize(UiSize::Small)
                                ->IntoEl());
            }
            trig->Child(left);
        } else {
            trig->Child(TextEl(a, items[i].title)
                            ->Font(font)
                            ->Fg(th.foreground)
                            ->Semibold());
        }
        trig->Child(
            IconEl(a,
                   items[i].open ? IconName::ChevronUp : IconName::ChevronDown,
                   14)
                ->Fg(th.mutedFg));
        gpui::AccordionItem* it =
            gpui::AccordionItem::New(cx)
                ->Open(items[i].open)
                ->Header(gpui::AccordionHeader::New(cx, trig));
        El* panel = gpui::AccordionPanel::New(cx);
        if (items[i].settings) {
            panel->PadL(52)->PadR(8)->PadT(0)->PadB(12);
        } else {
            // AccordionPanel: pb_2 px_3 at Medium.
            panel->PadX(12)->PadT(0)->PadB(8);
        }
        panel->Child(
            TextEl(a, items[i].body)->Font(font)->Fg(th.mutedFg)->Wrap());
        // AnimatedAccordionPanel: the panel's height is its natural one times
        // the progress, and the box around it clips what does not fit. The
        // natural height is what the last frame measured — Rust keeps it in
        // the element's state from its prepaint; here the panel reports its
        // own box, into a slot that is asked for whether or not the panel is
        // mounted so a closed item still knows how tall it will be.
        Str key = StrDup(a, fmt("%s-%d", id, i));
        Motion motion = MotionNew(kAccordionMotionMs);
        motion.ease = EaseOutCubic;
        float progress = MotionValue(cx, MotionId(StrL("accordion"), key),
                                     items[i].open ? 1.f : 0.f, motion);
        auto* natural = (Bounds*)MotionSlot(
            cx, MotionId(StrL("accordion-h"), key), (int)sizeof(Bounds));
        if (progress > 0.001f) {
            El* clip = Div(a)->W(kFill)->ClipY();
            if (natural && natural->h > 0) {
                clip->H(natural->h * progress);
            }
            if (natural) {
                panel->BoundsOut(natural);
            }
            // The item mounts its panel while it is open or on its way shut,
            // which is what keeps a collapse animating rather than vanishing.
            it->KeepMounted(true)->Panel(clip->Child(panel));
        }
        // The separator belongs to the item, so it lands under the panel and
        // not between the trigger and its body.
        El* itEl = it->IntoEl();
        if (i + 1 < nItems) {
            itEl->BorderB(1, th.border);
        }
        root->Child(itEl);
    }
    return root;
}

} // namespace component
} // namespace gpui
