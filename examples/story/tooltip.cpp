#include "Story.h"

enum {
    ClickTipRemove = 2600
};

El* TooltipRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* btn = StorySection(a, "Button",
                           "Add plain text or a keyboard shortcut hint.");
    El* btnRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Wrap();
    btnRow->Child(component::Button::New(a, StrL("btn0"))
                      ->Label(StrL("Search"))
                      ->Primary()
                      ->Tooltip(StrL("This is a search Button."))
                      ->IntoEl());
    btnRow->Child(component::Button::New(a, StrL("btn1"))
                      ->Label(StrL("Info"))
                      ->Tooltip(StrL("This is a tooltip with Action for "
                                     "display keybinding."))
                      ->IntoEl());
    btnRow->Child(component::Button::New(a, StrL("btn3"))
                      ->Label(StrL("Hover me"))
                      ->Tooltip(StrL("This is tooltip 3"))
                      ->IntoEl());
    StorySectionAdd(btn, btnRow);
    page->Child(btn);

    El* chk =
        StorySection(a, "Checkbox", "Tooltips work on selection controls.");
    StorySectionAdd(chk, component::Checkbox::New(a, StrL("check"))
                             ->Label(StrL("Remember me"))
                             ->Checked(true)
                             ->Tooltip(StrL("This is a tooltip"))
                             ->IntoEl());
    page->Child(chk);

    El* rad = StorySection(a, "Radio", "Explain an individual radio option.");
    StorySectionAdd(rad, component::Radio::New(a, StrL("radio"))
                             ->Label(StrL("Radio with tooltip"))
                             ->Checked(true)
                             ->IntoEl()
                             ->Tip(StrL("This is a radio button")));
    page->Child(rad);

    El* sw = StorySection(a, "Switch",
                          "Add context without extending the visible label.");
    StorySectionAdd(sw, component::Switch::New(a, StrL("switch"))
                            ->Checked(true)
                            ->IntoEl()
                            ->Tip(StrL("This is a switch")));
    page->Child(sw);

    El* tog = StorySection(a, "Toggle", "Describe text and icon-only toggles.");
    El* togRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    togRow->Child(component::Button::New(a, StrL("toggle1"))
                      ->Label(StrL("Bold"))
                      ->Outline()
                      ->Tooltip(StrL("Toggle bold"))
                      ->IntoEl());
    togRow->Child(component::Button::New(a, StrL("toggle2"))
                      ->Icon(IconName::Heart)
                      ->Ghost()
                      ->Tooltip(StrL("Toggle favorite"))
                      ->IntoEl());
    StorySectionAdd(tog, togRow);
    page->Child(tog);

    El* clip = StorySection(a, "Clipboard", "Clarify the copy action.");
    StorySectionAdd(clip, component::Clipboard::New(a, StrL("Hello, World!"))
                              ->IntoEl()
                              ->Tip(StrL("Copy to clipboard")));
    page->Child(clip);

    El* custom = StorySection(a, "Custom content",
                              "Build tooltip content with an action hint.");
    StorySectionAdd(custom,
                    StoryTxt(a, StrL("Hover me"), 14, ThemeNow().foreground)
                        ->Tip(StrL("This is a default tooltip style "
                                   "by GPUI.")));
    page->Child(custom);

    El* rem = StorySection(a, "Removed trigger",
                           "Dismiss cleanly when the trigger leaves the view.");
    if (app->tipRemoved) {
        StorySectionAdd(
            rem, StoryTxt(a, StrL("Trigger removed"), 13, ThemeNow().mutedFg));
    } else {
        StorySectionAdd(
            rem,
            component::Button::New(a, StrL("remove-tooltip-trigger"))
                ->Danger()
                ->Label(StrL("Remove me"))
                ->Tooltip(StrL("Clicking this button removes the trigger."))
                ->IntoEl()
                ->Click(ClickTipRemove));
    }
    page->Child(rem);
    return page;
}

void TooltipClick(StoryApp* app, int id) {
    if (id == ClickTipRemove) {
        app->tipRemoved = true;
    }
}

STORY_PAGE(StoryTooltip, TooltipRender, TooltipClick);
