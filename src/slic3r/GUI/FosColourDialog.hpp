#ifndef slic3r_GUI_FosColourDialog_hpp_
#define slic3r_GUI_FosColourDialog_hpp_

// FOS 8.6: the native colour chooser with a HEX field inside it.
//
// On Windows wxColourDialog IS the OS ChooseColor dialog. Its HSL/RGB boxes and
// "Add to Custom Colours" are Microsoft's controls, so a hex field cannot be placed
// between them by any wx means. Instead of copying the dialog template
// (CC_ENABLETEMPLATE, fragile) or putting a wrapper dialog in front of the chooser
// (rejected: an extra modal on every click), this subclass uses the virtual
// wxColourDialog::MSWOnInitDone() hook to create a real "HEX:" static and an EDIT
// control inside the live dialog, in the rectangle of the "Define Custom Colours >>"
// button, which CC_FULLOPEN leaves permanently disabled and which is hidden here.
// It relies only on the control ids from <colordlg.h> (COLOR_LUM, COLOR_MIX,
// COLOR_RED, ...), the same ids wx's own hook procedure reads the current colour from.
//
// Sync: typing a valid #RRGGBB / RRGGBB sends the documented SETRGBSTRING message
// to the dialog, which updates the picker and all six HSL/RGB edits. Any change
// made in the dialog (rainbow, basic/custom grid, RGB edits) reaches the hex field
// through the EN_CHANGE notifications of the Red/Green/Blue edits.
//
// If the dialog template ever lacks one of the anchor controls, nothing is added
// and the dialog stays stock.
//
// On macOS and Linux this is a plain wxColourDialog (their native pickers already
// carry a hex field).

#include <wx/colordlg.h>

namespace Slic3r { namespace GUI {

class FosColourDialog : public wxColourDialog
{
public:
    FosColourDialog(wxWindow *parent, wxColourData *data) : wxColourDialog(parent, data) {}

#ifdef __WXMSW__
    void MSWOnInitDone(WXHWND hDlg) override;

    // Entered from the dialog's window subclass procedure (FosColourDialog.cpp),
    // not meant for callers.
    void fos_on_hex_changed();
    void fos_on_rgb_changed();

private:
    void fos_set_hex_text(unsigned long rgb);

    WXHWND m_hDlg      = nullptr;
    WXHWND m_hex_edit  = nullptr;
    bool   m_updating  = false;   // guards the hex <-> RGB round trip
#endif
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_FosColourDialog_hpp_
