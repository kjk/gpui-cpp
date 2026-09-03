#include "Story.h"

// crates/story/src/stories/attachment_story.rs

// section(..).max_w(rems(42.5)) — 680 at the 16px root.
static const float kAttachmentSectionMaxW = 680;

struct AttachmentStory {
    static void OnOpenCard(AttachmentStory*, Ctx* cx, const ClickEvent*) {
        StoryPushNotification(cx, StrL("Opening design-mockups.png…"));
    }
    static void OnRemoveCard(AttachmentStory*, Ctx* cx, const ClickEvent*) {
        StoryPushNotification(cx, StrL("Removed design-mockups.png"));
    }

    static El* Render(AttachmentStory* self, Ctx* cx);
};

static El* AttachmentSection(Ctx* cx, const char* title, const char* desc,
                             float gap) {
    El* section = StorySection(cx, title, desc);
    StorySectionBody(section)->FlexCol()->Gap(gap)->MaxW(
        kAttachmentSectionMaxW);
    return section;
}

static component::AttachmentMedia* FileMedia(Ctx* cx) {
    return component::AttachmentMedia::New(cx)
        ->Child(component::Icon::New(cx, IconName::FileText)->IntoEl());
}

static component::AttachmentContent* TitleAndDescription(Ctx* cx,
                                                         const char* title,
                                                         const char* desc) {
    return component::AttachmentContent::New(cx)
        ->Title(component::AttachmentTitle::New(cx, Str(title)))
        ->Description(component::AttachmentDescription::New(cx, Str(desc)));
}

static El* CloseAction(Ctx* cx, const char* id, const char* tooltip) {
    return component::Button::New(cx, Str(id))
        ->Ghost()
        ->WithSize(UiSize::XSmall)
        ->Icon(IconName::Close)
        ->Tooltip(Str(tooltip))
        ->IntoEl();
}

// The SVG preview both the thumbnail and the overlay sections load.
static Str AttachmentPreviewSrc() {
    return StrL("https://pub.lbkrs.com/files/202503/vEnnmgUM6bo362ya/sdk.svg");
}

El* AttachmentStory::Render(AttachmentStory*, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow(cx->app);
    using component::AttachmentStatus;
    El* page = Div(a)->FlexCol()->W(kFill)->Gap(16);

    El* metadata = AttachmentSection(
        cx, "File metadata",
        "Compose typed metadata and actions, or keep using existing child "
        "elements.",
        12);
    StorySectionAdd(
        metadata,
        component::Attachment::New(cx)
            ->Media(FileMedia(cx))
            ->Content(
                TitleAndDescription(cx, "quarterly-report.pdf", "PDF · 2.4 MB"))
            ->Actions(component::AttachmentActions::New(cx)->Child(CloseAction(
                cx, "remove-report", "Remove quarterly-report.pdf")))
            ->IntoEl());
    StorySectionAdd(
        metadata,
        component::Attachment::New(cx)
            ->WithSize(UiSize::Small)
            ->Media(FileMedia(cx))
            ->Content(component::AttachmentContent::New(cx)
                          ->Child(component::AttachmentTitle::New(
                                      cx, StrL("research-data.csv"))
                                      ->IntoEl())
                          ->Child(component::AttachmentDescription::New(
                                      cx, StrL("CSV · 840 KB"))
                                      ->IntoEl()))
            ->IntoEl());
    page->Child(metadata);

    El* click = AttachmentSection(
        cx, "Whole-card click",
        "The card opens its target while actions stay independently "
        "clickable.",
        12);
    StorySectionAdd(
        click,
        component::Attachment::New(cx)
            ->Id(StrL("clickable-attachment"))
            ->OnClick(Listen(cx, &AttachmentStory::OnOpenCard))
            ->Media(FileMedia(cx))
            ->Content(
                TitleAndDescription(cx, "design-mockups.png", "PNG · 1.8 MB"))
            ->Actions(component::AttachmentActions::New(cx)->Child(
                component::Button::New(cx, StrL("remove-clickable-attachment"))
                    ->Ghost()
                    ->WithSize(UiSize::XSmall)
                    ->Icon(IconName::Close)
                    ->OnClick(Listen(cx, &AttachmentStory::OnRemoveCard))
                    ->IntoEl()))
            ->IntoEl());
    page->Child(click);

    El* states = AttachmentSection(
        cx, "Upload states",
        "Typed titles and descriptions inherit loading and failure states "
        "automatically.",
        12);
    StorySectionAdd(states, component::Attachment::New(cx)
                                ->Status(AttachmentStatus::Pending)
                                ->Media(FileMedia(cx))
                                ->Content(TitleAndDescription(
                                    cx, "meeting-notes.pdf", "Ready to upload"))
                                ->IntoEl());
    StorySectionAdd(
        states,
        component::Attachment::New(cx)
            ->Status(AttachmentStatus::Uploading)
            ->Media(FileMedia(cx))
            ->Content(
                TitleAndDescription(cx, "design-assets.zip", "Uploading · 68%")
                    ->Child(component::Progress::New(cx)
                                ->Id(StrL("attachment-upload-progress"))
                                ->Value(68)
                                ->IntoEl()))
            ->Actions(component::AttachmentActions::New(cx)->Child(
                CloseAction(cx, "cancel-upload", "Cancel upload")))
            ->IntoEl());
    StorySectionAdd(
        states,
        component::Attachment::New(cx)
            ->Status(AttachmentStatus::Processing)
            ->Media(FileMedia(cx))
            ->Content(component::AttachmentContent::New(cx)
                          ->Title(component::AttachmentTitle::New(
                                      cx, StrL("transcript.pdf"))
                                      ->WithShimmerStyle(
                                          component::ShimmerStyle::New()
                                              .HighlightColor(th.primary)
                                              .Spread(0.45f)
                                              .Reverse(true)))
                          ->Description(component::AttachmentDescription::New(
                              cx, StrL("Processing document"))))
            ->IntoEl());
    StorySectionAdd(
        states,
        component::Attachment::New(cx)
            ->Status(AttachmentStatus::Failed)
            ->Media(FileMedia(cx))
            ->Content(TitleAndDescription(cx, "archive.zip", "Upload failed"))
            ->Actions(
                component::AttachmentActions::New(cx)
                    ->Child(component::Button::New(cx, StrL("retry-upload"))
                                ->WithSize(UiSize::XSmall)
                                ->Label(StrL("Retry"))
                                ->IntoEl())
                    ->Child(
                        component::Button::New(cx, StrL("remove-failed-upload"))
                            ->Danger()
                            ->WithSize(UiSize::XSmall)
                            ->Icon(IconName::Delete)
                            ->Tooltip(StrL("Remove archive.zip"))
                            ->IntoEl()))
            ->IntoEl());
    Style successMedia = {};
    successMedia.color = th.success;
    StorySectionAdd(
        states,
        component::Attachment::New(cx)
            ->Status(AttachmentStatus::Complete)
            ->Media(component::AttachmentMedia::New(cx)
                        ->Refine(successMedia, StyleFieldColor)
                        ->Child(component::Icon::New(cx, IconName::CircleCheck)
                                    ->IntoEl()))
            ->Content(TitleAndDescription(cx, "published-report.pdf",
                                          "Uploaded · 1.8 MB"))
            ->Actions(component::AttachmentActions::New(cx)->Child(CloseAction(
                cx, "remove-complete-upload", "Remove published-report.pdf")))
            ->IntoEl());
    page->Child(states);

    El* optional = AttachmentSection(
        cx, "Optional slots",
        "Media, metadata, and actions remain independently composable.", 12);
    StorySectionAdd(optional, component::Attachment::New(cx)
                                  ->Media(FileMedia(cx))
                                  ->IntoEl());
    StorySectionAdd(optional, component::Attachment::New(cx)
                                  ->Content(TitleAndDescription(
                                      cx, "metadata-only.txt", "Text · 1 KB"))
                                  ->IntoEl());
    StorySectionAdd(
        optional,
        component::Attachment::New(cx)
            ->Content(component::AttachmentContent::New(cx)
                          ->Title(component::AttachmentTitle::New(
                              cx, StrL("ready-for-review.pdf"))))
            ->Actions(component::AttachmentActions::New(cx)->Child(
                component::Button::New(cx, StrL("attachment-review-file"))
                    ->Ghost()
                    ->WithSize(UiSize::Small)
                    ->Label(StrL("Open"))
                    ->IntoEl()))
            ->IntoEl());
    page->Child(optional);

    El* thumbnail = AttachmentSection(
        cx, "Thumbnail",
        "Vertical attachments can turn the media slot into a full-width "
        "preview.",
        0);
    StorySectionAdd(
        thumbnail,
        component::Attachment::New(cx)
            ->WithAxis(Axis::Vertical)
            ->Media(component::AttachmentMedia::New(cx)
                        ->Src(AttachmentPreviewSrc()))
            ->Content(
                TitleAndDescription(cx, "sdk-preview.svg", "SVG · 1280 × 720"))
            ->Actions(component::AttachmentActions::New(cx)->Child(
                CloseAction(cx, "remove-preview", "Remove sdk-preview.svg")))
            ->IntoEl());
    page->Child(thumbnail);

    El* overlays = AttachmentSection(
        cx, "Image overlays",
        "Image previews keep their overlays visible while only the image "
        "dims during upload.",
        0);
    StorySectionAdd(
        overlays,
        component::Attachment::New(cx)
            ->WithAxis(Axis::Vertical)
            ->Status(AttachmentStatus::Uploading)
            ->Media(component::AttachmentMedia::New(cx)
                        ->Src(AttachmentPreviewSrc())
                        ->Overlay(component::Spinner::New(cx)
                                      ->WithSize(UiSize::Small)
                                      ->Color(th.foreground)
                                      ->IntoEl()))
            ->Content(TitleAndDescription(cx, "preview.svg", "Uploading · 72%"))
            ->IntoEl());
    page->Child(overlays);

    El* sizes = AttachmentSection(
        cx, "Sizes",
        "Semantic sizes keep the media, text, and action density aligned.", 12);
    StorySectionAdd(sizes, component::Attachment::New(cx)
                               ->WithSize(UiSize::Large)
                               ->Media(FileMedia(cx))
                               ->Content(TitleAndDescription(
                                   cx, "large.pdf", "Large · PDF · 3.1 MB"))
                               ->IntoEl());
    StorySectionAdd(sizes, component::Attachment::New(cx)
                               ->Media(FileMedia(cx))
                               ->Content(TitleAndDescription(
                                   cx, "medium.pdf", "Medium · PDF · 2.4 MB"))
                               ->IntoEl());
    StorySectionAdd(sizes, component::Attachment::New(cx)
                               ->WithSize(UiSize::Small)
                               ->Media(FileMedia(cx))
                               ->Content(TitleAndDescription(
                                   cx, "small.csv", "Small · CSV · 840 KB"))
                               ->IntoEl());
    StorySectionAdd(
        sizes, component::Attachment::New(cx)
                   ->WithSize(UiSize::XSmall)
                   ->Media(FileMedia(cx))
                   ->Content(component::AttachmentContent::New(cx)->Title(
                       component::AttachmentTitle::New(cx, StrL("xsmall.txt"))))
                   ->IntoEl());
    StorySectionAdd(
        sizes,
        component::Attachment::New(cx)
            ->WithSize(UiSize::Small)
            ->Media(component::AttachmentMedia::New(cx)
                        ->WithSize(UiSize::Large)
                        ->Child(component::Icon::New(cx, IconName::FileText)
                                    ->IntoEl()))
            ->Content(TitleAndDescription(cx, "custom-media.pdf",
                                          "Large media in a small attachment"))
            ->IntoEl());
    page->Child(sizes);

    El* group = AttachmentSection(
        cx, "Group",
        "Attachment groups arrange multiple files in a scrollable row.", 0);
    StorySectionAdd(
        group,
        component::AttachmentGroup::New(cx, StrL("attachment-story-group"))
            ->Child(component::Attachment::New(cx)
                        ->Media(FileMedia(cx))
                        ->Content(TitleAndDescription(cx, "default.pdf",
                                                      "PDF · 2.4 MB"))
                        ->IntoEl())
            ->Child(component::Attachment::New(cx)
                        ->WithSize(UiSize::Small)
                        ->Media(FileMedia(cx))
                        ->Content(TitleAndDescription(cx, "small.csv",
                                                      "CSV · 840 KB"))
                        ->IntoEl())
            ->Child(component::Attachment::New(cx)
                        ->WithSize(UiSize::XSmall)
                        ->Media(FileMedia(cx))
                        ->Content(component::AttachmentContent::New(cx)
                                      ->Title(component::AttachmentTitle::New(
                                          cx, StrL("compact.txt"))))
                        ->IntoEl())
            ->IntoEl());
    page->Child(group);

    El* orientation = AttachmentSection(
        cx, "Orientation",
        "The same named slots support horizontal and vertical layouts.", 12);
    StorySectionAdd(orientation,
                    component::Attachment::New(cx)
                        ->WithAxis(Axis::Horizontal)
                        ->Media(FileMedia(cx))
                        ->Content(TitleAndDescription(cx, "horizontal.pdf",
                                                      "Horizontal layout"))
                        ->IntoEl());
    StorySectionAdd(orientation, component::Attachment::New(cx)
                                     ->WithAxis(Axis::Vertical)
                                     ->Media(FileMedia(cx))
                                     ->Content(TitleAndDescription(
                                         cx, "vertical.pdf", "Vertical layout"))
                                     ->IntoEl());
    page->Child(orientation);

    El* inheritance = AttachmentSection(
        cx, "Status inheritance",
        "Typed children inherit lifecycle state unless explicitly "
        "overridden.",
        12);
    StorySectionAdd(inheritance, component::Attachment::New(cx)
                                     ->Status(AttachmentStatus::Uploading)
                                     ->Media(FileMedia(cx))
                                     ->Content(TitleAndDescription(
                                         cx, "inherited-title.pdf",
                                         "Inherited loading appearance"))
                                     ->IntoEl());
    StorySectionAdd(
        inheritance,
        component::Attachment::New(cx)
            ->Status(AttachmentStatus::Uploading)
            ->Media(FileMedia(cx))
            ->Content(component::AttachmentContent::New(cx)
                          ->Title(component::AttachmentTitle::New(
                                      cx, StrL("stable-title.pdf"))
                                      ->Status(AttachmentStatus::Complete))
                          ->Description(component::AttachmentDescription::New(
                              cx, StrL("Explicit title status disables its "
                                       "shimmer"))))
            ->IntoEl());
    page->Child(inheritance);

    El* longNames = AttachmentSection(
        cx, "Long filenames",
        "Long metadata truncates within a constrained, zoom-aware surface.", 0);
    StorySectionAdd(
        longNames,
        component::Attachment::New(cx)
            ->Media(FileMedia(cx))
            ->Content(TitleAndDescription(
                cx, "accessibility-review-and-keyboard-navigation-findings.pdf",
                "Final report · reviewed by the desktop experience team"))
            ->IntoEl()
            // w_72
            ->W(288));
    page->Child(longNames);

    El* trigger = AttachmentSection(
        cx, "Attachment trigger",
        "Use the existing Button component to add files to a composer.", 0);
    StorySectionAdd(trigger,
                    component::Button::New(cx, StrL("attachment-add-files"))
                        ->Outline()
                        ->Icon(IconName::FileText)
                        ->Label(StrL("Add files…"))
                        ->IntoEl());
    page->Child(trigger);

    El* custom = AttachmentSection(
        cx, "Custom style",
        "Every public part accepts caller style refinements.", 0);
    Style customMedia = {};
    customMedia.radius = th.radius;
    customMedia.bg = Background(RgbaOpacity(th.primary, 0.12f));
    customMedia.color = th.primary;
    Style customTitle = {};
    customTitle.color = th.primary;
    StorySectionAdd(
        custom,
        component::Attachment::New(cx)
            ->Media(component::AttachmentMedia::New(cx)
                        ->Refine(customMedia, StyleFieldRadius | StyleFieldBg |
                                                  StyleFieldColor)
                        ->Child(component::Icon::New(cx, IconName::FileText)
                                    ->IntoEl()))
            ->Content(component::AttachmentContent::New(cx)
                          ->Title(component::AttachmentTitle::New(
                                      cx, StrL("custom-theme.json"))
                                      ->Refine(customTitle, StyleFieldColor))
                          ->Description(component::AttachmentDescription::New(
                              cx, StrL("JSON · 16 KB"))))
            ->IntoEl()
            ->W(kFill)
            ->Radius(th.radius)
            ->Bg(th.tokens.accent)
            ->Border(1, RgbaOpacity(th.accent, 0.5f)));
    page->Child(custom);
    return page;
}

STORY_PAGE(StoryAttachment, AttachmentStory);
