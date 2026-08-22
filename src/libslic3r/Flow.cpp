#include "Flow.hpp"
#include "I18N.hpp"
#include "Print.hpp"
#include <cmath>
#include <assert.h>

#include <boost/algorithm/string/predicate.hpp>

// Mark string for localization and translate.
#define L(s) Slic3r::I18N::translate(s)

namespace Slic3r {

FlowErrorNegativeSpacing::FlowErrorNegativeSpacing() : 
	FlowError("Flow::spacing() produced negative spacing. Did you set some extrusion width too small?") {}

FlowErrorNegativeFlow::FlowErrorNegativeFlow() :
    FlowError("Flow::mm3_per_mm() produced negative flow. Did you set some extrusion width too small?") {}

// This static method returns a sane extrusion width default.
float Flow::auto_extrusion_width(FlowRole role, float nozzle_diameter)
{
    switch (role) {
    case frSupportMaterial:
    case frSupportMaterialInterface:
    case frSupportTransition:
    case frTopSolidInfill:
        return nozzle_diameter;
    default:
    case frExternalPerimeter:
    case frPerimeter:
    case frSolidInfill:
    case frInfill:
        return 1.125f * nozzle_diameter;
    }
}

// Used by the Flow::extrusion_width() funtion to provide hints to the user on default extrusion width values,
// and to provide reasonable values to the PlaceholderParser.
static inline FlowRole opt_key_to_flow_role(const std::string &opt_key)
{
 	if (opt_key == "inner_wall_line_width" || 
 		// or all the defaults:
 		opt_key == "line_width" || opt_key == "initial_layer_line_width")
        return frPerimeter;
    else if (opt_key == "outer_wall_line_width")
        return frExternalPerimeter;
    else if (opt_key == "sparse_infill_line_width")
        return frInfill;
    else if (opt_key == "internal_solid_infill_line_width")
        return frSolidInfill;
	else if (opt_key == "top_surface_line_width")
		return frTopSolidInfill;
	else if (opt_key == "support_line_width")
    	return frSupportMaterial;
    else 
    	throw Slic3r::RuntimeError("opt_key_to_flow_role: invalid argument");
};

static inline void throw_on_missing_variable(const std::string &opt_key, const char *dependent_opt_key) 
{
	throw FlowErrorMissingVariable((boost::format(L("Failed to calculate line width of %1%. Cannot get value of \"%2%\" ")) % opt_key % dependent_opt_key).str());
}

// Used to provide hints to the user on default extrusion width values, and to provide reasonable values to the PlaceholderParser.
double Flow::extrusion_width(const std::string& opt_key, const ConfigOptionFloatOrPercent* opt, const ConfigOptionResolver& config, const unsigned int first_printing_extruder)
{
	assert(opt != nullptr);

#if 0
// This is the logic used for skit / brim, but not for the rest of the 1st layer.
	if (opt->value == 0. && first_layer) {
		// The "initial_layer_line_width" was set to zero, try a substitute.
		opt = config.option<ConfigOptionFloatOrPercent>("inner_wall_line_width");
		if (opt == nullptr)
    		throw_on_missing_variable(opt_key, "inner_wall_line_width");
	}
#endif

	if (opt->value == 0.) {
		// The role specific extrusion width value was set to zero, try the role non-specific extrusion width.
		opt = config.option<ConfigOptionFloatOrPercent>("line_width");
		if (opt == nullptr)
    		throw_on_missing_variable(opt_key, "line_width");
	}

    auto opt_nozzle_diameters = config.option<ConfigOptionFloats>("nozzle_diameter");
    if (opt_nozzle_diameters == nullptr)
        throw_on_missing_variable(opt_key, "nozzle_diameter");

    if (opt->percent) {
		return opt->get_abs_value(float(opt_nozzle_diameters->get_at(first_printing_extruder)));
	}

	if (opt->value == 0.) {
        // If user left option to 0, calculate a sane default width.
        return auto_extrusion_width(opt_key_to_flow_role(opt_key), float(opt_nozzle_diameters->get_at(first_printing_extruder)));
    }

	return opt->value;
}

// Used to provide hints to the user on default extrusion width values, and to provide reasonable values to the PlaceholderParser.
double Flow::extrusion_width(const std::string& opt_key, const ConfigOptionResolver &config, const unsigned int first_printing_extruder)
{
    return extrusion_width(opt_key, config.option<ConfigOptionFloatOrPercent>(opt_key), config, first_printing_extruder);
}

// This constructor builds a Flow object from an extrusion width config setting
// and other context properties.
Flow Flow::new_from_config_width(FlowRole role, const ConfigOptionFloatOrPercent &width, float nozzle_diameter, float height)
{
    if (height <= 0)
        throw Slic3r::InvalidArgument("Invalid flow height supplied to new_from_config_width()");

    float w;
    if (!width.percent  && width.value <= 0.) {
        // If user left option to 0, calculate a sane default width.
        w = auto_extrusion_width(role, nozzle_diameter);
    } else {
        // If user set a manual value, use it.
      w = float(width.get_abs_value(nozzle_diameter));
    }
    
    return Flow(w, height, rounded_rectangle_extrusion_spacing(w, height), nozzle_diameter, false);
}

// Adjust extrusion flow for new extrusion line spacing, maintaining the old spacing between extrusions.
Flow Flow::with_spacing(float new_spacing) const
{
    Flow out = *this;
    if (m_bridge) {
        // Diameter of the rounded extrusion.
        assert(m_width == m_height);
        float gap          = m_spacing - m_width;
        auto  new_diameter = new_spacing - gap;
        out.m_width        = out.m_height = new_diameter;
    } else {
        assert(m_width >= m_height);
        out.m_width += new_spacing - m_spacing;
        if (out.m_width < out.m_height)
            // FOS: clamp width to height minimum to avoid crash with mixed nozzle color patch flows
            out.m_width = out.m_height;
    }
    out.m_spacing = new_spacing;
    return out;
}

// Adjust the width / height of a rounded extrusion model to reach the prescribed cross section area while maintaining extrusion spacing.
Flow Flow::with_cross_section(float area_new) const
{
    assert(! m_bridge);
    assert(m_width >= m_height);

    // Adjust for bridge_flow, maintain the extrusion spacing.
    float area = this->mm3_per_mm();
    if (area_new > area + EPSILON) {
        // Increasing the flow rate.
        float new_full_spacing = area_new / m_height;
        if (new_full_spacing > m_spacing) {
            // Filling up the spacing without an air gap. Grow the extrusion in height.
            float height = area_new / m_spacing;
            return Flow(rounded_rectangle_extrusion_width_from_spacing(m_spacing, height), height, m_spacing, m_nozzle_diameter, false);
        } else {
            return this->with_width(rounded_rectangle_extrusion_width_from_spacing(area / m_height, m_height));
        }
    } else if (area_new < area - EPSILON) {
        // Decreasing the flow rate.
        float width_new = m_width - (area - area_new) / m_height;
        assert(width_new > 0);
        if (width_new > m_height) {
            // Shrink the extrusion width.
            return this->with_width(width_new);
        } else {
            // Create a rounded extrusion.
            auto dmr = float(sqrt(area_new / M_PI));
            return Flow(dmr, dmr, m_spacing, m_nozzle_diameter, false);
        }
    } else
        return *this;
}

float Flow::rounded_rectangle_extrusion_spacing(float width, float height)
{
    auto out = width - height * float(1. - 0.25 * PI);
    if (out <= 0.f)
        throw FlowErrorNegativeSpacing();
    return out;
}

float Flow::rounded_rectangle_extrusion_width_from_spacing(float spacing, float height)
{
    return float(spacing + height * (1. - 0.25 * PI));
}

float Flow::bridge_extrusion_spacing(float dmr)
{
    return dmr + BRIDGE_EXTRA_SPACING;
}

// This method returns extrusion volume per head move unit.
double Flow::mm3_per_mm() const
{
    float res = m_bridge ?
        // Area of a circle with dmr of this->width.
        float((m_width * m_width) * 0.25 * PI) :
        // Rectangle with semicircles at the ends. ~ h (w - 0.215 h)
        float(m_height * (m_width - m_height * (1. - 0.25 * PI)));
    //assert(res > 0.);
	if (res <= 0.)
		throw FlowErrorNegativeFlow();
    return res;
}

// FOS: see Flow.hpp. Mirrors the rescale in PrintRegion::flow(), which covers wall/infill roles
// only - support and tree support never went through it.
ConfigOptionFloatOrPercent fos_width_for_nozzle(const ConfigOptionFloatOrPercent &config_width,
                                                const PrintConfig               &print_config,
                                                float                            nozzle_diameter)
{
    ConfigOptionFloatOrPercent out = config_width;
    if (!print_config.has_mixed_nozzle_sizes.value || out.percent || out.value <= 0)
        return out;
    // FOS 8.6: the ratio base is PHYSICAL nozzle 1 - the slot the scalar widths are
    // authored against. nozzle_diameter is FILAMENT-indexed by the time it reaches Print
    // (stage 1b re-index), so get_at(0) means "filament 1's nozzle" and goes wrong under a
    // non-identity map. The physical snapshot always exists on 8.6 configs; the old read
    // stays as the fallback for configs that predate it.
    const float ref_nozzle = float(print_config.fos_physical_nozzle_diameter.values.empty()
                                       ? print_config.nozzle_diameter.get_at(0)
                                       : print_config.fos_physical_nozzle_diameter.values.front());
    if (ref_nozzle > 0.f)
        out.value = (out.value / ref_nozzle) * nozzle_diameter;
    return out;
}

double fos_abs_width_for_nozzle(const ConfigOptionFloatOrPercent &config_width,
                                const PrintConfig               &print_config,
                                double                           nozzle_diameter)
{
    return fos_width_for_nozzle(config_width, print_config, float(nozzle_diameter)).get_abs_value(nozzle_diameter);
}

Flow support_material_flow(const PrintObject *object, float layer_height)
{
    const PrintConfig &print_config = object->print()->config();
    // if object->config().support_filament == 0 (which means to not trigger tool change, but use the current extruder instead), get_at will return the 0th component.
    const float nozzle_diameter = float(print_config.nozzle_diameter.get_at(object->config().support_filament - 1));
    // The width parameter accepted by new_from_config_width is of type ConfigOptionFloatOrPercent, the Flow class takes care of the percent to value substitution.
    // FOS 8.5: support_line_width is already resolved to absolute mm for the support nozzle
    // (fos_stamp_per_nozzle_object, with a line_width fallback baked in on the GUI side), so it
    // is used directly - the 8.4 fos_width_for_nozzle ratio would double-apply. line_width
    // remains the fallback only if the resolve produced nothing (e.g. a legacy 3mf predating
    // 8.5, which has no fos_nozzle_* arrays).
    const ConfigOptionFloatOrPercent &raw_width =
        (object->config().support_line_width.value > 0) ? object->config().support_line_width : object->config().line_width;

    return Flow::new_from_config_width(
        frSupportMaterial,
        raw_width,
        nozzle_diameter,
        (layer_height > 0.f) ? layer_height : float(object->config().layer_height.value));
}
//BBS
Flow support_transition_flow(const PrintObject* object)
{
    //BBS: support transition of tree support is bridge flow
    float dmr = float(object->print()->config().nozzle_diameter.get_at(object->config().support_filament - 1));
    return Flow::bridging_flow(dmr, dmr);
}

Flow support_material_1st_layer_flow(const PrintObject *object, float layer_height)
{
    const PrintConfig &print_config = object->print()->config();
    const float nozzle_diameter = float(print_config.nozzle_diameter.get_at(object->config().support_filament - 1));
    // FOS 8.5: initial_layer_line_width is print-scope and NOT per-nozzle resolved (initial-layer
    // widths are out of 8.5 scope), so it still goes through the 8.4 ratio for the support nozzle.
    // support_line_width IS resolved, so it is used raw. line_width is the last-resort fallback and
    // is unresolved, so it also keeps the ratio.
    ConfigOptionFloatOrPercent width;
    // FOS 8.6: prefer the support filament's slot-authored first layer width (see
    // PrintRegion::flow); the ratio-scaled scalar stays as the legacy fallback, and
    // support_filament == 0 (mounted tool) lands there via the negative index.
    const std::vector<double> &fos_l1_arr = object->config().fos_nozzle_initial_layer_line_width.values;
    const int fos_l1_idx = object->config().support_filament.value - 1;
    if (fos_l1_idx >= 0 && fos_l1_idx < (int) fos_l1_arr.size() && fos_l1_arr[fos_l1_idx] > 0) {
        width.value   = fos_l1_arr[fos_l1_idx];
        width.percent = false;
    } else if (print_config.initial_layer_line_width.value > 0)
        width = fos_width_for_nozzle(print_config.initial_layer_line_width, print_config, nozzle_diameter);
    else if (object->config().support_line_width.value > 0)
        width = object->config().support_line_width;
    else
        width = fos_width_for_nozzle(object->config().line_width, print_config, nozzle_diameter);
    return Flow::new_from_config_width(
        frSupportMaterial,
        width,
        nozzle_diameter,
        (layer_height > 0.f) ? layer_height : float(print_config.initial_layer_print_height.value));
}

Flow support_material_interface_flow(const PrintObject *object, float layer_height)
{
    const PrintConfig &print_config = object->print()->config();
    // if object->config().support_interface_filament == 0 (which means to not trigger tool change, but use the current extruder instead), get_at will return the 0th component.
    const float nozzle_diameter = float(print_config.nozzle_diameter.get_at(object->config().support_interface_filament - 1));
    // FOS 8.5: the interface reuses support_line_width (there is no separate interface width key),
    // but it is resolved for the SUPPORT nozzle, while the interface may print on a DIFFERENT
    // nozzle. 8.4 fixed exactly this (interface 0.22 -> 0.44). Preserve it by RETARGETING the
    // resolved support width from the support nozzle to the interface nozzle by diameter ratio -
    // this is a nozzle->nozzle conversion, not the retired author-against-N1 ratio.
    ConfigOptionFloatOrPercent width;
    if (object->config().support_line_width.value > 0) {
        const float support_nozzle = float(print_config.nozzle_diameter.get_at(object->config().support_filament - 1));
        width = object->config().support_line_width; // resolved absolute mm for the support nozzle
        if (!width.percent && support_nozzle > 0.f && nozzle_diameter > 0.f && support_nozzle != nozzle_diameter)
            width.value = width.value * (nozzle_diameter / support_nozzle);
    } else {
        // line_width fallback is unresolved (authored against N1) -> keep the 8.4 ratio.
        width = fos_width_for_nozzle(object->config().line_width, print_config, nozzle_diameter);
    }
    return Flow::new_from_config_width(
        frSupportMaterialInterface,
        width,
        nozzle_diameter,
        (layer_height > 0.f) ? layer_height : float(object->config().layer_height.value));
}

}
