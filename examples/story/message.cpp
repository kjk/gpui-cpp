#include "Story.h"

// crates/story/src/stories/message_story.rs

// section(..).max_w(rems(42.5)) — 680 at the 16px root.
static const float kMessageSectionMaxW = 680;

struct MessageStory {
    static El* Render(MessageStory* self, Ctx* cx);
};

static El* MessageSection(Ctx* cx, const char* title, const char* desc,
                          float gap) {
    El* section = StorySection(cx, title, desc);
    StorySectionBody(section)->FlexCol()->Gap(gap)->MaxW(kMessageSectionMaxW);
    return section;
}

static El* MsgText(Ctx* cx, const char* text) {
    return TextEl(cx->a, Str(text))->Wrap();
}

static El* MsgAvatar(Ctx* cx, const char* name) {
    return component::Avatar::New(cx)->Name(Str(name))->Size(32)->IntoEl();
}

static component::Bubble* MsgBubble(Ctx* cx, component::BubbleVariant variant,
                                    const char* text) {
    return component::Bubble::New(cx)
        ->WithVariant(variant)
        ->Child(MsgText(cx, text));
}

El* MessageStory::Render(MessageStory*, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow(cx->app);
    using component::BubbleVariant;
    using component::MessageAlignment;
    El* page = Div(a)->FlexCol()->W(kFill)->Gap(16);

    El* alignment = MessageSection(
        cx, "Alignment",
        "The message owns alignment for all of its named slots.", 20);
    StorySectionAdd(
        alignment,
        component::Message::New(cx)
            ->AvatarSlot(component::MessageAvatar::New(cx)
                             ->Child(MsgAvatar(cx, "Alice")))
            ->Header(component::MessageHeader::New(cx)
                         ->Child(TextEl(a, StrL("Alice")))
                         ->Child(TextEl(a, StrL("10:24 AM"))))
            ->Content(component::MessageContent::New(cx)->WithBubble(MsgBubble(
                cx, BubbleVariant::Secondary, "Can you review this?")))
            ->Footer(component::MessageFooter::New(cx)
                         ->Child(TextEl(a, StrL("Read"))))
            ->IntoEl());
    StorySectionAdd(
        alignment, component::Message::New(cx)
                       ->Alignment(MessageAlignment::End)
                       ->Avatar(MsgAvatar(cx, "You"))
                       ->Header(component::MessageHeader::New(cx)
                                    ->Child(TextEl(a, StrL("You")))
                                    ->Child(TextEl(a, StrL("10:25 AM"))))
                       ->Content(component::MessageContent::New(cx)->WithBubble(
                           MsgBubble(cx, BubbleVariant::Filled,
                                     "Sure — I will send notes shortly.")))
                       ->Footer(component::MessageFooter::New(cx)
                                    ->Child(TextEl(a, StrL("Delivered"))))
                       ->IntoEl());
    page->Child(alignment);

    El* avatars = MessageSection(
        cx, "Avatar",
        "Use sender avatars, initials, or an empty slot to preserve "
        "alignment.",
        20);
    StorySectionAdd(
        avatars,
        component::Message::New(cx)
            ->Avatar(component::Avatar::New(cx)
                         ->Name(StrL("Alice Chen"))
                         ->Src(StrL("https://avatars.githubusercontent.com/"
                                    "u/5518?s=64"))
                         ->Size(32)
                         ->IntoEl())
            ->Header(component::MessageHeader::New(cx)
                         ->Child(TextEl(a, StrL("Alice Chen"))))
            ->Content(component::MessageContent::New(cx)->WithBubble(MsgBubble(
                cx, BubbleVariant::Secondary,
                "The sender image falls back to initials when unavailable.")))
            ->IntoEl());
    StorySectionAdd(
        avatars,
        component::Message::New(cx)
            ->Avatar(MsgAvatar(cx, "Jordan Park"))
            ->Header(component::MessageHeader::New(cx)
                         ->Child(TextEl(a, StrL("Jordan Park"))))
            ->Content(component::MessageContent::New(cx)->WithBubble(
                MsgBubble(cx, BubbleVariant::Muted,
                          "Initials remain available without an image.")))
            ->IntoEl());
    Style clearAvatar = {};
    clearAvatar.bg = Background(th.transparent);
    StorySectionAdd(
        avatars,
        component::Message::New(cx)
            ->AvatarSlot(component::MessageAvatar::New(cx)
                             ->Refine(clearAvatar, StyleFieldBg))
            ->Content(component::MessageContent::New(cx)->WithBubble(MsgBubble(
                cx, BubbleVariant::Secondary,
                "An empty avatar slot keeps grouped responses aligned.")))
            ->IntoEl());
    page->Child(avatars);

    El* meta = MessageSection(
        cx, "Header and footer",
        "Compose sender metadata, timestamps, and delivery status "
        "explicitly.",
        20);
    StorySectionAdd(
        meta, component::Message::New(cx)
                  ->Avatar(MsgAvatar(cx, "Support team"))
                  ->Header(component::MessageHeader::New(cx)
                               ->Child(Div(a)
                                           ->Fg(th.foreground)
                                           ->Child(TextEl(a, StrL("Support"))))
                               ->Child(TextEl(a, StrL("·")))
                               ->Child(TextEl(a, StrL("10:42 AM"))))
                  ->Content(component::MessageContent::New(cx)->WithBubble(
                      MsgBubble(cx, BubbleVariant::Secondary,
                                "Your issue has been assigned to the team.")))
                  ->Footer(component::MessageFooter::New(cx)
                               ->Child(TextEl(a, StrL("Read by 3 people"))))
                  ->IntoEl());
    StorySectionAdd(
        meta,
        component::Message::New(cx)
            ->Alignment(MessageAlignment::End)
            ->Header(component::MessageHeader::New(cx)
                         ->ContentInset(false)
                         ->Child(TextEl(a, StrL("You · Just now"))))
            ->Content(component::MessageContent::New(cx)->WithBubble(MsgBubble(
                cx, BubbleVariant::Filled, "Thank you for the quick update.")))
            ->Footer(component::MessageFooter::New(cx)
                         ->ContentInset(false)
                         ->Child(TextEl(a, StrL("Delivered"))))
            ->IntoEl());
    page->Child(meta);

    El* actions = MessageSection(
        cx, "Actions",
        "Keep copy, feedback, and retry actions keyboard-accessible.", 20);
    StorySectionAdd(
        actions,
        component::Message::New(cx)
            ->Content(component::MessageContent::New(cx)->WithBubble(MsgBubble(
                cx, BubbleVariant::Muted,
                "The install failure is coming from the workspace package.")))
            ->Footer(
                component::MessageFooter::New(cx)
                    ->Child(component::Button::New(cx, StrL("message-copy"))
                                ->Ghost()
                                ->WithSize(UiSize::XSmall)
                                ->Icon(IconName::Copy)
                                ->Tooltip(StrL("Copy message"))
                                ->IntoEl())
                    ->Child(component::Button::New(cx, StrL("message-like"))
                                ->Ghost()
                                ->WithSize(UiSize::XSmall)
                                ->Icon(IconName::Heart)
                                ->Tooltip(StrL("Like message"))
                                ->IntoEl())
                    ->Child(component::Button::New(cx, StrL("message-save"))
                                ->Ghost()
                                ->WithSize(UiSize::XSmall)
                                ->Icon(IconName::Star)
                                ->Tooltip(StrL("Save message"))
                                ->IntoEl()))
            ->IntoEl());
    StorySectionAdd(
        actions, component::Message::New(cx)
                     ->Alignment(MessageAlignment::End)
                     ->Content(component::MessageContent::New(cx)->WithBubble(
                         MsgBubble(cx, BubbleVariant::Destructive,
                                   "The response could not be sent.")))
                     ->Footer(component::MessageFooter::New(cx)
                                  ->Child(Div(a)->Fg(th.danger)->Child(
                                      TextEl(a, StrL("Failed to send"))))
                                  ->Child(component::Button::New(
                                              cx, StrL("message-retry"))
                                              ->Ghost()
                                              ->WithSize(UiSize::XSmall)
                                              ->Label(StrL("Retry"))
                                              ->IntoEl()))
                     ->IntoEl());
    page->Child(actions);

    El* attach = MessageSection(
        cx, "Attachment",
        "Mix image previews, file attachments, and text within one message.",
        20);
    StorySectionAdd(
        attach,
        component::Message::New(cx)
            ->Alignment(MessageAlignment::End)
            ->Content(
                component::MessageContent::New(cx)
                    ->Child(
                        component::Attachment::New(cx)
                            ->WithAxis(Axis::Vertical)
                            ->Media(component::AttachmentMedia::New(cx)->Src(
                                StrL("https://pub.lbkrs.com/files/"
                                     "202503/vEnnmgUM6bo362ya/"
                                     "sdk.svg")))
                            ->IntoEl())
                    ->WithBubble(
                        MsgBubble(cx, BubbleVariant::Filled,
                                  "Can you use this image on the cover?")))
            ->IntoEl());
    StorySectionAdd(
        attach,
        component::Message::New(cx)
            ->Avatar(MsgAvatar(cx, "Alice"))
            ->Content(
                component::MessageContent::New(cx)
                    ->WithBubble(MsgBubble(cx, BubbleVariant::Secondary,
                                           "Done. Here is the updated report."))
                    ->Child(
                        component::Attachment::New(cx)
                            ->Media(component::AttachmentMedia::New(cx)->Child(
                                component::Icon::New(cx, IconName::FileText)
                                    ->IntoEl()))
                            ->Content(
                                component::AttachmentContent::New(cx)
                                    ->Title(component::AttachmentTitle::New(
                                        cx, StrL("sales-dashboard.pdf")))
                                    ->Description(
                                        component::AttachmentDescription::New(
                                            cx, StrL("PDF · 2.4 MB"))))
                            ->Actions(
                                component::AttachmentActions::New(cx)->Child(
                                    component::Button::New(
                                        cx, StrL("message-open-attachment"))
                                        ->Ghost()
                                        ->WithSize(UiSize::Small)
                                        ->Label(StrL("Open"))
                                        ->IntoEl()))
                            ->IntoEl()))
            ->IntoEl());
    page->Child(attach);

    El* multiple = MessageSection(
        cx, "Multiple bubbles",
        "A message can hold multiple surfaces, reactions, and long-form "
        "text.",
        0);
    StorySectionAdd(
        multiple,
        component::Message::New(cx)
            ->Avatar(MsgAvatar(cx, "Assistant"))
            ->Header(component::MessageHeader::New(cx)
                         ->Child(TextEl(a, StrL("Assistant")))
                         ->Child(TextEl(a, StrL("Just now"))))
            ->Content(
                component::MessageContent::New(cx)
                    ->WithBubble(MsgBubble(
                        cx, BubbleVariant::Secondary,
                        "I reviewed the upload and message rendering paths."))
                    ->WithBubble(
                        MsgBubble(cx, BubbleVariant::Muted,
                                  "Keep lifecycle state on the attachment, "
                                  "preserve the sender's alignment, and "
                                  "expose every action as an existing "
                                  "semantic Button.")
                            ->Reactions(
                                component::BubbleReactions::New(cx)->Action(
                                    component::Button::New(
                                        cx, StrL("message-bubble-like"))
                                        ->Ghost()
                                        ->WithSize(UiSize::XSmall)
                                        ->Label(StrL("👍 2"))
                                        ->Tooltip(
                                            StrL("Like this response"))))))
            ->Footer(component::MessageFooter::New(cx)
                         ->Child(TextEl(a, StrL("Response complete"))))
            ->IntoEl());
    page->Child(multiple);

    El* group = MessageSection(
        cx, "Group",
        "Group consecutive messages while keeping each row composable.", 0);
    StorySectionAdd(
        group,
        component::MessageGroup::New(cx)
            ->Child(
                component::Message::New(cx)
                    ->Avatar(MsgAvatar(cx, "Alice"))
                    ->Header(component::MessageHeader::New(cx)
                                 ->Child(TextEl(a, StrL("Alice"))))
                    ->Content(component::MessageContent::New(cx)->WithBubble(
                        MsgBubble(cx, BubbleVariant::Secondary,
                                  "I attached the draft.")))
                    ->IntoEl())
            ->Child(
                component::Message::New(cx)
                    ->AvatarSlot(component::MessageAvatar::New(cx)
                                     ->Refine(clearAvatar, StyleFieldBg))
                    ->Content(component::MessageContent::New(cx)->WithBubble(
                        MsgBubble(cx, BubbleVariant::Secondary,
                                  "The second page needs attention.")))
                    ->IntoEl())
            ->IntoEl()
            ->W(kFill));
    page->Child(group);

    El* custom = MessageSection(
        cx, "Custom style",
        "Every structural part accepts GPUI style refinements.", 0);
    Style noInset = {};
    StorySectionAdd(custom,
                    component::Message::New(cx)
                        ->Header(component::MessageHeader::New(cx)
                                     ->ContentInset(false)
                                     ->Child(TextEl(a, StrL("System"))))
                        ->Content(component::MessageContent::New(cx)->Child(
                            MsgText(cx, "The conversation has been archived.")))
                        ->Footer(component::MessageFooter::New(cx)
                                     ->ContentInset(false)
                                     ->Child(TextEl(a, StrL("Just now"))))
                        ->IntoEl()
                        ->Pad(12)
                        ->Radius(th.radiusLg)
                        ->Bg(RgbaOpacity(th.muted, 0.35f)));
    (void)noInset;
    page->Child(custom);

    El* ghost = MessageSection(
        cx, "Ghost surface",
        "Typed ghost bubbles automatically remove metadata insets.", 0);
    Style stack = {};
    stack.gapX = 12;
    stack.gapY = 12;
    StorySectionAdd(
        ghost, component::Message::New(cx)
                   ->WithStackStyle(stack, StyleFieldGap)
                   ->Header(component::MessageHeader::New(cx)
                                ->Child(TextEl(a, StrL("System")))
                                ->Child(TextEl(a, StrL("Just now"))))
                   ->Content(component::MessageContent::New(cx)->WithBubble(
                       MsgBubble(cx, BubbleVariant::Ghost,
                                 "The conversation has been archived.")))
                   ->Footer(component::MessageFooter::New(cx)->Child(
                       TextEl(a, StrL("No further action required"))))
                   ->IntoEl());
    page->Child(ghost);
    return page;
}

STORY_PAGE(StoryMessage, MessageStory);
