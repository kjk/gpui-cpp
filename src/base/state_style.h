/* Semantic-state styles — crates/base/src/state_style.rs

   A control looks one way, and then differently when it is selected, pressed,
   checked, focused or disabled. Rust layers those with GPUI's StyleRefinement:
   a partial style where an unset field means "leave whatever is underneath".
   `resolve_style` puts them in one fixed order so no two controls drift apart —

     1. the instance style the caller built,
     2. the value states: checked, pressed, selected, focused,
     3. disabled, always resolved last.

   The Style in this tree is a whole style, not a refinement, so a refinement
   here is a Style plus the set of fields that were actually named. Only the
   fields a control state overrides are refinable; the rest of a control's look
   is layout, and no semantic state moves a control. */

#include "gpui/gpui.h"

namespace gpui {

// The fields a state may name. A field not named is left as it was.
enum StateField : uint32_t {
    StateFieldBg = 1u << 0,
    StateFieldFg = 1u << 1,
    StateFieldBorder = 1u << 2, // width and color together, as `border_*` is
    StateFieldRadius = 1u << 3,
    StateFieldHoverBg = 1u << 4,
    StateFieldHoverFg = 1u << 5,
};

struct StateStyle {
    Style style = {};
    uint32_t set = 0;

    StateStyle& Bg(Rgba c);
    StateStyle& Fg(Rgba c);
    StateStyle& Border(float w, Rgba c);
    StateStyle& Radius(float v);
    StateStyle& HoverBg(Rgba c);
    StateStyle& HoverFg(Rgba c);

    bool Has(StateField f) const { return (set & (uint32_t)f) != 0; }
};

// StyleRefinement::refine: `over` wins for the fields it names, and only those.
void StateStyleRefine(StateStyle* into, const StateStyle& over);

// resolve_style. The states come in the order above; one that is not active is
// simply left out, which is what Rust's `then_some(..).flatten()` does.
StateStyle StateStyleResolve(const StateStyle& instance,
                             const StateStyle* const* states, int n);

// Put a resolved style on an element.
El* ElRefine(El* e, const StateStyle& s);

} // namespace gpui
