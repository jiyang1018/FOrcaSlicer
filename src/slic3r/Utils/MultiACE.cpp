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

} // namespace Slic3r
