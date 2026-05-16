#pragma once

#include <vector>
#include <optional>
#include "PrintConfig.hpp"

namespace Slic3r {

// Describes one sync event where the inner-wall/infill nozzle (0.4) fires.
// Produced by NozzleLayerPlanner, consumed by PerimeterGenerator and Fill.
struct SyncPoint {
    // Z height at which this sync event occurs (bottom of the sync layer)
    float z { 0.f };

    // Total accumulated height of 0.2-nozzle layers since the last sync.
    // This is the layer height the inner nozzle will print at this event.
    float accumulated_height { 0.f };

    // Number of outer-wall layers accumulated since last sync
    int accumulated_layers { 0 };

    // Slope-compensated gap width between wall[0] and wall[1] at this Z.
    // Computed so a single inner-nozzle bead at 100% flow fills the gap exactly.
    // Only valid when gap_compensation_active is true.
    float gap_offset { 0.f };

    // True  = cross-section is expanding at this Z (outer wall steps outward)
    // False = cross-section is contracting at this Z (outer wall steps inward)
    bool expanding { true };

    // Which extruder index owns the outer wall at this sync point.
    // Determined by color-to-nozzle priority rule:
    //   1. color match (only one extruder has the color)
    //   2. smallest nozzle extruder when color exists on multiple heads
    int outer_wall_extruder { 0 };

    // True when outer and inner wall are on different nozzle sizes.
    // False when:
    //   - single nozzle owns all walls (color forced it)
    //   - same nozzle size on both wall extruders
    // When false, gap_offset is 0 and no compensation is applied.
    bool gap_compensation_active { false };
};


class NozzleLayerPlanner {
public:
    NozzleLayerPlanner(const PrintConfig& config,
                       const PrintObjectConfig& object_config);

    // -----------------------------------------------------------------------
    // Main planning entry point.
    //
    // Called from PrintObject slicing pipeline after ALH layer heights are
    // computed, before PerimeterGenerator and Fill run.
    //
    // alh_layer_heights : layer heights already decided by ALH for the outer
    //                     wall nozzle, one value per layer, bottom to top.
    // overhang_angles   : per-layer max overhang angle, used for slope
    //                     compensation (same data support detection uses).
    // inner_lh_max      : user's maximum layer height for the inner nozzle.
    //                     0 = follow outer wall height (1:1 mode).
    //
    // Returns the list of sync points, also stored internally for query.
    // -----------------------------------------------------------------------
    const std::vector<SyncPoint>& plan(
        const std::vector<float>& alh_layer_heights,
        const std::vector<float>& overhang_angles,
        float                     inner_lh_max
    );

    // -----------------------------------------------------------------------
    // Per-layer query interface — used by PerimeterGenerator and Fill
    // -----------------------------------------------------------------------

    // Returns true if the given Z is a sync layer (inner nozzle fires here).
    bool is_sync_layer(float z) const;

    // Returns the SyncPoint at or just below z, or nullptr if none.
    const SyncPoint* sync_point_at(float z) const;

    // Which extruder index owns the outer wall at this Z.
    int outer_wall_extruder_at(float z) const;

    // Gap offset between wall[0] and wall[1] at this Z.
    // Returns 0 if gap compensation is not active at this Z.
    float gap_offset_at(float z) const;

    // True if the cross-section is expanding at this Z.
    bool expanding_at(float z) const;

    // True if gap compensation is active at this Z.
    bool gap_compensation_active_at(float z) const;

    // The accumulated layer height the inner nozzle should use at this Z.
    // Returns the outer wall layer height if not a sync layer.
    float inner_layer_height_at(float z) const;

private:
    // -----------------------------------------------------------------------
    // Accumulator logic
    //
    // Before each outer-wall layer, check:
    //   if (accumulator + next_alh_layer_height) > inner_lh_max:
    //       fire sync point at current accumulator height
    //       reset accumulator
    //   accumulator += next_alh_layer_height
    //
    // End of print: fire final sync if accumulator > 0
    // -----------------------------------------------------------------------
    void run_accumulator(
        const std::vector<float>& alh_layer_heights,
        const std::vector<float>& z_positions,
        float                     inner_lh_max
    );

    // -----------------------------------------------------------------------
    // Gap offset computation
    //
    // Given the slope angle of the outer wall at this Z, compute the offset
    // between wall[0] and wall[1] such that a single inner-nozzle bead at
    // 100% flow exactly fills the trapezoidal gap.
    //
    // The gap cross-section is a trapezoid:
    //   bottom width  = gap_bottom  (previous sync cycle boundary)
    //   top width     = gap_top     (this sync cycle boundary, varies with slope)
    //   height        = accumulated_height
    //
    // We solve for inner_wall_offset such that:
    //   trapezoid_area == inner_nozzle_diameter * accumulated_height
    // -----------------------------------------------------------------------
    float compute_gap_offset(
        float slope_angle,
        float accumulated_height,
        float outer_nozzle_dia,
        float inner_nozzle_dia
    ) const;

    // -----------------------------------------------------------------------
    // Outer wall extruder determination
    //
    // Priority:
    //   1. If only one extruder has the color needed at this Z -> that extruder
    //   2. If multiple extruders have the color -> smallest nozzle diameter
    //   3. Fallback -> extruder 0
    //
    // In phase 1 this is simplified: returns the extruder with the smallest
    // nozzle diameter among all extruders. Full color-aware routing is phase 4.
    // -----------------------------------------------------------------------
    int determine_outer_wall_extruder() const;

    // -----------------------------------------------------------------------
    // Snap a height value to the nearest valid increment above the minimum.
    // For U1: increment = 0.04mm, but we do not hardcode this —
    // it is read from the printer profile's layer height step if set,
    // otherwise we treat any positive float as valid.
    // -----------------------------------------------------------------------
    float snap_to_increment(float height) const;

    // -----------------------------------------------------------------------
    // Returns true if two extruders have different nozzle diameters,
    // meaning gap compensation should be active.
    // -----------------------------------------------------------------------
    bool nozzle_sizes_differ(int extruder_a, int extruder_b) const;

    const PrintConfig&        m_config;
    const PrintObjectConfig&  m_object_config;
    std::vector<SyncPoint>    m_sync_points;

    // Cached values derived from config at construction time
    int    m_outer_wall_extruder  { 0 };
    int    m_inner_wall_extruder  { 1 };
    float  m_outer_nozzle_dia     { 0.2f };
    float  m_inner_nozzle_dia     { 0.4f };
    float  m_layer_height_increment { 0.04f };
};

} // namespace Slic3r
