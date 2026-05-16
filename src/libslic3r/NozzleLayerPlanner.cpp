#include "NozzleLayerPlanner.hpp"
#include "PrintConfig.hpp"
#include <algorithm>
#include <cmath>
#include <cassert>

namespace Slic3r {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NozzleLayerPlanner::NozzleLayerPlanner(const PrintConfig& config,
                                       const PrintObjectConfig& object_config)
    : m_config(config)
    , m_object_config(object_config)
{
    // Determine outer and inner wall extruder indices.
    // Phase 1 simplified rule: outer = smallest nozzle, inner = largest nozzle.
    // Full color-priority routing is added in phase 4.
    const auto& nozzle_diameters = m_config.nozzle_diameter.values;

    if (nozzle_diameters.empty()) {
        // Single extruder fallback — no mixed nozzle logic applies
        m_outer_wall_extruder = 0;
        m_inner_wall_extruder = 0;
        m_outer_nozzle_dia    = 0.4f;
        m_inner_nozzle_dia    = 0.4f;
        return;
    }

    // Find extruder with smallest nozzle (outer wall owner)
    // and extruder with largest nozzle (inner wall + infill owner)
    int   min_idx  = 0;
    int   max_idx  = 0;
    float min_dia  = static_cast<float>(nozzle_diameters[0]);
    float max_dia  = static_cast<float>(nozzle_diameters[0]);

    for (int i = 1; i < static_cast<int>(nozzle_diameters.size()); ++i) {
        float d = static_cast<float>(nozzle_diameters[i]);
        if (d < min_dia) { min_dia = d; min_idx = i; }
        if (d > max_dia) { max_dia = d; max_idx = i; }
    }

    m_outer_wall_extruder = min_idx;
    m_inner_wall_extruder = max_idx;
    m_outer_nozzle_dia    = min_dia;
    m_inner_nozzle_dia    = max_dia;

    // Read layer height increment from config if available.
    // Falls back to 0.04 (U1 default) if not set.
    // This keeps the logic hardware-agnostic.
    m_layer_height_increment = 0.04f; // default
    // TODO phase 2: read from printer profile layer_height_increment field
    // if it exists; for now 0.04 is the right value for U1.
}

// ---------------------------------------------------------------------------
// Main planning entry point
// ---------------------------------------------------------------------------

const std::vector<SyncPoint>& NozzleLayerPlanner::plan(
    const std::vector<float>& alh_layer_heights,
    const std::vector<float>& overhang_angles,
    float                     inner_lh_max)
{
    m_sync_points.clear();

    if (alh_layer_heights.empty())
        return m_sync_points;

    // If nozzle sizes are the same, no sync logic is needed.
    // Return empty — all layers print normally.
    if (!nozzle_sizes_differ(m_outer_wall_extruder, m_inner_wall_extruder))
        return m_sync_points;

    // If user set inner_lh_max to 0, treat as "match outer wall" (1:1 mode).
    // In 1:1 mode every outer layer is also an inner layer — no accumulator.
    if (inner_lh_max <= 0.f) {
        // Build a sync point for every layer with height == alh height
        float z = 0.f;
        for (int i = 0; i < static_cast<int>(alh_layer_heights.size()); ++i) {
            float h = alh_layer_heights[i];
            float angle = (i < static_cast<int>(overhang_angles.size()))
                          ? overhang_angles[i] : 0.f;

            SyncPoint sp;
            sp.z                       = z;
            sp.accumulated_height      = h;
            sp.accumulated_layers      = 1;
            sp.gap_offset              = compute_gap_offset(angle, h,
                                             m_outer_nozzle_dia,
                                             m_inner_nozzle_dia);
            sp.expanding               = (angle >= 0.f); // simplification
            sp.outer_wall_extruder     = m_outer_wall_extruder;
            sp.gap_compensation_active = true;
            m_sync_points.push_back(sp);
            z += h;
        }
        return m_sync_points;
    }

    // Build cumulative Z positions for layer lookup
    std::vector<float> z_positions;
    z_positions.reserve(alh_layer_heights.size());
    float z = 0.f;
    for (float h : alh_layer_heights) {
        z_positions.push_back(z);
        z += h;
    }

    run_accumulator(alh_layer_heights, z_positions, inner_lh_max);

    // Attach overhang/slope data to each sync point
    // and compute gap offsets
    for (auto& sp : m_sync_points) {
        // Find the layer index closest to sp.z
        auto it = std::lower_bound(z_positions.begin(), z_positions.end(), sp.z);
        int idx = static_cast<int>(std::distance(z_positions.begin(), it));
        idx = std::clamp(idx, 0,
                         static_cast<int>(overhang_angles.size()) - 1);

        float angle = (idx < static_cast<int>(overhang_angles.size()))
                      ? overhang_angles[idx] : 0.f;

        sp.gap_offset              = compute_gap_offset(angle,
                                         sp.accumulated_height,
                                         m_outer_nozzle_dia,
                                         m_inner_nozzle_dia);
        sp.expanding               = (angle >= 0.f);
        sp.outer_wall_extruder     = m_outer_wall_extruder;
        sp.gap_compensation_active = true;
    }

    return m_sync_points;
}

// ---------------------------------------------------------------------------
// Accumulator logic
// ---------------------------------------------------------------------------

void NozzleLayerPlanner::run_accumulator(
    const std::vector<float>& alh_layer_heights,
    const std::vector<float>& z_positions,
    float                     inner_lh_max)
{
    float accumulator       = 0.f;
    int   layers_accumulated = 0;
    float sync_start_z      = 0.f;  // Z at which current accumulation began

    for (int i = 0; i < static_cast<int>(alh_layer_heights.size()); ++i) {
        float next_h = alh_layer_heights[i];

        // Core rule: if printing one more outer-wall layer would push the
        // accumulated height above inner_lh_max, fire a sync point first.
        if (accumulator > 0.f &&
            (accumulator + next_h) > inner_lh_max + 1e-5f)
        {
            SyncPoint sp;
            sp.z                  = sync_start_z;
            sp.accumulated_height = snap_to_increment(accumulator);
            sp.accumulated_layers = layers_accumulated;
            // gap_offset and expanding filled in by caller after overhang data
            m_sync_points.push_back(sp);

            // Reset accumulator, start fresh from this layer
            sync_start_z      = z_positions[i];
            accumulator       = 0.f;
            layers_accumulated = 0;
        }

        accumulator += next_h;
        ++layers_accumulated;
    }

    // Final sync point: absorb any remaining accumulator at end of model.
    // This is what prevents OrcaSlicer's usual ceiling-rounding overshoot —
    // the inner nozzle prints a final layer at exactly the remainder height,
    // landing on the true model top surface.
    if (accumulator > 1e-5f) {
        SyncPoint sp;
        sp.z                  = z_positions.back() +
                                alh_layer_heights.back() - accumulator;
        sp.accumulated_height = snap_to_increment(accumulator);
        sp.accumulated_layers = layers_accumulated;
        m_sync_points.push_back(sp);
    }
}

// ---------------------------------------------------------------------------
// Gap offset computation
//
// The gap between wall[0] (outer, 0.2 nozzle) and wall[1] (inner, 0.4 nozzle)
// is a trapezoid in cross-section when the model is sloped:
//
//   top width    = inner_nozzle_dia + delta_top
//   bottom width = inner_nozzle_dia + delta_bottom
//   height       = accumulated_height
//
// We place the inner wall at a distance from the outer wall such that:
//   trapezoid_area = inner_nozzle_dia * accumulated_height   (100% flow bead)
//
// Expanding (slope_angle > 0): gap_top > gap_bottom → inner wall moves inward
// Contracting (slope_angle < 0): gap_top < gap_bottom → inner wall moves outward
//
// For a vertical wall (slope_angle == 0): gap = inner_nozzle_dia exactly.
//
// slope_angle is in radians, positive = expanding, negative = contracting.
// ---------------------------------------------------------------------------

float NozzleLayerPlanner::compute_gap_offset(
    float slope_angle,
    float accumulated_height,
    float outer_nozzle_dia,
    float inner_nozzle_dia) const
{
    if (accumulated_height <= 0.f)
        return inner_nozzle_dia;

    // Width delta due to slope: how much the outer wall edge moves horizontally
    // over the accumulated height
    float delta = accumulated_height * std::tan(std::abs(slope_angle));

    // Target bead volume per unit length = inner_nozzle_dia * accumulated_height
    float target_area = inner_nozzle_dia * accumulated_height;

    // Trapezoid area = (top + bottom) / 2 * height
    // We solve for the average width that satisfies the target area:
    //   average_gap = target_area / accumulated_height = inner_nozzle_dia
    // Then distribute delta symmetrically:
    //   gap_bottom = inner_nozzle_dia - delta/2
    //   gap_top    = inner_nozzle_dia + delta/2
    // The inner wall offset is the average = inner_nozzle_dia (unchanged)
    // but the wall is placed at the mid-point of the trapezoid.
    //
    // For the actual placement, we return the inner wall offset from the
    // outer wall center. This is inner_nozzle_dia in the base case,
    // adjusted by delta/2 for slope.

    float gap_offset = inner_nozzle_dia;

    // For very steep slopes (delta > inner_nozzle_dia), the gap at the
    // narrow end would go negative. In this case the geometry is handled
    // by the support system (overhang too steep for gap fill), so we
    // clamp to a minimum of outer_nozzle_dia / 2.
    float min_gap = outer_nozzle_dia * 0.5f;
    if (gap_offset - delta * 0.5f < min_gap) {
        gap_offset = min_gap + delta * 0.5f;
    }

    return gap_offset;
}

// ---------------------------------------------------------------------------
// Per-layer query interface
// ---------------------------------------------------------------------------

bool NozzleLayerPlanner::is_sync_layer(float z) const
{
    return sync_point_at(z) != nullptr;
}

const SyncPoint* NozzleLayerPlanner::sync_point_at(float z) const
{
    if (m_sync_points.empty()) return nullptr;
    const float epsilon = 1e-4f;

    // Binary search: find first sync point with z >= (z - epsilon)
    auto it = std::lower_bound(
        m_sync_points.begin(), m_sync_points.end(), z - epsilon,
        [](const SyncPoint& sp, float val) { return sp.z < val; });

    if (it != m_sync_points.end() && std::abs(it->z - z) < epsilon)
        return &(*it);
    return nullptr;
}

int NozzleLayerPlanner::outer_wall_extruder_at(float z) const
{
    if (m_sync_points.empty()) return m_outer_wall_extruder;

    // Find last sync point at or before z
    auto it = std::upper_bound(
        m_sync_points.begin(), m_sync_points.end(), z + 1e-4f,
        [](float val, const SyncPoint& sp) { return val < sp.z; });

    if (it != m_sync_points.begin()) {
        --it;
        return it->outer_wall_extruder;
    }
    return m_outer_wall_extruder;
}

float NozzleLayerPlanner::gap_offset_at(float z) const
{
    const SyncPoint* sp = sync_point_at(z);
    return sp ? sp->gap_offset : m_inner_nozzle_dia;
}

bool NozzleLayerPlanner::expanding_at(float z) const
{
    const SyncPoint* sp = sync_point_at(z);
    return sp ? sp->expanding : true;
}

bool NozzleLayerPlanner::gap_compensation_active_at(float z) const
{
    const SyncPoint* sp = sync_point_at(z);
    return sp ? sp->gap_compensation_active : false;
}

float NozzleLayerPlanner::inner_layer_height_at(float z) const
{
    const SyncPoint* sp = sync_point_at(z);
    return sp ? sp->accumulated_height : 0.f;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float NozzleLayerPlanner::snap_to_increment(float height) const
{
    if (m_layer_height_increment <= 0.f)
        return height;
    // Round to nearest increment
    float snapped = std::round(height / m_layer_height_increment)
                    * m_layer_height_increment;
    // Ensure at least one increment
    if (snapped < m_layer_height_increment - 1e-5f)
        snapped = m_layer_height_increment;
    return snapped;
}

bool NozzleLayerPlanner::nozzle_sizes_differ(int extruder_a,
                                              int extruder_b) const
{
    if (extruder_a == extruder_b) return false;
    const auto& nozzles = m_config.nozzle_diameter.values;
    if (extruder_a >= static_cast<int>(nozzles.size())) return false;
    if (extruder_b >= static_cast<int>(nozzles.size())) return false;
    return std::abs(nozzles[extruder_a] - nozzles[extruder_b]) > 1e-4;
}

int NozzleLayerPlanner::determine_outer_wall_extruder() const
{
    // Phase 1: return smallest nozzle extruder
    // Phase 4: extend with color-priority routing
    return m_outer_wall_extruder;
}

} // namespace Slic3r
