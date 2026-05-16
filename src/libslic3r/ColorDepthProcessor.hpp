#pragma once

#include <vector>
#include "ExPolygon.hpp"

namespace Slic3r {

class PrintObject;

enum class ThinSectionBehavior {
    BleedThrough,   // color fills entire thin section
    Split50_50      // boundary at centroid, both colors get equal share
};

// Per-filament color depth configuration.
// Stored in the painted model data alongside the region assignments.
struct PatchDepthConfig {
    int                  filament_id         { 0 };
    float                depth_mm            { 0.f }; // 0 = full depth (default)
    bool                 individual_mode     { false };
    ThinSectionBehavior  thin_section_behavior { ThinSectionBehavior::Split50_50 };

    bool has_depth() const { return depth_mm > 1e-5f; }
};

// Result of thin section detection — reported to the UI as a non-blocking notice
struct ThinSectionWarning {
    float z;               // Z height where thin section was detected
    int   filament_id;     // which filament's depth caused it
    float part_thickness;  // actual part thickness at this Z (< depth_mm)
};

class ColorDepthProcessor {
public:
    // -----------------------------------------------------------------------
    // Apply inward offset to color regions for one layer.
    //
    // color_regions : one ExPolygons entry per filament, for this layer.
    //                 Index matches filament_id.
    // depth_configs : depth config per filament. Filaments with depth_mm == 0
    //                 are returned unchanged.
    // layer_z       : Z height of this layer (for thin section detection).
    //
    // Returns modified color regions with depth offsets applied.
    // Regions that shrink to empty due to offset are handled per
    // thin_section_behavior in the config.
    // -----------------------------------------------------------------------
    static std::vector<ExPolygons> apply_depth(
        const std::vector<ExPolygons>&      color_regions,
        const std::vector<PatchDepthConfig>& depth_configs,
        float                               layer_z
    );

    // -----------------------------------------------------------------------
    // Detect thin sections across the entire object.
    // Called during slice preview — before final G-code, so warnings reach
    // the user while they can still change settings.
    //
    // Returns list of warnings, one per detected thin section.
    // Empty list = no issues.
    // -----------------------------------------------------------------------
    static std::vector<ThinSectionWarning> detect_thin_sections(
        const PrintObject&                   object,
        const std::vector<PatchDepthConfig>& depth_configs
    );

private:
    // Apply ClipperLib inward offset to a polygon set.
    // Returns empty ExPolygons if offset collapses the region.
    static ExPolygons inward_offset(const ExPolygons& region, float offset_mm);

    // Resolve a collapsed region according to thin_section_behavior.
    // Returns the resolved region (may be the full original, or a 50/50 split).
    static ExPolygons resolve_thin_section(
        const ExPolygons&   original_region,
        const ExPolygons&   neighbor_region,  // the region on the other side
        ThinSectionBehavior behavior
    );

    // Compute the 50/50 split boundary for a thin section.
    // Returns the half of original_region closest to its centroid.
    static ExPolygons split_50_50(const ExPolygons& region);
};

} // namespace Slic3r
