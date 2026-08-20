/* The actions every keyboard-driven control shares —
   crates/base/src/actions.rs.

   Rust declares them with `actions!(ui, [..])` and dispatches the *type*: a
   binding names it, `on_action::<SelectUp>(..)` matches on it. An action here
   is the hash of its name, so these are the names — Rust's own, namespace
   included, so "ui::SelectUp" is `ui::SelectUp`. One accessor each, because a
   name spelled two ways is two actions and nothing would say so.

   `Confirm` carries a `secondary` flag in Rust — one action type with two
   payloads, which is how a combobox tells enter from ctrl-enter. A binding
   here resolves to an id and nothing else, so the two payloads are two
   names. */

#include "gpui/gpui.h"

namespace gpui {

namespace action {

uint32_t Confirm();
// ui::Confirm { secondary: true }, which is what the secondary chord binds.
uint32_t ConfirmSecondary();
uint32_t Cancel();
uint32_t SelectUp();
uint32_t SelectDown();
uint32_t SelectLeft();
uint32_t SelectRight();
uint32_t SelectFirst();
uint32_t SelectLast();
uint32_t SelectPrevColumn();
uint32_t SelectNextColumn();
uint32_t SelectPageUp();
uint32_t SelectPageDown();

} // namespace action

} // namespace gpui
