#ifndef slic3r_FosFilamentArrangement_hpp_
#define slic3r_FosFilamentArrangement_hpp_

// FOS 8.6 stage 3: the cross-diameter arrangement optimiser (Direction A).
//
// FOS's output is a PARTITION - which filaments belong to which nozzle-diameter
// group - never a filament -> head binding. The within-group placement (which
// concrete head, which ACE slot) is the supply system's, decided at preflight
// from what is physically loaded (Direction B); binding it here would
// over-constrain (see FOS_TOPOLOGY_and_stage3_optimizer_design, section 5).
//
// Only cross-diameter moves need a re-slice, and only the slicer can make
// them: line width is baked into the extrusions at slice time. That is also
// why the objective carries a per-nozzle PRINT-TIME term the supply system's
// own optimisers have no reason to model - moving a colour to a smaller
// nozzle buys fewer swaps but slower extrusion.
//
// The swap model and constants are ports of multiACE's own optimisers
// (tools/post_process_virtual_toolheads.py): bins are heads, a head swaps
// when its active filament changes, the first load per head is free, an
// inline swap stalls the print ~210 s. Brute force over head assignments
// with a distinct-filament cap, exactly like compute_swap_aware_layout /
// compute_head_mode_optimize (their max_colors guard).
//
// Two passes, not a loop: the tool sequence comes from painting and layer
// order, not from the map, so one slice yields the events (ToolOrdering),
// this computes the partition, one re-slice fixes the widths. Holds while
// layer heights are uniform across the plate - the seam with the MAPS work.

#include <map>
#include <string>
#include <vector>

namespace Slic3r {

struct FosArrangementInput
{
    // Toolchange sequence in print order, 0-based filament indices - the
    // flattened ToolOrdering layer_tools() extruders. No G-code parsing.
    std::vector<unsigned int> events;
    // Distinct filaments per layer, print order. Reserved for the Belady /
    // layer-boundary tier; unused by the current cost model.
    std::vector<std::vector<unsigned int>> layer_filaments;
    // Per-filament consumed filament in mm (PrintStatistics::filament_stats).
    std::map<unsigned int, double> filament_mm;

    // Machine, head-indexed.
    std::vector<double>        phys_nozzle_diameter;
    // MultiMaterialTopology as int (mmtNormal / mmtMulti / mmtHead). Kept as
    // int so this header does not pull in PrintConfig.
    int                        topology   = 0;
    int                        unit_count = 0;               // multi mode
    std::vector<unsigned char> head_ace;                     // head mode ticks

    // Current map, filament -> nozzle index, -1 unmapped (fos_filament_nozzle).
    std::vector<int> filament_nozzle;

    // Per-NOZZLE representative print parameters for the time term, indexed
    // like phys_nozzle_diameter. Feed from the resolved fos_nozzle_* arrays
    // (e.g. sparse infill width/speed) - absolute mm and mm/s.
    std::vector<double> nozzle_ref_width;
    std::vector<double> nozzle_ref_speed;

    double filament_diameter = 1.75;  // for mm -> extruded volume
    double layer_height      = 0.2;
    // Brute-force guard, multiACE's own default. events with more distinct
    // filaments than this return ok=false rather than stalling the UI.
    int max_distinct = 12;
};

struct FosArrangementResult
{
    bool        ok = false;   // a search ran and produced numbers
    std::string reason;       // set when !ok, or when ok but not improved

    // The proposal: filament -> nozzle index whose DIAMETER the filament
    // should be sliced for. The index is a representative of its diameter
    // group, not a head binding. -1 = filament not in the events (untouched).
    std::vector<int> proposed_nozzle;

    int    swaps_current      = 0;
    int    swaps_proposed     = 0;
    double stall_s_current    = 0.;
    double stall_s_proposed   = 0.;
    double extrude_s_current  = 0.;
    double extrude_s_proposed = 0.;

    double total_s_current()  const { return stall_s_current + extrude_s_current; }
    double total_s_proposed() const { return stall_s_proposed + extrude_s_proposed; }
    // Strictly better in total seconds - the caller should only offer the
    // proposal (and its re-slice) when set.
    bool improved = false;
};

// multiACE's swap stall constants (post_process_virtual_toolheads.py:1044).
// Background unloads are not modelled here yet: they need per-event remaining
// minutes and the bg-enabled head set, neither of which the slicer holds.
constexpr double FOS_SWAP_STALL_INLINE_S = 210.;
constexpr double FOS_SWAP_STALL_BG_S     = 30.;

FosArrangementResult compute_filament_arrangement(const FosArrangementInput &in);

} // namespace Slic3r

#endif // slic3r_FosFilamentArrangement_hpp_
