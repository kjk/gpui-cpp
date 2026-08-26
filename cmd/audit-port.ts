// Structural fidelity gate for crates/base -> src/base and crates/ui -> src/ui.
// The Rust checkout is optional in CI; when present, its module declarations
// are checked against the pinned ledger as well as the C++ destinations.

import { existsSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";

type CrateName = "base" | "ui";
type Status = "full" | "partial" | "adapter" | "excluded";
type Entry = {
  crate: CrateName;
  module: string;
  status: Status;
  targets: string[];
  reason?: string;
};

const root = resolve(import.meta.dir, "..");
const pinnedGpuiComponent = "7885c41663c7a6cc68ad0c99b1ba33550f807ff0";

const baseModules = `
accordion actions alert_dialog animation async_util auto_scroll avatar button
calendar checkbox collapsible color_picker combobox component_traits
date_picker dialog dock element_ext event focus_trap geometry global_state
history hover_card index_path input link list_settings macos_accessibility
measure motion number_input otp_input pagination popover popup positioner
progress radio radio_group resizable scrollbar select sheet slider state_style
styled switch table tabs text_boundary text_selection theme theme_tokens toast
toggle toggle_group tooltip tree virtual_list
`.trim().split(/\s+/);

const uiModules = `
async_util component_traits element_ext global_state icon index_path inspector
root sizing styled time title_bar virtual_list window_border window_ext
accordion alert avatar badge breadcrumb button chart checkbox clipboard
collapsible color_picker combobox command description_list dialog dock form
group_box highlighter history hover_card input kbd label link list menu
native_menu notification pagination plot popover progress radio rating
resizable scroll searchable_list select separator setting sheet sidebar skeleton
slider spinner status_bar stepper switch tab table tag text theme tooltip tree
`.trim().split(/\s+/);

const partialBase = new Set([
  "global_state", "input", "macos_accessibility", "scrollbar", "styled",
  "text_selection",
]);
const adapterBase = new Set(["component_traits", "element_ext", "event", "measure"]);
const partialUi = new Set([
  "global_state", "input", "inspector", "text", "theme",
]);
const adapterUi = new Set([
  "async_util", "component_traits", "element_ext", "highlighter", "styled",
]);

const partialReasons: Record<string, string> = {
  "base/global_state": "the App global carries selection/popover state; entity-stack coverage remains partial",
  "base/input": "synchronous function-pointer providers and a flat text buffer replace Rust tasks, trait objects and Rope",
  "base/macos_accessibility": "the portable semantic tree exists; only the native macOS hit-test forwarding seam is connected",
  "base/scrollbar": "the renderer-backed element does not expose every Rust style override",
  "base/styled": "StyleRefinement is represented by the runtime El builder surface",
  "base/text_selection": "selection is window-owned rather than a GPUI entity graph",
  "ui/global_state": "selection ordering/stack state is present; text-view state remains split",
  "ui/input": "native content-type/autofill synchronization and some rich editor integrations remain smaller than Rust",
  "ui/inspector": "the inspector is intentionally smaller than GPUI's debug inspector",
  "ui/text": "a dependency-free HTML vocabulary replaces html5ever and advanced highlighting remains scanner-backed",
  "ui/theme": "filesystem watch/reload and the richer highlight/list/sheet settings remain smaller than Rust",
};

const adapterReasons: Record<string, string> = {
  "base/component_traits": "Rust traits are C++ builder conventions and function tables",
  "base/element_ext": "extension traits are methods on El plus forwarding helpers",
  "base/event": "typed GPUI closures are generational Listener records",
  "base/measure": "measurement is routed through the synchronous runtime layout seam",
  "ui/component_traits": "the UI façade re-exports Base's C++ trait conventions",
  "ui/element_ext": "extension traits are methods on El plus forwarding helpers",
  "ui/highlighter": "tree-sitter/syntect are excluded; a dependency-free scanner is used",
  "ui/styled": "fluent traits are C++ builders and the shared sizing vocabulary",
};

const baseOverrides: Record<string, string[]> = {
  async_util: [],
  dock: [
    "src/base/dock.h", "src/base/dock.cpp", "src/base/dock_area.cpp",
    "src/base/dock_state.h", "src/base/dock_state.cpp",
    "src/base/tiles.h", "src/base/tiles.cpp",
  ],
  input: [
    "src/base/input.h", "src/base/input.cpp",
    "src/base/input_keys.h", "src/base/input_keys.cpp",
  ],
  macos_accessibility: ["src/gpui/window_mac.cpp"],
};

const uiOverrides: Record<string, string[]> = {
  async_util: [],
  component_traits: ["src/ui/component_traits.h", "src/base/component_traits.h"],
  element_ext: ["src/ui/element_ext.h", "src/base/element_ext.h"],
  global_state: ["src/ui/global_state.h", "src/ui/global_state.cpp"],
  history: ["src/ui/history.h", "src/base/history.h", "src/base/history.cpp"],
  index_path: ["src/ui/index_path.h", "src/base/index_path.h", "src/base/index_path.cpp"],
  styled: ["src/ui/styled.h", "src/base/styled.h", "src/ui/sizing.h"],
  dock: ["src/ui/dock.h", "src/ui/dock.cpp", "src/ui/tiles.h", "src/ui/tiles.cpp"],
  highlighter: [
    "src/ui/highlighter.h", "src/ui/highlighter.cpp",
    "src/ui/syntax.h", "src/ui/syntax.cpp",
  ],
  list: ["src/ui/list.h", "src/ui/list.cpp", "src/base/list.h", "src/base/list.cpp"],
  menu: [
    "src/ui/menu.h", "src/ui/menu.cpp", "src/ui/popup_menu.h",
    "src/base/popup_menu.h", "src/base/popup_menu.cpp",
  ],
  plot: ["src/ui/plot.h", "src/ui/plot.cpp", "src/ui/sankey.h", "src/base/sankey.h", "src/base/sankey.cpp"],
  resizable: [
    "src/ui/resizable.h", "src/ui/resizable.cpp",
    "src/base/resizable.h", "src/base/resizable.cpp",
  ],
  text: [
    "src/ui/text.h", "src/ui/text.cpp", "src/ui/html.h", "src/ui/html.cpp",
  ],
  table: [
    "src/ui/table.h", "src/ui/table.cpp", "src/ui/data_table.h",
    "src/base/data_table.h", "src/base/data_table.cpp",
  ],
  theme: [
    "src/ui/theme.h", "src/ui/theme.cpp", "src/ui/theme_data.cpp",
  ],
};

function defaultTargets(crate: CrateName, module: string): string[] {
  const stem = `src/${crate}/${module}`;
  const targets: string[] = [];
  if (existsSync(join(root, `${stem}.h`))) targets.push(`${stem}.h`);
  if (existsSync(join(root, `${stem}.cpp`))) targets.push(`${stem}.cpp`);
  return targets;
}

function entriesFor(crate: CrateName, modules: string[]): Entry[] {
  return modules.map((module) => {
    const overrides = crate === "base" ? baseOverrides : uiOverrides;
    const partial = crate === "base" ? partialBase : partialUi;
    const adapter = crate === "base" ? adapterBase : adapterUi;
    let status: Status = partial.has(module) ? "partial" : adapter.has(module) ? "adapter" : "full";
    if (module === "async_util") status = "excluded";
    const key = `${crate}/${module}`;
    return {
      crate,
      module,
      status,
      targets: module in overrides ? overrides[module] : defaultTargets(crate, module),
      reason: module === "async_util"
        ? "async is a standing repository non-goal"
        : partialReasons[key] ?? adapterReasons[key],
    };
  });
}

const entries = [...entriesFor("base", baseModules), ...entriesFor("ui", uiModules)];
const errors: string[] = [];

for (const entry of entries) {
  if (entry.status !== "excluded" && entry.targets.length === 0) {
    errors.push(`${entry.crate}/${entry.module}: no C++ destination`);
  }
  for (const target of entry.targets) {
    if (!existsSync(join(root, target))) {
      errors.push(`${entry.crate}/${entry.module}: missing ${target}`);
    }
  }
  if (entry.status !== "full" && !entry.reason) {
    errors.push(`${entry.crate}/${entry.module}: ${entry.status} entry has no reason`);
  }
}

function modulesIn(path: string): string[] {
  const text = readFileSync(path, "utf8");
  const found = [...text.matchAll(/^\s*(?:pub\s+)?mod\s+([a-zA-Z0-9_]+)/gm)]
    .map((m) => m[1]);
  return [...new Set(found)].sort();
}

function sameMembers(label: string, actual: string[], expected: string[]) {
  const a = [...actual].sort();
  const e = [...expected].sort();
  for (const name of e) if (!a.includes(name)) errors.push(`${label}: unclassified upstream module ${name}`);
  for (const name of a) if (!e.includes(name)) errors.push(`${label}: ledger names missing upstream module ${name}`);
}

const rustRoot = join(root, ".work", "gpui-component", "crates");
if (existsSync(rustRoot)) {
  sameMembers("base", modulesIn(join(rustRoot, "base", "src", "lib.rs")), baseModules);
  sameMembers("ui", modulesIn(join(rustRoot, "ui", "src", "lib.rs")), uiModules);
}

const runText = readFileSync(join(root, "cmd", "run.ts"), "utf8");
const pinAt = runText.indexOf("export const gpuiComponent");
const pinText = pinAt >= 0 ? runText.slice(pinAt, pinAt + 500) : "";
if (!pinText.includes(pinnedGpuiComponent)) {
  errors.push(`ledger pin ${pinnedGpuiComponent} differs from cmd/run.ts`);
}

// Layering invariant: GPUI owns only the small style record its renderer
// consumes; the component palette and its Base conversion live under ui.
const gpuiHeader = readFileSync(join(root, "src/gpui/gpui.h"), "utf8");
const gpuiSources = ["src/gpui/gpui.cpp", "src/gpui/entity.cpp", "src/gpui/window_common.cpp"]
  .map((path) => readFileSync(join(root, path), "utf8"))
  .join("\n");
const uiThemeHeader = readFileSync(join(root, "src/ui/theme.h"), "utf8");
const baseThemeTokens = readFileSync(join(root, "src/base/theme_tokens.h"), "utf8");
const windowCommon = readFileSync(join(root, "src/gpui/window_common.cpp"), "utf8");
if (/\bstruct\s+Theme(?:Tokens)?\b/.test(gpuiHeader)) {
  errors.push("theme layering: component Theme type leaked into gpui/gpui.h");
}
if (/\bTheme(?:Now|Light|Dark|Install|SemanticTokens)\s*\(/.test(gpuiSources)) {
  errors.push("theme layering: GPUI runtime reads the component palette directly");
}
if (!/\bstruct\s+Theme\b/.test(uiThemeHeader) ||
    !/\bstruct\s+ThemeTokens\b/.test(uiThemeHeader)) {
  errors.push("theme layering: ui/theme.h does not own both component theme types");
}
if (/\bThemeSemanticTokens\s*\(/.test(baseThemeTokens)) {
  errors.push("theme layering: Base declares a conversion from the component palette");
}

// Accessibility invariant: Base/UI roles are frame data owned by GPUI, then
// projected after layout beside focus/hit state. Actions must route back
// through the same listener/state seams as pointer and keyboard input.
for (const marker of [
  "struct AccessibilityInfo", "struct AccessibilityNode",
  "Vec<AccessibilityNode> accessibility", "AccessibilityCollect(El* root",
  "OnAccessibilityIncrement", "AccessibilityActionSetValue",
]) {
  if (!gpuiHeader.includes(marker)) {
    errors.push(`accessibility runtime: missing ${marker}`);
  }
}
if (!windowCommon.includes("AccessibilityCollect(root, &win->accessibility)") ||
    !windowCommon.includes("WindowAccessibilityPerform(")) {
  errors.push("accessibility runtime: frame projection or action dispatch is not wired");
}

const counts = (status: Status) => entries.filter((e) => e.status === status).length;
console.log(
  `port audit: ${entries.length} modules at ${pinnedGpuiComponent.slice(0, 12)} ` +
  `(${counts("full")} full, ${counts("partial")} partial, ` +
  `${counts("adapter")} adapters, ${counts("excluded")} excluded)`,
);
if (!existsSync(rustRoot)) console.log("port audit: Rust checkout absent; checked pinned ledger and C++ destinations");

if (errors.length) {
  for (const error of errors) console.error(`error: ${error}`);
  process.exit(1);
}
