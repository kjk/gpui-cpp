/* Project the portable frame accessibility tree through Windows UI
   Automation. This is a raw fragment provider: nodes are light COM handles
   containing only the stable frame id, and every query resolves that id in
   the latest Window::accessibility rather than retaining arena strings. */

#include "gpui/accessibility_win.h"

#include <uiautomation.h>

namespace gpui {

struct WinAccessibilityNode;

static BSTR AccessibilityBstr(Str value) {
    if (!value.s || value.len <= 0) {
        return SysAllocStringLen(nullptr, 0);
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, value.s, value.len, nullptr, 0);
    if (n <= 0) {
        return SysAllocStringLen(nullptr, 0);
    }
    BSTR out = SysAllocStringLen(nullptr, (UINT)n);
    if (!out) {
        return nullptr;
    }
    MultiByteToWideChar(CP_UTF8, 0, value.s, value.len, out, n);
    return out;
}

static void VariantInt(VARIANT* out, int value) {
    VariantInit(out);
    out->vt = VT_I4;
    out->lVal = value;
}

static void VariantBool(VARIANT* out, bool value) {
    VariantInit(out);
    out->vt = VT_BOOL;
    out->boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
}

static void VariantDouble(VARIANT* out, double value) {
    VariantInit(out);
    out->vt = VT_R8;
    out->dblVal = value;
}

static void VariantString(VARIANT* out, Str value) {
    VariantInit(out);
    out->vt = VT_BSTR;
    out->bstrVal = AccessibilityBstr(value);
}

static int AccessibilityControlType(AccessibilityRole role) {
    switch (role) {
        case AccessibilityRole::Alert:
        case AccessibilityRole::Heading:
            return UIA_TextControlTypeId;
        case AccessibilityRole::AlertDialog:
        case AccessibilityRole::Dialog:
            return UIA_WindowControlTypeId;
        case AccessibilityRole::Button:
            return UIA_ButtonControlTypeId;
        case AccessibilityRole::Cell:
        case AccessibilityRole::Row:
            return UIA_DataItemControlTypeId;
        case AccessibilityRole::CheckBox:
        case AccessibilityRole::Switch:
            return UIA_CheckBoxControlTypeId;
        case AccessibilityRole::ColumnHeader:
            return UIA_HeaderItemControlTypeId;
        case AccessibilityRole::ComboBox:
            return UIA_ComboBoxControlTypeId;
        case AccessibilityRole::DateInput:
        case AccessibilityRole::DateTimeInput:
        case AccessibilityRole::EmailInput:
        case AccessibilityRole::MultilineTextInput:
        case AccessibilityRole::PasswordInput:
        case AccessibilityRole::PhoneNumberInput:
        case AccessibilityRole::TextInput:
        case AccessibilityRole::UrlInput:
            return UIA_EditControlTypeId;
        case AccessibilityRole::Link:
            return UIA_HyperlinkControlTypeId;
        case AccessibilityRole::List:
        case AccessibilityRole::ListBox:
            return UIA_ListControlTypeId;
        case AccessibilityRole::ListBoxOption:
        case AccessibilityRole::ListItem:
            return UIA_ListItemControlTypeId;
        case AccessibilityRole::Menu:
            return UIA_MenuControlTypeId;
        case AccessibilityRole::MenuBar:
            return UIA_MenuBarControlTypeId;
        case AccessibilityRole::MenuItem:
            return UIA_MenuItemControlTypeId;
        case AccessibilityRole::ProgressIndicator:
            return UIA_ProgressBarControlTypeId;
        case AccessibilityRole::RadioButton:
            return UIA_RadioButtonControlTypeId;
        case AccessibilityRole::Slider:
            return UIA_SliderControlTypeId;
        case AccessibilityRole::SpinButton:
            return UIA_SpinnerControlTypeId;
        case AccessibilityRole::Tab:
            return UIA_TabItemControlTypeId;
        case AccessibilityRole::Table:
            return UIA_DataGridControlTypeId;
        case AccessibilityRole::TabList:
            return UIA_TabControlTypeId;
        case AccessibilityRole::Toolbar:
            return UIA_ToolBarControlTypeId;
        case AccessibilityRole::Tooltip:
            return UIA_ToolTipControlTypeId;
        case AccessibilityRole::GenericContainer:
        case AccessibilityRole::Group:
        case AccessibilityRole::Navigation:
        case AccessibilityRole::RadioGroup:
        case AccessibilityRole::Region:
        case AccessibilityRole::RowGroup:
        case AccessibilityRole::None:
        default:
            return UIA_GroupControlTypeId;
    }
}

static const wchar_t* AccessibilityRoleName(AccessibilityRole role) {
    switch (role) {
        case AccessibilityRole::Alert:
            return L"alert";
        case AccessibilityRole::AlertDialog:
            return L"alertdialog";
        case AccessibilityRole::Button:
            return L"button";
        case AccessibilityRole::Cell:
            return L"cell";
        case AccessibilityRole::CheckBox:
            return L"checkbox";
        case AccessibilityRole::ColumnHeader:
            return L"columnheader";
        case AccessibilityRole::ComboBox:
            return L"combobox";
        case AccessibilityRole::Dialog:
            return L"dialog";
        case AccessibilityRole::Heading:
            return L"heading";
        case AccessibilityRole::Link:
            return L"link";
        case AccessibilityRole::List:
            return L"list";
        case AccessibilityRole::ListBox:
            return L"listbox";
        case AccessibilityRole::ListBoxOption:
            return L"option";
        case AccessibilityRole::ListItem:
            return L"listitem";
        case AccessibilityRole::Menu:
            return L"menu";
        case AccessibilityRole::MenuBar:
            return L"menubar";
        case AccessibilityRole::MenuItem:
            return L"menuitem";
        case AccessibilityRole::Navigation:
            return L"navigation";
        case AccessibilityRole::ProgressIndicator:
            return L"progressbar";
        case AccessibilityRole::RadioButton:
            return L"radio";
        case AccessibilityRole::RadioGroup:
            return L"radiogroup";
        case AccessibilityRole::Region:
            return L"region";
        case AccessibilityRole::Row:
            return L"row";
        case AccessibilityRole::RowGroup:
            return L"rowgroup";
        case AccessibilityRole::Slider:
            return L"slider";
        case AccessibilityRole::SpinButton:
            return L"spinbutton";
        case AccessibilityRole::Switch:
            return L"switch";
        case AccessibilityRole::Tab:
            return L"tab";
        case AccessibilityRole::Table:
            return L"table";
        case AccessibilityRole::TabList:
            return L"tablist";
        case AccessibilityRole::TextInput:
        case AccessibilityRole::MultilineTextInput:
        case AccessibilityRole::EmailInput:
        case AccessibilityRole::PasswordInput:
        case AccessibilityRole::PhoneNumberInput:
        case AccessibilityRole::UrlInput:
        case AccessibilityRole::DateInput:
        case AccessibilityRole::DateTimeInput:
            return L"textbox";
        case AccessibilityRole::Toolbar:
            return L"toolbar";
        case AccessibilityRole::Tooltip:
            return L"tooltip";
        case AccessibilityRole::GenericContainer:
        case AccessibilityRole::Group:
        case AccessibilityRole::None:
        default:
            return L"group";
    }
}

static bool AccessibilityTextRole(AccessibilityRole role) {
    switch (role) {
        case AccessibilityRole::DateInput:
        case AccessibilityRole::DateTimeInput:
        case AccessibilityRole::EmailInput:
        case AccessibilityRole::MultilineTextInput:
        case AccessibilityRole::PasswordInput:
        case AccessibilityRole::PhoneNumberInput:
        case AccessibilityRole::TextInput:
        case AccessibilityRole::UrlInput:
            return true;
        default:
            return false;
    }
}

static bool AccessibilitySelectionItemRole(AccessibilityRole role) {
    return role == AccessibilityRole::ListBoxOption ||
           role == AccessibilityRole::RadioButton ||
           role == AccessibilityRole::Tab;
}

static bool AccessibilityInvokePattern(const AccessibilityNode& node) {
    if (!(node.actions & AccessibilityActionDefault)) {
        return false;
    }
    if (node.info.role == AccessibilityRole::Button ||
        node.info.role == AccessibilityRole::Link ||
        node.info.role == AccessibilityRole::MenuItem) {
        return true;
    }
    // Controls with a more specific UIA action pattern should not also claim
    // Invoke merely because both route to the same portable Default action.
    return node.info.toggled == AccessibilityToggled::Unset &&
           !node.info.hasExpanded &&
           !(node.info.hasSelected &&
             AccessibilitySelectionItemRole(node.info.role));
}

struct WinAccessibility : IRawElementProviderSimple,
                          IRawElementProviderFragment,
                          IRawElementProviderFragmentRoot {
    LONG refs = 1;
    Window* win = nullptr;
    HWND hwnd = nullptr;

    WinAccessibility(Window* window, HWND handle) : win(window), hwnd(handle) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE
    get_ProviderOptions(ProviderOptions* out) override;
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID id,
                                                 IUnknown** out) override;
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID id,
                                               VARIANT* out) override;
    HRESULT STDMETHODCALLTYPE
    get_HostRawElementProvider(IRawElementProviderSimple** out) override;
    HRESULT STDMETHODCALLTYPE
    Navigate(NavigateDirection direction,
             IRawElementProviderFragment** out) override;
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** out) override;
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* out) override;
    HRESULT STDMETHODCALLTYPE
    GetEmbeddedFragmentRoots(SAFEARRAY** out) override;
    HRESULT STDMETHODCALLTYPE SetFocus() override;
    HRESULT STDMETHODCALLTYPE
    get_FragmentRoot(IRawElementProviderFragmentRoot** out) override;
    HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(
        double x, double y, IRawElementProviderFragment** out) override;
    HRESULT STDMETHODCALLTYPE
    GetFocus(IRawElementProviderFragment** out) override;

    const AccessibilityNode* Node(uint32_t id) const {
        return win ? WindowAccessibilityNode(win, id) : nullptr;
    }
    int NodeIndex(uint32_t id) const {
        if (!win) {
            return -1;
        }
        for (int i = 0; i < win->accessibility.len; i++) {
            if (win->accessibility[i].id == id) {
                return i;
            }
        }
        return -1;
    }
    IRawElementProviderFragment* NewNode(int index);
};

struct WinAccessibilityNode : IRawElementProviderSimple,
                              IRawElementProviderFragment,
                              IInvokeProvider,
                              IToggleProvider,
                              IValueProvider,
                              IRangeValueProvider,
                              IExpandCollapseProvider,
                              ISelectionItemProvider {
    LONG refs = 1;
    WinAccessibility* root = nullptr;
    uint32_t id = 0;

    WinAccessibilityNode(WinAccessibility* owner, uint32_t nodeId)
        : root(owner), id(nodeId) {
        root->AddRef();
    }
    ~WinAccessibilityNode() { root->Release(); }

    const AccessibilityNode* Node() const { return root->Node(id); }
    HRESULT Missing() const {
        return Node() ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE
    get_ProviderOptions(ProviderOptions* out) override;
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID pattern,
                                                 IUnknown** out) override;
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID property,
                                               VARIANT* out) override;
    HRESULT STDMETHODCALLTYPE
    get_HostRawElementProvider(IRawElementProviderSimple** out) override;
    HRESULT STDMETHODCALLTYPE
    Navigate(NavigateDirection direction,
             IRawElementProviderFragment** out) override;
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** out) override;
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* out) override;
    HRESULT STDMETHODCALLTYPE
    GetEmbeddedFragmentRoots(SAFEARRAY** out) override;
    HRESULT STDMETHODCALLTYPE SetFocus() override;
    HRESULT STDMETHODCALLTYPE
    get_FragmentRoot(IRawElementProviderFragmentRoot** out) override;
    HRESULT STDMETHODCALLTYPE Invoke() override;
    HRESULT STDMETHODCALLTYPE Toggle() override;
    HRESULT STDMETHODCALLTYPE get_ToggleState(ToggleState* out) override;
    HRESULT STDMETHODCALLTYPE SetValue(LPCWSTR value) override;
    HRESULT STDMETHODCALLTYPE get_Value(BSTR* out) override;
    HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL* out) override;
    HRESULT STDMETHODCALLTYPE SetValue(double value) override;
    HRESULT STDMETHODCALLTYPE get_Value(double* out) override;
    HRESULT STDMETHODCALLTYPE get_Maximum(double* out) override;
    HRESULT STDMETHODCALLTYPE get_Minimum(double* out) override;
    HRESULT STDMETHODCALLTYPE get_LargeChange(double* out) override;
    HRESULT STDMETHODCALLTYPE get_SmallChange(double* out) override;
    HRESULT STDMETHODCALLTYPE Expand() override;
    HRESULT STDMETHODCALLTYPE Collapse() override;
    HRESULT STDMETHODCALLTYPE
    get_ExpandCollapseState(ExpandCollapseState* out) override;
    HRESULT STDMETHODCALLTYPE Select() override;
    HRESULT STDMETHODCALLTYPE AddToSelection() override;
    HRESULT STDMETHODCALLTYPE RemoveFromSelection() override;
    HRESULT STDMETHODCALLTYPE get_IsSelected(BOOL* out) override;
    HRESULT STDMETHODCALLTYPE
    get_SelectionContainer(IRawElementProviderSimple** out) override;
};

IRawElementProviderFragment* WinAccessibility::NewNode(int index) {
    if (!win || index < 0 || index >= win->accessibility.len) {
        return nullptr;
    }
    return static_cast<IRawElementProviderFragment*>(
        new WinAccessibilityNode(this, win->accessibility[index].id));
}

HRESULT WinAccessibility::QueryInterface(REFIID iid, void** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    if (iid == __uuidof(IUnknown) ||
        iid == __uuidof(IRawElementProviderSimple)) {
        *out = static_cast<IRawElementProviderSimple*>(this);
    } else if (iid == __uuidof(IRawElementProviderFragment)) {
        *out = static_cast<IRawElementProviderFragment*>(this);
    } else if (iid == __uuidof(IRawElementProviderFragmentRoot)) {
        *out = static_cast<IRawElementProviderFragmentRoot*>(this);
    }
    if (!*out) {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG WinAccessibility::AddRef() {
    return (ULONG)InterlockedIncrement(&refs);
}

ULONG WinAccessibility::Release() {
    ULONG left = (ULONG)InterlockedDecrement(&refs);
    if (!left) {
        delete this;
    }
    return left;
}

HRESULT WinAccessibility::get_ProviderOptions(ProviderOptions* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = ProviderOptions_ServerSideProvider;
    return S_OK;
}

HRESULT WinAccessibility::GetPatternProvider(PATTERNID, IUnknown** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    return S_OK;
}

HRESULT WinAccessibility::GetPropertyValue(PROPERTYID property, VARIANT* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    VariantInit(out);
    if (!win || !hwnd) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    if (property == UIA_ControlTypePropertyId) {
        VariantInt(out, UIA_PaneControlTypeId);
    } else if (property == UIA_NamePropertyId) {
        int n = GetWindowTextLengthW(hwnd);
        BSTR value = SysAllocStringLen(nullptr, (UINT)n);
        if (!value && n > 0) {
            return E_OUTOFMEMORY;
        }
        if (value) {
            GetWindowTextW(hwnd, value, n + 1);
        }
        out->vt = VT_BSTR;
        out->bstrVal = value;
    } else if (property == UIA_IsControlElementPropertyId ||
               property == UIA_IsContentElementPropertyId ||
               property == UIA_IsEnabledPropertyId) {
        VariantBool(out, true);
    } else if (property == UIA_IsKeyboardFocusablePropertyId) {
        VariantBool(out, true);
    } else if (property == UIA_HasKeyboardFocusPropertyId) {
        VariantBool(out, ::GetFocus() == hwnd);
    }
    return S_OK;
}

HRESULT WinAccessibility::get_HostRawElementProvider(
    IRawElementProviderSimple** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    return hwnd ? UiaHostProviderFromHwnd(hwnd, out)
                : UIA_E_ELEMENTNOTAVAILABLE;
}

HRESULT WinAccessibility::Navigate(NavigateDirection direction,
                                   IRawElementProviderFragment** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    if (!win) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    if (direction != NavigateDirection_FirstChild &&
        direction != NavigateDirection_LastChild) {
        return S_OK;
    }
    int found = -1;
    for (int i = 0; i < win->accessibility.len; i++) {
        if (win->accessibility[i].parent != -1) {
            continue;
        }
        found = i;
        if (direction == NavigateDirection_FirstChild) {
            break;
        }
    }
    *out = NewNode(found);
    return S_OK;
}

HRESULT WinAccessibility::GetRuntimeId(SAFEARRAY** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    return S_OK;
}

HRESULT WinAccessibility::get_BoundingRectangle(UiaRect* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = {};
    if (!hwnd) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    RECT rect = {};
    POINT origin = {};
    GetClientRect(hwnd, &rect);
    ClientToScreen(hwnd, &origin);
    out->left = origin.x;
    out->top = origin.y;
    out->width = rect.right - rect.left;
    out->height = rect.bottom - rect.top;
    return S_OK;
}

HRESULT WinAccessibility::GetEmbeddedFragmentRoots(SAFEARRAY** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    return S_OK;
}

HRESULT WinAccessibility::SetFocus() {
    if (!hwnd) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    ::SetFocus(hwnd);
    return S_OK;
}

HRESULT WinAccessibility::get_FragmentRoot(
    IRawElementProviderFragmentRoot** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = static_cast<IRawElementProviderFragmentRoot*>(this);
    AddRef();
    return S_OK;
}

HRESULT WinAccessibility::ElementProviderFromPoint(
    double x, double y, IRawElementProviderFragment** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    if (!win || !hwnd) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    POINT point = {(LONG)x, (LONG)y};
    ScreenToClient(hwnd, &point);
    int found = -1;
    for (int i = 0; i < win->accessibility.len; i++) {
        const Bounds& b = win->accessibility[i].bounds;
        if ((float)point.x >= b.x && (float)point.x <= b.Right() &&
            (float)point.y >= b.y && (float)point.y <= b.Bottom()) {
            // The tree is preorder; a later containing node is a deeper child
            // or a later painted sibling and is the one hit-testing should use.
            found = i;
        }
    }
    *out = NewNode(found);
    return S_OK;
}

HRESULT WinAccessibility::GetFocus(IRawElementProviderFragment** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    if (!win) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    for (int i = 0; i < win->accessibility.len; i++) {
        const AccessibilityNode& node = win->accessibility[i];
        if (node.focusId && node.focusId == win->focusId) {
            *out = NewNode(i);
            break;
        }
    }
    return S_OK;
}

HRESULT WinAccessibilityNode::QueryInterface(REFIID iid, void** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    if (iid == __uuidof(IUnknown) ||
        iid == __uuidof(IRawElementProviderSimple)) {
        *out = static_cast<IRawElementProviderSimple*>(this);
    } else if (iid == __uuidof(IRawElementProviderFragment)) {
        *out = static_cast<IRawElementProviderFragment*>(this);
    } else if (iid == __uuidof(IInvokeProvider)) {
        *out = static_cast<IInvokeProvider*>(this);
    } else if (iid == __uuidof(IToggleProvider)) {
        *out = static_cast<IToggleProvider*>(this);
    } else if (iid == __uuidof(IValueProvider)) {
        *out = static_cast<IValueProvider*>(this);
    } else if (iid == __uuidof(IRangeValueProvider)) {
        *out = static_cast<IRangeValueProvider*>(this);
    } else if (iid == __uuidof(IExpandCollapseProvider)) {
        *out = static_cast<IExpandCollapseProvider*>(this);
    } else if (iid == __uuidof(ISelectionItemProvider)) {
        *out = static_cast<ISelectionItemProvider*>(this);
    }
    if (!*out) {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG WinAccessibilityNode::AddRef() {
    return (ULONG)InterlockedIncrement(&refs);
}

ULONG WinAccessibilityNode::Release() {
    ULONG left = (ULONG)InterlockedDecrement(&refs);
    if (!left) {
        delete this;
    }
    return left;
}

HRESULT WinAccessibilityNode::get_ProviderOptions(ProviderOptions* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = ProviderOptions_ServerSideProvider;
    return S_OK;
}

HRESULT WinAccessibilityNode::GetPatternProvider(PATTERNID pattern,
                                                 IUnknown** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    const AccessibilityNode* node = Node();
    if (!node) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    if (pattern == UIA_InvokePatternId && AccessibilityInvokePattern(*node)) {
        return QueryInterface(__uuidof(IInvokeProvider), (void**)out);
    }
    if (pattern == UIA_TogglePatternId &&
        node->info.toggled != AccessibilityToggled::Unset) {
        return QueryInterface(__uuidof(IToggleProvider), (void**)out);
    }
    if (pattern == UIA_ValuePatternId &&
        AccessibilityTextRole(node->info.role)) {
        return QueryInterface(__uuidof(IValueProvider), (void**)out);
    }
    if (pattern == UIA_RangeValuePatternId && node->slider &&
        node->info.hasNumericValue) {
        return QueryInterface(__uuidof(IRangeValueProvider), (void**)out);
    }
    if (pattern == UIA_ExpandCollapsePatternId && node->info.hasExpanded) {
        return QueryInterface(__uuidof(IExpandCollapseProvider), (void**)out);
    }
    if (pattern == UIA_SelectionItemPatternId && node->info.hasSelected &&
        AccessibilitySelectionItemRole(node->info.role)) {
        return QueryInterface(__uuidof(ISelectionItemProvider), (void**)out);
    }
    return S_OK;
}

HRESULT WinAccessibilityNode::GetPropertyValue(PROPERTYID property,
                                               VARIANT* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    VariantInit(out);
    const AccessibilityNode* node = Node();
    if (!node) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    const AccessibilityInfo& info = node->info;
    if (property == UIA_ControlTypePropertyId) {
        VariantInt(out, AccessibilityControlType(info.role));
    } else if (property == UIA_NamePropertyId) {
        VariantString(out, info.label);
    } else if (property == UIA_AutomationIdPropertyId) {
        VariantString(out, info.authorId);
    } else if (property == UIA_HelpTextPropertyId) {
        VariantString(out, info.placeholder);
    } else if (property == UIA_AriaRolePropertyId) {
        VariantInit(out);
        out->vt = VT_BSTR;
        out->bstrVal = SysAllocString(AccessibilityRoleName(info.role));
    } else if (property == UIA_IsEnabledPropertyId) {
        VariantBool(out, !info.disabled);
    } else if (property == UIA_IsControlElementPropertyId ||
               property == UIA_IsContentElementPropertyId) {
        VariantBool(out, true);
    } else if (property == UIA_IsKeyboardFocusablePropertyId) {
        VariantBool(out, (node->actions & AccessibilityActionFocus) != 0);
    } else if (property == UIA_HasKeyboardFocusPropertyId) {
        VariantBool(out, node->focusId && root->win &&
                             node->focusId == root->win->focusId);
    } else if (property == UIA_IsPasswordPropertyId) {
        VariantBool(out, info.role == AccessibilityRole::PasswordInput);
    } else if (property == UIA_OrientationPropertyId &&
               info.orientation != AccessibilityOrientation::Unset) {
        VariantInt(out, info.orientation == AccessibilityOrientation::Vertical
                            ? OrientationType_Vertical
                            : OrientationType_Horizontal);
    } else if (property == UIA_PositionInSetPropertyId &&
               info.hasPositionInSet) {
        VariantInt(out, info.positionInSet);
    } else if (property == UIA_SizeOfSetPropertyId && info.hasSizeOfSet) {
        VariantInt(out, info.sizeOfSet);
    } else if (property == UIA_LevelPropertyId && info.hasLevel) {
        VariantInt(out, info.level);
    } else if (property == UIA_SelectionItemIsSelectedPropertyId &&
               info.hasSelected) {
        VariantBool(out, info.selected);
    } else if (property == UIA_GridRowCountPropertyId && info.hasRowCount) {
        VariantInt(out, info.rowCount);
    } else if (property == UIA_GridColumnCountPropertyId &&
               info.hasColumnCount) {
        VariantInt(out, info.columnCount);
    } else if (property == UIA_GridItemRowPropertyId && info.hasRowIndex) {
        VariantInt(out, std::max(0, info.rowIndex - 1));
    } else if (property == UIA_GridItemColumnPropertyId &&
               info.hasColumnIndex) {
        VariantInt(out, std::max(0, info.columnIndex - 1));
    } else if (property == UIA_ToggleToggleStatePropertyId &&
               info.toggled != AccessibilityToggled::Unset) {
        ToggleState state = ToggleState_Off;
        get_ToggleState(&state);
        VariantInt(out, state);
    } else if (property == UIA_ValueValuePropertyId &&
               AccessibilityTextRole(info.role)) {
        VariantString(out, info.value);
    } else if (property == UIA_ValueIsReadOnlyPropertyId &&
               AccessibilityTextRole(info.role)) {
        VariantBool(out, !(node->actions & AccessibilityActionSetValue));
    } else if (property == UIA_RangeValueValuePropertyId && node->slider) {
        VariantDouble(out, node->slider->value.End());
    } else if (property == UIA_RangeValueMinimumPropertyId && node->slider) {
        VariantDouble(out, node->slider->min);
    } else if (property == UIA_RangeValueMaximumPropertyId && node->slider) {
        VariantDouble(out, node->slider->max);
    } else if (property == UIA_ExpandCollapseExpandCollapseStatePropertyId &&
               info.hasExpanded) {
        VariantInt(out, info.expanded ? ExpandCollapseState_Expanded
                                      : ExpandCollapseState_Collapsed);
    } else if (property == UIA_IsOffscreenPropertyId && root->hwnd) {
        RECT client = {};
        GetClientRect(root->hwnd, &client);
        bool off = node->bounds.Right() <= 0 || node->bounds.Bottom() <= 0 ||
                   node->bounds.x >= client.right ||
                   node->bounds.y >= client.bottom;
        VariantBool(out, off);
    }
    return S_OK;
}

HRESULT WinAccessibilityNode::get_HostRawElementProvider(
    IRawElementProviderSimple** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    return S_OK;
}

HRESULT WinAccessibilityNode::Navigate(NavigateDirection direction,
                                       IRawElementProviderFragment** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    int index = root->NodeIndex(id);
    if (!root->win || index < 0) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    const AccessibilityNode& node = root->win->accessibility[index];
    if (direction == NavigateDirection_Parent) {
        if (node.parent < 0) {
            *out = static_cast<IRawElementProviderFragment*>(root);
            root->AddRef();
        } else {
            *out = root->NewNode(node.parent);
        }
        return S_OK;
    }
    if (direction == NavigateDirection_FirstChild ||
        direction == NavigateDirection_LastChild) {
        int child = -1;
        for (int i = 0; i < root->win->accessibility.len; i++) {
            if (root->win->accessibility[i].parent != index) {
                continue;
            }
            child = i;
            if (direction == NavigateDirection_FirstChild) {
                break;
            }
        }
        *out = root->NewNode(child);
        return S_OK;
    }
    int sibling = -1;
    if (direction == NavigateDirection_NextSibling) {
        for (int i = index + 1; i < root->win->accessibility.len; i++) {
            if (root->win->accessibility[i].parent == node.parent) {
                sibling = i;
                break;
            }
        }
    } else if (direction == NavigateDirection_PreviousSibling) {
        for (int i = index - 1; i >= 0; i--) {
            if (root->win->accessibility[i].parent == node.parent) {
                sibling = i;
                break;
            }
        }
    }
    *out = root->NewNode(sibling);
    return S_OK;
}

HRESULT WinAccessibilityNode::GetRuntimeId(SAFEARRAY** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = SafeArrayCreateVector(VT_I4, 0, 2);
    if (!*out) {
        return E_OUTOFMEMORY;
    }
    LONG* values = nullptr;
    HRESULT hr = SafeArrayAccessData(*out, (void**)&values);
    if (FAILED(hr)) {
        SafeArrayDestroy(*out);
        *out = nullptr;
        return hr;
    }
    values[0] = UiaAppendRuntimeId;
    values[1] = (LONG)id;
    SafeArrayUnaccessData(*out);
    return S_OK;
}

HRESULT WinAccessibilityNode::get_BoundingRectangle(UiaRect* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = {};
    const AccessibilityNode* node = Node();
    if (!node || !root->hwnd) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    POINT origin = {};
    ClientToScreen(root->hwnd, &origin);
    out->left = origin.x + node->bounds.x;
    out->top = origin.y + node->bounds.y;
    out->width = node->bounds.w;
    out->height = node->bounds.h;
    return S_OK;
}

HRESULT WinAccessibilityNode::GetEmbeddedFragmentRoots(SAFEARRAY** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    return S_OK;
}

HRESULT WinAccessibilityNode::SetFocus() {
    if (!root->win || !root->hwnd) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    ::SetFocus(root->hwnd);
    return WindowAccessibilityPerform(root->win, id, AccessibilityAction::Focus)
               ? S_OK
               : UIA_E_INVALIDOPERATION;
}

HRESULT WinAccessibilityNode::get_FragmentRoot(
    IRawElementProviderFragmentRoot** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    if (!root->win) {
        *out = nullptr;
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *out = static_cast<IRawElementProviderFragmentRoot*>(root);
    root->AddRef();
    return S_OK;
}

HRESULT WinAccessibilityNode::Invoke() {
    return root->win && WindowAccessibilityPerform(root->win, id,
                                                   AccessibilityAction::Default)
               ? S_OK
               : UIA_E_INVALIDOPERATION;
}

HRESULT WinAccessibilityNode::Toggle() {
    const AccessibilityNode* node = Node();
    return node && node->info.toggled != AccessibilityToggled::Unset &&
                   WindowAccessibilityPerform(root->win, id,
                                              AccessibilityAction::Default)
               ? S_OK
               : UIA_E_INVALIDOPERATION;
}

HRESULT WinAccessibilityNode::get_ToggleState(ToggleState* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    const AccessibilityNode* node = Node();
    if (!node) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *out = node->info.toggled == AccessibilityToggled::Mixed
               ? ToggleState_Indeterminate
           : node->info.toggled == AccessibilityToggled::True ? ToggleState_On
                                                              : ToggleState_Off;
    return S_OK;
}

HRESULT WinAccessibilityNode::SetValue(LPCWSTR value) {
    if (!root->win) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    int wn = value ? (int)wcslen(value) : 0;
    int n = wn ? WideCharToMultiByte(CP_UTF8, 0, value, wn, nullptr, 0, nullptr,
                                     nullptr)
               : 0;
    char* text = n ? (char*)Alloc(nullptr, n) : nullptr;
    if (n && !text) {
        return E_OUTOFMEMORY;
    }
    if (n) {
        WideCharToMultiByte(CP_UTF8, 0, value, wn, text, n, nullptr, nullptr);
    }
    bool changed = WindowAccessibilityPerform(
        root->win, id, AccessibilityAction::SetValue, Str(text, n));
    if (text) {
        Free(nullptr, text);
    }
    return changed ? S_OK : UIA_E_INVALIDOPERATION;
}

HRESULT WinAccessibilityNode::get_Value(BSTR* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    const AccessibilityNode* node = Node();
    if (!node) {
        *out = nullptr;
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *out = AccessibilityBstr(node->info.value);
    return *out ? S_OK : E_OUTOFMEMORY;
}

HRESULT WinAccessibilityNode::get_IsReadOnly(BOOL* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    const AccessibilityNode* node = Node();
    if (!node) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *out = node->slider
               ? (node->info.disabled ? TRUE : FALSE)
               : ((node->actions & AccessibilityActionSetValue) ? FALSE : TRUE);
    return S_OK;
}

HRESULT WinAccessibilityNode::SetValue(double value) {
    return root->win && WindowAccessibilitySetNumericValue(root->win, id,
                                                           (float)value)
               ? S_OK
               : UIA_E_INVALIDOPERATION;
}

HRESULT WinAccessibilityNode::get_Value(double* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    const AccessibilityNode* node = Node();
    if (!node || !node->slider) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *out = node->slider->value.End();
    return S_OK;
}

HRESULT WinAccessibilityNode::get_Maximum(double* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    const AccessibilityNode* node = Node();
    if (!node || !node->slider) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *out = node->slider->max;
    return S_OK;
}

HRESULT WinAccessibilityNode::get_Minimum(double* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    const AccessibilityNode* node = Node();
    if (!node || !node->slider) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *out = node->slider->min;
    return S_OK;
}

HRESULT WinAccessibilityNode::get_LargeChange(double* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    const AccessibilityNode* node = Node();
    if (!node || !node->slider) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    double step = node->slider->step > 0
                      ? node->slider->step
                      : (node->slider->max - node->slider->min) / 100.0;
    *out = step * 10;
    return S_OK;
}

HRESULT WinAccessibilityNode::get_SmallChange(double* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    const AccessibilityNode* node = Node();
    if (!node || !node->slider) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *out = node->slider->step > 0
               ? node->slider->step
               : (node->slider->max - node->slider->min) / 100.0;
    return S_OK;
}

HRESULT WinAccessibilityNode::Expand() {
    const AccessibilityNode* node = Node();
    if (!node) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    if (node->info.expanded) {
        return S_OK;
    }
    return WindowAccessibilityPerform(root->win, id,
                                      AccessibilityAction::Default)
               ? S_OK
               : UIA_E_INVALIDOPERATION;
}

HRESULT WinAccessibilityNode::Collapse() {
    const AccessibilityNode* node = Node();
    if (!node) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    if (!node->info.expanded) {
        return S_OK;
    }
    return WindowAccessibilityPerform(root->win, id,
                                      AccessibilityAction::Default)
               ? S_OK
               : UIA_E_INVALIDOPERATION;
}

HRESULT WinAccessibilityNode::get_ExpandCollapseState(
    ExpandCollapseState* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    const AccessibilityNode* node = Node();
    if (!node) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *out = node->info.expanded ? ExpandCollapseState_Expanded
                               : ExpandCollapseState_Collapsed;
    return S_OK;
}

HRESULT WinAccessibilityNode::Select() {
    const AccessibilityNode* node = Node();
    if (!node) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    if (node->info.selected) {
        return S_OK;
    }
    return WindowAccessibilityPerform(root->win, id,
                                      AccessibilityAction::Default)
               ? S_OK
               : UIA_E_INVALIDOPERATION;
}

HRESULT WinAccessibilityNode::AddToSelection() {
    return Select();
}

HRESULT WinAccessibilityNode::RemoveFromSelection() {
    // The semantic record does not say whether its container allows an empty
    // or multiple selection. Rust keeps that policy in the widget, so asking
    // the item to remove itself cannot safely be translated into a toggle.
    const AccessibilityNode* node = Node();
    if (!node) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    return node->info.selected ? UIA_E_INVALIDOPERATION : S_OK;
}

HRESULT WinAccessibilityNode::get_IsSelected(BOOL* out) {
    if (!out) {
        return E_INVALIDARG;
    }
    const AccessibilityNode* node = Node();
    if (!node) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *out = node->info.selected ? TRUE : FALSE;
    return S_OK;
}

HRESULT WinAccessibilityNode::get_SelectionContainer(
    IRawElementProviderSimple** out) {
    if (!out) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    int index = root->NodeIndex(id);
    if (!root->win || index < 0) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    int parent = root->win->accessibility[index].parent;
    if (parent < 0) {
        return root
            ->QueryInterface(__uuidof(IRawElementProviderSimple), (void**)out);
    }
    IRawElementProviderFragment* fragment = root->NewNode(parent);
    if (!fragment) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    HRESULT hr = fragment->QueryInterface(__uuidof(IRawElementProviderSimple),
                                          (void**)out);
    fragment->Release();
    return hr;
}

WinAccessibility* AccessibilityWinNew(Window* win, void* hwnd) {
    if (!win || !hwnd) {
        return nullptr;
    }
    return new WinAccessibility(win, (HWND)hwnd);
}

void AccessibilityWinClose(WinAccessibility* accessibility) {
    if (!accessibility) {
        return;
    }
    accessibility->win = nullptr;
    accessibility->hwnd = nullptr;
    accessibility->Release();
}

intptr_t AccessibilityWinGetObject(WinAccessibility* accessibility,
                                   uintptr_t wParam, intptr_t lParam) {
    if (!accessibility || !accessibility->hwnd) {
        return 0;
    }
    return (intptr_t)UiaReturnRawElementProvider(
        accessibility->hwnd, (WPARAM)wParam, (LPARAM)lParam,
        static_cast<IRawElementProviderSimple*>(accessibility));
}

void AccessibilityWinTreeChanged(WinAccessibility* accessibility) {
    if (!accessibility || !accessibility->win || !UiaClientsAreListening()) {
        return;
    }
    UiaRaiseStructureChangedEvent(
        static_cast<IRawElementProviderSimple*>(accessibility),
        StructureChangeType_ChildrenInvalidated, nullptr, 0);
}

void AccessibilityWinFocusChanged(WinAccessibility* accessibility,
                                  int focusId) {
    if (!accessibility || !accessibility->win || !focusId ||
        !UiaClientsAreListening()) {
        return;
    }
    for (int i = 0; i < accessibility->win->accessibility.len; i++) {
        if (accessibility->win->accessibility[i].focusId != focusId) {
            continue;
        }
        IRawElementProviderFragment* fragment = accessibility->NewNode(i);
        IRawElementProviderSimple* provider = nullptr;
        if (fragment) {
            fragment->QueryInterface(__uuidof(IRawElementProviderSimple),
                                     (void**)&provider);
            fragment->Release();
        }
        if (provider) {
            UiaRaiseAutomationEvent(provider,
                                    UIA_AutomationFocusChangedEventId);
            provider->Release();
        }
        break;
    }
}

// Linked only by the repository test runner (it is deliberately absent from
// accessibility_win.h). This exercises the COM projection itself without
// opening a second native window or making the public API carry test types.
bool AccessibilityWinSmokeTest(Window* win, uint32_t nodeId) {
    const AccessibilityNode* expected = WindowAccessibilityNode(win, nodeId);
    if (!expected) {
        return false;
    }
    WinAccessibility* root =
        AccessibilityWinNew(win, (void*)GetDesktopWindow());
    int index = root ? root->NodeIndex(nodeId) : -1;
    IRawElementProviderFragment* fragment =
        root ? root->NewNode(index) : nullptr;
    IRawElementProviderSimple* simple = nullptr;
    bool ok = fragment &&
              SUCCEEDED(fragment->QueryInterface(
                  __uuidof(IRawElementProviderSimple), (void**)&simple)) &&
              simple;
    VARIANT property = {};
    if (ok) {
        ok = SUCCEEDED(simple->GetPropertyValue(UIA_ControlTypePropertyId,
                                                &property)) &&
             property.vt == VT_I4 && property.lVal != 0;
        VariantClear(&property);
    }
    if (ok) {
        ok = SUCCEEDED(simple
                           ->GetPropertyValue(UIA_NamePropertyId, &property)) &&
             property.vt == VT_BSTR;
        BSTR wanted = AccessibilityBstr(expected->info.label);
        ok = ok && wanted && property.bstrVal &&
             wcscmp(wanted, property.bstrVal) == 0;
        SysFreeString(wanted);
        VariantClear(&property);
    }
    struct PatternExpectation {
        PATTERNID id;
        bool wanted;
    } patterns[] = {
        {UIA_InvokePatternId, AccessibilityInvokePattern(*expected)},
        {UIA_TogglePatternId, expected->info
                                      .toggled != AccessibilityToggled::Unset},
        {UIA_ValuePatternId, AccessibilityTextRole(expected->info.role)},
        {UIA_RangeValuePatternId, expected->slider && expected->info
                                                          .hasNumericValue},
        {UIA_ExpandCollapsePatternId, expected->info.hasExpanded},
        {UIA_SelectionItemPatternId,
         expected->info.hasSelected &&
             AccessibilitySelectionItemRole(expected->info.role)},
    };
    for (const PatternExpectation& pattern : patterns) {
        IUnknown* provider = nullptr;
        bool got =
            simple &&
            SUCCEEDED(simple->GetPatternProvider(pattern.id, &provider)) &&
            provider;
        ok = ok && got == pattern.wanted;
        if (provider) {
            provider->Release();
        }
    }
    SAFEARRAY* runtime = nullptr;
    if (fragment) {
        ok = ok && SUCCEEDED(fragment->GetRuntimeId(&runtime)) && runtime &&
             SafeArrayGetDim(runtime) == 1;
        if (runtime) {
            SafeArrayDestroy(runtime);
        }
        IRawElementProviderFragmentRoot* gotRoot = nullptr;
        ok = ok && SUCCEEDED(fragment->get_FragmentRoot(&gotRoot)) && gotRoot;
        if (gotRoot) {
            gotRoot->Release();
        }
    }
    AccessibilityWinClose(root);
    if (simple) {
        VariantInit(&property);
        ok = ok && simple->GetPropertyValue(UIA_NamePropertyId, &property) ==
                       (HRESULT)UIA_E_ELEMENTNOTAVAILABLE;
        VariantClear(&property);
        simple->Release();
    }
    if (fragment) {
        fragment->Release();
    }
    return ok;
}

} // namespace gpui
