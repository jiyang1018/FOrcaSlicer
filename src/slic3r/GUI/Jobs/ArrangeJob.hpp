#ifndef ARRANGEJOB_HPP
#define ARRANGEJOB_HPP


#include <optional>

#include "Job.hpp"
#include "libslic3r/Arrange.hpp"

namespace Slic3r {

class ModelInstance;

namespace GUI {

class Plater;

class ArrangeJob : public Job
{
    using ArrangePolygon = arrangement::ArrangePolygon;
    using ArrangePolygons = arrangement::ArrangePolygons;

    //BBS: add locked logic
    ArrangePolygons m_selected, m_unselected, m_unprintable, m_locked;
    std::vector<ModelInstance*> m_unarranged;
    std::map<int, ArrangePolygons> m_selected_groups;   // groups of selected items for sequential printing
    std::vector<int> m_uncompatible_plates;  // plate indices with different printing sequence than global

    arrangement::ArrangeParams params;
    int current_plate_index = 0;
    Polygon bed_poly;
    Plater *m_plater;

    // BBS: add flag for whether on current part plate
    bool only_on_partplate{false};

    // FOS 8.6.6: Nest - only the selected instances move, onto the current plate, rotation on
    bool m_fos_nest{false};
    // Object-to-object gap (mm) for Nest, from the Close / Loose menu choice
    double m_fos_nest_gap_mm{2.};
    // Objects whose support / brim Nest had to estimate (no current slice of the plate)
    int m_fos_tree_unsliced{0};
    void prepare_fos_nest();

    // clear m_selected and m_unselected, reserve space for next usage
    void clear_input();

    // Prepare the selected and unselected items separately. If nothing is
    // selected, behaves as if everything would be selected.
    void prepare_selected();

    void prepare_all();

    //BBS:prepare the items from current selected partplate
    void prepare_partplate();
    void prepare_wipe_tower();

    ArrangePolygon prepare_arrange_polygon(void* instance);

protected:

    void check_unprintable();

public:

    void prepare();

    void process(Ctl &ctl) override;

    explicit ArrangeJob(bool fos_nest = false, double fos_nest_gap_mm = 2.);

    int status_range() const
    {
        // ensure finalize() is called after all operations in process() is finished.
        return int(m_selected.size() + m_unprintable.size() + 1);
    }

    void finalize(bool canceled, std::exception_ptr &e) override;
};

std::optional<arrangement::ArrangePolygon> get_wipe_tower_arrangepoly(const Plater &);

// The gap between logical beds in the x axis expressed in ratio of
// the current bed width.
static const constexpr double LOGICAL_BED_GAP = 1. / 5.;

//BBS: add sudoku-style strides for x and y
// Stride between logical beds
double bed_stride_x(const Plater* plater);
double bed_stride_y(const Plater* plater);

arrangement::ArrangeParams init_arrange_params(Plater *p);

}} // namespace Slic3r::GUI

#endif // ARRANGEJOB_HPP
