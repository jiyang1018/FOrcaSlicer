#ifndef slic3r_MultiACE_hpp_
#define slic3r_MultiACE_hpp_

// FOS: print host for multiACE (github.com/decay71/multiACE), a Klipper add-on
// for the Snapmaker U1 that drives multiple Anycubic ACE units.
//
// Derives from OctoPrint only to reuse its host / apikey / cafile plumbing and
// make_url(); the wire format is entirely different. The upload is a bare
// multipart POST of the RAW slicer export to the store-only inbox, equivalent
// to the call multiACE documents:
//
//   curl -F "file=@model.gcode" http://<printer-ip>/multiace/api/preflight/inbox
//
// multiACE runs its preflight in the browser when the user picks the file up,
// so there is no start-print post-upload action. The inbox answers 409 for a
// file that already carries multiACE's processed markers, so never send G-code
// that has been through multiace post_process_virtual_toolheads.py.

#include <string>

#include <wx/string.h>

#include "OctoPrint.hpp"

namespace Slic3r {

class DynamicPrintConfig;
class Http;

class MultiACE : public OctoPrint
{
public:
    MultiACE(DynamicPrintConfig *config);
    ~MultiACE() override = default;

    const char* get_name() const override;

    bool     test(wxString &curl_msg) const override;
    wxString get_test_ok_msg() const override;
    wxString get_test_failed_msg(wxString &msg) const override;

    // Store-only drop point: nothing to start, so no post-upload actions.
    PrintHostPostUploadActions get_post_upload_actions() const override { return {}; }
    bool has_auto_discovery() const override { return false; }
    bool can_test() const override { return true; }

    bool upload(PrintHostUpload upload_data, ProgressFn progress_fn, ErrorFn error_fn, InfoFn info_fn) const override;

    // Paths are relative to the printer root; nginx maps /multiace/ onto the
    // multiACE backend on 127.0.0.1:7126, stripping the prefix.
    static std::string inbox_path()  { return "multiace/api/preflight/inbox"; }
    static std::string health_path() { return "multiace/api/health"; }
};

} // namespace Slic3r

#endif // slic3r_MultiACE_hpp_
