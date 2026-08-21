/* Themed group box — crates/ui/src/group_box.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct GroupBox {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str title = {};
    // GroupBox::title takes `impl IntoElement`, not a string: the usage card
    // gives it a row with a label and a button in it.
    El* titleEl = nullptr;
    El* child = nullptr;
    // GroupBoxVariant: Normal by default, with fill() and outline() as the
    // other two.
    bool outline = false;
    bool filled = false;

    // The story's Custom Style section refines the box, its title and its
    // content container; these are the pieces it reaches for.
    bool titleSemibold = false;
    float titlePadX = 0;
    Background contentBg = {};
    bool hasContentBg = false;
    float contentRadius = -1;
    float contentPad = -1;
    float contentBorder = -1;

    static GroupBox* New(Ctx* cx, Str title);
    GroupBox* Title(El* e);
    GroupBox* Child(El* e);
    GroupBox* Outline();
    GroupBox* Filled(bool v);
    GroupBox* TitleSemibold(bool v = true);
    GroupBox* TitlePadX(float px);
    GroupBox* ContentBg(Background c);
    GroupBox* ContentRadius(float px);
    GroupBox* ContentPad(float px);
    GroupBox* ContentBorder(float px);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
