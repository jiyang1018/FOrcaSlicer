#include "FosFilamentArrangement.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace Slic3r {

// Mirrors of the MultiMaterialTopology enum values (PrintConfig.hpp). Kept as
// local constants so this module does not depend on PrintConfig; the caller
// passes the enum's int value straight through.
static constexpr int fos_topo_normal = 0;
static constexpr int fos_topo_multi  = 1;
static constexpr int fos_topo_head   = 2;

namespace {

struct SearchOutcome
{
    bool                feasible = false;
    int                 swaps    = 0;
    double              stall_s  = 0.;
    double              extrude_s= 0.;
    std::vector<int>    head_of;      // per distinct filament, assigned head
};

// Best assignment of the distinct filaments onto heads under per-head capacity
// and a per-filament allowed-head mask. Cost = swaps * inline stall + the
// extrusion-time term. Brute force with early capacity pruning - the same
// approach as multiACE's compute_swap_aware_layout, bounded by max_distinct.
SearchOutcome search_best(const std::vector<unsigned int> &distinct,
                          const std::vector<unsigned int> &events,
                          const std::vector<int>          &capacity,
                          const std::vector<std::vector<char>> &allowed, // [filament][head]
                          const std::vector<double>       &extrude_s_on_head, // [filament*H + head]
                          size_t                           H)
{
    SearchOutcome best;
    const size_t n = distinct.size();
    if (n == 0) {
        best.feasible = true;
        return best;
    }

    // Map filament index -> position in `distinct` for the event walk.
    std::map<unsigned int, size_t> pos;
    for (size_t i = 0; i < n; ++i)
        pos[distinct[i]] = i;

    std::vector<int> assign(n, 0);   // odometer over heads
    std::vector<int> used(H, 0);
    double best_cost = std::numeric_limits<double>::infinity();

    // Odometer enumeration with per-digit capacity/mask pruning: digit i only
    // takes heads allowed for filament i, and a branch is abandoned as soon as
    // a head exceeds its capacity. Worst case H^n with n <= max_distinct.
    std::vector<int> digit(n, -1);
    size_t level = 0;
    std::fill(used.begin(), used.end(), 0);
    while (true) {
        if (level == n) {
            // Complete assignment: cost it.
            int swaps = 0;
            {
                std::vector<int> cur(H, -1);
                for (unsigned int ev : events) {
                    auto it = pos.find(ev);
                    if (it == pos.end())
                        continue;
                    const int h = digit[it->second];
                    if (cur[h] == -1)
                        cur[h] = int(it->second);       // first load is free
                    else if (cur[h] != int(it->second)) {
                        ++swaps;
                        cur[h] = int(it->second);
                    }
                }
            }
            double extrude = 0.;
            for (size_t i = 0; i < n; ++i)
                extrude += extrude_s_on_head[i * H + size_t(digit[i])];
            const double cost = double(swaps) * FOS_SWAP_STALL_INLINE_S + extrude;
            if (cost < best_cost) {
                best_cost      = cost;
                best.feasible  = true;
                best.swaps     = swaps;
                best.stall_s   = double(swaps) * FOS_SWAP_STALL_INLINE_S;
                best.extrude_s = extrude;
                best.head_of.assign(n, 0);
                for (size_t i = 0; i < n; ++i)
                    best.head_of[i] = digit[i];
            }
            // Backtrack.
            --level;
            used[size_t(digit[level])] -= 1;
            continue;
        }
        // Advance the digit at this level.
        int h = digit[level] + 1;
        while (h < int(H) && (!allowed[level][size_t(h)] || used[size_t(h)] >= capacity[size_t(h)]))
            ++h;
        if (h == int(H)) {
            // Exhausted this level - backtrack.
            digit[level] = -1;
            if (level == 0)
                break;
            --level;
            used[size_t(digit[level])] -= 1;
            continue;
        }
        digit[level] = h;
        used[size_t(h)] += 1;
        ++level;
        if (level < n)
            digit[level] = -1;
    }
    return best;
}

} // anonymous namespace

FosArrangementResult compute_filament_arrangement(const FosArrangementInput &in)
{
    FosArrangementResult out;
    const size_t H = in.phys_nozzle_diameter.size();

    if (in.topology == fos_topo_normal) {
        out.reason = "no supply topology declared - nothing to arrange";
        return out;
    }
    if (in.topology == fos_topo_multi && in.unit_count <= 0) {
        out.reason = "multi mode with unknown unit count - capacity unknown";
        return out;
    }
    if (H == 0 || in.events.empty()) {
        out.reason = "no heads or no toolchange events";
        return out;
    }

    // Distinct filaments in print order of first use.
    std::vector<unsigned int> distinct;
    {
        std::set<unsigned int> seen;
        for (unsigned int ev : in.events)
            if (seen.insert(ev).second)
                distinct.push_back(ev);
    }
    if (int(distinct.size()) > in.max_distinct) {
        out.reason = "more distinct filaments than the brute-force cap";
        return out;
    }

    // Per-head capacity, straight from the topology - the same bound the
    // capacity tier of the map-vs-nozzle guard enforces.
    std::vector<int> capacity(H, 1);
    for (size_t h = 0; h < H; ++h) {
        if (in.topology == fos_topo_multi)
            capacity[h] = in.unit_count;
        else // head mode
            capacity[h] = (h < in.head_ace.size() && in.head_ace[h]) ? 4 : 1;
    }

    // Extrusion-time estimate for filament i printed through head h:
    //   volume / (width(h) * layer_height * speed(h))
    // Volume is fixed by geometry; path length changes with the line width.
    const size_t n = distinct.size();
    std::vector<double> extrude_s(n * H, 0.);
    const double fil_area = 0.25 * 3.141592653589793 * in.filament_diameter * in.filament_diameter;
    for (size_t i = 0; i < n; ++i) {
        auto   it  = in.filament_mm.find(distinct[i]);
        double vol = (it != in.filament_mm.end()) ? it->second * fil_area : 0.;
        for (size_t h = 0; h < H; ++h) {
            const double w = (h < in.nozzle_ref_width.size() && in.nozzle_ref_width[h] > 0)
                                 ? in.nozzle_ref_width[h] : in.phys_nozzle_diameter[h] * 1.1;
            const double v = (h < in.nozzle_ref_speed.size() && in.nozzle_ref_speed[h] > 0)
                                 ? in.nozzle_ref_speed[h] : 100.;
            const double lh = in.layer_height > 0 ? in.layer_height : 0.2;
            extrude_s[i * H + h] = (w > 0 && v > 0) ? vol / (w * lh * v) : 0.;
        }
    }

    // Masks. Proposal: any head. Baseline: only heads whose diameter equals
    // the filament's CURRENTLY mapped diameter, so the reported gain is purely
    // the cross-diameter move - the within-group placement is the supply
    // system's and is simulated identically in both runs.
    std::vector<std::vector<char>> allow_all(n, std::vector<char>(H, 1));
    std::vector<std::vector<char>> allow_cur(n, std::vector<char>(H, 0));
    for (size_t i = 0; i < n; ++i) {
        const unsigned int f = distinct[i];
        double cur_dia = -1.;
        if (f < in.filament_nozzle.size()) {
            const int nz = in.filament_nozzle[f];
            if (nz >= 0 && size_t(nz) < H)
                cur_dia = in.phys_nozzle_diameter[size_t(nz)];
        }
        for (size_t h = 0; h < H; ++h)
            allow_cur[i][h] = (cur_dia < 0.) ? 1
                : (std::abs(in.phys_nozzle_diameter[h] - cur_dia) <= 1e-6 ? 1 : 0);
    }

    const SearchOutcome cur  = search_best(distinct, in.events, capacity, allow_cur, extrude_s, H);
    const SearchOutcome prop = search_best(distinct, in.events, capacity, allow_all, extrude_s, H);

    if (!prop.feasible) {
        out.reason = "no feasible arrangement under the declared capacity";
        return out;
    }
    // Survivor floor: the baseline may be infeasible (a stale map exceeding
    // capacity); the proposal then stands on its own and is always offered.
    out.ok = true;
    out.proposed_nozzle.assign(
        *std::max_element(distinct.begin(), distinct.end()) + 1, -1);
    for (size_t i = 0; i < n; ++i)
        out.proposed_nozzle[distinct[i]] = prop.head_of[i];

    out.swaps_proposed     = prop.swaps;
    out.stall_s_proposed   = prop.stall_s;
    out.extrude_s_proposed = prop.extrude_s;
    if (cur.feasible) {
        out.swaps_current     = cur.swaps;
        out.stall_s_current   = cur.stall_s;
        out.extrude_s_current = cur.extrude_s;
        out.improved = out.total_s_proposed() + 1e-9 < out.total_s_current();
        if (!out.improved)
            out.reason = "current arrangement is already optimal under this model";
    } else {
        out.swaps_current     = -1;
        out.improved          = true;
        out.reason = "current map is infeasible under the declared capacity";
    }
    return out;
}

} // namespace Slic3r
