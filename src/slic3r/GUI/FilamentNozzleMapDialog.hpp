#ifndef slic3r_GUI_FilamentNozzleMapDialog_hpp_
#define slic3r_GUI_FilamentNozzleMapDialog_hpp_

// FOS 8.5 stage 1a: node-and-wire editor assigning filaments to nozzles.
//
// SCOPE. This window reads and writes two project-config arrays only:
//   fos_filament_nozzle   - per filament, the 0-based nozzle it feeds, or -1 for unassigned
//   fos_filament_mms_slot - per filament, the 0-based slot inside that nozzle's MMS
// As of stage 1b these ARE live. PresetBundle::full_fff_config() re-indexes the physical
// per-nozzle arrays (nozzle_diameter, extruder_offset and the fos_nozzle_* width/speed
// arrays) by filament through fos_filament_nozzle, so every existing consumer reads the
// nozzle that actually serves a given filament. Changing a mapping therefore re-slices.
//
// The T number is deliberately NOT remapped: the G-code still writes the filament index, so
// FOS stays vendor-neutral. Translating that into a supply system's own slot addressing
// (multiACE encodes slot as a synthetic T = ace * 4 + slot; Sidecar is not documented yet)
// belongs in the Send-to-MMS path, where mms_system is known per extruder.
//
// LAYOUT. Nozzles are pinned in the left column and cannot be moved. Filament nodes are
// free - drag to move, wheel to zoom, drag the background to pan, Auto align to tidy.
// Node positions are cached in app_config, not in the 3mf: they are a view preference,
// not project data.

#include <string>
#include <vector>

#include <wx/colour.h>
#include <wx/dialog.h>
#include <wx/window.h>

#include "GUI_Utils.hpp"

class Button;

namespace Slic3r { namespace GUI {

// Slots a multi-material supply system presents to one nozzle. multiACE and Sidecar both
// feed a single nozzle from four bays; if a future system differs this becomes per-system.
static const int FOS_MMS_SLOTS = 4;

// A nozzle node. Fixed position, one input pin per MMS slot (or a single pin when the
// extruder is fed directly, i.e. mms_system == mmsNone).
struct FosNozzleNode
{
    int      index      = 0;   // 0-based nozzle / extruder index
    double   diameter   = 0.4;
    int      mms_system = 0;   // MultiMaterialSupply value from the printer preset
    wxString mms_name;         // empty when mms_system is None
    int      pin_count  = 1;   // 1, or FOS_MMS_SLOTS
    double   x = 0.0, y = 0.0; // model space, top-left corner
};

// A filament node. Free position, exactly one output pin on its left edge.
struct FosFilamentNode
{
    int      index       = 0;  // 0-based filament index
    wxString brand;
    wxString material;
    wxString variant;   // "Silk", "High Speed", ... - derived from the preset name, see the .cpp
    wxColour colour;
    bool     transparent = false;
    double   x = 0.0, y = 0.0;
};

class FilamentNozzleMapCanvas : public wxWindow
{
public:
    explicit FilamentNozzleMapCanvas(wxWindow *parent);

    // Rebuild the node lists from the current presets and load the saved map.
    void reload_from_config();
    // Push the current map back into project_config. Returns true if anything changed.
    bool write_to_config();
    // Tidy: nozzles in a column, filaments ordered by the nozzle they feed.
    void auto_align();
    // Frame the whole graph. Safe to call any time - no-ops until the canvas has a
    // real client size, which it does NOT have during construction.
    void zoom_to_fit();
    // Restore the pre-mapping default: filament i -> nozzle i while nozzles last.
    void reset_to_identity();
    // True when at least one filament has no nozzle. Drives the dialog warning.
    bool has_unassigned() const;

private:
    // ----- model
    std::vector<FosNozzleNode>   m_nozzles;
    std::vector<FosFilamentNode> m_filaments;
    std::vector<int>             m_nozzle_of;  // per filament: nozzle index, or -1
    std::vector<int>             m_slot_of;    // per filament: MMS slot index
    // Paint order, holding FILAMENT INDICES. Last entry paints last, i.e. on top. Kept
    // separate from m_filaments so that vector stays index-addressable - m_nozzle_of and
    // every hit-test result are filament indices, so m_filaments must not be reordered.
    std::vector<int>             m_draw_order;

    // ----- view transform. Model units are physical pixels at zoom 1, already DIP-scaled.
    double m_zoom  = 1.0;
    double m_pan_x = 0.0;
    double m_pan_y = 0.0;

    // ----- DIP-scaled metrics, computed once per DPI change
    int m_node_w = 0, m_header_h = 0, m_nozzle_h = 0, m_filament_h = 0;
    int m_pin_r = 0, m_col_gap = 0, m_row_gap = 0, m_margin = 0;

    // ----- interaction
    enum class Drag { None, Node, Wire, Pan };
    Drag    m_drag           = Drag::None;
    int     m_drag_filament  = -1;  // filament being moved, or the wire's filament end
    double  m_drag_dx        = 0.0; // grab offset inside the node, model space
    double  m_drag_dy        = 0.0;
    int     m_wire_nozzle    = -1;  // wire drag anchored on this nozzle pin
    int     m_wire_slot      = -1;
    bool    m_wire_from_nozzle = false;
    wxPoint m_mouse_now;
    wxPoint m_mouse_last;
    int     m_hover_filament = -1;
    int     m_hover_nozzle   = -1;
    int     m_hover_slot     = -1;
    bool    m_dirty          = false;
    // Set whenever the layout changes; consumed by the first on_size() that has a
    // usable client size. This is what makes the opening view correct.
    bool    m_fit_pending    = true;

    // ----- geometry helpers
    void    compute_metrics();
    wxPoint to_screen(double mx, double my) const;
    void    to_model(const wxPoint &p, double &mx, double &my) const;
    int     scaled(int v) const;

    wxRect  nozzle_rect(const FosNozzleNode &n) const;
    wxRect  filament_rect(const FosFilamentNode &f) const;
    wxPoint nozzle_pin_pos(const FosNozzleNode &n, int slot) const;
    wxPoint filament_pin_pos(const FosFilamentNode &f) const;

    int  hit_filament_node(const wxPoint &p) const;
    int  hit_filament_pin(const wxPoint &p) const;
    bool hit_nozzle_pin(const wxPoint &p, int &nozzle, int &slot) const;

    // ----- linking. One filament feeds at most one nozzle slot; one slot takes one filament.
    void link(int filament, int nozzle, int slot);
    void unlink_filament(int filament);
    int  filament_on(int nozzle, int slot) const;
    int  first_free_slot(int nozzle) const;
    void bring_to_front(int filament);

    // ----- layout cache (app_config, not the 3mf)
    void load_positions();
    void save_positions() const;

    // ----- painting
    void paint_event(wxPaintEvent &evt);
    void render(wxDC &dc);
    void draw_nozzle(wxDC &dc, const FosNozzleNode &n);
    void draw_filament(wxDC &dc, const FosFilamentNode &f);
    void draw_wire(wxDC &dc, const wxPoint &a, const wxPoint &b, const wxColour &clr, bool thick);
    void draw_swatch(wxDC &dc, const wxRect &r, const wxColour &clr, bool transparent);
    wxFont scaled_font(const wxFont &base) const;

    // ----- events
    void on_mouse_down(wxMouseEvent &evt);
    void on_mouse_up(wxMouseEvent &evt);
    void on_mouse_move(wxMouseEvent &evt);
    void on_mouse_wheel(wxMouseEvent &evt);
    void on_right_down(wxMouseEvent &evt);
    void on_leave(wxMouseEvent &evt);
    // wx REQUIRES this whenever CaptureMouse() is used - without it, capture being
    // stolen (dialog closing mid-drag, Alt-Tab, a modal opening) asserts or aborts.
    void on_capture_lost(wxMouseCaptureLostEvent &evt);
    void on_size(wxSizeEvent &evt);
};

class FilamentNozzleMapDialog : public DPIDialog
{
public:
    explicit FilamentNozzleMapDialog(wxWindow *parent);
    ~FilamentNozzleMapDialog() override = default;

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    FilamentNozzleMapCanvas *m_canvas       = nullptr;
    Button                  *m_btn_align    = nullptr;
    Button                  *m_btn_reset    = nullptr;
    Button                  *m_btn_ok       = nullptr;
    Button                  *m_btn_cancel   = nullptr;
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_FilamentNozzleMapDialog_hpp_
