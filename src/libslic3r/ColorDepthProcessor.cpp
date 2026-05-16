#include "ColorDepthProcessor.hpp"
#include "ClipperUtils.hpp"
#include <algorithm>
#include <cmath>

namespace Slic3r {

// ---------------------------------------------------------------------------
// apply_depth
// ---------------------------------------------------------------------------

std::vector<ExPolygons> ColorDepthProcessor::apply_depth(
    const std::vector<ExPolygons>&       color_regions,
    const std::vector<PatchDepthConfig>& depth_configs,
    float                                layer_z)
{
    (void)layer_z;
    std::vector<ExPolygons> result = color_regions;

    for (const auto& cfg : depth_configs) {
        // Skip filaments with no depth setting
        if (!cfg.has_depth())
            continue;

        int fid = cfg.filament_id;
        if (fid < 0 || fid >= static_cast<int>(result.size()))
            continue;

        ExPolygons& region = result[fid];
        if (region.empty())
            continue;

        // Apply inward offset by depth_mm
        ExPolygons shrunk = inward_offset(region, cfg.depth_mm);

        if (shrunk.empty()) {
            // Thin section: inward offset collapsed the region
            // Find the neighboring region (any non-empty adjacent filament)
            ExPolygons neighbor;
            for (int i = 0; i < static_cast<int>(result.size()); ++i) {
                if (i == fid) continue;
                if (!result[i].empty()) {
                    neighbor = result[i];
                    break;
                }
            }
            region = resolve_thin_section(region, neighbor,
                                          cfg.thin_section_behavior);
        } else {
            // The shrunk region is what this filament prints.
            // The difference (original - shrunk) gets assigned to the
            // neighboring filament (the inner region).
            // For phase 1 we simply replace the region with the shrunk version.
            // Full neighbor assignment (giving the difference to inner filament)
            // is implemented in phase 3 when the painting gizmo is integrated.
            region = shrunk;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// detect_thin_sections
// ---------------------------------------------------------------------------

std::vector<ThinSectionWarning> ColorDepthProcessor::detect_thin_sections(
    const PrintObject&                   object,
    const std::vector<PatchDepthConfig>& depth_configs)
{
    // Phase 2: full implementation using PrintObject layer data
    // Stubbed for phase 1 -- returns empty, no warnings generated
    (void)object;
    (void)depth_configs;
    return {};
}

// ---------------------------------------------------------------------------
// inward_offset
//
// Uses ClipperLib via OrcaSlicer's ClipperUtils wrapper.
// A negative offset (shrink) is applied to the polygon set.
// Returns empty if the offset collapses all polygons.
// ---------------------------------------------------------------------------

ExPolygons ColorDepthProcessor::inward_offset(const ExPolygons& region,
                                               float offset_mm)
{
    if (region.empty() || offset_mm <= 0.f)
        return region;

    // Convert mm to Clipper integer units
    float offset_scaled = -offset_mm * static_cast<float>(SCALING_FACTOR);

    ExPolygons result = offset_ex(region, offset_scaled);
    return result;
}

// ---------------------------------------------------------------------------
// resolve_thin_section
// ---------------------------------------------------------------------------

ExPolygons ColorDepthProcessor::resolve_thin_section(
    const ExPolygons&   original_region,
    const ExPolygons&   neighbor_region,
    ThinSectionBehavior behavior)
{
    (void)neighbor_region;
    switch (behavior) {
        case ThinSectionBehavior::BleedThrough:
            // Color fills entire thin section — return original unchanged
            return original_region;

        case ThinSectionBehavior::Split50_50:
        default:
            // Return the half closest to the centroid
            return split_50_50(original_region);
    }
}

// ---------------------------------------------------------------------------
// split_50_50
//
// Approximate 50/50 split: apply an inward offset of half the minimum
// bounding dimension. This gives each color roughly equal area in the
// thin section.
// ---------------------------------------------------------------------------

ExPolygons ColorDepthProcessor::split_50_50(const ExPolygons& region)
{
    if (region.empty())
        return region;

    BoundingBox bb = get_extents(region);
    float min_dim  = static_cast<float>(
        std::min(bb.size().x(), bb.size().y())) / SCALING_FACTOR;

    float half_depth = min_dim * 0.5f;
    if (half_depth <= 0.f)
        return region;

    ExPolygons half = inward_offset(region, half_depth);
    return half.empty() ? region : half;
}

} // namespace Slic3r
