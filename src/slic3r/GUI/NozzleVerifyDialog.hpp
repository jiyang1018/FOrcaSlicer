#pragma once

#include <wx/wx.h>
#include <vector>
#include <functional>

namespace Slic3r {
namespace GUI {

// Displayed alongside the existing SliceInfoDialog after slicing completes,
// only when the printer profile contains mixed nozzle sizes.
//
// Shows each extruder head's configured nozzle size and filament color/material.
// The user must click Verify to confirm physical nozzles match the profile
// before the print button becomes active.
//
// Re-slicing resets verification state — the dialog is shown again and the
// print button is re-gated.

struct ExtruderInfo {
    int         extruder_id;      // 0-based index
    float       nozzle_diameter;  // mm
    std::string filament_name;
    std::string material_type;    // PLA, PETG, TPU, etc.
    wxColour    filament_color;
};

class NozzleVerifyDialog : public wxDialog {
public:
    // on_verified: called when user clicks Verify.
    // Caller (Plater) connects this to enabling the print button.
    NozzleVerifyDialog(wxWindow*                        parent,
                       const std::vector<ExtruderInfo>& extruders,
                       std::function<void()>            on_verified);

    // Returns true if the user has clicked Verify in this session.
    // Resets to false on each new slice.
    bool is_verified() const { return m_verified; }
	void reset_verification()  { m_verified = false; }
    void update_extruders(const std::vector<ExtruderInfo>& extruders);

private:
    void build_ui(const std::vector<ExtruderInfo>& extruders);
	void on_verify_clicked(wxCommandEvent& event);
    void on_close(wxCloseEvent& event);

    bool                  m_verified { false };
    std::function<void()> m_on_verified;
	wxButton*             m_verify_btn { nullptr };

    wxDECLARE_EVENT_TABLE();
};

} // namespace GUI
} // namespace Slic3r
