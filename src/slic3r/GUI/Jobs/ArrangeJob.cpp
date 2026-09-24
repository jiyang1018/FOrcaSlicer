#include "ArrangeJob.hpp"

#include "libslic3r/BuildVolume.hpp"
#include "libslic3r/SVG.hpp"
#include "libslic3r/MTUtils.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/ModelArrange.hpp"
#include "libslic3r/TriangleMeshSlicer.hpp"
#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/Geometry.hpp"
#include "libslic3r/Geometry/ConvexHull.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/Layer.hpp"

#include <chrono>
#include <map>
#include <sstream>
#include <numeric>
#include <random>

#include "slic3r/GUI/PartPlate.hpp"
#include "slic3r/GUI/GLCanvas3D.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/NotificationManager.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/GUI_ObjectList.hpp"

#include "libnest2d/common.hpp"

#define SAVE_ARRANGE_POLY 0

namespace Slic3r {
// FOS 8.6.6: libslic3r functions with no header declaration, used by Nest's auto-brim estimate
// (fos_auto_brim_mm). Defined in Model.cpp and Brim.cpp, both at namespace Slic3r scope.
double getadhesionCoeff(const ModelVolumePtrs objectVolumes);
double configBrimWidthByVolumeGroups(double adhesion, double maxSpeed, const std::vector<ModelVolume *> modelVolumePtrs,
                                     const ExPolygons &expolys, double &groupHeight);
namespace GUI {
    using ArrangePolygon = arrangement::ArrangePolygon;

// Cache the wti info
class WipeTower: public GLCanvas3D::WipeTowerInfo {
public:
    explicit WipeTower(const GLCanvas3D::WipeTowerInfo &wti)
        : GLCanvas3D::WipeTowerInfo(wti)
    {}

    explicit WipeTower(GLCanvas3D::WipeTowerInfo &&wti)
        : GLCanvas3D::WipeTowerInfo(std::move(wti))
    {}

    void apply_arrange_result(const Vec2d& tr, double rotation, int item_id)
    {
        m_pos = unscaled(tr); m_rotation = rotation;
        apply_wipe_tower();
    }

    ArrangePolygon get_arrange_polygon() const
    {
        Polygon ap({
            {scaled(m_bb.min)},
            {scaled(m_bb.max.x()), scaled(m_bb.min.y())},
            {scaled(m_bb.max)},
            {scaled(m_bb.min.x()), scaled(m_bb.max.y())}
            });

        ArrangePolygon ret;
        ret.poly.contour = std::move(ap);
        ret.translation  = scaled(m_pos);
        ret.rotation     = m_rotation;
        //BBS
        ret.name = "WipeTower";
        ret.is_virt_object = true;
        ret.is_wipe_tower = true;
        ++ret.priority;

        BOOST_LOG_TRIVIAL(debug) << " arrange: wipe tower info:" << m_bb << ", m_pos: " << m_pos.transpose();

        return ret;
    }
};

// BBS: add partplate logic
static WipeTower get_wipe_tower(const Plater &plater, int plate_idx)
{
    return WipeTower{plater.canvas3D()->get_wipe_tower_info(plate_idx)};
}

arrangement::ArrangePolygon get_wipetower_arrange_poly(WipeTower* tower)
{
    ArrangePolygon ap = tower->get_arrange_polygon();
    ap.bed_idx = 0;
    ap.setter = NULL; // do not move wipe tower
    return ap;
}

void ArrangeJob::clear_input()
{
    const Model &model = m_plater->model();

    size_t count = 0, cunprint = 0; // To know how much space to reserve
    for (auto obj : model.objects)
        for (auto mi : obj->instances)
            mi->printable ? count++ : cunprint++;

    params.nonprefered_regions.clear();
    m_selected.clear();
    m_unselected.clear();
    m_unprintable.clear();
    m_locked.clear();
    m_unarranged.clear();
    m_uncompatible_plates.clear();
    m_selected.reserve(count + 1 /* for optional wti */);
    m_unselected.reserve(count + 1 /* for optional wti */);
    m_unprintable.reserve(cunprint /* for optional wti */);
    m_locked.reserve(count + 1 /* for optional wti */);
    current_plate_index = 0;
}

ArrangePolygon ArrangeJob::prepare_arrange_polygon(void* model_instance)
{
    ModelInstance* instance = (ModelInstance*)model_instance;
    const Slic3r::DynamicPrintConfig& config = wxGetApp().preset_bundle->full_config();
    return get_instance_arrange_poly(instance, config);
}

void ArrangeJob::prepare_selected() {
    PartPlateList& plate_list = m_plater->get_partplate_list();

    clear_input();

    Model& model = m_plater->model();
    bool selected_is_locked = false;
    //BBS: remove logic for unselected object
    //double stride = bed_stride_x(m_plater);

    std::vector<const Selection::InstanceIdxsList*>
        obj_sel(model.objects.size(), nullptr);

    for (auto& s : m_plater->get_selection().get_content())
        if (s.first < int(obj_sel.size()))
            obj_sel[size_t(s.first)] = &s.second;

    // Go through the objects and check if inside the selection
    for (size_t oidx = 0; oidx < model.objects.size(); ++oidx) {
        const Selection::InstanceIdxsList* instlist = obj_sel[oidx];
        ModelObject* mo = model.objects[oidx];

        std::vector<bool> inst_sel(mo->instances.size(), false);

        if (instlist)
            for (auto inst_id : *instlist)
                inst_sel[size_t(inst_id)] = true;

        for (size_t i = 0; i < inst_sel.size(); ++i) {
            ModelInstance* mi = mo->instances[i];
            ArrangePolygon&& ap = prepare_arrange_polygon(mo->instances[i]);
            //BBS: partplate_list preprocess
            //remove the locked plate's instances, neither in selected, nor in un-selected
            bool locked = plate_list.preprocess_arrange_polygon(oidx, i, ap, inst_sel[i]);
            if (!locked)
                {
                ArrangePolygons& cont = mo->instances[i]->printable ?
                    (inst_sel[i] ? m_selected :
                        m_unselected) :
                    m_unprintable;

                ap.itemid = cont.size();
                cont.emplace_back(std::move(ap));
                }
            else
                {
                //skip this object due to be locked in plate
                ap.itemid = m_locked.size();
                m_locked.emplace_back(std::move(ap));
                if (inst_sel[i])
                    selected_is_locked = true;
                BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": skip locked instance, obj_id %1%, instance_id %2%, name %3%") % oidx % i % mo->name;
                }
            }
        }


    // If the selection was empty arrange everything
    //if (m_selected.empty()) m_selected.swap(m_unselected);
    if (m_selected.empty()) {
        if (!selected_is_locked)
            m_selected.swap(m_unselected);
        else {
            m_plater->get_notification_manager()->push_notification(NotificationType::BBLPlateInfo,
                NotificationManager::NotificationLevel::WarningNotificationLevel, into_u8(_L("All the selected objects are on a locked plate.\nCannot auto-arrange these objects.")));
            }
        }

    prepare_wipe_tower();


    // The strides have to be removed from the fixed items. For the
    // arrangeable (selected) items bed_idx is ignored and the
    // translation is irrelevant.
    //BBS: remove logic for unselected object
    //for (auto &p : m_unselected) p.translation(X) -= p.bed_idx * stride;
}

void ArrangeJob::prepare_all() {
    clear_input();

    PartPlateList& plate_list = m_plater->get_partplate_list();    
    for (size_t i = 0; i < plate_list.get_plate_count(); i++) {
        PartPlate* plate = plate_list.get_plate(i);
        bool same_as_global_print_seq = true;
        plate->get_real_print_seq(&same_as_global_print_seq);
        if (plate->is_locked() == false && !same_as_global_print_seq) {
            plate->lock(true);
            m_uncompatible_plates.push_back(i);
        }
    }


    Model &model = m_plater->model();
    bool selected_is_locked = false;

    // Go through the objects and check if inside the selection
    for (size_t oidx = 0; oidx < model.objects.size(); ++oidx) {
        ModelObject *mo = model.objects[oidx];

        for (size_t i = 0; i < mo->instances.size(); ++i) {
            ModelInstance * mi = mo->instances[i];
            ArrangePolygon&& ap = prepare_arrange_polygon(mo->instances[i]);
            //BBS: partplate_list preprocess
            //remove the locked plate's instances, neither in selected, nor in un-selected
            bool locked = plate_list.preprocess_arrange_polygon(oidx, i, ap, true);
            if (!locked)
            {
                ArrangePolygons& cont = mo->instances[i]->printable ? m_selected :m_unprintable;

                ap.itemid = cont.size();
                cont.emplace_back(std::move(ap));
            }
            else
            {
                //skip this object due to be locked in plate
                ap.itemid = m_locked.size();
                m_locked.emplace_back(std::move(ap));
                selected_is_locked = true;
                BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": skip locked instance, obj_id %1%, instance_id %2%") % oidx % i;
            }
        }
    }


    // If the selection was empty arrange everything
    //if (m_selected.empty()) m_selected.swap(m_unselected);
    if (m_selected.empty()) {
        if (!selected_is_locked) {
            m_plater->get_notification_manager()->push_notification(NotificationType::BBLPlateInfo,
                NotificationManager::NotificationLevel::WarningNotificationLevel, into_u8(_L("No arrangeable objects are selected.")));
        }
        else {
            m_plater->get_notification_manager()->push_notification(NotificationType::BBLPlateInfo,
                NotificationManager::NotificationLevel::WarningNotificationLevel, into_u8(_L("All the selected objects are on a locked plate.\nCannot auto-arrange these objects.")));
        }
    }

    prepare_wipe_tower();

    // add the virtual object into unselect list if has
    plate_list.preprocess_exclude_areas(m_unselected, MAX_NUM_PLATES);
}

arrangement::ArrangePolygon estimate_wipe_tower_info(int plate_index, std::set<int>& extruder_ids)
{
    PartPlateList& ppl = wxGetApp().plater()->get_partplate_list();
    const auto& full_config = wxGetApp().preset_bundle->full_config();
    int plate_count = ppl.get_plate_count();
    int plate_index_valid = std::min(plate_index, plate_count - 1);

    // we have to estimate the depth using the extruder number of all plates
    int extruder_size = extruder_ids.size();

    auto arrange_poly = ppl.get_plate(plate_index_valid)->estimate_wipe_tower_polygon(full_config, plate_index, extruder_size);
    arrange_poly.bed_idx = plate_index;
    return arrange_poly;
}

// 准备料塔。逻辑如下：
// 1. 以下几种情况不需要料塔：
//    1）料塔被禁用，
//    2）逐件打印，
//    3）不允许不同材料落在相同盘，且没有多色对象
// 2. 以下情况需要料塔：
//    1）某对象是多色对象；
//    2）打开了支撑，且支撑体与接触面使用的是不同材料
//    3）允许不同材料落在相同盘，且所有选定对象中使用了多种热床温度相同的材料
//     （所有对象都是单色的，但不同对象的材料不同，例如：对象A使用红色PLA，对象B使用白色PLA）
void ArrangeJob::prepare_wipe_tower()
{
    bool need_wipe_tower = false;

    // if wipe tower is explicitly disabled, no need to estimate
    DynamicPrintConfig& current_config = wxGetApp().preset_bundle->prints.get_edited_preset().config;
    auto                op = current_config.option("enable_prime_tower");
    bool enable_prime_tower = op && op->getBool();
    if (!enable_prime_tower || params.is_seq_print) return;

    bool smooth_timelapse = false;
    auto sop = current_config.option("timelapse_type");
    if (sop) { smooth_timelapse = sop->getInt() == TimelapseType::tlSmooth; }
    if (smooth_timelapse) { need_wipe_tower = true; }

    // estimate if we need wipe tower for all plates:
    // need wipe tower if some object has multiple extruders (has paint-on colors or support material)
    for (const auto& item : m_selected) {
        std::set<int> obj_extruders;
        obj_extruders.insert(item.extrude_ids.begin(), item.extrude_ids.end());
        if (obj_extruders.size() > 1) {
            need_wipe_tower = true;
            BOOST_LOG_TRIVIAL(info) << "arrange: need wipe tower because object " << item.name << " has multiple extruders (has paint-on colors)";
            break;
        }
    }

    // if multile extruders have same bed temp, we need wipe tower
    // 允许不同材料落在相同盘，且所有选定对象中使用了多种热床温度相同的材料
    if (params.allow_multi_materials_on_same_plate) {
        std::map<int, std::set<int>> bedTemp2extruderIds;
        for (const auto& item : m_selected)
            for (auto id : item.extrude_ids) { bedTemp2extruderIds[item.bed_temp].insert(id); }
        for (const auto& be : bedTemp2extruderIds) {
            if (be.second.size() > 1) {
                need_wipe_tower = true;
                BOOST_LOG_TRIVIAL(info) << "arrange: need wipe tower because allow_multi_materials_on_same_plate=true and we have multiple extruders of same type";
                break;
            }
        }
    }
    BOOST_LOG_TRIVIAL(info) << "arrange: need_wipe_tower=" << need_wipe_tower;


    ArrangePolygon    wipe_tower_ap;
    wipe_tower_ap.name = "WipeTower";
    wipe_tower_ap.is_virt_object = true;
    wipe_tower_ap.is_wipe_tower = true;
    const GLCanvas3D* canvas3D = static_cast<const GLCanvas3D*>(m_plater->canvas3D());

    std::set<int> extruder_ids;
    PartPlateList& ppl = wxGetApp().plater()->get_partplate_list();
    int plate_count = ppl.get_plate_count();
    if (!only_on_partplate) {
        extruder_ids = ppl.get_extruders(true);
    }

    int bedid_unlocked = 0;
    for (int bedid = 0; bedid < MAX_NUM_PLATES; bedid++) {
        int plate_index_valid = std::min(bedid, plate_count - 1);
        PartPlate* pl = ppl.get_plate(plate_index_valid);
        if(bedid<plate_count && pl->is_locked())
            continue;
        if (auto wti = get_wipe_tower(*m_plater, bedid)) {
            // wipe tower is already there
            wipe_tower_ap = get_wipetower_arrange_poly(&wti);
            wipe_tower_ap.bed_idx = bedid_unlocked;
            m_unselected.emplace_back(wipe_tower_ap);
        }
        else if (need_wipe_tower) {
            if (only_on_partplate) {
                auto plate_extruders = pl->get_extruders(true);
                extruder_ids.clear();
                extruder_ids.insert(plate_extruders.begin(), plate_extruders.end());
            }
            wipe_tower_ap = estimate_wipe_tower_info(bedid, extruder_ids);
            wipe_tower_ap.bed_idx = bedid_unlocked;
            m_unselected.emplace_back(wipe_tower_ap);
        }
        bedid_unlocked++;
    }
}


//BBS: prepare current part plate for arranging
void ArrangeJob::prepare_partplate() {
    clear_input();

    PartPlateList& plate_list = m_plater->get_partplate_list();
    PartPlate* plate = plate_list.get_curr_plate();
    current_plate_index = plate_list.get_curr_plate_index();
    assert(plate != nullptr);

    if (plate->empty())
    {
        //no instances on this plate
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": no instances in current plate!");

        return;
    }

    if (plate->is_locked()) {
        m_plater->get_notification_manager()->push_notification(NotificationType::BBLPlateInfo,
            NotificationManager::NotificationLevel::WarningNotificationLevel, into_u8(_L("This plate is locked.\nCannot auto-arrange on this plate.")));
        return;
    }

    Model& model = m_plater->model();

    // Go through the objects and check if inside the selection
    for (size_t oidx = 0; oidx < model.objects.size(); ++oidx)
    {
        ModelObject* mo = model.objects[oidx];
        for (size_t inst_idx = 0; inst_idx < mo->instances.size(); ++inst_idx)
        {
            bool             in_plate = plate->contain_instance(oidx, inst_idx) || plate->intersect_instance(oidx, inst_idx);
            ArrangePolygon&& ap = prepare_arrange_polygon(mo->instances[inst_idx]);

            ArrangePolygons& cont = mo->instances[inst_idx]->printable ?
                (in_plate ? m_selected : m_unselected) :
                m_unprintable;
            bool locked = plate_list.preprocess_arrange_polygon_other_locked(oidx, inst_idx, ap, in_plate);
            if (!locked)
            {
                ap.itemid = cont.size();
                cont.emplace_back(std::move(ap));
            }
            else
            {
                //skip this object due to be not in current plate, treated as locked
                ap.itemid = m_locked.size();
                m_locked.emplace_back(std::move(ap));
                //BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": skip locked instance, obj_id %1%, name %2%") % oidx % mo->name;
            }
        }
    }

    // BBS
    if (auto wti = get_wipe_tower(*m_plater, current_plate_index)) {
        ArrangePolygon&& ap = get_wipetower_arrange_poly(&wti);
        m_unselected.emplace_back(std::move(ap));
    }

    // add the virtual object into unselect list if has
    plate_list.preprocess_exclude_areas(m_unselected, current_plate_index + 1);
}

// FOS 8.6.6: Nest silhouette. The stock arrange polygon is the convex hull, which hides every
// pocket (a C shape nests as a D). Replace it with the real top-view outline: model parts
// projected to XY in the SAME frame ModelInstance::get_arrange_polygon() uses (Z offset only,
// Z rotation left to ArrangePolygon::rotation), unioned, holes dropped, simplified.
// Keeps the hull (returns false) when the outline is effectively convex, is not one piece,
// or the mesh is too heavy to project quickly - the concave NFP is only paid where it helps.
static bool fos_nest_silhouette(const ModelInstance *mi, Polygon &out)
{
    static constexpr size_t FOS_MAX_TRIS      = 300000;
    static constexpr double FOS_SIMPLIFY_MM   = 0.2;
    static constexpr double FOS_CONVEX_RATIO  = 0.97;  // outline / hull area above this = keep hull
    static constexpr size_t FOS_MAX_POINTS    = 256;

    const ModelObject *mo   = mi->get_object();
    size_t             tris = 0;
    for (const ModelVolume *mv : mo->volumes)
        if (mv->is_model_part())
            tris += mv->mesh().its.indices.size();
    if (tris == 0 || tris > FOS_MAX_TRIS)
        return false;

    Vec3d rotation = mi->get_rotation();
    rotation.z()   = 0.;
    Geometry::Transformation t(mi->get_transformation());
    t.set_offset(mi->get_offset().z() * Vec3d::UnitZ());
    t.set_rotation(rotation);

    Polygons proj;
    for (const ModelVolume *mv : mo->volumes)
        if (mv->is_model_part())
            append(proj, project_mesh(mv->mesh().its, t.get_matrix() * mv->get_matrix(), []() {}));

    ExPolygons ex = union_ex(proj);
    if (ex.size() != 1)
        return false;

    const Polygon hull      = Geometry::convex_hull(ex.front().contour.points);
    // (the caller thins a kept hull separately, see fos_nest_thin_hull)
    const double  hull_area = std::abs(hull.area());
    if (hull_area <= 0.)
        return false;

    double   tol = scaled<double>(FOS_SIMPLIFY_MM);
    Polygon  best;
    for (int pass = 0; pass < 4; ++pass, tol *= 2.) {
        best.points.clear();
        double best_area = 0.;
        for (Polygon &p : ex.front().contour.simplify(tol))
            if (std::abs(p.area()) > best_area) {
                best_area = std::abs(p.area());
                best      = std::move(p);
            }
        if (best.points.size() <= FOS_MAX_POINTS)
            break;
    }
    if (best.points.size() < 3 || best.points.size() > FOS_MAX_POINTS)
        return false;
    if (std::abs(best.area()) > FOS_CONVEX_RATIO * hull_area)
        return false;

    best.make_counter_clockwise();
    out = std::move(best);
    return true;
}

// FOS 8.6.6: Nest support footprint.
// Per-object option: the object's own override, else the global print config.
template<class T> static const T *fos_obj_opt(const ModelObject *mo, const DynamicPrintConfig &cfg, const char *key)
{
    if (mo->config.has(key))
        if (const T *o = dynamic_cast<const T *>(mo->config.option(key)))
            return o;
    return dynamic_cast<const T *>(cfg.option(key));
}

// The PrintObject + PrintInstance a GUI instance was sliced as, on the current plate. The Print
// keeps its own Model copy, so match by ObjectID, never by pointer.
static const PrintObject *fos_print_object(const ModelInstance *mi, const PrintInstance *&pi_out)
{
    const ModelObject *mo    = mi->get_object();
    Print             &print = wxGetApp().plater()->get_partplate_list().get_current_fff_print();
    for (const PrintObject *po : print.objects()) {
        if (po->model_object()->id() != mo->id())
            continue;
        for (const PrintInstance &pi : po->instances())
            if (pi.model_instance->id() == mi->id()) {
                pi_out = &pi;
                return po;
            }
    }
    return nullptr;
}

// Object-centred slice frame -> Nest frame. A PrintObject is sliced centred on itself
// (Print.hpp trafo_centered): point + center_offset is relative to the instance origin, still
// rotated by the slice-time Z rotation, which Nest carries separately in ArrangePolygon::rotation.
static void fos_slice_to_nest(const PrintObject *po, Polygon &c)
{
    c.translate(po->center_offset());
    c.rotate(-Geometry::Transformation(po->trafo()).get_rotation().z());
}

// Sliced support outlines. Orca's own tree fills SupportLayer::lslices (TreeSupport.cpp, for
// brim/skirt); the organic tree and normal support fill support_islands (SupportCommon.cpp).
static void fos_sliced_support(const PrintObject *po, Polygons &out)
{
    for (const SupportLayer *sl : po->support_layers()) {
        const ExPolygons &src = !sl->lslices.empty() ? sl->lslices : sl->support_islands;
        for (const ExPolygon &ex : src) {
            Polygon c = ex.contour;
            fos_slice_to_nest(po, c);
            out.emplace_back(std::move(c));
        }
    }
}

// Sliced object brim + support brim (Print::m_brimMap / m_supportBrimMap, Brim.cpp). Those are
// plate-local and hold every instance of the object together, so shift back by this instance's
// shift_without_plate_offset() and keep only pieces near this instance's own footprint.
static void fos_sliced_brim(const PrintObject *po, const PrintInstance *pi, const BoundingBox &near_bb, Polygons &out)
{
    Print &print = wxGetApp().plater()->get_partplate_list().get_current_fff_print();
    const Point shift = pi->shift_without_plate_offset();
    for (auto *map : {&print.get_brimMap(), &print.get_supportBrimMap()}) {
        auto it = map->find(po->id());
        if (it == map->end())
            continue;
        for (Polygon &c : it->second.polygons_covered_by_width()) {
            c.translate(-shift);
            fos_slice_to_nest(po, c);
            if (get_extents(c).overlap(near_bb))
                out.emplace_back(std::move(c));
        }
    }
}

// Unsliced estimate: downward faces steeper than the support threshold, projected to XY in the
// Nest frame (same frame as fos_nest_silhouette). Faces touching the bed are not overhangs.
static Polygons fos_overhang_projection(const ModelInstance *mi, double threshold_deg)
{
    static constexpr size_t FOS_MAX_TRIS = 300000;
    static constexpr double FOS_BED_Z_MM = 0.3;
    const ModelObject *mo = mi->get_object();
    size_t tris = 0;
    for (const ModelVolume *mv : mo->volumes)
        if (mv->is_model_part())
            tris += mv->mesh().its.indices.size();
    if (tris == 0 || tris > FOS_MAX_TRIS)
        return {};

    Vec3d rotation = mi->get_rotation();
    rotation.z()   = 0.;
    Geometry::Transformation t(mi->get_transformation());
    t.set_offset(mi->get_offset().z() * Vec3d::UnitZ());
    t.set_rotation(rotation);

    const double cos_thr = std::cos(threshold_deg * PI / 180.);
    Polygons     tri_polys;
    for (const ModelVolume *mv : mo->volumes) {
        if (!mv->is_model_part())
            continue;
        const Transform3d M    = t.get_matrix() * mv->get_matrix();
        const double      flip = M.linear().determinant() < 0. ? -1. : 1.; // mirrored: faces turn inside out
        const indexed_triangle_set &its = mv->mesh().its;
        for (const stl_triangle_vertex_indices &f : its.indices) {
            const Vec3d v0 = M * its.vertices[f[0]].cast<double>();
            const Vec3d v1 = M * its.vertices[f[1]].cast<double>();
            const Vec3d v2 = M * its.vertices[f[2]].cast<double>();
            Vec3d n = (v1 - v0).cross(v2 - v0) * flip;
            const double len = n.norm();
            if (len <= 0.)
                continue;
            n /= len;
            if (-n.z() <= cos_thr || std::min({v0.z(), v1.z(), v2.z()}) < FOS_BED_Z_MM)
                continue;
            Polygon tp;
            tp.points = {Point::new_scale(v0.x(), v0.y()), Point::new_scale(v1.x(), v1.y()), Point::new_scale(v2.x(), v2.y())};
            tri_polys.emplace_back(std::move(tp));
        }
    }
    return to_polygons(union_ex(tri_polys));
}

// Merge the outline and its support into ONE contour for the placer. Tree bases often stand
// apart from the model, so close the gaps (offset out then back in) with a growing radius; if
// it still will not join, fall back to the convex hull of everything. Holes are dropped.
static Polygon fos_single_contour(const Polygons &polys)
{
    ExPolygons u = union_ex(polys);
    if (u.size() > 1)
        for (double r_mm : {2., 4., 8., 16.}) {
            const float r = float(scaled<double>(r_mm));
            u             = offset_ex(offset_ex(u, r), -r);
            if (u.size() <= 1)
                break;
        }
    if (u.size() == 1)
        return u.front().contour;
    Points pts;
    for (const Polygon &p : polys)
        append(pts, p.points);
    return Geometry::convex_hull(pts);
}

static Polygon fos_thin(Polygon c)
{
    static constexpr size_t FOS_MAX_POINTS = 256;
    double tol = scaled<double>(0.2);
    for (int pass = 0; pass < 4 && c.points.size() > FOS_MAX_POINTS; ++pass, tol *= 2.) {
        Polygon best;
        double  best_area = 0.;
        for (Polygon &s : c.simplify(tol))
            if (std::abs(s.area()) > best_area) {
                best_area = std::abs(s.area());
                best      = std::move(s);
            }
        if (best.points.size() >= 3)
            c = std::move(best);
    }
    c.make_counter_clockwise();
    return c;
}

// FOS 8.6.6: Orca's own auto-brim rule, reused for objects Nest has no sliced brim for (s37:
// objects waiting off the plate are never sliced, and a flat brim_width estimate padded every
// 27 mm cube by 5 mm although auto brim gives them none). Both are defined in libslic3r without
// a header declaration: Model.cpp and Brim.cpp.
// (declared at namespace Slic3r scope near the top of this file)

// Auto brim width (mm) as Brim.cpp would give it, from the first layer sliced out of the mesh.
// Mirrors make_brim's per-part rule too: a part over 10 mm tall whose first layer is thinner
// than 2 x 1.1 mm (area / perimeter) and would get no brim gets an extra 5 mm.
static double fos_auto_brim_mm(const ModelInstance *mi, const DynamicPrintConfig &cfg)
{
    static constexpr double FOS_HALF_MIN_ADH_MM = 1.1;
    static constexpr double FOS_ADDITIONAL_MM   = 5.;
    static constexpr double FOS_FLOW_WIDTH_MM   = 0.45;

    const ModelObject *mo = mi->get_object();
    std::vector<ModelVolume *> parts;
    for (ModelVolume *mv : mo->volumes)
        if (mv->is_model_part())
            parts.push_back(mv);
    if (parts.empty())
        return 0.;

    const ConfigOption *flh = cfg.option("initial_layer_print_height");
    const float z = float(0.5 * (flh != nullptr ? flh->getFloat() : 0.2));
    ExPolygons first;
    for (ModelVolume *mv : parts) {
        MeshSlicingParamsEx params;
        params.trafo = mi->get_matrix() * mv->get_matrix();
        std::vector<ExPolygons> s = slice_mesh_ex(mv->mesh().its, {z}, params);
        if (!s.empty())
            append(first, std::move(s.front()));
    }
    first = union_ex(first);
    if (first.empty())
        return 0.;

    double height = 0.;
    double w = configBrimWidthByVolumeGroups(getadhesionCoeff(mo->volumes), Model::findMaxSpeed(mo), parts, first, height);
    if (w < 5. && height > 10.)
        for (const ExPolygon &ex : first)
            if (unscale<double>(ex.area() / ex.contour.length()) < FOS_HALF_MIN_ADH_MM && w < FOS_FLOW_WIDTH_MM) {
                w += FOS_ADDITIONAL_MM;
                break;
            }
    return w;
}

// FOS 8.6.6: Nest remembers what it last read from a good slice, per instance, keyed by a
// fingerprint of what that footprint depends on. The slice goes stale on changes that do not
// touch a given object's support or brim (s37: changing a support setting also re-runs the
// brim of every object, and the auto-brim estimate then padded cubes that have no brim at all).
// Footprints are in the Nest frame, so moving or Z-rotating an object keeps them valid.
struct FosExtrasCacheEntry
{
    std::string key;
    Polygons    polys;
};
static std::map<std::pair<size_t, size_t>, FosExtrasCacheEntry> s_fos_support_cache, s_fos_brim_cache;

// Fingerprint: this instance's shape in the Nest frame (volumes, their transforms, the instance
// transform without XY offset and Z rotation) plus every resolved option whose key starts with
// one of `prefixes` (object override first, then global).
static std::string fos_extras_key(const ModelInstance *mi, const DynamicPrintConfig &cfg, std::initializer_list<const char *> prefixes)
{
    const ModelObject *mo = mi->get_object();
    std::ostringstream ss;
    ss.precision(6);

    Vec3d rotation = mi->get_rotation();
    rotation.z()   = 0.;
    Geometry::Transformation t(mi->get_transformation());
    t.set_offset(Vec3d::Zero());
    t.set_rotation(rotation);
    const Transform3d T = t.get_matrix();
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            ss << T(r, c) << ',';
    for (const ModelVolume *mv : mo->volumes) {
        ss << '|' << int(mv->type()) << ':' << mv->mesh().its.indices.size() << ':' << mv->mesh().its.vertices.size();
        const Transform3d &V = mv->get_matrix();
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                ss << ',' << V(r, c);
    }
    for (const std::string &k : cfg.keys()) {
        bool match = false;
        for (const char *p : prefixes)
            if (boost::starts_with(k, p))
                match = true;
        if (!match)
            continue;
        const ConfigOption *o = mo->config.has(k) ? mo->config.option(k) : cfg.option(k);
        if (o != nullptr)
            ss << '|' << k << '=' << o->serialize();
    }
    return ss.str();
}

// Widen an instance's Nest polygon for its support and brim.
// Preferred source is the last slice of the current plate - the real support (grid and tree
// both spread past the model at the support spots, s37) plus the real object and support brim.
// Support and brim are validated separately (posSupportMaterial / psSkirtBrim), since moving an
// object re-runs the brim but keeps its support. Anything not covered by a current slice is
// estimated and counted, so the user can be told to slice and Nest again:
//  - support: the overhang projection padded by FOS_TREE_PAD_MM (tree bases start away from the
//    model, s37 measured 9-10 mm) or support_expansion + FOS_GRID_PAD_MM (grid). An object with
//    no overhang past its threshold angle gets no support padding at all.
//  - brim: brim_object_gap + brim_width for any outer brim type (auto uses brim_width as a
//    stand-in; the real auto width is only known after slicing).
static void fos_nest_add_extras(const ModelInstance *mi, const DynamicPrintConfig &cfg, Polygon &contour, int &estimated)
{
    static constexpr double FOS_TREE_PAD_MM = 10.;
    static constexpr double FOS_GRID_PAD_MM = 3.;

    const ModelObject *mo = mi->get_object();
    // Read through the untyped interface: an enum loaded from a 3mf may be stored generic.
    const ConfigOption *en_opt  = fos_obj_opt<ConfigOption>(mo, cfg, "enable_support");
    const ConfigOption *typ_opt = fos_obj_opt<ConfigOption>(mo, cfg, "support_type");
    const ConfigOption *bt_opt  = fos_obj_opt<ConfigOption>(mo, cfg, "brim_type");
    const bool support_on = en_opt != nullptr && en_opt->getBool() && typ_opt != nullptr;
    const BrimType brim   = bt_opt != nullptr ? BrimType(bt_opt->getInt()) : btNoBrim;
    const bool brim_on    = brim != btNoBrim && brim != btInnerOnly;
    if (!support_on && !brim_on)
        return;

    const PrintInstance *pi    = nullptr;
    const PrintObject   *po    = fos_print_object(mi, pi);
    Print               &print = wxGetApp().plater()->get_partplate_list().get_current_fff_print();
    const bool support_sliced  = support_on && po != nullptr && po->is_step_done(posSupportMaterial);
    const bool brim_sliced     = brim_on && po != nullptr && pi != nullptr && print.is_step_done(psSkirtBrim);

    Polygons parts{contour};
    bool     used_estimate = false;
    const std::pair<size_t, size_t> cache_id{mo->id().id, mi->id().id};

    if (support_on) {
        const std::string key = fos_extras_key(mi, cfg, {"support", "tree_support", "enable_support", "raft", "layer_height", "initial_layer"});
        auto              hit = s_fos_support_cache.find(cache_id);
        if (support_sliced) {
            Polygons sup;
            fos_sliced_support(po, sup);
            s_fos_support_cache[cache_id] = {key, sup};
            append(parts, std::move(sup));
        } else if (hit != s_fos_support_cache.end() && hit->second.key == key) {
            append(parts, hit->second.polys);
        } else {
            const bool tree = is_tree(SupportType(typ_opt->getInt()));
            const ConfigOption *thr = fos_obj_opt<ConfigOption>(mo, cfg, "support_threshold_angle");
            const ConfigOption *exp = fos_obj_opt<ConfigOption>(mo, cfg, "support_expansion");
            const double thr_deg = (thr != nullptr && thr->getInt() > 0) ? double(thr->getInt()) : 30.;
            const double pad     = tree ? FOS_TREE_PAD_MM : std::max(0., exp != nullptr ? exp->getFloat() : 0.) + FOS_GRID_PAD_MM;
            const Polygons over  = fos_overhang_projection(mi, thr_deg);
            if (!over.empty())
                append(parts, offset(over, float(scaled<double>(pad))));
            used_estimate = true;
        }
    }

    if (brim_on) {
        // The auto brim also depends on the filament (adhesion) and speeds; a change there
        // re-slices the brim anyway, and the cache is refreshed on the next good slice.
        const std::string key = fos_extras_key(mi, cfg, {"brim", "enable_support", "support_type"});
        auto              hit = s_fos_brim_cache.find(cache_id);
        if (brim_sliced) {
            BoundingBox near_bb = get_extents(parts);
            near_bb.offset(scaled<double>(25.)); // max auto brim 20 mm + gap
            Polygons brim_polys;
            fos_sliced_brim(po, pi, near_bb, brim_polys);
            s_fos_brim_cache[cache_id] = {key, brim_polys};
            append(parts, std::move(brim_polys));
        } else if (hit != s_fos_brim_cache.end() && hit->second.key == key) {
            append(parts, hit->second.polys);
        } else {
            const ConfigOption *bw  = fos_obj_opt<ConfigOption>(mo, cfg, "brim_width");
            const ConfigOption *gap = fos_obj_opt<ConfigOption>(mo, cfg, "brim_object_gap");
            const double width = brim == btAutoBrim ? fos_auto_brim_mm(mi, cfg) : (bw != nullptr ? bw->getFloat() : 0.);
            if (width > 0.)
                append(parts, offset(contour, float(scaled<double>(width + (gap != nullptr ? gap->getFloat() : 0.)))));
            used_estimate = true;
        }
    }

    if (used_estimate)
        ++estimated;
    if (parts.size() > 1)
        contour = fos_thin(fos_single_contour(parts));

}

// FOS 8.6.6: a kept convex hull can be very dense (a sphere projects to ~180 points), and
// every NFP union the placer does scales with it. Thin it to a few dozen points. Simplify may
// cut inward by up to the tolerance; 0.2 mm is well inside the smallest Nest half-gap (1 mm).
static void fos_nest_thin_hull(Polygon &p)
{
    static constexpr size_t FOS_HULL_MAX_POINTS = 48;
    if (p.points.size() <= FOS_HULL_MAX_POINTS)
        return;
    for (double tol_mm : {0.1, 0.2}) {
        Polygons s = p.simplify(scaled<double>(tol_mm));
        if (s.size() == 1 && s.front().points.size() >= 3) {
            Polygon h = Geometry::convex_hull(s.front().points);
            if (h.points.size() >= 3) {
                p = std::move(h);
                if (p.points.size() <= FOS_HULL_MAX_POINTS)
                    return;
            }
        }
    }
}

// FOS 8.6.6: Nest. Like prepare_partplate(), except only SELECTED printable instances are
// arrangeable. Unselected instances touching the current plate become fixed obstacles in
// plate-local coordinates with no setter, so finalize() can never move them. Instances on
// other plates are left out entirely and are therefore untouched.
void ArrangeJob::prepare_fos_nest()
{
    clear_input();

    PartPlateList& plate_list = m_plater->get_partplate_list();
    PartPlate*     plate      = plate_list.get_curr_plate();
    current_plate_index       = plate_list.get_curr_plate_index();
    assert(plate != nullptr);

    if (plate->is_locked()) {
        m_plater->get_notification_manager()->push_notification(NotificationType::BBLPlateInfo,
            NotificationManager::NotificationLevel::WarningNotificationLevel, into_u8(_L("This plate is locked.\nCannot auto-arrange on this plate.")));
        return;
    }

    Model& model = m_plater->model();

    std::vector<const Selection::InstanceIdxsList*> obj_sel(model.objects.size(), nullptr);
    for (auto& s : m_plater->get_selection().get_content())
        if (s.first >= 0 && s.first < int(obj_sel.size()))
            obj_sel[size_t(s.first)] = &s.second;

    const Vec3d origin = plate->get_origin();
    const DynamicPrintConfig fos_cfg = wxGetApp().preset_bundle->full_config();
    m_fos_tree_unsliced = 0;

    for (size_t oidx = 0; oidx < model.objects.size(); ++oidx) {
        ModelObject* mo = model.objects[oidx];
        for (size_t inst_idx = 0; inst_idx < mo->instances.size(); ++inst_idx) {
            ModelInstance* mi       = mo->instances[inst_idx];
            const bool     selected = obj_sel[oidx] != nullptr && obj_sel[oidx]->count(int(inst_idx)) > 0;
            const bool     in_plate = plate->contain_instance(oidx, inst_idx) || plate->intersect_instance(oidx, inst_idx);

            if (selected && mi->printable) {
                ArrangePolygon ap = prepare_arrange_polygon(mi);
                if (!fos_nest_silhouette(mi, ap.poly.contour))
                    fos_nest_thin_hull(ap.poly.contour);
                fos_nest_add_extras(mi, fos_cfg, ap.poly.contour, m_fos_tree_unsliced);
                ap.itemid         = m_selected.size();
                m_selected.emplace_back(std::move(ap));
            } else if (in_plate) {
                ArrangePolygon ap = prepare_arrange_polygon(mi);
                if (!fos_nest_silhouette(mi, ap.poly.contour))
                    fos_nest_thin_hull(ap.poly.contour);
                fos_nest_add_extras(mi, fos_cfg, ap.poly.contour, m_fos_tree_unsliced);
                ap.translation(X) -= scaled<double>(origin.x());
                ap.translation(Y) -= scaled<double>(origin.y());
                ap.bed_idx = 0;
                ap.setter  = nullptr; // FOS: obstacle only, never moved
                ap.itemid  = m_unselected.size();
                m_unselected.emplace_back(std::move(ap));
            }
        }
    }

    if (m_selected.empty())
        m_plater->get_notification_manager()->push_notification(NotificationType::BBLPlateInfo,
            NotificationManager::NotificationLevel::WarningNotificationLevel, into_u8(_L("No arrangeable objects are selected.")));

    if (auto wti = get_wipe_tower(*m_plater, current_plate_index)) {
        ArrangePolygon&& ap = get_wipetower_arrange_poly(&wti);
        m_unselected.emplace_back(std::move(ap));
    }

    plate_list.preprocess_exclude_areas(m_unselected, current_plate_index + 1);
}

//BBS: add partplate logic
void ArrangeJob::prepare()
{
    m_plater->get_notification_manager()->push_notification(NotificationType::ArrangeOngoing,
        NotificationManager::NotificationLevel::RegularNotificationLevel, _u8L("Arranging..."));
    m_plater->get_notification_manager()->bbl_close_plateinfo_notification();

    params = init_arrange_params(m_plater);

    //BBS update extruder params and speed table before arranging
    const Slic3r::DynamicPrintConfig& config = wxGetApp().preset_bundle->full_config();
    auto& print = wxGetApp().plater()->get_partplate_list().get_current_fff_print();
    auto print_config = print.config();
    int numExtruders = wxGetApp().preset_bundle->filament_presets.size();

    Model::setExtruderParams(config, numExtruders);
    Model::setPrintSpeedTable(config, print_config);

    int state = m_plater->get_prepare_state();
    if (m_fos_nest) {
        // FOS 8.6.6: Nest always rotates, whatever the arrange dialog says
        params.allow_rotations = true;
        params.fos_concave_nfp = true;
        only_on_partplate      = true;
        prepare_fos_nest();
    }
    else if (state == Job::JobPrepareState::PREPARE_STATE_DEFAULT) {
        only_on_partplate = false;
        prepare_all();
    }
    else if (state == Job::JobPrepareState::PREPARE_STATE_MENU) {
        only_on_partplate = true;   // only arrange items on current plate
        prepare_partplate();
    }


#if SAVE_ARRANGE_POLY
    if (1)
    { // subtract excluded region and get a polygon bed
        auto& print = wxGetApp().plater()->get_partplate_list().get_current_fff_print();
        auto print_config = print.config();
        bed_poly.points = get_bed_shape(*m_plater->config());
        Polygons exclude_polys = get_bed_excluded_area(print_config);
        bed_poly = diff({ bed_poly }, exclude_polys)[0];
    }

    BoundingBox bbox = bed_poly.bounding_box();
    Point center = bbox.center();
    auto polys_to_draw = m_selected;
    for (auto it = polys_to_draw.begin(); it != polys_to_draw.end(); it++) {
        it->poly.translate(center);
        bbox.merge(it->poly);
    }
    SVG svg("SVG/arrange_poly.svg", bbox);
    if (svg.is_opened()) {
        svg.draw_outline(bed_poly);
        //svg.draw_grid(bbox, "gray", scale_(0.05));
        std::vector<std::string> color_array = { "red","black","yellow","gree","blue" };
        for (auto it = polys_to_draw.begin(); it != polys_to_draw.end(); it++) {
            std::string color = color_array[(it - polys_to_draw.begin()) % color_array.size()];
            svg.add_comment(it->name);
            svg.draw_text(get_extents(it->poly).min, it->name.c_str(), color.c_str());
            svg.draw_outline(it->poly, color);
        }
    }
#endif

    check_unprintable();
}

void ArrangeJob::check_unprintable()
{
    for (auto it = m_selected.begin(); it != m_selected.end();) {
        if (it->poly.area() < 0.001 || it->height>params.printable_height)
        {
#if SAVE_ARRANGE_POLY
            SVG svg(data_dir() + "/SVG/arrange_unprintable_"+it->name+".svg", get_extents(it->poly));
            if (svg.is_opened())
                svg.draw_outline(it->poly);
#endif
            if (it->poly.area() < 0.001) {
                auto msg = (boost::format(
                    _utf8("Object %s has zero size and can't be arranged."))
                    % _utf8(it->name)).str();
                m_plater->get_notification_manager()->push_notification(NotificationType::BBLPlateInfo,
                    NotificationManager::NotificationLevel::WarningNotificationLevel, msg);
            }
            m_unprintable.push_back(*it);
            it = m_selected.erase(it);
        }
        else
            it++;
    }
}

void ArrangeJob::process(Ctl &ctl)
{
    static const auto arrangestr = _u8L("Arranging");
    ctl.update_status(0, arrangestr);
    ctl.call_on_main_thread([this]{ prepare(); }).wait();;

    auto & partplate_list = m_plater->get_partplate_list();

    const Slic3r::DynamicPrintConfig& global_config = wxGetApp().preset_bundle->full_config();
    PresetBundle* preset_bundle = wxGetApp().preset_bundle;
    const bool is_bbl = wxGetApp().preset_bundle->is_bbl_vendor();
    if (is_bbl && params.avoid_extrusion_cali_region && global_config.opt_bool("scan_first_layer"))
        partplate_list.preprocess_nonprefered_areas(m_unselected, MAX_NUM_PLATES);

    update_arrange_params(params, m_plater->config(), m_selected);
    update_selected_items_inflation(m_selected, m_plater->config(), params);
    update_unselected_items_inflation(m_unselected, m_plater->config(), params);
    update_selected_items_axis_align(m_selected, m_plater->config(), params);

    // FOS 8.6.6: Nest uses ONE fixed object gap chosen from the menu (Close / Loose), for the
    // selected objects and for fixed objects on the plate alike. The stock padding is replaced,
    // not capped: it is brim-driven and grows to 12 mm per side (selected) and 24 mm (fixed)
    // with tree support enabled whether or not an object grows a single branch. Virtual regions
    // and the wipe tower keep their stock padding. By-object printing keeps stock, where the
    // gap is head clearance.
    if (m_fos_nest && !params.is_seq_print) {
        const coord_t half_gap = coord_t(scaled(m_fos_nest_gap_mm / 2.));
        for (ArrangePolygon &ap : m_selected)
            ap.inflation = half_gap;
        for (ArrangePolygon &ap : m_unselected)
            if (!ap.is_virt_object)
                ap.inflation = half_gap;
        // Keep every object at least FOS_NEST_EDGE_MM from the bed edge (the U1 spiral-lift
        // boundary warning, 3DScene.cpp SPIRAL_LIFT_SAFETY_MARGIN = 3.5). The inflated outline
        // touches the bin edge, so the bin itself only needs to shrink by the remainder.
        static constexpr double FOS_NEST_EDGE_MM = 3.6;
        const double edge = std::max(0., FOS_NEST_EDGE_MM - m_fos_nest_gap_mm / 2.);
        params.bed_shrink_x = std::max(params.bed_shrink_x, float(edge));
        params.bed_shrink_y = std::max(params.bed_shrink_y, float(edge));
    }

    Points      bedpts = get_shrink_bedpts(m_plater->config(),params);

    partplate_list.preprocess_exclude_areas(params.excluded_regions, 1, scale_(1));

    BOOST_LOG_TRIVIAL(debug) << "arrange bedpts:" << bedpts[0].transpose() << ", " << bedpts[1].transpose() << ", " << bedpts[2].transpose() << ", " << bedpts[3].transpose();

    params.stopcondition = [&ctl]() { return ctl.was_canceled(); };

    params.progressind = [this, &ctl](unsigned num_finished, std::string str = "") {
        ctl.update_status(num_finished * 100 / status_range(), _u8L("Arranging") + str);
    };

    {
        BOOST_LOG_TRIVIAL(warning)<< "Arrange full params: "<< params.to_json();
        BOOST_LOG_TRIVIAL(info) << boost::format("arrange: items selected before arranging: %1%") % m_selected.size();
        for (auto selected : m_selected) {
            BOOST_LOG_TRIVIAL(debug) << selected.name << ", extruder: " << selected.extrude_ids.back() << ", bed: " << selected.bed_idx << ", filemant_type:" << selected.filament_temp_type
                << ", trans: " << selected.translation.transpose();
        }
        BOOST_LOG_TRIVIAL(debug) << "arrange: items unselected before arrange: " << m_unselected.size();
        for (auto item : m_unselected)
            BOOST_LOG_TRIVIAL(debug) << item.name << ", bed: " << item.bed_idx << ", trans: " << item.translation.transpose()
            <<", bbox:"<<get_extents(item.poly).min.transpose()<<","<<get_extents(item.poly).max.transpose();
    }

    const auto fos_t0 = std::chrono::steady_clock::now();
    if (!m_fos_nest) {
        arrangement::arrange(m_selected, m_unselected, bedpts, params);
    } else {
        // FOS 8.6.6: Nest search, after Deepnest's approach (github.com/Jack000/Deepnest; the
        // idea only, no code taken). The placer is greedy - it puts each object in the best
        // spot for that object, in one fixed order (largest first), and never revisits, so an
        // unlucky order strands objects that a person can still fit by hand. Re-run the same
        // placement with the order perturbed (adjacent swaps, as Deepnest's mutation) and keep
        // the best result: most objects on the plate first, then the smallest pile footprint.
        // The order is imposed through ArrangePolygon::priority, the first key of the
        // placer's sort. Fixed seed, so the same scene nests the same way every time.
        static constexpr double FOS_NEST_BUDGET_S  = 6.0;  // stop searching after this long
        static constexpr double FOS_NEST_POLISH_S  = 2.0;  // once all fit, polish this long at most
        static constexpr int    FOS_NEST_MAX_TRIES = 60;
        static constexpr double FOS_NEST_SWAP_P    = 0.3;

        const ArrangePolygons base = m_selected;
        const size_t          n    = base.size();

        // Largest first, the placer's own default.
        std::vector<size_t> order0(n);
        std::iota(order0.begin(), order0.end(), size_t(0));
        std::stable_sort(order0.begin(), order0.end(), [&base](size_t a, size_t b) {
            return std::abs(base[a].poly.area()) > std::abs(base[b].poly.area());
        });

        auto score = [](const ArrangePolygons &res, int &placed, double &footprint) {
            placed = 0;
            BoundingBox bb;
            for (const ArrangePolygon &ap : res)
                if (ap.bed_idx == 0) {
                    ++placed;
                    bb.merge(get_extents(ap.transformed_poly()));
                }
            footprint = bb.defined ? double(bb.size().x()) * double(bb.size().y()) : 0.;
        };

        std::mt19937    rng(0x464F53u);
        ArrangePolygons best;
        int             best_placed = -1;
        double          best_fp     = 0.;
        double          all_fit_at  = -1.;
        for (int attempt = 0; attempt < FOS_NEST_MAX_TRIES && !ctl.was_canceled(); ++attempt) {
            std::vector<size_t> order = order0;
            if (attempt > 1) { // attempts 0 and 1: the stock order on each gravity axis
                std::bernoulli_distribution swap(FOS_NEST_SWAP_P);
                for (size_t i = 0; i + 1 < n; ++i)
                    if (swap(rng))
                        std::swap(order[i], order[i + 1]);
            }
            ArrangePolygons trial = base;
            for (size_t rank = 0; rank < n; ++rank)
                trial[order[rank]].priority = int(n - rank); // higher priority is placed first

            // Alternate the gravity axis so the search also tries packing the other way.
            params.fos_gravity_axis = attempt % 2;
            arrangement::arrange(trial, m_unselected, bedpts, params);

            int    placed;
            double fp;
            score(trial, placed, fp);
            if (placed > best_placed || (placed == best_placed && fp < best_fp)) {
                best             = std::move(trial);
                best_placed      = placed;
                best_fp          = fp;
            }

            const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - fos_t0).count();
            if (best_placed == int(n) && all_fit_at < 0.)
                all_fit_at = t;
            if (t > FOS_NEST_BUDGET_S || (all_fit_at >= 0. && t - all_fit_at > FOS_NEST_POLISH_S))
                break;
        }
        // Restore the stock priorities; only the placement result is kept.
        for (size_t i = 0; i < n && i < best.size(); ++i)
            best[i].priority = base[i].priority;
        if (!best.empty())
            m_selected = std::move(best);
    }


    // sort by item id
    std::sort(m_selected.begin(), m_selected.end(), [](auto a, auto b) {return a.itemid < b.itemid; });
    {
        BOOST_LOG_TRIVIAL(info) << boost::format("arrange: items selected after arranging: %1%") % m_selected.size();
        for (auto selected : m_selected)
            BOOST_LOG_TRIVIAL(debug) << selected.name << ", extruder: " << selected.extrude_ids.back() << ", bed: " << selected.bed_idx
                                     << ", bed_temp: " << selected.first_bed_temp << ", print_temp: " << selected.print_temp
                                     << ", trans: " << unscale<double>(selected.translation(X)) << ","<< unscale<double>(selected.translation(Y));
        BOOST_LOG_TRIVIAL(debug) << "arrange: items unselected after arrange: "<< m_unselected.size();
        for (auto item : m_unselected)
            BOOST_LOG_TRIVIAL(debug) << item.name << ", bed: " << item.bed_idx << ", trans: " << item.translation.transpose();
    }

    // put unpackable items to m_unprintable so they goes outside
    bool we_have_unpackable_items = false;
    for (auto item : m_selected) {
        if (item.bed_idx < 0) {
            //BBS: already processed in m_selected
            //m_unprintable.push_back(std::move(item));
            we_have_unpackable_items = true;
        }
    }

    // finalize just here.
    ctl.update_status(100,
        ctl.was_canceled() ? _u8L("Arranging canceled.") :
        we_have_unpackable_items ? _u8L("Arranging is done but there are unpacked items. Reduce spacing and try again.") : _u8L("Arranging done."));
}

ArrangeJob::ArrangeJob(bool fos_nest, double fos_nest_gap_mm)
    : m_plater{wxGetApp().plater()}, m_fos_nest{fos_nest}, m_fos_nest_gap_mm{fos_nest_gap_mm} { }

static std::string concat_strings(const std::set<std::string> &strings,
                                  const std::string &delim = "\n")
{
    return std::accumulate(
        strings.begin(), strings.end(), std::string(""),
        [delim](const std::string &s, const std::string &name) {
            return s + name + delim;
        });
}

void ArrangeJob::finalize(bool canceled, std::exception_ptr &eptr) {
    try {
        if (eptr)
            std::rethrow_exception(eptr);
    } catch (libnest2d::GeometryException &) {
        show_error(m_plater, _(L("Arrange failed. "
                                 "Found some exceptions when processing object geometries.")));
        eptr = nullptr;
    } catch (...) {
        eptr = std::current_exception();
    }

    if (canceled || eptr)
        return;

    // Unprintable items go to the last virtual bed
    int beds = 0;

    //BBS: partplate
    PartPlateList& plate_list = m_plater->get_partplate_list();
    //clear all the relations before apply the arrangement results
    if (only_on_partplate) {
        plate_list.clear(false, false, true, current_plate_index);
    }
    else
        plate_list.clear(false, false, true, -1);
    //BBS: adjust the bed_index, create new plates, get the max bed_index
    for (ArrangePolygon& ap : m_selected) {
        //if (ap.bed_idx < 0) continue;  // bed_idx<0 means unarrangable
        //BBS: partplate postprocess
        if (only_on_partplate)
            plate_list.postprocess_bed_index_for_current_plate(ap);
        else
            plate_list.postprocess_bed_index_for_selected(ap);

        beds = std::max(ap.bed_idx, beds);

        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": arrange selected %4%: bed_id %1%, trans {%2%,%3%}") % ap.bed_idx % unscale<double>(ap.translation(X)) % unscale<double>(ap.translation(Y)) % ap.name;
    }

    //BBS: adjust the bed_index, create new plates, get the max bed_index
    for (ArrangePolygon& ap : m_unselected)
    {
        if (ap.is_virt_object)
            continue;

        //BBS: partplate postprocess
        if (!only_on_partplate)
            plate_list.postprocess_bed_index_for_unselected(ap);

        beds = std::max(ap.bed_idx, beds);
        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(":arrange unselected %4%: bed_id %1%, trans {%2%,%3%}") % ap.bed_idx % unscale<double>(ap.translation(X)) % unscale<double>(ap.translation(Y)) % ap.name;
    }

    for (ArrangePolygon& ap : m_locked) {
        beds = std::max(ap.bed_idx, beds);

        plate_list.postprocess_arrange_polygon(ap, false);

        ap.apply();
    }

    // Apply the arrange result to all selected objects
    for (ArrangePolygon& ap : m_selected) {
        //BBS: partplate postprocess
        plate_list.postprocess_arrange_polygon(ap, true);

        ap.apply();
    }

    // Apply the arrange result to unselected objects(due to the sukodu-style column changes, the position of unselected may also be modified)
    for (ArrangePolygon& ap : m_unselected)
    {
        if (ap.is_virt_object)
            continue;

        //BBS: partplate postprocess
        plate_list.postprocess_arrange_polygon(ap, false);

        ap.apply();
    }

    // Move the unprintable items to the last virtual bed.
    // Note ap.apply() moves relatively according to bed_idx, so we need to subtract the orignal bed_idx
    for (ArrangePolygon& ap : m_unprintable) {
        ap.bed_idx = beds + 1;
        plate_list.postprocess_arrange_polygon(ap, true);

        ap.apply();
        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(":arrange m_unprintable: name: %4%, bed_id %1%, trans {%2%,%3%}") % ap.bed_idx % unscale<double>(ap.translation(X)) % unscale<double>(ap.translation(Y)) % ap.name;
    }

    m_plater->update();
    // BBS
    //wxGetApp().obj_manipul()->set_dirty();

    if (!m_unarranged.empty()) {
        std::set<std::string> names;
        for (ModelInstance *mi : m_unarranged)
            names.insert(mi->get_object()->name);

        m_plater->get_notification_manager()->push_notification(GUI::format(
            _L("Arrangement ignored the following objects which can't fit into a single bed:\n%s"),
            concat_strings(names, "\n")));
    }
    m_plater->get_notification_manager()->close_notification_of_type(NotificationType::ArrangeOngoing);

    // FOS 8.6.6: support / brim of some objects was estimated, not taken from a slice
    if (m_fos_nest && m_fos_tree_unsliced > 0)
        m_plater->get_notification_manager()->push_notification(NotificationType::BBLPlateInfo,
            NotificationManager::NotificationLevel::RegularNotificationLevel,
            GUI::format(_L("Nest estimated the support or brim of %1% object(s) because the plate has not been sliced since they changed. Slice the plate, then Nest again for an exact fit."), m_fos_tree_unsliced));

    //BBS: reload all objects due to arrange
    if (only_on_partplate) {
        plate_list.rebuild_plates_after_arrangement(!only_on_partplate, true, current_plate_index);
    }
    else {
        plate_list.rebuild_plates_after_arrangement(!only_on_partplate, true);
    }

    // unlock the plates we just locked
    for (int i : m_uncompatible_plates)
        plate_list.get_plate(i)->lock(false);

    // BBS: update slice context and gcode result.
    m_plater->update_slicing_context_to_current_partplate();

    wxGetApp().obj_list()->reload_all_plates();

    m_plater->update();

    m_plater->m_arrange_running.store(false);
}

std::optional<arrangement::ArrangePolygon>
get_wipe_tower_arrangepoly(const Plater &plater)
{
    int id = plater.canvas3D()->fff_print()->get_plate_index();
    if (auto wti = get_wipe_tower(plater, id))
        return get_wipetower_arrange_poly(&wti);

    return {};
}

//BBS: add sudoku-style stride
double bed_stride_x(const Plater* plater) {
    double bedwidth = plater->build_volume().bounding_box().size().x();
    return (1. + LOGICAL_BED_GAP) * bedwidth;
}

double bed_stride_y(const Plater* plater) {
    double beddepth = plater->build_volume().bounding_box().size().y();
    return (1. + LOGICAL_BED_GAP) * beddepth;
}

// call before get selected and unselected
arrangement::ArrangeParams init_arrange_params(Plater *p)
{
    arrangement::ArrangeParams         params;
    GLCanvas3D::ArrangeSettings       &settings     = p->canvas3D()->get_arrange_settings();
    auto                              &print        = wxGetApp().plater()->get_partplate_list().get_current_fff_print();
    const PrintConfig                 &print_config = print.config();

    auto [object_skirt_offset, object_skirt_witdh] = print.object_skirt_offset();

    params.clearance_height_to_rod             = print_config.extruder_clearance_height_to_rod.value;
    params.clearance_height_to_lid             = print_config.extruder_clearance_height_to_lid.value;
    params.clearance_radius                    = print_config.extruder_clearance_radius.value + object_skirt_offset * 2;
    params.object_skirt_offset                 = object_skirt_offset;
    params.printable_height                    = print_config.printable_height.value;
    params.allow_rotations                     = settings.enable_rotation;
    params.nozzle_height                       = print_config.nozzle_height.value;
    params.align_center                        = print_config.best_object_pos.value;
    params.allow_multi_materials_on_same_plate = settings.allow_multi_materials_on_same_plate;
    params.avoid_extrusion_cali_region         = settings.avoid_extrusion_cali_region;
    params.is_seq_print                        = settings.is_seq_print;
    params.min_obj_distance                    = scaled(settings.distance);
    params.align_to_y_axis                     = settings.align_to_y_axis;

    int state = p->get_prepare_state();
    if (state == Job::JobPrepareState::PREPARE_STATE_MENU) {
        PartPlateList &plate_list = p->get_partplate_list();
        PartPlate *    plate      = plate_list.get_curr_plate();
        bool plate_same_as_global = true;
        params.is_seq_print       = plate->get_real_print_seq(&plate_same_as_global) == PrintSequence::ByObject;
        // if plate's print sequence is not the same as global, the settings.distance is no longer valid, we set it to auto
        if (!plate_same_as_global)
            params.min_obj_distance = 0;
    }

    if (params.is_seq_print) {
        params.bed_shrink_x = BED_SHRINK_SEQ_PRINT;
        params.bed_shrink_y = BED_SHRINK_SEQ_PRINT;
    }
    return params;
}

}} // namespace Slic3r::GUI
