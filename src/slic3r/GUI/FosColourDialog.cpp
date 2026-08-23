#include "FosColourDialog.hpp"

#ifdef __WXMSW__

#include "I18N.hpp"

#include <wx/msw/wrapwin.h>
#include <commctrl.h>
#include <colordlg.h>   // COLOR_* control ids of the system ChooseColor dialog

#include <cstdio>
#include <string>

#ifdef _MSC_VER
#pragma comment(lib, "comctl32.lib")
#endif

namespace Slic3r { namespace GUI {

namespace {

// Ids for the two controls this class adds. Anything above the highest id in
// <colordlg.h> (COLOR_* stop below 0x300) and below the IDOK/IDCANCEL range is free.
const int      FOS_HEX_LABEL_ID  = 0x7F10;
const int      FOS_HEX_EDIT_ID   = 0x7F11;
const UINT_PTR FOS_SUBCLASS_ID   = 0x466F53;   // 'FoS'

// Rectangle of a dialog control in the dialog's client coordinates. Empty when
// the control does not exist.
RECT fos_item_rect(HWND hDlg, int id)
{
    RECT r = {0, 0, 0, 0};
    HWND h = ::GetDlgItem(hDlg, id);
    if (h != nullptr) {
        ::GetWindowRect(h, &r);
        ::MapWindowPoints(HWND_DESKTOP, hDlg, reinterpret_cast<POINT *>(&r), 2);
    }
    return r;
}

// "#RRGGBB" or "RRGGBB", any case, surrounding blanks ignored. Anything else fails.
bool fos_parse_hex(const wchar_t *text, unsigned long &rgb)
{
    std::wstring s(text);
    size_t b = s.find_first_not_of(L" \t");
    size_t e = s.find_last_not_of(L" \t");
    if (b == std::wstring::npos) return false;
    s = s.substr(b, e - b + 1);
    if (!s.empty() && s[0] == L'#') s.erase(0, 1);
    if (s.size() != 6) return false;
    unsigned long v = 0;
    for (wchar_t c : s) {
        unsigned d;
        if (c >= L'0' && c <= L'9') d = c - L'0';
        else if (c >= L'a' && c <= L'f') d = 10 + (c - L'a');
        else if (c >= L'A' && c <= L'F') d = 10 + (c - L'A');
        else return false;
        v = (v << 4) | d;
    }
    rgb = v;
    return true;
}

// Window subclass on the dialog itself: sees the EN_CHANGE notifications of every
// edit in the dialog before the dialog procedure does.
LRESULT CALLBACK fos_subclass_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                   UINT_PTR /*id*/, DWORD_PTR ref)
{
    auto *self = reinterpret_cast<FosColourDialog *>(ref);
    if (msg == WM_COMMAND && HIWORD(wParam) == EN_CHANGE) {
        const int id = LOWORD(wParam);
        if (id == FOS_HEX_EDIT_ID) {
            self->fos_on_hex_changed();
            return 0;
        }
        if (id == COLOR_RED || id == COLOR_GREEN || id == COLOR_BLUE)
            self->fos_on_rgb_changed();   // then let the dialog see it too
    } else if (msg == WM_NCDESTROY) {
        ::RemoveWindowSubclass(hWnd, fos_subclass_proc, FOS_SUBCLASS_ID);
    }
    return ::DefSubclassProc(hWnd, msg, wParam, lParam);
}

} // namespace

void FosColourDialog::MSWOnInitDone(WXHWND hDlg)
{
    wxColourDialog::MSWOnInitDone(hDlg);

    HWND dlg = static_cast<HWND>(hDlg);
    m_hDlg   = hDlg;

    // Anchors: the Lum edit + label give the edit height and the label->edit gap.
    // The field itself takes over the rectangle of "Define Custom Colours >>"
    // (COLOR_MIX): with CC_FULLOPEN that button is permanently disabled, and it is
    // the only free space in the template (it shares its row with Lum / Blue, so
    // the field cannot go beside them). If the button is enabled the dialog was
    // opened collapsed; then it is left alone and no field is added.
    const RECT r_lum   = fos_item_rect(dlg, COLOR_LUM);
    const RECT r_lum_l = fos_item_rect(dlg, COLOR_LUMACCEL);
    const RECT r_mix   = fos_item_rect(dlg, COLOR_MIX);
    HWND       mix     = ::GetDlgItem(dlg, COLOR_MIX);
    if (mix == nullptr || ::IsRectEmpty(&r_lum) || ::IsRectEmpty(&r_lum_l) || ::IsRectEmpty(&r_mix))
        return;   // unknown template: leave the dialog stock
    if (::IsWindowEnabled(mix))
        return;   // collapsed dialog: the button is needed to expand it

    ::ShowWindow(mix, SW_HIDE);

    const int edit_h  = r_lum.bottom - r_lum.top;
    const int gap     = r_lum.left - r_lum_l.right;
    const int row_top = r_mix.top + ((r_mix.bottom - r_mix.top) - edit_h) / 2;
    const int label_w = r_lum_l.right - r_lum_l.left;
    const int label_h = r_lum_l.bottom - r_lum_l.top;
    const int label_l = r_mix.left;
    const int label_t = row_top + (edit_h - label_h) / 2;
    const int edit_l  = label_l + label_w + gap;
    int       edit_w  = edit_h * 5;                       // "#RRGGBB" plus caret
    if (edit_w > r_mix.right - edit_l) edit_w = r_mix.right - edit_l;

    HINSTANCE inst = reinterpret_cast<HINSTANCE>(::GetWindowLongPtr(dlg, GWLP_HINSTANCE));
    HFONT     font = reinterpret_cast<HFONT>(::SendMessage(dlg, WM_GETFONT, 0, 0));

    const wxString label_text = _L("HEX:");
    HWND label = ::CreateWindowExW(0, L"STATIC", label_text.wc_str(),
                                   WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                   label_l, label_t, label_w, label_h,
                                   dlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(FOS_HEX_LABEL_ID)), inst, nullptr);
    HWND edit  = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL | ES_UPPERCASE,
                                   edit_l, row_top, edit_w, edit_h,
                                   dlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(FOS_HEX_EDIT_ID)), inst, nullptr);
    if (label == nullptr || edit == nullptr)
        return;

    if (font != nullptr) {
        ::SendMessage(label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        ::SendMessage(edit,  WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    ::SendMessage(edit, EM_LIMITTEXT, 7, 0);   // "#RRGGBB"

    // Tab order: straight after the Blue edit, before "Add to Custom Colours".
    if (HWND blue = ::GetDlgItem(dlg, COLOR_BLUE))
        ::SetWindowPos(edit, blue, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    m_hex_edit = edit;
    ::SetWindowSubclass(dlg, fos_subclass_proc, FOS_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));

    const wxColour init = GetColourData().GetColour();
    if (init.IsOk())
        fos_set_hex_text((static_cast<unsigned long>(init.Red()) << 16) |
                         (static_cast<unsigned long>(init.Green()) << 8) |
                          static_cast<unsigned long>(init.Blue()));
}

void FosColourDialog::fos_set_hex_text(unsigned long rgb)
{
    if (m_hex_edit == nullptr) return;
    wchar_t want[16];
    _snwprintf_s(want, _countof(want), _TRUNCATE, L"#%02lX%02lX%02lX",
                 (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);

    // Leave the field alone while it already says this colour (typed with or
    // without the '#', in any case), so typing is never disturbed by the echo.
    wchar_t have[16] = L"";
    ::GetWindowTextW(static_cast<HWND>(m_hex_edit), have, _countof(have));
    unsigned long have_rgb = 0;
    if (fos_parse_hex(have, have_rgb) && have_rgb == rgb) return;

    m_updating = true;
    ::SetWindowTextW(static_cast<HWND>(m_hex_edit), want);
    m_updating = false;
}

void FosColourDialog::fos_on_rgb_changed()
{
    if (m_updating || m_hDlg == nullptr) return;
    HWND dlg = static_cast<HWND>(m_hDlg);
    BOOL ok_r = FALSE, ok_g = FALSE, ok_b = FALSE;
    UINT r = ::GetDlgItemInt(dlg, COLOR_RED,   &ok_r, FALSE);
    UINT g = ::GetDlgItemInt(dlg, COLOR_GREEN, &ok_g, FALSE);
    UINT b = ::GetDlgItemInt(dlg, COLOR_BLUE,  &ok_b, FALSE);
    if (!ok_r || !ok_g || !ok_b) return;   // an edit is mid-typing / empty
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    fos_set_hex_text((static_cast<unsigned long>(r) << 16) | (static_cast<unsigned long>(g) << 8) | b);
}

void FosColourDialog::fos_on_hex_changed()
{
    if (m_updating || m_hDlg == nullptr || m_hex_edit == nullptr) return;
    wchar_t text[16] = L"";
    ::GetWindowTextW(static_cast<HWND>(m_hex_edit), text, _countof(text));
    unsigned long rgb = 0;
    if (!fos_parse_hex(text, rgb)) return;   // incomplete or invalid: wait for more

    // The documented way for a hook to change the current selection: the dialog
    // repaints the picker and rewrites all six HSL/RGB edits synchronously.
    static const UINT s_set_rgb = ::RegisterWindowMessageW(L"commdlg_SetRGBColor");   // SETRGBSTRING
    const COLORREF cr = RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    m_updating = true;
    ::SendMessage(static_cast<HWND>(m_hDlg), s_set_rgb, 0, static_cast<LPARAM>(cr));
    m_updating = false;
}

}} // namespace Slic3r::GUI

#endif // __WXMSW__
