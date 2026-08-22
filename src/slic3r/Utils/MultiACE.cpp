#include "MultiACE.hpp"

#include <algorithm>
#include <sstream>
#include <exception>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <boost/filesystem/path.hpp>

#include <wx/progdlg.h>

#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/format.hpp"
#include "Http.hpp"
#include <set>
#include "nlohmann/json.hpp"

namespace fs = boost::filesystem;

namespace Slic3r {

MultiACE::MultiACE(DynamicPrintConfig *config) : OctoPrint(config) {}

const char* MultiACE::get_name() const { return "multiACE"; }

wxString MultiACE::get_test_ok_msg() const
{
    return _(L("Connection to multiACE is working correctly."));
}

wxString MultiACE::get_test_failed_msg(wxString &msg) const
{
    return GUI::format_wxstr("%s: %s\n\n%s"
        , _L("Could not connect to multiACE")
        , msg
        , _L("Note: the address must be the printer's LAN IP, and multiACE must be installed on it."));
}

bool MultiACE::test(wxString &msg) const
{
    const char *name = get_name();
    bool res = true;
    auto url = make_url(health_path());

    BOOST_LOG_TRIVIAL(info) << boost::format("%1%: Checking health at: %2%") % name % url;

    auto http = Http::get(std::move(url));
    set_auth(http);
    http.on_error([&](std::string body, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(error) << boost::format("%1%: Error getting health: %2%, HTTP %3%, body: `%4%`") % name % error % status % body;
            res = false;
            msg = format_error(body, error, status);
        })
        .on_complete([&](std::string body, unsigned) {
            BOOST_LOG_TRIVIAL(debug) << boost::format("%1%: Health answer: %2%") % name % body;
            // multiACE answers {"status":"ok","version":...}. Anything else on
            // this path means we reached some other service on that address.
            if (body.find("\"status\"") == std::string::npos) {
                res = false;
                msg = _L("The address answered, but not with multiACE. Check that multiACE is installed on this printer.");
            }
        })
#ifdef WIN32
        .ssl_revoke_best_effort(m_ssl_revoke_best_effort)
#endif
        .perform_sync();

    return res;
}

bool MultiACE::upload(PrintHostUpload upload_data, ProgressFn progress_fn, ErrorFn error_fn, InfoFn info_fn) const
{
    (void) info_fn;

    const char *name = get_name();
    const auto upload_filename = upload_data.upload_path.filename();
    std::string url = make_url(inbox_path());
    bool res = true;

    BOOST_LOG_TRIVIAL(info) << boost::format("%1%: Uploading file %2% to %3%, filename: %4%")
        % name % upload_data.source_path % url % upload_filename.string();

    auto http = Http::post(std::move(url));
    set_auth(http);
    // Field name and shape must match multiACE's documented curl exactly.
    http.form_add_file("file", upload_data.source_path.string(), upload_filename.string())
        .on_complete([&](std::string body, unsigned status) {
            BOOST_LOG_TRIVIAL(debug) << boost::format("%1%: File uploaded: HTTP %2%: %3%") % name % status % body;
        })
        .on_error([&](std::string body, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(error) << boost::format("%1%: Error uploading file: %2%, HTTP %3%, body: `%4%`") % name % error % status % body;
            if (status == 404 || status == 405) {
                // multiACE mounts its web UI as a catch-all at /, and StaticFiles
                // answers GET/HEAD only, so an unknown path returns 405 rather
                // than 404. Either code means this build predates the inbox.
                error_fn(GUI::format_wxstr(
                    _L("This printer is running a version of multiACE that has no send-to-multiACE "
                       "inbox (HTTP %s). Update multiACE on the printer to 0.99.7b or newer, then "
                       "try again."),
                    std::to_string(status)));
            } else {
                // multiACE returns {"detail": "..."} on every other rejection;
                // format_error surfaces the body so the server's own wording
                // reaches the user -- notably the 409 for G-code that has
                // already been through multiACE's post-processor.
                error_fn(format_error(body, error, status));
            }
            res = false;
        })
        .on_progress([&](Http::Progress progress, bool &cancel) {
            progress_fn(std::move(progress), cancel);
            if (cancel) {
                BOOST_LOG_TRIVIAL(info) << name << ": Upload canceled";
                res = false;
            }
        })
#ifdef WIN32
        .ssl_revoke_best_effort(m_ssl_revoke_best_effort)
#endif
        .perform_sync();

    return res;
}

// FOS: topology fetch. See TopologyReport in the header for the contract.
MultiACE::TopologyReport MultiACE::fetch_topology(std::string host)
{
    TopologyReport r;

    // mms_host was per-extruder up to v2.3.2-fos.8.5.7-beta.2 and a stored vector value
    // arrives verbatim as "a,a,a,a" - the same truncation fos_send_to_mms applies.
    const size_t comma = host.find(',');
    if (comma != std::string::npos)
        host.erase(comma);
    while (!host.empty() && (host.front() == ' ' || host.front() == '\t')) host.erase(host.begin());
    while (!host.empty() && (host.back()  == ' ' || host.back()  == '\t')) host.pop_back();
    if (host.empty()) {
        r.error = "no printer LAN IP set";
        return r;
    }

    std::string url;
    if (host.rfind("http://", 0) == 0 || host.rfind("https://", 0) == 0)
        url = host + (host.back() == '/' ? "" : "/") + livedata_path();
    else
        url = "http://" + host + "/" + livedata_path();

    BOOST_LOG_TRIVIAL(info) << boost::format("MultiACE: fetching topology from %1%") % url;

    std::string body;
    long        status = 0;
    bool        done   = false;
    auto http = Http::get(std::move(url));
    http.timeout_connect(3)
        .timeout_max(6)
        .on_complete([&](std::string b, unsigned s) { body = std::move(b); status = s; done = true; })
        .on_error([&](std::string b, std::string e, unsigned s) { body = std::move(b); r.error = std::move(e); status = s; })
        .perform_sync();

    if (status == 409) {
        // "preflight is disabled while a head is set to manual" - by design, not an error.
        r.manual_hold = true;
        return r;
    }
    if (!done) {
        if (r.error.empty())
            r.error = "no response";
        if (status != 0)
            r.error += " (HTTP " + std::to_string(status) + ")";
        return r;
    }

    try {
        const nlohmann::json j = nlohmann::json::parse(body);
        const nlohmann::json &ctx = j.at("head_ctx");

        r.mode = ctx.value("mode", "normal");
        if (r.mode == "single")
            r.mode = "multi";   // ace.py rewrites single to multi; mirror it
        if (r.mode != "normal" && r.mode != "multi" && r.mode != "head") {
            r.error = "unknown supply mode \"" + r.mode + "\"";
            r.mode.clear();
            return r;
        }

        // Which heads a unit drives. ace_heads is the list; the legacy single-ACE field
        // ace_head is the same fallback head_maps() itself applies. head_ace is identity-
        // padded outside head mode and deliberately never read here.
        std::vector<int> ace_heads;
        if (ctx.contains("ace_heads") && ctx["ace_heads"].is_array())
            for (const auto &v : ctx["ace_heads"])
                ace_heads.push_back(v.get<int>());
        if (ace_heads.empty() && r.mode == "head" && ctx.contains("ace_head"))
            ace_heads.push_back(ctx["ace_head"].get<int>());
        int head_n = 4;
        for (int h : ace_heads)
            head_n = std::max(head_n, h + 1);
        r.ace_driven_heads.assign(size_t(head_n), false);
        for (int h : ace_heads)
            if (h >= 0)
                r.ace_driven_heads[size_t(h)] = true;

        // Supply, head-indexed, read off Klipper's extruder objects. String keys.
        if (ctx.contains("head_nozzles") && ctx["head_nozzles"].is_object())
            for (const auto &el : ctx["head_nozzles"].items()) {
                const int idx = std::atoi(el.key().c_str());
                if (idx < 0 || idx > 15)
                    continue;
                if (int(r.head_nozzles.size()) <= idx)
                    r.head_nozzles.resize(size_t(idx) + 1, 0.);
                r.head_nozzles[size_t(idx)] = el.value().get<double>();
            }

        // Units seen. live_slots drops empty and non-identified slots, so this is a
        // LOWER BOUND on the connected count - the caller's dialog says so.
        std::set<int> units;
        if (j.contains("live_slots") && j["live_slots"].is_array())
            for (const auto &s : j["live_slots"])
                if (s.contains("ace") && s["ace"].is_number())
                    units.insert(s["ace"].get<int>());
        r.units_seen = int(units.size());

        r.ok = true;
    } catch (const std::exception &e) {
        r.error = std::string("unexpected reply: ") + e.what();
    }
    return r;
}

} // namespace Slic3r
