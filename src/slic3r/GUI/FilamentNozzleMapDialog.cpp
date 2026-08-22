#include "FilamentNozzleMapDialog.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#include <string>

#include <wx/dcbuffer.h>
#include <wx/geometry.h>
#include <wx/menu.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/PrintConfig.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
// FOS: MainFrame must be complete - the ctor static_casts wxGetApp().mainframe to wxWindow*.
#include "MainFrame.hpp"
#include "MsgDialog.hpp"
#include "Plater.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"

namespace Slic3r { namespace GUI {

// Popup menu ids. Kept in disjoint bands so one GetPopupMenuSelectionFromUser() return
// value tells us both what was chosen and which list it came from.
static const int FOS_MENU_UNLINK       = 1;
static const int FOS_MENU_DELETE_FIL   = 2;
static const int FOS_MENU_PRESET_BASE = 10000;  // + index into the offerable list
static const int FOS_MENU_FILAMENT_BASE = 100;   // + filament index
static const int FOS_MENU_NOZZLE_BASE   = 1000;  // + nozzle * 16 + slot

static const char *FOS_LAYOUT_KEY = "fos_filament_map_layout";

// Auto align packs at most this many filament nodes into one column. Note the assigned split
// below (odd nozzles / even nozzles) can never overflow it on its own: one filament per slot,
// FOS_MMS_SLOTS slots per nozzle, two nozzles per column = 8 exactly. The chunking is kept as
// a backstop in case either of those limits ever changes.
static const size_t FOS_AUTO_ALIGN_COL_MAX = 8;

// ---------------------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------------------

static bool fos_is_dark()
{
    return wxGetApp().app_config != nullptr && wxGetApp().app_config->get("dark_color_mode") == "1";
}

// The filament preset backing filament `idx`, preferring the edited copy so that unsaved
// changes in the Filament tab show up here immediately.
static const Preset *fos_filament_preset(size_t idx)
{
    auto *bundle = wxGetApp().preset_bundle;
    if (bundle == nullptr || idx >= bundle->filament_presets.size())
        return nullptr;
    const std::string &name   = bundle->filament_presets[idx];
    const Preset &     edited = bundle->filaments.get_edited_preset();
    return (edited.name == name) ? &edited : bundle->filaments.find_preset(name);
}

static wxString fos_preset_string(const Preset *preset, const char *key)
{
    if (preset == nullptr)
        return wxString();
    const auto *opt = preset->config.option<ConfigOptionStrings>(key);
    if (opt == nullptr || opt->values.empty())
        return wxString();
    return wxString::FromUTF8(opt->values.front().c_str());
}

static bool fos_preset_bool(const Preset *preset, const char *key)
{
    if (preset == nullptr)
        return false;
    const auto *opt = preset->config.option<ConfigOptionBools>(key);
    return opt != nullptr && !opt->values.empty() && opt->values.front() != 0;
}

// "Silk", "High Speed", "SnapSpeed" and the like exist ONLY in the preset NAME. Probed the
// bundled profiles: Generic PLA Silk @base.json carries filament_type "PLA" and
// filament_vendor "Generic", exactly like plain Generic PLA - the only other difference is the
// opaque filament_id (GFL96222). So there is no field to read; strip the " @..." suffix, then
// the leading vendor token, then the material token, and whatever is left is the variant.
// "Generic PLA Silk" -> "Silk";  "Snapmaker PLA Basic @U1" -> "Basic";  "Generic PETG" -> "".
static wxString fos_filament_variant(const wxString &preset_name, const wxString &vendor, const wxString &material)
{
    wxString s  = preset_name;
    const int at = s.Find(" @");
    if (at != wxNOT_FOUND)
        s = s.Left(at);
    s.Trim(true).Trim(false);
    if (!vendor.empty() && s.StartsWith(vendor))
        s = s.Mid(vendor.length());
    s.Trim(false);
    if (!material.empty() && s.StartsWith(material))
        s = s.Mid(material.length());
    s.Trim(true).Trim(false);
    return s;
}

// Mirrors fos_mms_name() in MainFrame.cpp. Kept local rather than exported so the two can
// diverge if a system ever needs a different label here than in the print menu.
static wxString fos_mms_label(int mms_system)
{
    switch (mms_system) {
    case 1:  return wxString("multiACE");
    case 2:  return wxString("Sidecar");
    default: return wxString();
    }
}

static wxString fos_ellipsize(wxDC &dc, const wxString &text, int max_w)
{
    if (text.empty() || dc.GetTextExtent(text).x <= max_w)
        return text;
    wxString out = text;
    while (!out.empty() && dc.GetTextExtent(out + "...").x > max_w)
        out.RemoveLast();
    return out + "...";
}

// ---------------------------------------------------------------------------------------
// FilamentNozzleMapCanvas - construction and model
// ---------------------------------------------------------------------------------------

FilamentNozzleMapCanvas::FilamentNozzleMapCanvas(wxWindow *parent)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif
    compute_metrics();

    Bind(wxEVT_PAINT,       &FilamentNozzleMapCanvas::paint_event,   this);
    Bind(wxEVT_LEFT_DOWN,   &FilamentNozzleMapCanvas::on_mouse_down, this);
    Bind(wxEVT_LEFT_UP,     &FilamentNozzleMapCanvas::on_mouse_up,   this);
    Bind(wxEVT_MOTION,      &FilamentNozzleMapCanvas::on_mouse_move, this);
    Bind(wxEVT_MOUSEWHEEL,  &FilamentNozzleMapCanvas::on_mouse_wheel,this);
    Bind(wxEVT_RIGHT_DOWN,  &FilamentNozzleMapCanvas::on_right_down, this);
    Bind(wxEVT_LEAVE_WINDOW,&FilamentNozzleMapCanvas::on_leave,      this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &FilamentNozzleMapCanvas::on_capture_lost, this);
    Bind(wxEVT_SIZE,        &FilamentNozzleMapCanvas::on_size,       this);

    reload_from_config();
}

int FilamentNozzleMapCanvas::scaled(int v) const { return FromDIP(v); }

void FilamentNozzleMapCanvas::compute_metrics()
{
    m_node_w     = FromDIP(150);
    m_header_h   = FromDIP(24);
    // Nozzle and filament nodes are deliberately the SAME size, so a wire between them runs
    // flat and the two columns read as one grid. The height has to clear four numbered MMS
    // pins and three text lines, hence 88 rather than the old 78/92 pair.
    m_nozzle_h   = FromDIP(88);
    m_filament_h = FromDIP(88);
    m_pin_r      = FromDIP(5);
    m_col_gap    = FromDIP(160);
    m_row_gap    = FromDIP(24);
    m_margin     = FromDIP(30);
}

void FilamentNozzleMapCanvas::reload_from_config()
{
    m_nozzles.clear();
    m_filaments.clear();

    auto *bundle = wxGetApp().preset_bundle;
    if (bundle == nullptr)
        return;

    // ----- nozzles, from the printer preset
    const DynamicPrintConfig &printer = bundle->printers.get_edited_preset().config;
    const auto *nd  = printer.option<ConfigOptionFloats>("nozzle_diameter");
    // NOTE: coEnum in a DynamicPrintConfig is ConfigOptionEnumGeneric, so read via getInt().
    const ConfigOption *mms  = printer.option("mms_system");
    const ConfigOption *topo = printer.option("fos_mms_topology");
    const auto *qty  = printer.option<ConfigOptionInt>("fos_mms_unit_count");
    const auto *hace = printer.option<ConfigOptionBools>("fos_mms_head_ace");
    const size_t nozzle_n = (nd != nullptr && !nd->values.empty()) ? nd->values.size() : 1;

    for (size_t i = 0; i < nozzle_n; ++i) {
        FosNozzleNode n;
        n.index      = int(i);
        n.diameter   = (nd != nullptr && i < nd->values.size()) ? nd->values[i] : 0.4;
        n.mms_system = (mms != nullptr) ? mms->getInt() : 0;
        n.mms_name   = fos_mms_label(n.mms_system);
        // FOS: pins = how many alternative supplies feed this nozzle. A pin means a different
        // thing per mode -- a UNIT in multi, a SLOT in head -- so the two never mix.
        n.pin_count = 1;
        if (n.mms_system != 0) {
            const int tp = (topo != nullptr) ? topo->getInt() : (int) mmtNormal;
            if (tp == (int) mmtMulti)
                n.pin_count = std::max(1, qty != nullptr ? qty->value : 1);
            else if (tp == (int) mmtHead)
                n.pin_count = (hace != nullptr && i < hace->values.size() && hace->values[i]) ? FOS_MMS_SLOTS : 1;
        }
        m_nozzles.push_back(n);
    }

    // ----- filaments, from the project colours plus each filament preset
    const auto *colours = bundle->project_config.option<ConfigOptionStrings>("filament_colour");
    const size_t filament_n = bundle->filament_presets.size();

    for (size_t i = 0; i < filament_n; ++i) {
        FosFilamentNode f;
        f.index    = int(i);
        const Preset *preset = fos_filament_preset(i);
        f.brand    = fos_preset_string(preset, "filament_vendor");
        f.material = fos_preset_string(preset, "filament_type");
        f.transparent = fos_preset_bool(preset, "fos_filament_transparent");
        f.variant = fos_filament_variant(wxString::FromUTF8(bundle->filament_presets[i].c_str()),
                                         f.brand, f.material);
        if (f.brand.empty())
            f.brand = _L("Unknown brand");
        if (f.material.empty())
            f.material = _L("Unknown");
        std::string hex = (colours != nullptr && i < colours->values.size()) ? colours->values[i] : std::string("#FFFFFF");
        f.colour = wxColour(wxString::FromUTF8(hex.c_str()));
        if (!f.colour.IsOk())
            f.colour = *wxWHITE;
        m_filaments.push_back(f);
    }

    // ----- the map itself
    m_nozzle_of.assign(filament_n, -1);
    m_slot_of.assign(filament_n, 0);
    m_draw_order.resize(filament_n);
    for (size_t i = 0; i < filament_n; ++i)
        m_draw_order[i] = int(i);
    const auto *map_noz  = bundle->project_config.option<ConfigOptionInts>("fos_filament_nozzle");
    const auto *map_slot = bundle->project_config.option<ConfigOptionInts>("fos_filament_mms_slot");
    for (size_t i = 0; i < filament_n; ++i) {
        int nozzle = (map_noz != nullptr && i < map_noz->values.size()) ? map_noz->values[i] : -1;
        int slot   = (map_slot != nullptr && i < map_slot->values.size()) ? map_slot->values[i] : 0;
        if (nozzle < 0 || nozzle >= int(m_nozzles.size())) {
            m_nozzle_of[i] = -1;
            m_slot_of[i]   = 0;
            continue;
        }
        if (slot < 0 || slot >= m_nozzles[nozzle].pin_count)
            slot = 0;
        // A slot already taken by an earlier filament loses the newcomer rather than
        // silently double-booking. A stale 3mf can carry a map from a different printer.
        if (filament_on(nozzle, slot) != -1) {
            int free_slot = first_free_slot(nozzle);
            if (free_slot < 0) { m_nozzle_of[i] = -1; m_slot_of[i] = 0; continue; }
            slot = free_slot;
        }
        m_nozzle_of[i] = nozzle;
        m_slot_of[i]   = slot;
    }

    load_positions();
    m_dirty = false;
    Refresh();
}

bool FilamentNozzleMapCanvas::has_unassigned() const
{
    return std::find(m_nozzle_of.begin(), m_nozzle_of.end(), -1) != m_nozzle_of.end();
}

int FilamentNozzleMapCanvas::filament_on(int nozzle, int slot) const
{
    for (size_t i = 0; i < m_nozzle_of.size(); ++i)
        if (m_nozzle_of[i] == nozzle && m_slot_of[i] == slot)
            return int(i);
    return -1;
}

int FilamentNozzleMapCanvas::first_free_slot(int nozzle) const
{
    if (nozzle < 0 || nozzle >= int(m_nozzles.size()))
        return -1;
    for (int s = 0; s < m_nozzles[nozzle].pin_count; ++s)
        if (filament_on(nozzle, s) == -1)
            return s;
    return -1;
}

void FilamentNozzleMapCanvas::link(int filament, int nozzle, int slot)
{
    if (filament < 0 || filament >= int(m_nozzle_of.size()))
        return;
    if (nozzle < 0 || nozzle >= int(m_nozzles.size()))
        return;
    if (slot < 0 || slot >= m_nozzles[nozzle].pin_count)
        slot = 0;
    // One slot holds one filament: evict whoever is sitting there.
    int occupant = filament_on(nozzle, slot);
    if (occupant != -1 && occupant != filament) {
        m_nozzle_of[occupant] = -1;
        m_slot_of[occupant]   = 0;
    }
    m_nozzle_of[filament] = nozzle;
    m_slot_of[filament]   = slot;
    m_dirty = true;
}

void FilamentNozzleMapCanvas::unlink_filament(int filament)
{
    if (filament < 0 || filament >= int(m_nozzle_of.size()))
        return;
    if (m_nozzle_of[filament] == -1)
        return;
    m_nozzle_of[filament] = -1;
    m_slot_of[filament]   = 0;
    m_dirty = true;
}

void FilamentNozzleMapCanvas::reset_to_identity()
{
    for (size_t i = 0; i < m_nozzle_of.size(); ++i) {
        m_nozzle_of[i] = (i < m_nozzles.size()) ? int(i) : -1;
        m_slot_of[i]   = 0;
    }
    m_dirty = true;
    auto_align();
}

bool FilamentNozzleMapCanvas::write_to_config()
{
    auto *bundle = wxGetApp().preset_bundle;
    if (bundle == nullptr)
        return false;
    auto *map_noz  = bundle->project_config.option<ConfigOptionInts>("fos_filament_nozzle", true);
    auto *map_slot = bundle->project_config.option<ConfigOptionInts>("fos_filament_mms_slot", true);
    std::vector<int> noz(m_nozzle_of.begin(), m_nozzle_of.end());
    std::vector<int> slot(m_slot_of.begin(), m_slot_of.end());
    const bool changed = (map_noz->values != noz) || (map_slot->values != slot);
    map_noz->values  = noz;
    map_slot->values = slot;
    save_positions();
    m_dirty = false;
    return changed;
}

// ---------------------------------------------------------------------------------------
// geometry
// ---------------------------------------------------------------------------------------

wxPoint FilamentNozzleMapCanvas::to_screen(double mx, double my) const
{
    return wxPoint(int(std::lround(mx * m_zoom + m_pan_x)), int(std::lround(my * m_zoom + m_pan_y)));
}

void FilamentNozzleMapCanvas::to_model(const wxPoint &p, double &mx, double &my) const
{
    mx = (double(p.x) - m_pan_x) / m_zoom;
    my = (double(p.y) - m_pan_y) / m_zoom;
}

wxRect FilamentNozzleMapCanvas::nozzle_rect(const FosNozzleNode &n) const
{
    const wxPoint tl = to_screen(n.x, n.y);
    return wxRect(tl.x, tl.y, int(m_node_w * m_zoom), int(m_nozzle_h * m_zoom));
}

wxRect FilamentNozzleMapCanvas::filament_rect(const FosFilamentNode &f) const
{
    const wxPoint tl = to_screen(f.x, f.y);
    return wxRect(tl.x, tl.y, int(m_node_w * m_zoom), int(m_filament_h * m_zoom));
}

// Pins sit on the right edge on a FIXED four-slot grid, whether or not an MMS is attached.
// A directly-fed nozzle therefore puts its single pin exactly where slot 1 of an MMS nozzle
// would be, so wires stay at a constant height when a supply system is switched on or off.
wxPoint FilamentNozzleMapCanvas::nozzle_pin_pos(const FosNozzleNode &n, int slot) const
{
    const int    idx      = std::max(0, std::min(slot, std::max(1, n.pin_count) - 1));
    const double body_top = n.y + m_header_h;
    const double body_h   = m_nozzle_h - m_header_h;
    const double y        = body_top + body_h * (double(idx) + 0.5) / double(FOS_MMS_SLOTS);
    return to_screen(n.x + m_node_w, y);
}

// Same vertical offset as nozzle slot 1, so an unmapped filament lines up with the pin it is
// most likely to be wired to. Node heights are equal, so this is the identical offset.
wxPoint FilamentNozzleMapCanvas::filament_pin_pos(const FosFilamentNode &f) const
{
    const double body_top = f.y + m_header_h;
    const double body_h   = m_filament_h - m_header_h;
    return to_screen(f.x, body_top + body_h * 0.5 / double(FOS_MMS_SLOTS));
}

int FilamentNozzleMapCanvas::hit_filament_node(const wxPoint &p) const
{
    // Walk the paint order backwards so the node drawn last (on top) wins the hit.
    for (int k = int(m_draw_order.size()) - 1; k >= 0; --k) {
        const int idx = m_draw_order[k];
        if (idx >= 0 && idx < int(m_filaments.size()) && filament_rect(m_filaments[idx]).Contains(p))
            return idx;
    }
    return -1;
}

// Move a filament to the end of the paint order, so a clicked node stops hiding under its
// neighbours. Purely visual - nothing else reads m_draw_order.
void FilamentNozzleMapCanvas::bring_to_front(int filament)
{
    auto it = std::find(m_draw_order.begin(), m_draw_order.end(), filament);
    if (it == m_draw_order.end())
        return;
    m_draw_order.erase(it);
    m_draw_order.push_back(filament);
}

int FilamentNozzleMapCanvas::hit_filament_pin(const wxPoint &p) const
{
    const int r = int(m_pin_r * m_zoom) + FromDIP(3);
    for (size_t i = 0; i < m_filaments.size(); ++i) {
        const wxPoint c = filament_pin_pos(m_filaments[i]);
        if (std::abs(p.x - c.x) <= r && std::abs(p.y - c.y) <= r)
            return int(i);
    }
    return -1;
}

bool FilamentNozzleMapCanvas::hit_nozzle_pin(const wxPoint &p, int &nozzle, int &slot) const
{
    const int r = int(m_pin_r * m_zoom) + FromDIP(3);
    for (const auto &n : m_nozzles) {
        for (int s = 0; s < n.pin_count; ++s) {
            const wxPoint c = nozzle_pin_pos(n, s);
            if (std::abs(p.x - c.x) <= r && std::abs(p.y - c.y) <= r) {
                nozzle = n.index;
                slot   = s;
                return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------------------
// layout
// ---------------------------------------------------------------------------------------

void FilamentNozzleMapCanvas::auto_align()
{
    // Column 1 - nozzles, fixed, top down.
    for (size_t i = 0; i < m_nozzles.size(); ++i) {
        m_nozzles[i].x = m_margin;
        m_nozzles[i].y = m_margin + double(i) * (m_nozzle_h + m_row_gap);
    }

    // Vertical centre of the nozzle stack. Both filament columns are centred on it, so the
    // graph reads as one balanced row rather than three stacks hanging off the top margin.
    const double nozzle_span = m_nozzles.empty() ? 0.0
        : double(m_nozzles.size()) * m_nozzle_h + double(m_nozzles.size() - 1) * m_row_gap;
    const double nozzle_mid = m_margin + nozzle_span * 0.5;

    std::vector<int> assigned, unassigned;
    for (size_t i = 0; i < m_filaments.size(); ++i)
        ((m_nozzle_of[i] >= 0) ? assigned : unassigned).push_back(int(i));

    // Assigned sort by (nozzle, slot, index) so the wires run roughly parallel.
    std::sort(assigned.begin(), assigned.end(), [this](int a, int b) {
        if (m_nozzle_of[a] != m_nozzle_of[b]) return m_nozzle_of[a] < m_nozzle_of[b];
        if (m_slot_of[a] != m_slot_of[b])     return m_slot_of[a] < m_slot_of[b];
        return a < b;
    });

    // Build the column list. Column 0 is the nozzles; filament columns follow.
    //  - up to 8 assigned      -> one column
    //  - more than 8 assigned  -> split by nozzle parity: nozzles 1 and 3 in the first column,
    //                             2 and 4 in the second. Both slots are reserved even if one
    //                             ends up empty, so the unassigned block always starts at the
    //                             same place rather than sliding left.
    //  - unassigned            -> the columns after that, 8 per column.
    std::vector<std::vector<int>> columns;
    if (assigned.size() <= FOS_AUTO_ALIGN_COL_MAX) {
        columns.push_back(assigned);
    } else {
        std::vector<int> odd_nozzles, even_nozzles;
        for (int id : assigned)
            ((m_nozzle_of[id] % 2) == 0 ? odd_nozzles : even_nozzles).push_back(id);
        columns.push_back(odd_nozzles);   // nozzles 1, 3 (0-based 0, 2)
        columns.push_back(even_nozzles);  // nozzles 2, 4 (0-based 1, 3)
    }
    for (size_t i = 0; i < unassigned.size(); i += FOS_AUTO_ALIGN_COL_MAX)
        columns.emplace_back(unassigned.begin() + i,
                             unassigned.begin() + std::min(unassigned.size(), i + FOS_AUTO_ALIGN_COL_MAX));

    // Every column is centred on the nozzle stack's midpoint, so the graph stays balanced.
    for (size_t c = 0; c < columns.size(); ++c) {
        const std::vector<int> &ids = columns[c];
        if (ids.empty())
            continue;   // reserved-but-empty slot: consume the x position, draw nothing
        const double x    = m_margin + double(c + 1) * (m_node_w + m_col_gap);
        const double span = double(ids.size()) * m_filament_h + double(ids.size() - 1) * m_row_gap;
        double       y    = nozzle_mid - span * 0.5;
        for (int id : ids) {
            m_filaments[id].x = x;
            m_filaments[id].y = y;
            y += m_filament_h + m_row_gap;
        }
    }

    m_fit_pending = true;
    zoom_to_fit();
    Refresh();
}

// Frame the whole graph against the ACTUAL node positions.
//
// This must not run during construction. GetClientSize() there returns a placeholder, and
// fitting against it drove the zoom straight to the 0.35 floor and centred the view on a
// ~20px viewport - the "zoomed way out on first launch" bug. Bail until the size is real and
// let on_size() call this once the dialog has been laid out.
void FilamentNozzleMapCanvas::zoom_to_fit()
{
    const wxSize cs = GetClientSize();
    if (cs.x <= 1 || cs.y <= 1 || (m_nozzles.empty() && m_filaments.empty()))
        return;   // leave m_fit_pending set - on_size() will retry

    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    bool   first = true;
    auto   acc   = [&](double x, double y, double w, double h) {
        if (first) { min_x = x; min_y = y; max_x = x + w; max_y = y + h; first = false; return; }
        min_x = std::min(min_x, x); min_y = std::min(min_y, y);
        max_x = std::max(max_x, x + w); max_y = std::max(max_y, y + h);
    };

    // Frame the WORKING SET - the nozzles and whatever is wired to them - not every node.
    // Unassigned filaments are a staging area that can run several columns off to the right;
    // including them centres the view on empty space and pushes the part you actually care
    // about into the left third. They stay reachable by panning.
    // If nothing is assigned yet there is no working set, so fall back to framing everything.
    bool any_assigned = false;
    for (size_t i = 0; i < m_filaments.size(); ++i)
        if (m_nozzle_of[i] >= 0) { any_assigned = true; break; }

    for (const auto &n : m_nozzles)
        acc(n.x, n.y, m_node_w, m_nozzle_h);
    for (size_t i = 0; i < m_filaments.size(); ++i)
        if (!any_assigned || m_nozzle_of[i] >= 0)
            acc(m_filaments[i].x, m_filaments[i].y, m_node_w, m_filament_h);

    const double content_w = std::max(1.0, (max_x - min_x) + 2.0 * m_margin);
    const double content_h = std::max(1.0, (max_y - min_y) + 2.0 * m_margin);
    m_zoom  = std::max(0.35, std::min(1.0, std::min(double(cs.x) / content_w,
                                                    double(cs.y) / content_h)));
    m_pan_x = (double(cs.x) - (max_x - min_x) * m_zoom) * 0.5 - min_x * m_zoom;
    m_pan_y = (double(cs.y) - (max_y - min_y) * m_zoom) * 0.5 - min_y * m_zoom;
    // NOTE: m_fit_pending is deliberately NOT cleared here. The canvas is resized several
    // times while the dialog lays itself out (SetSizeHints / Layout / Fit / CentreOnParent),
    // and fitting to an intermediate size then refusing to re-fit was leaving the final view
    // framed against a canvas that no longer existed. The flag is cleared the moment the user
    // pans, zooms or drags - see on_mouse_down / on_mouse_wheel.
    Refresh();
}

// Node positions are a view preference, so they live in app_config rather than the 3mf.
// Format: "f<idx>:<x>,<y>;" repeated. Unknown or stale entries are ignored.
void FilamentNozzleMapCanvas::save_positions() const
{
    if (wxGetApp().app_config == nullptr)
        return;
    std::ostringstream ss;
    for (const auto &f : m_filaments)
        ss << "f" << f.index << ":" << int(std::lround(f.x)) << "," << int(std::lround(f.y)) << ";";
    wxGetApp().app_config->set(FOS_LAYOUT_KEY, ss.str());
}

void FilamentNozzleMapCanvas::load_positions()
{
    auto_align(); // always start from a sane layout, then overlay anything we remembered

    if (wxGetApp().app_config == nullptr)
        return;
    const std::string saved = wxGetApp().app_config->get(FOS_LAYOUT_KEY);
    if (saved.empty())
        return;

    std::istringstream ss(saved);
    std::string        token;
    while (std::getline(ss, token, ';')) {
        if (token.size() < 6 || token[0] != 'f')
            continue;
        const size_t colon = token.find(':');
        const size_t comma = token.find(',', colon == std::string::npos ? 0 : colon);
        if (colon == std::string::npos || comma == std::string::npos)
            continue;
        int idx = 0, x = 0, y = 0;
        try {
            idx = std::stoi(token.substr(1, colon - 1));
            x   = std::stoi(token.substr(colon + 1, comma - colon - 1));
            y   = std::stoi(token.substr(comma + 1));
        } catch (...) {
            continue;
        }
        if (idx >= 0 && idx < int(m_filaments.size())) {
            m_filaments[idx].x = double(x);
            m_filaments[idx].y = double(y);
        }
    }

    // Re-fit against the RESTORED positions. auto_align() above already fitted, but to the
    // tidy layout - which the overlay has just replaced. Framing the wrong geometry is what
    // put the opening view nowhere near the nodes on subsequent launches.
    m_fit_pending = true;
    zoom_to_fit();
}

// ---------------------------------------------------------------------------------------
// painting
// ---------------------------------------------------------------------------------------

wxFont FilamentNozzleMapCanvas::scaled_font(const wxFont &base) const
{
    wxFont f = base;
    const int pt = std::max(6, int(std::lround(base.GetPointSize() * m_zoom)));
    f.SetPointSize(pt);
    return f;
}

void FilamentNozzleMapCanvas::paint_event(wxPaintEvent & WXUNUSED(evt))
{
    wxAutoBufferedPaintDC dc(this);
    render(dc);
}

void FilamentNozzleMapCanvas::draw_swatch(wxDC &dc, const wxRect &r, const wxColour &clr, bool transparent)
{
    // The checker is ALWAYS painted. An opaque filament then covers the middle of it with a
    // solid disc; a translucent one lets the checker read through that same disc at 50%.
    // Keeping the checker visible as a ring around both states is what makes clear vs opaque
    // tell apart at a glance - a filled square gave the two almost the same silhouette.
    const int cell = std::max(2, int(std::lround(FromDIP(4) * m_zoom)));
    dc.SetPen(*wxTRANSPARENT_PEN);
    for (int y = 0; y < r.height; y += cell) {
        for (int x = 0; x < r.width; x += cell) {
            const bool odd = ((x / cell) + (y / cell)) % 2 == 1;
            dc.SetBrush(wxBrush(odd ? wxColour(0xC0, 0xC0, 0xC0) : wxColour(0xFF, 0xFF, 0xFF)));
            dc.DrawRectangle(r.x + x, r.y + y,
                             std::min(cell, r.width - x), std::min(cell, r.height - y));
        }
    }

    const wxColour edge = fos_is_dark() ? wxColour(0x70, 0x70, 0x70) : wxColour(0x90, 0x90, 0x90);
    dc.SetPen(wxPen(edge, 1));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(r);

    // Disc a little smaller than the box side, so the checker always shows as a ring.
    const wxPoint centre(r.x + r.width / 2, r.y + r.height / 2);
    const int     radius = std::max(2, int(std::lround(std::min(r.width, r.height) * 0.5 * 0.78)));

    if (transparent) {
        // Clip to the disc and repaint the checker inside it with the colour at 50% over each
        // cell. wxDC has no alpha brush on MSW, so the blend is done per cell by hand.
        static const double PI = 3.14159265358979323846;
        const int           N  = 48;
        wxPoint             poly[N];
        for (int i = 0; i < N; ++i) {
            const double a = 2.0 * PI * double(i) / double(N);
            poly[i] = wxPoint(centre.x + int(std::lround(double(radius) * std::cos(a))),
                              centre.y + int(std::lround(double(radius) * std::sin(a))));
        }
        const wxColour over_light((clr.Red() + 0xFF) / 2, (clr.Green() + 0xFF) / 2, (clr.Blue() + 0xFF) / 2);
        const wxColour over_dark ((clr.Red() + 0xC0) / 2, (clr.Green() + 0xC0) / 2, (clr.Blue() + 0xC0) / 2);

        dc.SetClippingRegion(wxRegion(N, poly));
        dc.SetPen(*wxTRANSPARENT_PEN);
        for (int y = 0; y < r.height; y += cell) {
            for (int x = 0; x < r.width; x += cell) {
                const bool odd = ((x / cell) + (y / cell)) % 2 == 1;
                dc.SetBrush(wxBrush(odd ? over_dark : over_light));
                dc.DrawRectangle(r.x + x, r.y + y,
                                 std::min(cell, r.width - x), std::min(cell, r.height - y));
            }
        }
        dc.DestroyClippingRegion();

        dc.SetPen(wxPen(edge, 1));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawCircle(centre, radius);
    } else {
        dc.SetPen(wxPen(edge, 1));
        dc.SetBrush(wxBrush(clr));
        dc.DrawCircle(centre, radius);
    }
}

// Cubic bezier sampled into a polyline. wxDC has no curve primitive we can rely on across
// platforms, and 24 segments is visually smooth at every zoom this canvas allows.
void FilamentNozzleMapCanvas::draw_wire(wxDC &dc, const wxPoint &a, const wxPoint &b, const wxColour &clr, bool thick)
{
    const double dx = std::max(double(FromDIP(40)), std::abs(double(b.x - a.x)) * 0.45);
    const wxPoint2DDouble p0(a.x, a.y);
    const wxPoint2DDouble p1(a.x + dx, a.y);
    const wxPoint2DDouble p2(b.x - dx, b.y);
    const wxPoint2DDouble p3(b.x, b.y);

    const int  N = 24;
    wxPoint    pts[N + 1];
    for (int i = 0; i <= N; ++i) {
        const double t = double(i) / double(N);
        const double u = 1.0 - t;
        const double x = u * u * u * p0.m_x + 3 * u * u * t * p1.m_x + 3 * u * t * t * p2.m_x + t * t * t * p3.m_x;
        const double y = u * u * u * p0.m_y + 3 * u * u * t * p1.m_y + 3 * u * t * t * p2.m_y + t * t * t * p3.m_y;
        pts[i] = wxPoint(int(std::lround(x)), int(std::lround(y)));
    }
    dc.SetPen(wxPen(clr, std::max(1, int(std::lround((thick ? 3 : 2) * m_zoom)))));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawLines(N + 1, pts);
}

// FOS: nozzle diameter -> grey. Identical steps to the sidebar nozzle panel's swatch
// (Plater.cpp, get_diam_color), so the two views read as one system. Keyed on the VALUE
// rather than a formatted string, so "0.4" and "0.40" cannot diverge; anything
// unrecognised falls back to white, matching the sidebar's else branch.
static wxColour fos_diam_colour(double d)
{
    if (std::abs(d - 0.2) < 0.001) return wxColour(0xFF, 0xFF, 0xFF);
    if (std::abs(d - 0.4) < 0.001) return wxColour(0xE6, 0xE6, 0xE6);
    if (std::abs(d - 0.6) < 0.001) return wxColour(0xCC, 0xCC, 0xCC);
    if (std::abs(d - 0.8) < 0.001) return wxColour(0xB3, 0xB3, 0xB3);
    return wxColour(0xFF, 0xFF, 0xFF);
}

void FilamentNozzleMapCanvas::draw_nozzle(wxDC &dc, const FosNozzleNode &n)
{
    const bool   dark   = fos_is_dark();
    const wxRect r      = nozzle_rect(n);
    const int    radius = std::max(2, int(std::lround(FromDIP(6) * m_zoom)));

    dc.SetPen(wxPen(dark ? wxColour(0x5A, 0x5A, 0x5A) : wxColour(0xBC, 0xBC, 0xBC), 1));
    dc.SetBrush(wxBrush(dark ? wxColour(0x33, 0x33, 0x33) : wxColour(0xFA, 0xFA, 0xFA)));
    dc.DrawRoundedRectangle(r, radius);

    // header - FOS: shaded by nozzle diameter, same four steps as the sidebar swatch.
    wxRect hr(r.x, r.y, r.width, int(m_header_h * m_zoom));
    dc.SetBrush(wxBrush(fos_diam_colour(n.diameter)));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRoundedRectangle(hr, radius);
    dc.DrawRectangle(hr.x, hr.y + radius, hr.width, hr.height - radius);

    // FOS: re-stroke the outline AFTER the header fill so the border encloses the header,
    // exactly as draw_filament does. The header uses a transparent pen and would otherwise
    // paint over the node's top edge, leaving the header sitting outside the border.
    dc.SetPen(wxPen(dark ? wxColour(0x5A, 0x5A, 0x5A) : wxColour(0xBC, 0xBC, 0xBC), 1));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRoundedRectangle(r, radius);

    const int pad = std::max(2, int(std::lround(FromDIP(8) * m_zoom)));
    // Every diameter shade is light (0xB3..0xFF), so the header label is dark in BOTH
    // themes - following `dark` here would put white text on a near-white header.
    dc.SetTextForeground(wxColour(0x26, 0x26, 0x26));
    dc.SetFont(scaled_font(::Label::Head_13));
    dc.DrawText(wxString::Format(_L("Nozzle %d"), n.index + 1), hr.x + pad, hr.y + pad / 2);

    // body: diameter, then the supply system when one is selected. Restore the
    // theme-aware colour - the body sits on the node background, not on the header.
    dc.SetTextForeground(dark ? *wxWHITE : wxColour(0x26, 0x26, 0x26));
    dc.SetFont(scaled_font(::Label::Body_12));
    int ty = hr.GetBottom() + pad / 2;
    dc.DrawText(wxString::Format("%.1f mm", n.diameter), r.x + pad, ty);
    if (!n.mms_name.empty()) {
        ty += int(std::lround(FromDIP(16) * m_zoom));
        dc.SetTextForeground(dark ? wxColour(0x9E, 0xC0, 0x9E) : wxColour(0x3B, 0x7A, 0x3B));
        dc.DrawText(fos_ellipsize(dc, n.mms_name, r.width - 2 * pad), r.x + pad, ty);
    }

    // input pins
    const int pr = std::max(2, int(std::lround(m_pin_r * m_zoom)));
    for (int s = 0; s < n.pin_count; ++s) {
        const wxPoint c        = nozzle_pin_pos(n, s);
        const bool    occupied = filament_on(n.index, s) != -1;
        const bool    hovered  = (m_hover_nozzle == n.index && m_hover_slot == s);
        dc.SetPen(wxPen(dark ? wxColour(0xD0, 0xD0, 0xD0) : wxColour(0x55, 0x55, 0x55), 1));
        dc.SetBrush(wxBrush(hovered  ? wxColour(0xFF, 0xA5, 0x00) :
                            occupied ? wxColour(0x4C, 0xAF, 0x50) :
                                       (dark ? wxColour(0x60, 0x60, 0x60) : wxColour(0xEE, 0xEE, 0xEE))));
        dc.DrawCircle(c, pr);

        // FOS: number the MMS slots 1..4 top to bottom, so a wire can be read straight off
        // against the physical bay order. A directly fed nozzle has one pin and needs no label.
        if (n.pin_count > 1) {
            dc.SetFont(scaled_font(::Label::Body_10));
            dc.SetTextForeground(dark ? wxColour(0xB0, 0xB0, 0xB0) : wxColour(0x66, 0x66, 0x66));
            const wxString lbl = wxString::Format("%d", s + 1);
            const wxSize   ts  = dc.GetTextExtent(lbl);
            const int      gap = std::max(1, int(std::lround(FromDIP(3) * m_zoom)));
            dc.DrawText(lbl, c.x - pr - gap - ts.x, c.y - ts.y / 2);
        }
    }
}

void FilamentNozzleMapCanvas::draw_filament(wxDC &dc, const FosFilamentNode &f)
{
    const bool   dark   = fos_is_dark();
    const wxRect r      = filament_rect(f);
    const int    radius = std::max(2, int(std::lround(FromDIP(6) * m_zoom)));
    const bool   linked = m_nozzle_of[f.index] != -1;

    dc.SetPen(wxPen(linked ? (dark ? wxColour(0x5A, 0x5A, 0x5A) : wxColour(0xBC, 0xBC, 0xBC))
                           : wxColour(0xE0, 0x8A, 0x2E),
                    linked ? 1 : std::max(1, int(std::lround(2 * m_zoom)))));
    dc.SetBrush(wxBrush(dark ? wxColour(0x33, 0x33, 0x33) : wxColour(0xFA, 0xFA, 0xFA)));
    dc.DrawRoundedRectangle(r, radius);

    wxRect hr(r.x, r.y, r.width, int(m_header_h * m_zoom));
    dc.SetBrush(wxBrush(dark ? wxColour(0x45, 0x45, 0x52) : wxColour(0xDF, 0xDF, 0xEA)));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRoundedRectangle(hr, radius);
    dc.DrawRectangle(hr.x, hr.y + radius, hr.width, hr.height - radius);

    // FOS: re-stroke the outline AFTER the header fill. The header is drawn with a
    // transparent pen and paints over the node's top edge, so an unassigned node's thicker
    // orange border stopped at the header instead of enclosing it - the header appeared to
    // sit outside the border. Stroking last makes assigned and unassigned nodes consistent.
    dc.SetPen(wxPen(linked ? (dark ? wxColour(0x5A, 0x5A, 0x5A) : wxColour(0xBC, 0xBC, 0xBC))
                           : wxColour(0xE0, 0x8A, 0x2E),
                    linked ? 1 : std::max(1, int(std::lround(2 * m_zoom)))));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRoundedRectangle(r, radius);

    const int pad = std::max(2, int(std::lround(FromDIP(8) * m_zoom)));
    dc.SetTextForeground(dark ? *wxWHITE : wxColour(0x26, 0x26, 0x26));
    dc.SetFont(scaled_font(::Label::Head_13));
    dc.DrawText(wxString::Format(_L("Filament %d"), f.index + 1), hr.x + pad, hr.y + pad / 2);

    // brand, material, then the variant ("Silk", "High Speed", ...) when the preset has one
    const int swatch = int(std::lround(FromDIP(44) * m_zoom));   // 2x, per request
    const int text_w = r.width - 2 * pad - swatch - pad;
    const int line_h = int(std::lround(FromDIP(15) * m_zoom));
    dc.SetFont(scaled_font(::Label::Body_12));
    int ty = hr.GetBottom() + pad / 2;
    dc.DrawText(fos_ellipsize(dc, f.brand, text_w), r.x + pad, ty);
    ty += line_h;
    dc.SetTextForeground(dark ? wxColour(0xB0, 0xB0, 0xB0) : wxColour(0x66, 0x66, 0x66));
    dc.DrawText(fos_ellipsize(dc, f.material, text_w), r.x + pad, ty);
    if (!f.variant.empty()) {
        ty += line_h;
        dc.SetFont(scaled_font(::Label::Body_10));
        dc.SetTextForeground(dark ? wxColour(0x8C, 0xA8, 0xC8) : wxColour(0x4A, 0x6C, 0x92));
        dc.DrawText(fos_ellipsize(dc, f.variant, text_w), r.x + pad, ty);
    }

    // colour swatch, still anchored to the node's bottom-right corner
    wxRect sr(r.GetRight() - pad - swatch, r.GetBottom() - pad - swatch, swatch, swatch);
    draw_swatch(dc, sr, f.colour, f.transparent);

    // output pin
    const int     pr = std::max(2, int(std::lround(m_pin_r * m_zoom)));
    const wxPoint c  = filament_pin_pos(f);
    const bool    hovered = (m_hover_filament == f.index);
    dc.SetPen(wxPen(dark ? wxColour(0xD0, 0xD0, 0xD0) : wxColour(0x55, 0x55, 0x55), 1));
    dc.SetBrush(wxBrush(hovered ? wxColour(0xFF, 0xA5, 0x00) :
                        linked  ? wxColour(0x4C, 0xAF, 0x50) :
                                  (dark ? wxColour(0x60, 0x60, 0x60) : wxColour(0xEE, 0xEE, 0xEE))));
    dc.DrawCircle(c, pr);
}

void FilamentNozzleMapCanvas::render(wxDC &dc)
{
    const bool   dark = fos_is_dark();
    const wxSize sz   = GetClientSize();

    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(dark ? wxColour(0x24, 0x24, 0x24) : wxColour(0xF0, 0xF0, 0xF0)));
    dc.DrawRectangle(0, 0, sz.x, sz.y);

    // dot grid, so panning and zooming read as motion
    const int step = std::max(6, int(std::lround(FromDIP(24) * m_zoom)));
    dc.SetPen(wxPen(dark ? wxColour(0x38, 0x38, 0x38) : wxColour(0xD8, 0xD8, 0xD8), 1));
    const int ox = int(std::lround(m_pan_x)) % step;
    const int oy = int(std::lround(m_pan_y)) % step;
    for (int x = ox; x < sz.x; x += step)
        for (int y = oy; y < sz.y; y += step)
            dc.DrawPoint(x, y);

    // wires first, so nodes sit on top of them
    for (size_t i = 0; i < m_filaments.size(); ++i) {
        const int nozzle = m_nozzle_of[i];
        if (nozzle < 0 || nozzle >= int(m_nozzles.size()))
            continue;
        const wxPoint a = nozzle_pin_pos(m_nozzles[nozzle], m_slot_of[i]);
        const wxPoint b = filament_pin_pos(m_filaments[i]);
        draw_wire(dc, a, b, m_filaments[i].colour, false);
    }

    // the wire currently being dragged
    if (m_drag == Drag::Wire) {
        wxPoint anchor;
        bool    ok = false;
        if (m_wire_from_nozzle && m_wire_nozzle >= 0 && m_wire_nozzle < int(m_nozzles.size())) {
            anchor = nozzle_pin_pos(m_nozzles[m_wire_nozzle], m_wire_slot);
            ok     = true;
        } else if (!m_wire_from_nozzle && m_drag_filament >= 0 && m_drag_filament < int(m_filaments.size())) {
            anchor = filament_pin_pos(m_filaments[m_drag_filament]);
            ok     = true;
        }
        if (ok)
            draw_wire(dc, m_wire_from_nozzle ? anchor : m_mouse_now,
                          m_wire_from_nozzle ? m_mouse_now : anchor,
                          wxColour(0xFF, 0xA5, 0x00), true);
    }

    for (const auto &n : m_nozzles)
        draw_nozzle(dc, n);
    for (int idx : m_draw_order)
        if (idx >= 0 && idx < int(m_filaments.size()))
            draw_filament(dc, m_filaments[idx]);
}

// ---------------------------------------------------------------------------------------
// events
// ---------------------------------------------------------------------------------------

void FilamentNozzleMapCanvas::on_size(wxSizeEvent &evt)
{
    // First size event with a usable client area is where the opening view actually gets
    // framed - everything before this point ran without knowing how big the canvas is.
    if (m_fit_pending)
        zoom_to_fit();
    Refresh();
    evt.Skip();
}

// Capture can be taken away at any time. Drop every drag cleanly rather than leaving
// m_drag / m_drag_filament pointing at a gesture that will never receive its mouse-up.
void FilamentNozzleMapCanvas::on_capture_lost(wxMouseCaptureLostEvent & WXUNUSED(evt))
{
    m_drag             = Drag::None;
    m_drag_filament    = -1;
    m_wire_nozzle      = -1;
    m_wire_slot        = -1;
    m_wire_from_nozzle = false;
    Refresh();
}

void FilamentNozzleMapCanvas::on_leave(wxMouseEvent &evt)
{
    m_hover_filament = m_hover_nozzle = m_hover_slot = -1;
    Refresh();
    evt.Skip();
}

void FilamentNozzleMapCanvas::on_mouse_down(wxMouseEvent &evt)
{
    const wxPoint p = evt.GetPosition();
    m_mouse_now = m_mouse_last = p;
    m_fit_pending = false;   // the user is driving the view now; stop auto-framing it

    int nozzle = -1, slot = -1;
    if (hit_nozzle_pin(p, nozzle, slot)) {
        // Grabbing an occupied pin picks the existing wire up rather than adding a second
        // one - dropping it on empty space is how a link is broken.
        const int occupant = filament_on(nozzle, slot);
        if (occupant != -1)
            unlink_filament(occupant);
        m_drag             = Drag::Wire;
        m_wire_from_nozzle = true;
        m_wire_nozzle      = nozzle;
        m_wire_slot        = slot;
        m_drag_filament    = -1;
        if (!HasCapture()) CaptureMouse();
        Refresh();
        return;
    }

    const int fp = hit_filament_pin(p);
    if (fp != -1) {
        bring_to_front(fp);
        unlink_filament(fp);
        m_drag             = Drag::Wire;
        m_wire_from_nozzle = false;
        m_drag_filament    = fp;
        m_wire_nozzle      = -1;
        if (!HasCapture()) CaptureMouse();
        Refresh();
        return;
    }

    const int fn = hit_filament_node(p);
    if (fn != -1) {
        double mx = 0.0, my = 0.0;
        to_model(p, mx, my);
        bring_to_front(fn);
        m_drag          = Drag::Node;
        m_drag_filament = fn;
        m_drag_dx       = mx - m_filaments[fn].x;
        m_drag_dy       = my - m_filaments[fn].y;
        if (!HasCapture()) CaptureMouse();
        return;
    }

    // Nozzle nodes are deliberately immovable; clicking one pans like the background does.
    m_drag = Drag::Pan;
    if (!HasCapture()) CaptureMouse();
}

void FilamentNozzleMapCanvas::on_mouse_move(wxMouseEvent &evt)
{
    const wxPoint p = evt.GetPosition();
    m_mouse_now = p;

    if (m_drag == Drag::Node && m_drag_filament >= 0) {
        double mx = 0.0, my = 0.0;
        to_model(p, mx, my);
        m_filaments[m_drag_filament].x = mx - m_drag_dx;
        m_filaments[m_drag_filament].y = my - m_drag_dy;
        Refresh();
    } else if (m_drag == Drag::Pan) {
        m_pan_x += double(p.x - m_mouse_last.x);
        m_pan_y += double(p.y - m_mouse_last.y);
        Refresh();
    } else if (m_drag == Drag::Wire) {
        Refresh();
    } else {
        // hover feedback on pins
        const int prev_f = m_hover_filament, prev_n = m_hover_nozzle, prev_s = m_hover_slot;
        m_hover_filament = hit_filament_pin(p);
        m_hover_nozzle = m_hover_slot = -1;
        int n = -1, s = -1;
        if (hit_nozzle_pin(p, n, s)) { m_hover_nozzle = n; m_hover_slot = s; }
        if (prev_f != m_hover_filament || prev_n != m_hover_nozzle || prev_s != m_hover_slot)
            Refresh();
        SetCursor((m_hover_filament != -1 || m_hover_nozzle != -1) ? wxCursor(wxCURSOR_HAND) : wxNullCursor);
    }
    m_mouse_last = p;
}

void FilamentNozzleMapCanvas::on_mouse_up(wxMouseEvent &evt)
{
    if (HasCapture())
        ReleaseMouse();

    const wxPoint p = evt.GetPosition();

    if (m_drag == Drag::Wire) {
        if (m_wire_from_nozzle) {
            // dropped on a filament pin, or anywhere on a filament node
            int target = hit_filament_pin(p);
            if (target == -1)
                target = hit_filament_node(p);
            if (target != -1)
                link(target, m_wire_nozzle, m_wire_slot);
        } else {
            int nozzle = -1, slot = -1;
            if (hit_nozzle_pin(p, nozzle, slot)) {
                link(m_drag_filament, nozzle, slot);
            } else {
                // dropped on a nozzle body: take the first slot that is still free
                for (const auto &n : m_nozzles) {
                    if (nozzle_rect(n).Contains(p)) {
                        const int free_slot = first_free_slot(n.index);
                        link(m_drag_filament, n.index, free_slot < 0 ? 0 : free_slot);
                        break;
                    }
                }
            }
        }
    } else if (m_drag == Drag::Node) {
        m_dirty = true;
    }

    m_drag             = Drag::None;
    m_drag_filament    = -1;
    m_wire_nozzle      = -1;
    m_wire_slot        = -1;
    m_wire_from_nozzle = false;
    Refresh();
}

void FilamentNozzleMapCanvas::on_mouse_wheel(wxMouseEvent &evt)
{
    const wxPoint p = evt.GetPosition();
    double mx = 0.0, my = 0.0;
    to_model(p, mx, my);

    m_fit_pending = false;   // manual zoom wins from here on
    const double factor = (evt.GetWheelRotation() > 0) ? 1.1 : (1.0 / 1.1);
    const double zoom   = std::max(0.35, std::min(3.0, m_zoom * factor));
    if (std::abs(zoom - m_zoom) < 1e-6)
        return;
    m_zoom = zoom;
    // keep the model point under the cursor pinned to the cursor
    m_pan_x = double(p.x) - mx * m_zoom;
    m_pan_y = double(p.y) - my * m_zoom;
    Refresh();
}

void FilamentNozzleMapCanvas::on_right_down(wxMouseEvent &evt)
{
    const wxPoint p = evt.GetPosition();

    int nozzle = -1, slot = -1;
    const bool on_nozzle_pin   = hit_nozzle_pin(p, nozzle, slot);
    const int  on_filament_pin = on_nozzle_pin ? -1 : hit_filament_pin(p);
    // FOS: the node BODY is only consulted once both pin tests miss, so the existing pin
    // menus keep priority and a right-click on a pin behaves exactly as before.
    const int  on_filament_node = (on_nozzle_pin || on_filament_pin != -1)
                                      ? -1 : hit_filament_node(p);
    if (!on_nozzle_pin && on_filament_pin == -1 && on_filament_node == -1)
        return;

    // Declared out here so the handler below can index it: GetPopupMenuSelectionFromUser()
    // is synchronous, so the list stays alive across the call.
    std::vector<std::string> offerable;

    wxMenu menu;
    if (on_nozzle_pin) {
        const int occupant = filament_on(nozzle, slot);
        menu.Append(FOS_MENU_UNLINK, _L("Unlink"));
        menu.Enable(FOS_MENU_UNLINK, occupant != -1);
        wxMenu *sub = new wxMenu();
        for (const auto &f : m_filaments)
            sub->Append(FOS_MENU_FILAMENT_BASE + f.index,
                        wxString::Format("%d - %s %s", f.index + 1, f.brand, f.material));
        menu.AppendSubMenu(sub, _L("Assign filament"));
    } else if (on_filament_node != -1) {
        // FOS: filament NODE body menu.
        menu.Append(FOS_MENU_DELETE_FIL, _L("Delete this from pool"));
        // delete_filament() itself refuses to drop the last filament; mirror that here so
        // the item reads as unavailable rather than silently doing nothing.
        menu.Enable(FOS_MENU_DELETE_FIL, m_filaments.size() > 1);

        // FOS: the FLP list for this node, from the shared builder - constrained to the
        // assigned nozzle's diameter, or to every variant this model ships when unassigned.
        offerable = wxGetApp().preset_bundle->fos_offerable_filament_presets(
                        m_nozzle_of[on_filament_node]);
        if (!offerable.empty()) {
            wxMenu *sub = new wxMenu();
            for (size_t i = 0; i < offerable.size(); ++i)
                sub->Append(FOS_MENU_PRESET_BASE + int(i),
                            wxString::FromUTF8(offerable[i].c_str()));
            menu.AppendSubMenu(sub, _L("Change filament"));
        }
    } else {
        menu.Append(FOS_MENU_UNLINK, _L("Unlink"));
        menu.Enable(FOS_MENU_UNLINK, m_nozzle_of[on_filament_pin] != -1);
        wxMenu *sub = new wxMenu();
        for (const auto &n : m_nozzles) {
            if (n.pin_count <= 1) {
                sub->Append(FOS_MENU_NOZZLE_BASE + n.index * 16,
                            wxString::Format(_L("Nozzle %d"), n.index + 1));
            } else {
                wxMenu *slots = new wxMenu();
                for (int s = 0; s < n.pin_count; ++s)
                    slots->Append(FOS_MENU_NOZZLE_BASE + n.index * 16 + s,
                                  wxString::Format(_L("Slot %d"), s + 1));
                sub->AppendSubMenu(slots, wxString::Format("%s (%s)",
                                   wxString::Format(_L("Nozzle %d"), n.index + 1), n.mms_name));
            }
        }
        menu.AppendSubMenu(sub, _L("Assign to nozzle"));
    }

    const int sel = GetPopupMenuSelectionFromUser(menu, p);
    if (sel == wxID_NONE)
        return;

    if (sel == FOS_MENU_DELETE_FIL && on_filament_node != -1) {
        const FosFilamentNode &fn = m_filaments[on_filament_node];
        wxString msg = wxString::Format(_L("Delete filament %d (%s %s) from the pool?"),
                                        fn.index + 1, fn.brand, fn.material);
        MessageDialog dlg(this, msg, _L("Delete filament"), wxICON_WARNING | wxYES | wxNO);
        dlg.SetButtonLabel(wxID_YES, _L("Delete"));
        dlg.SetButtonLabel(wxID_NO,  _L("Cancel"));
        if (dlg.ShowModal() != wxID_YES)
            return;
        // FOS: a pool delete is STRUCTURAL and applies immediately, like the sidebar's own
        // delete - this dialog's OK/Cancel governs the WIRING only. Persist the wiring
        // first: reload_from_config() re-reads project_config and would otherwise discard
        // wire edits made since the dialog opened.
        write_to_config();
        wxGetApp().plater()->sidebar().delete_filament(size_t(on_filament_node), -1);
        reload_from_config();
        Refresh();
        return;
    }

    if (sel >= FOS_MENU_PRESET_BASE) {
        const size_t pi = size_t(sel - FOS_MENU_PRESET_BASE);
        if (on_filament_node != -1 && pi < offerable.size()) {
            // Persist wiring first - reload_from_config() below re-reads project_config.
            write_to_config();
            // Mirrors the sidebar combo's commit sequence (Plater.cpp, TYPE_FILAMENT branch).
            // KNOWN GAP: that path also recomputes flushing volumes when the preset's
            // support-ness flips; is_support_filament() is not reachable from here.
            wxGetApp().preset_bundle->set_filament_preset(size_t(on_filament_node), offerable[pi]);
            wxGetApp().plater()->update_project_dirty_from_presets();
            wxGetApp().preset_bundle->export_selections(*wxGetApp().app_config);
            wxGetApp().plater()->sidebar().update_dynamic_filament_list();
            wxGetApp().plater()->sidebar().update_nozzle_filament_slots();
            wxGetApp().plater()->sidebar().update_all_preset_comboboxes(false);
            reload_from_config();
        }
        Refresh();
        return;
    }

    if (sel == FOS_MENU_UNLINK) {
        unlink_filament(on_nozzle_pin ? filament_on(nozzle, slot) : on_filament_pin);
    } else if (sel >= FOS_MENU_NOZZLE_BASE) {
        const int code = sel - FOS_MENU_NOZZLE_BASE;
        link(on_filament_pin, code / 16, code % 16);
    } else if (sel >= FOS_MENU_FILAMENT_BASE) {
        link(sel - FOS_MENU_FILAMENT_BASE, nozzle, slot);
    }
    Refresh();
}

// ---------------------------------------------------------------------------------------
// FilamentNozzleMapDialog
// ---------------------------------------------------------------------------------------

FilamentNozzleMapDialog::FilamentNozzleMapDialog(wxWindow *parent)
    : DPIDialog(parent != nullptr ? parent : static_cast<wxWindow *>(wxGetApp().mainframe),
                wxID_ANY,
                _L("Filament and nozzle mapping"),
                wxDefaultPosition,
                wxDefaultSize,
                wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    SetBackgroundColour(*wxWHITE);

    wxBoxSizer *main = new wxBoxSizer(wxVERTICAL);

    // Stage 1b: the map is live. The note now explains the two things that actually surprise
    // people - that a mapping change re-slices, and that the T number is still the filament
    // index because supply-system addressing is applied at send time, not at slice time.
    wxPanel *banner = new wxPanel(this, wxID_ANY);
    banner->SetBackgroundColour(wxColour(0xE8, 0xEF, 0xF7));
    ::Label *banner_text = new ::Label(banner, ::Label::Body_12,
        _L("Line width and layer height follow the nozzle a filament is assigned to, so "
           "changing a mapping re-slices. The tool number in the G-code stays the filament "
           "number; supply system slot addressing is applied when you send to the system."));
    banner_text->SetForegroundColour(wxColour(0x2A, 0x45, 0x66));
    banner_text->Wrap(FromDIP(640));
    wxBoxSizer *banner_sizer = new wxBoxSizer(wxHORIZONTAL);
    banner_sizer->Add(banner_text, 1, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(8));
    banner->SetSizer(banner_sizer);
    main->Add(banner, 0, wxEXPAND);

    m_canvas = new FilamentNozzleMapCanvas(this);
    m_canvas->SetMinSize(wxSize(FromDIP(900), FromDIP(520)));
    main->Add(m_canvas, 1, wxEXPAND | wxALL, FromDIP(6));

    wxBoxSizer *btns = new wxBoxSizer(wxHORIZONTAL);

    // FOS: pool add, immediately left of Auto align.
    m_btn_add = new Button(this, _L("Add filament"));
    m_btn_add->SetStyle(ButtonStyle::Regular, ButtonType::Window);
    m_btn_add->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        // Same contract as the node delete: structural, immediate, wiring persisted first.
        // A new filament is appended UNASSIGNED (-1) by PresetBundle::set_num_filaments,
        // which is what we want - it must not inherit any nozzle's diameter.
        m_canvas->write_to_config();
        wxGetApp().plater()->sidebar().add_filament();
        m_canvas->reload_from_config();
        m_canvas->Refresh();
    });
    btns->Add(m_btn_add, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));

    m_btn_align = new Button(this, _L("Auto align"));
    m_btn_align->SetStyle(ButtonStyle::Regular, ButtonType::Window);
    m_btn_align->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { m_canvas->auto_align(); });
    btns->Add(m_btn_align, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));

    m_btn_reset = new Button(this, _L("Reset to default"));
    m_btn_reset->SetStyle(ButtonStyle::Regular, ButtonType::Window);
    m_btn_reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { m_canvas->reset_to_identity(); });
    btns->Add(m_btn_reset, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));

    btns->AddStretchSpacer();

    m_btn_cancel = new Button(this, _L("Cancel"));
    m_btn_cancel->SetStyle(ButtonStyle::Regular, ButtonType::Window);
    m_btn_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });

    m_btn_ok = new Button(this, _L("OK"));
    m_btn_ok->SetStyle(ButtonStyle::Confirm, ButtonType::Window);
    m_btn_ok->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (m_canvas->write_to_config()) {
            // Stage 1b: the map feeds full_fff_config()'s per-nozzle re-index, so a change
            // here alters line widths and XY offsets. Print::invalidate_state_by_config_options
            // routes both keys to the catch-all (invalidate_all_steps), so re-slicing is
            // required, not optional.
            wxGetApp().plater()->update_project_dirty_from_presets();
            wxGetApp().plater()->schedule_background_process();
        }

        // FOS: a filament KEEPS its preset when its nozzle assignment changes - we never
        // swap it silently. That can leave a preset whose diameter no longer matches the
        // nozzle it now feeds. Name them here rather than letting it surface as a wrong
        // line width at slice time. Advisory only: nothing is blocked and nothing changed.
        {
            PresetBundle *pb = wxGetApp().preset_bundle;
            wxString shifted;
            for (size_t i = 0; i < pb->filament_presets.size(); ++i) {
                const std::vector<std::string> ok =
                    pb->fos_offerable_filament_presets(pb->fos_assigned_nozzle_for(i));
                if (ok.empty())
                    continue;   // nothing offerable to compare against; do not cry wolf
                if (std::find(ok.begin(), ok.end(), pb->filament_presets[i]) == ok.end())
                    shifted += wxString::Format("\n    %d  -  %s", int(i) + 1,
                                   wxString::FromUTF8(pb->filament_presets[i].c_str()));
            }
            if (!shifted.empty()) {
                MessageDialog dlg(this,
                    _L("These filament presets no longer match the nozzle they are assigned to:")
                        + shifted + "\n\n" +
                    _L("Nothing was changed for you - the presets are exactly as you left them. "
                       "Verify them before slicing."),
                    _L("Check filament presets"), wxICON_WARNING | wxOK);
                dlg.ShowModal();
            }
        }

        EndModal(wxID_OK);
    });
    // FOS: OK sits LEFT of Cancel - Windows convention, and it matches the rest of the app
    // (ReleaseNote.cpp adds OK then Cancel). Sizer insertion order is what positions them,
    // so BOTH Add calls live here rather than beside their construction: the OK handler
    // above is long enough that a split pair reads as an accident. The rightmost button
    // carries wxRIGHT for the trailing margin, so that moved to Cancel.
    btns->Add(m_btn_ok,     0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));
    btns->Add(m_btn_cancel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(10));

    main->Add(btns, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(10));

    SetSizer(main);
    main->SetSizeHints(this);
    Layout();
    Fit();
    CentreOnParent();
    wxGetApp().UpdateDlgDarkUI(this);
}

void FilamentNozzleMapDialog::on_dpi_changed(const wxRect & WXUNUSED(suggested_rect))
{
    if (m_btn_align != nullptr)  m_btn_align->Rescale();
    if (m_btn_reset != nullptr)  m_btn_reset->Rescale();
    if (m_btn_ok != nullptr)     m_btn_ok->Rescale();
    if (m_btn_cancel != nullptr) m_btn_cancel->Rescale();
    if (m_canvas != nullptr) {
        m_canvas->SetMinSize(wxSize(FromDIP(900), FromDIP(520)));
        m_canvas->reload_from_config();
    }
    Layout();
    Fit();
    Refresh();
}

}} // namespace Slic3r::GUI
