#include "NozzleVerifyDialog.hpp"
#include "I18N.hpp"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/panel.h>
#include <wx/statline.h>

namespace Slic3r {
namespace GUI {

wxBEGIN_EVENT_TABLE(NozzleVerifyDialog, wxDialog)
    EVT_BUTTON(wxID_OK, NozzleVerifyDialog::on_verify_clicked)
    EVT_CLOSE(NozzleVerifyDialog::on_close)
wxEND_EVENT_TABLE()

NozzleVerifyDialog::NozzleVerifyDialog(
    wxWindow*                        parent,
    const std::vector<ExtruderInfo>& extruders,
    std::function<void()>            on_verified)
    : wxDialog(parent, wxID_ANY, _L("Verify nozzle sizes"),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_on_verified(std::move(on_verified))
{
    build_ui(extruders);
    Fit();
    Layout();
}

void NozzleVerifyDialog::build_ui(const std::vector<ExtruderInfo>& extruders)
{
    wxBoxSizer* top_sizer = new wxBoxSizer(wxVERTICAL);

    // Header
    wxStaticText* header = new wxStaticText(
        this, wxID_ANY,
        _L("Please confirm that the physical nozzles match the profile below."));
    header->Wrap(320);
    top_sizer->Add(header, 0, wxALL, 10);
    top_sizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // Extruder grid: one row per extruder
    // Columns: Head # | Nozzle size | Material | Color swatch
    wxFlexGridSizer* grid = new wxFlexGridSizer(
        static_cast<int>(extruders.size()) + 1, // rows (header + extruders)
        4,    // columns
        6, 12 // vgap, hgap
    );
    grid->AddGrowableCol(2, 1); // material column stretches

    // Column headers
    auto add_header_cell = [&](const wxString& text) {
        wxStaticText* t = new wxStaticText(this, wxID_ANY, text);
        wxFont f = t->GetFont();
        f.SetWeight(wxFONTWEIGHT_BOLD);
        t->SetFont(f);
        grid->Add(t, 0, wxALIGN_CENTER_VERTICAL);
    };
    add_header_cell(_L("Head"));
    add_header_cell(_L("Nozzle"));
    add_header_cell(_L("Material"));
    add_header_cell(_L("Color"));

    // One row per extruder
    for (const auto& ext : extruders) {
        // Head number (1-based for display)
        grid->Add(
            new wxStaticText(this, wxID_ANY,
                             wxString::Format("T%d", ext.extruder_id + 1)),
            0, wxALIGN_CENTER_VERTICAL);

        // Nozzle diameter
        grid->Add(
            new wxStaticText(this, wxID_ANY,
                             wxString::Format("%.2f mm", ext.nozzle_diameter)),
            0, wxALIGN_CENTER_VERTICAL);

        // Material name
        wxString mat_label = wxString::FromUTF8(ext.filament_name);
        if (!ext.material_type.empty())
            mat_label += " (" + wxString::FromUTF8(ext.material_type) + ")";
        grid->Add(
            new wxStaticText(this, wxID_ANY, mat_label),
            0, wxALIGN_CENTER_VERTICAL | wxEXPAND);

        // Color swatch — small panel painted with filament color
        wxPanel* swatch = new wxPanel(this, wxID_ANY,
                                      wxDefaultPosition, wxSize(24, 24));
        swatch->SetBackgroundColour(ext.filament_color);
        swatch->SetMinSize(wxSize(24, 24));
        grid->Add(swatch, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_CENTER_HORIZONTAL);
    }

    top_sizer->Add(grid, 0, wxEXPAND | wxALL, 10);
    top_sizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // Verify button — centered, prominent
    m_verify_btn = new wxButton(this, wxID_OK, _L("Verify"));
    m_verify_btn->SetMinSize(wxSize(120, -1));

    wxBoxSizer* btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->AddStretchSpacer();
    btn_sizer->Add(m_verify_btn, 0, wxALL, 6);
    btn_sizer->AddStretchSpacer();
    top_sizer->Add(btn_sizer, 0, wxEXPAND | wxBOTTOM, 8);

    SetSizer(top_sizer);
}

void NozzleVerifyDialog::on_verify_clicked(wxCommandEvent& /*event*/)
{
    m_verified = true;
    if (m_on_verified)
        m_on_verified();
// Keep the dialog open so the user can reference nozzle sizes during setup.
    // They can close it manually. Do not call EndModal here.
}

void NozzleVerifyDialog::on_close(wxCloseEvent& /*event*/)
{
    Hide(); // hide instead of destroy so it can be reshown
}

void NozzleVerifyDialog::update_extruders(const std::vector<ExtruderInfo>& extruders)
{
    if (GetSizer()) {
        GetSizer()->Clear(true);
        SetSizer(nullptr, true);
    }
    build_ui(extruders);
    Fit();
    Layout();
    Refresh();
}

} // namespace GUI
} // namespace Slic3r
