#ifndef slic3r_GUI_CustomNotebook_hpp_
#define slic3r_GUI_CustomNotebook_hpp_

// FOS R1: owner-drawn notebook, extracted verbatim from Plater.cpp (where it
// was pasted inline with its own "CustomNotebook.h" comment and never moved).
//
// Use THIS, never a raw wxNotebook, for anything on the settings pages - the
// rule at Tab.cpp ("Because of DarkMode we use our own Notebook") plus a
// second, worse reason found in fos8-s29: a raw wxNotebook's paint handler
// goes through comctl32, which calls RedrawWindow on the page's children FROM
// INSIDE THE PAINT, and in this styling context that self-sustains into a
// permanent ~300-420 repaints/sec storm burning a full core from the moment
// the Print Settings pages are built until the app exits. No FOS code calls
// any invalidation API while it runs (function breakpoints: 0 hits) - the
// loop lives entirely inside the native control. This class paints its own
// tabs (wxBG_STYLE_PAINT, no comctl32), so neither the storm nor the
// dark-mode problem exists.

#include <wx/wx.h>
#include <vector>

#include "../GUI_App.hpp"

namespace Slic3r { namespace GUI {

class CustomNotebook : public wxControl
{
public:
    CustomNotebook(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize)
        : wxControl(parent, id, pos, size, wxBORDER_NONE), m_selectedIndex(-1), m_tabHeight(24), m_tabPadding(10), m_roundRadius(5)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        UpdateColors();

        Bind(wxEVT_PAINT, &CustomNotebook::OnPaint, this);
        Bind(wxEVT_ERASE_BACKGROUND, &CustomNotebook::OnEraseBackground, this);
        Bind(wxEVT_LEFT_DOWN, &CustomNotebook::OnLeftDown, this);
        Bind(wxEVT_SIZE, &CustomNotebook::OnSize, this);
    }

    void AddPage(wxWindow* page, const wxString& text)
    {
        m_tabs.push_back({text, page});
        if (page) {
            page->Reparent(this);
            page->Hide();
            page->SetBackgroundColour(m_selectedTabColor);
        }

        if (m_selectedIndex == -1) {
            SetSelection(0);
        }

        UpdateLayout();
        InvalidateBestSize();
        Refresh();
    }

    void DeleteAllPages()
    {
        for (auto& tab : m_tabs) {
            if (tab.page) {
                tab.page->Destroy();
            }
        }
        m_tabs.clear();
        m_selectedIndex = -1;
        UpdateLayout();
        Refresh();
    }

    size_t GetPageCount() const { return m_tabs.size(); }

    wxWindow* GetPage(size_t index) const { return (index < m_tabs.size()) ? m_tabs[index].page : nullptr; }

    int GetSelection() const { return m_selectedIndex; }

    void SetSelection(size_t index)
    {
        if (index >= m_tabs.size() || static_cast<int>(index) == m_selectedIndex)
            return;

        if (m_selectedIndex != -1 && m_tabs[m_selectedIndex].page) {
            m_tabs[m_selectedIndex].page->Hide();
        }

        m_selectedIndex = index;

        if (m_selectedIndex != -1 && m_tabs[m_selectedIndex].page) {
            m_tabs[m_selectedIndex].page->Show();
        }

        UpdateLayout();
        Refresh();
    }

    // FOS R1: wxNotebook-API shim so the Tab.cpp call sites swap without churn.
    // Only the horizontal padding is meaningful for this tab strip.
    void SetPadding(const wxSize &padding)
    {
        if (padding.x >= 0)
            m_tabPadding = padding.x;
        Refresh();
    }

protected:
    void OnPaint(wxPaintEvent& event)
    {
        UpdateColors();

        wxPaintDC dc(this);

        // 1. background
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_bgColor));
        dc.DrawRectangle(GetClientRect());

        // 2. tab strip background
        dc.SetPen(wxPen(m_dividerColor, 1));
        dc.SetBrush(wxBrush(m_dividerColor));
        wxRect labelRect(0, 0, GetSize().x, m_tabHeight);
        dc.DrawRoundedRectangle(labelRect, m_roundRadius);
        dc.DrawRectangle(0, m_tabHeight - 2, GetSize().x, 4);

        // 3. tabs
        wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
        font.SetPointSize(m_textSize);
        dc.SetFont(font);

        auto height = dc.GetCharHeight();
        if (height > m_tabHeight - 2) {
            m_tabHeight = height + 2;
            Layout();
        }

        int xPos = 0;
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            bool isSelected = static_cast<int>(i) == m_selectedIndex;

            int textWidth, textHeight;
            dc.GetTextExtent(m_tabs[i].text, &textWidth, &textHeight);
            int tabWidth = textWidth + 2 * m_tabPadding;

            if (isSelected) {
                dc.SetPen(wxPen(m_dividerColor, 1));
                dc.SetBrush(wxBrush(m_bgColor));
                wxRect selectedRect(xPos, 0, tabWidth, m_tabHeight + 2);
                dc.DrawRectangle(selectedRect);
                dc.DrawRoundedRectangle(selectedRect, m_roundRadius);

                dc.SetPen(wxPen(m_bgColor, 1));
                dc.SetBrush(wxBrush(m_bgColor));
                dc.DrawRectangle(xPos, m_tabHeight, tabWidth, 4);
            }

            dc.SetTextForeground(isSelected ? m_selectedTextColor : m_textColor);
            dc.DrawText(m_tabs[i].text, xPos + m_tabPadding, (m_tabHeight - textHeight) / 2);

            xPos += tabWidth;
        }

        // 4. outer border
        dc.SetPen(wxPen(m_borderColor, 1));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRoundedRectangle(GetClientRect(), m_roundRadius);
    }

    void OnLeftDown(wxMouseEvent& event)
    {
        wxPoint pos = event.GetPosition();
        if (pos.y > m_tabHeight) {
            event.Skip();
            return;
        }

        int tabIndex = HitTest(pos);
        if (tabIndex != -1 && tabIndex != m_selectedIndex) {
            SetSelection(tabIndex);
            Refresh();
        }
    }

    void OnSize(wxSizeEvent& event)
    {
        UpdateLayout();
        Refresh();
        event.Skip();
    }

    void OnEraseBackground(wxEraseEvent& event) {}

    // FOS R1: wxNotebook aggregates its pages' best sizes; a plain wxControl
    // reports a tiny default, so inside an OptionsGroup widget line the sizer
    // collapsed this control and the page content painted over the tab strip.
    // Mirror wxNotebook: strip height plus the largest page, plus the insets
    // UpdateLayout() applies (x: 2+2, y: +1 top, +3 bottom).
    wxSize DoGetBestSize() const override
    {
        wxSize best(0, 0);
        for (const auto &tab : m_tabs)
            if (tab.page)
                best.IncTo(tab.page->GetBestSize());
        best.x += 4;
        best.y += m_tabHeight + 5;
        return best;
    }

private:
    struct TabInfo
    {
        wxString  text;
        wxWindow* page;
    };

    wxRect GetTabRect(size_t index) const
    {
        if (index >= m_tabs.size())
            return wxRect();

        wxClientDC dc(const_cast<CustomNotebook*>(this));
        wxFont     font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
        font.SetPointSize(m_textSize);
        dc.SetFont(font);

        int textWidth, textHeight;
        dc.GetTextExtent(m_tabs[index].text, &textWidth, &textHeight);
        int tabWidth = textWidth + 2 * m_tabPadding;

        int x = 0;
        for (size_t i = 0; i < index; ++i) {
            dc.GetTextExtent(m_tabs[i].text, &textWidth, &textHeight);
            x += textWidth + 2 * m_tabPadding;
        }

        return wxRect(x, 0, tabWidth, m_tabHeight);
    }

    int HitTest(const wxPoint& pt) const
    {
        if (pt.y > m_tabHeight)
            return -1;

        wxClientDC dc(const_cast<CustomNotebook*>(this));
        wxFont     font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
        font.SetPointSize(m_textSize);
        dc.SetFont(font);

        int xPos = 0;
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            int textWidth, textHeight;
            dc.GetTextExtent(m_tabs[i].text, &textWidth, &textHeight);
            int tabWidth = textWidth + 2 * m_tabPadding;

            if (pt.x >= xPos && pt.x <= xPos + tabWidth) {
                return i;
            }

            xPos += tabWidth;
        }

        return -1;
    }

    void UpdateColors()
    {
        bool is_dark = wxGetApp().app_config->get("dark_color_mode") == "1";

        if (!is_dark) {
            m_bgColor           = wxColour(255, 255, 255);
            m_borderColor       = wxColour(240, 240, 240);
            m_selectedTabColor  = wxColour(255, 255, 255);
            m_textColor         = wxColour(194, 194, 193);
            m_dividerColor      = wxColour(240, 240, 240);
            m_selectedTextColor = wxColour(0, 0, 0);
        } else {
            m_bgColor           = wxColour(45, 45, 49);
            m_borderColor       = wxColour(76, 76, 85);
            m_selectedTabColor  = wxColour(45, 45, 49);
            m_textColor         = wxColour(104, 105, 107);
            m_dividerColor      = wxColour(51, 51, 55);
            m_selectedTextColor = wxColour(255, 255, 255);
        }
    }

    void UpdateLayout()
    {
        if (m_selectedIndex != -1 && m_tabs[m_selectedIndex].page) {
            wxSize size = GetSize();
            m_tabs[m_selectedIndex].page->SetSize(2, m_tabHeight + 1, size.x - 4, size.y - m_tabHeight - 4);
            m_tabs[m_selectedIndex].page->Layout();
        }
    }

private:
    std::vector<TabInfo> m_tabs;
    int                  m_selectedIndex;

    wxColour m_bgColor;
    wxColour m_borderColor;
    wxColour m_selectedTabColor;
    wxColour m_textColor;
    wxColour m_selectedTextColor;
    wxColour m_dividerColor;

    int m_tabHeight;
    int m_tabPadding;
    int m_roundRadius;
#ifdef _WIN32
    int m_textSize = 10;
#else
    int m_textSize = 13;
#endif
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_CustomNotebook_hpp_
