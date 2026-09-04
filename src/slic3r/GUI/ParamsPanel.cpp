///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.0-4761b0c)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Preset.hpp"
#include "ParamsPanel.hpp"
#include "Tab.hpp"
#include "PresetComboBoxes.hpp"
#include "libslic3r/LocalesUtils.hpp"
#include "format.hpp"
#include "MainFrame.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp"
#include "NotificationManager.hpp"

#include "Widgets/Label.hpp"
#include "Widgets/SwitchButton.hpp"
#include "Widgets/Button.hpp"
#include "GUI_Factories.hpp"


namespace Slic3r {
namespace GUI {


TipsDialog::TipsDialog(wxWindow *parent, const wxString &title, const wxString &description, std::string app_key)
    : DPIDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX),
    m_app_key(app_key)
{
    SetBackgroundColour(*wxWHITE);
    std::string icon_path = (boost::format("%1%/images/Snapmaker_OrcaTitle.ico") % resources_dir()).str();
    SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

    wxBoxSizer *m_sizer_main = new wxBoxSizer(wxVERTICAL);

    m_top_line = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_top_line->SetBackgroundColour(wxColour(166, 169, 170));

    m_sizer_main->Add(m_top_line, 0, wxEXPAND, 0);

    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, FromDIP(20));

    m_msg = new wxStaticText(this, wxID_ANY, description, wxDefaultPosition, wxDefaultSize, 0);
    m_msg->Wrap(-1);
    m_msg->SetFont(::Label::Body_13);
    m_msg->SetForegroundColour(wxColour(107, 107, 107));
    m_msg->SetBackgroundColour(wxColour(255, 255, 255));

    m_sizer_main->Add(m_msg, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(40));

    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, FromDIP(5));

    wxBoxSizer *m_sizer_bottom = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *m_sizer_left   = new wxBoxSizer(wxHORIZONTAL);

    auto dont_show_again = create_item_checkbox(_L("Don't show again"), this, _L("Don't show again"), "do_not_show_tips");
    m_sizer_left->Add(dont_show_again, 1, wxALL, FromDIP(5));

    m_sizer_bottom->Add(m_sizer_left, 1, wxEXPAND, FromDIP(5));

    wxBoxSizer *m_sizer_right = new wxBoxSizer(wxHORIZONTAL);

    m_confirm = new Button(this, _L("OK"));
    StateColor btn_bg_green(std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed), std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
                            std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));

    m_confirm->SetBackgroundColor(btn_bg_green);
    m_confirm->SetBorderColor(wxColour(0, 150, 136));
    m_confirm->SetTextColor(wxColour(255, 255, 255));
    m_confirm->SetSize(TIPS_DIALOG_BUTTON_SIZE);
    m_confirm->SetMinSize(TIPS_DIALOG_BUTTON_SIZE);
    m_confirm->SetCornerRadius(FromDIP(12));
    m_confirm->Bind(wxEVT_LEFT_DOWN, &TipsDialog::on_ok, this);
    m_sizer_right->Add(m_confirm, 0, wxALL, FromDIP(5));

    m_sizer_bottom->Add(m_sizer_right, 0, wxEXPAND, FromDIP(5));
    m_sizer_main->Add(m_sizer_bottom, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(40));
    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, FromDIP(20));

    SetSizer(m_sizer_main);
    Layout();
    Fit();
    Centre(wxBOTH);

    wxGetApp().UpdateDlgDarkUI(this);
}

wxBoxSizer *TipsDialog::create_item_checkbox(wxString title, wxWindow *parent, wxString tooltip, std::string param)
{
    wxBoxSizer *m_sizer_checkbox = new wxBoxSizer(wxHORIZONTAL);

    m_sizer_checkbox->Add(0, 0, 0, wxEXPAND | wxLEFT, 5);

    auto checkbox = new ::CheckBox(parent);
    m_sizer_checkbox->Add(checkbox, 0, wxALIGN_CENTER, 0);
    m_sizer_checkbox->Add(0, 0, 0, wxEXPAND | wxLEFT, 8);

    auto checkbox_title = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition, wxSize(-1, -1), 0);
    checkbox_title->SetForegroundColour(wxColour(144, 144, 144));
    checkbox_title->SetFont(::Label::Body_13);
    checkbox_title->Wrap(-1);
    m_sizer_checkbox->Add(checkbox_title, 0, wxALIGN_CENTER | wxALL, 3);

    m_show_again = wxGetApp().app_config->get(param) == "true" ? true : false;
    checkbox->SetValue(m_show_again);

    checkbox->Bind(wxEVT_TOGGLEBUTTON, [this, checkbox, param](wxCommandEvent &e) {
        m_show_again = m_show_again ? false : true;
        e.Skip();
    });

    return m_sizer_checkbox;
}

void TipsDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    if (m_confirm) m_confirm->SetMinSize(TIPS_DIALOG_BUTTON_SIZE);
    if (m_cancel) m_cancel->SetMinSize(TIPS_DIALOG_BUTTON_SIZE);
    Fit();
    Refresh();
}

void TipsDialog::on_ok(wxMouseEvent &event)
{
    if (m_show_again) {
        if (!m_app_key.empty())
        wxGetApp().app_config->set_bool(m_app_key, m_show_again);
    }
    EndModal(wxID_OK);
}

void ParamsPanel::Highlighter::set_timer_owner(wxEvtHandler *owner, int timerid /* = wxID_ANY*/)
{
    m_timer.SetOwner(owner, timerid);
}

void ParamsPanel::Highlighter::init(std::pair<wxWindow *, bool *> params, wxWindow *parent)
    {
    if (m_timer.IsRunning()) invalidate();
    if (!params.first || !params.second) return;

    m_timer.Start(300, false);

    m_bitmap         = params.first;
    m_show_blink_ptr = params.second;
    m_parent         = parent;

    *m_show_blink_ptr = true;
    }

void ParamsPanel::Highlighter::invalidate()
{
    m_timer.Stop();

    if (m_bitmap && m_show_blink_ptr) {
        *m_show_blink_ptr = false;
        m_bitmap->Show(*m_show_blink_ptr);
        if (m_parent) {
            m_parent->Layout();
            m_parent->Refresh();
        }
        m_show_blink_ptr = nullptr;
        m_bitmap         = nullptr;
        m_parent         = nullptr;
    }

    m_blink_counter = 0;
}

void ParamsPanel::Highlighter::blink()
{
    if (m_bitmap && m_show_blink_ptr) {
        *m_show_blink_ptr = !*m_show_blink_ptr;
        m_bitmap->Show(*m_show_blink_ptr);
        if (m_parent) {
            m_parent->Layout();
            m_parent->Refresh();
        }
    } else
        return;

    if ((++m_blink_counter) == 11) invalidate();
}

ParamsPanel::ParamsPanel( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style, const wxString& name )
    : wxPanel( parent, id, pos, size, style, name )
{
    // BBS: new layout
    SetBackgroundColour(*wxWHITE);
#if __WXOSX__
    m_top_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_top_sizer->SetSizeHints(this);
    this->SetSizer(m_top_sizer);

    // Create additional panel to Fit() it from OnActivate()
    // It's needed for tooltip showing on OSX
    m_tmp_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBK_LEFT | wxTAB_TRAVERSAL);
    auto  sizer = new wxBoxSizer(wxHORIZONTAL);
    m_tmp_panel->SetSizer(sizer);
    m_tmp_panel->Layout();

#else
    ParamsPanel*panel = this;
    m_top_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_top_sizer->SetSizeHints(panel);
    panel->SetSizer(m_top_sizer);
#endif //__WXOSX__

    if (dynamic_cast<Notebook*>(parent)) {
        // BBS: new layout
        m_top_panel = new StaticBox(this, wxID_ANY, wxDefaultPosition);
        m_top_panel->SetBackgroundColor(0xF8F8F8);
        m_top_panel->SetBackgroundColor2(0xF1F1F1);

        m_process_icon = new ScalableButton(m_top_panel, wxID_ANY, "process");

        m_title_label = new Label(m_top_panel, _L("Process"));

        //int width, height;
        // BBS: new layout
        m_mode_region = new SwitchButton(m_top_panel);
        m_mode_region->SetMaxSize({em_unit(this) * 12, -1});
        m_mode_region->SetLabels(_L("Global"), _L("Objects"));
        //m_mode_region->GetSize(&width, &height);
        m_tips_arrow = new ScalableButton(m_top_panel, wxID_ANY, "tips_arrow");
        m_tips_arrow->Hide();

        m_title_view = new Label(m_top_panel, Label::Body_12, _L("Advance")); // ORCA match size with advanced toggle on tab.cpp m_static_title
        m_mode_view = new SwitchButton(m_top_panel, wxID_ABOUT);

        // BBS: new layout
        //m_search_btn = new ScalableButton(m_top_panel, wxID_ANY, "search", wxEmptyString, wxDefaultSize, wxDefaultPosition, wxBU_EXACTFIT | wxNO_BORDER, true);
        //m_search_btn->SetToolTip(format_wxstr(_L("Search in settings [%1%]"), _L("Ctrl+") + "F");
        //m_search_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { wxGetApp().plater()->search(false); });

        m_compare_btn = new ScalableButton(m_top_panel, wxID_ANY, "compare", wxEmptyString, wxDefaultSize, wxDefaultPosition, wxBU_EXACTFIT | wxNO_BORDER, true);
        m_compare_btn->SetToolTip(_L("Compare presets"));
        m_compare_btn->Bind(wxEVT_BUTTON, ([this](wxCommandEvent e) { wxGetApp().mainframe->diff_dialog.show(); }));

        m_setting_btn = new ScalableButton(m_top_panel, wxID_ANY, "table", wxEmptyString, wxDefaultSize, wxDefaultPosition, wxBU_EXACTFIT | wxNO_BORDER, true);
        m_setting_btn->SetToolTip(_L("View all object's settings"));
        m_setting_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { wxGetApp().plater()->PopupObjectTable(-1, -1, {0, 0}); });

        m_highlighter.set_timer_owner(this, 0);
        this->Bind(wxEVT_TIMER, [this](wxTimerEvent &)
        {
            m_highlighter.blink();
        });
    }



    //m_export_to_file = new Button( this, _L("Export To File"), "");
    //m_import_from_file = new Button( this, _L("Import From File") );

    // Initialize the page.
#if __WXOSX__
    auto page_parent = m_tmp_panel;
#else
    auto page_parent = this;
#endif

    // BBS: fix scroll to tip view
    class PageScrolledWindow : public wxScrolledWindow
    {
    public:
        PageScrolledWindow(wxWindow *parent)
            : wxScrolledWindow(parent,
                               wxID_ANY,
                               wxDefaultPosition,
                               wxDefaultSize,
                               wxVSCROLL) // hide hori-bar will cause hidden field mis-position
        {
            // ShowScrollBar(GetHandle(), SB_BOTH, FALSE);
            Bind(wxEVT_SCROLL_CHANGED, [this](auto &e) {
                wxWindow *child = dynamic_cast<wxWindow *>(e.GetEventObject());
                if (child != this)
                    EnsureVisible(child);
            });
        }
        virtual bool ShouldScrollToChildOnFocus(wxWindow *child)
        {
            EnsureVisible(child);
            return false;
        }
        void EnsureVisible(wxWindow* win)
        {
            const wxRect viewRect(m_targetWindow->GetClientRect());
            const wxRect winRect(m_targetWindow->ScreenToClient(win->GetScreenPosition()), win->GetSize());
            if (viewRect.Contains(winRect)) {
                return;
            }
            if (winRect.GetWidth() > viewRect.GetWidth() || winRect.GetHeight() > viewRect.GetHeight()) {
                return;
            }
            int stepx, stepy;
            GetScrollPixelsPerUnit(&stepx, &stepy);

            int startx, starty;
            GetViewStart(&startx, &starty);
            // first in vertical direction:
            if (stepy > 0) {
                int diff = 0;

                if (winRect.GetTop() < 0) {
                    diff = winRect.GetTop();
                } else if (winRect.GetBottom() > viewRect.GetHeight()) {
                    diff = winRect.GetBottom() - viewRect.GetHeight() + 1;
                    // round up to next scroll step if we can't get exact position,
                    // so that the window is fully visible:
                    diff += stepy - 1;
                }
                starty = (starty * stepy + diff) / stepy;
            }
            // then horizontal:
            if (stepx > 0) {
                int diff = 0;
                if (winRect.GetLeft() < 0) {
                    diff = winRect.GetLeft();
                } else if (winRect.GetRight() > viewRect.GetWidth()) {
                    diff = winRect.GetRight() - viewRect.GetWidth() + 1;
                    // round up to next scroll step if we can't get exact position,
                    // so that the window is fully visible:
                    diff += stepx - 1;
                }
                startx = (startx * stepx + diff) / stepx;
            }
            Scroll(startx, starty);
        }
    };

    m_page_view = new PageScrolledWindow(page_parent);
    m_page_view->SetBackgroundColour(*wxWHITE);
    m_page_sizer = new wxBoxSizer(wxVERTICAL);

    m_page_view->SetSizer(m_page_sizer);
    m_page_view->SetScrollbars(1, 20, 1, 2);
    //m_page_view->SetScrollRate( 5, 5 );

    if (m_mode_region)
        m_mode_region->Bind(wxEVT_TOGGLEBUTTON, &ParamsPanel::OnToggled, this);
    if (m_mode_view)
        m_mode_view->Bind(wxEVT_TOGGLEBUTTON, &ParamsPanel::OnToggled, this);
    Bind(wxEVT_TOGGLEBUTTON, &ParamsPanel::OnToggled, this); // For Tab's mode switch
    //Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { wxGetApp().plater()->search(false); }, wxID_FIND);
    //m_export_to_file->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { wxGetApp().mainframe->export_config(); });
    //m_import_from_file->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { wxGetApp().mainframe->load_config_file(); });
}

// FOS 8.6.4: per-nozzle rows sit AFTER m_top_panel (the N1 row), which is no longer
// guaranteed to be at index 0 - the desynced layer-height notice is inserted above it.
// Derive the base from where m_top_panel actually is, so row order survives anything else
// being added at the top of the tab's main sizer.
static int fos_prp_row_base(Tab* tab)
{
    auto* sizer = tab ? tab->get_main_sizer() : nullptr;
    if (!sizer || !tab->get_top_panel()) return 0;
    for (size_t k = 0; k < sizer->GetItemCount(); ++k)
        if (sizer->GetItem(k)->GetWindow() == tab->get_top_panel())
            return (int) k;
    return 0;
}

void ParamsPanel::create_layout()
{
#ifdef __WINDOWS__
    this->SetDoubleBuffered(true);
    m_page_view->SetDoubleBuffered(true);
#endif //__WINDOWS__

    m_left_sizer = new wxBoxSizer( wxVERTICAL );
    // BBS: new layout
    m_left_sizer->SetMinSize( wxSize(40 * em_unit(this), -1 ) );

    if (m_top_panel) {
        m_mode_sizer = new wxBoxSizer( wxHORIZONTAL );
        m_mode_sizer->AddSpacer(FromDIP(SidebarProps::TitlebarMargin()));
        m_mode_sizer->Add(m_process_icon, 0, wxALIGN_CENTER);
        m_mode_sizer->AddSpacer(FromDIP(SidebarProps::ElementSpacing()));
        m_mode_sizer->Add( m_title_label, 0, wxALIGN_CENTER );
        m_mode_sizer->AddStretchSpacer(2);
        m_mode_sizer->Add(m_mode_region, 0, wxALIGN_CENTER);
        m_mode_sizer->AddSpacer(FromDIP(SidebarProps::ElementSpacing()));
        m_mode_sizer->Add(m_tips_arrow, 0, wxALIGN_CENTER);
        m_mode_sizer->AddStretchSpacer(8);
        m_mode_sizer->Add( m_title_view, 0, wxALIGN_CENTER );
        m_mode_sizer->AddSpacer(FromDIP(SidebarProps::ElementSpacing()));
        m_mode_sizer->Add(m_mode_view, 0, wxALIGN_CENTER);
        m_mode_sizer->AddSpacer(FromDIP(SidebarProps::ElementSpacing() * 6)); // ORCA using spacer prevents shaky mode_view when tips_arrow highlighting mode_region instead using AddStretchSpacer
        m_mode_sizer->Add(m_setting_btn, 0, wxALIGN_CENTER);
        m_mode_sizer->AddSpacer(FromDIP(SidebarProps::IconSpacing()));
        m_mode_sizer->Add(m_compare_btn, 0, wxALIGN_CENTER);

        m_mode_sizer->AddSpacer(FromDIP(SidebarProps::TitlebarMargin()));
        //m_mode_sizer->Add( m_search_btn, 0, wxALIGN_CENTER );
        //m_mode_sizer->AddSpacer(16);
        m_mode_sizer->SetMinSize(-1, FromDIP(30));
        m_top_panel->SetSizer(m_mode_sizer);
        //m_left_sizer->Add( m_top_panel, 0, wxEXPAND );
    }

    if (m_tab_print) {
        m_left_sizer->Add(m_tab_print, 0, wxEXPAND);
        // FOS: insert per-nozzle PRP rows into tab's main sizer between header and page tabs
        auto* tab_print = dynamic_cast<Tab*>(m_tab_print);
        if (tab_print) {
            // FOS: skip i=0 (N1) - the N1 PRP row is m_tab_print itself. Row count follows the
            // printer's nozzle count; the vector can still be empty here (rows are created in
            // refresh_tabs(), which runs before create_layout() in rebuild_panels()).
            for (int i = 1; i < (int) m_tab_print_nozzle.size(); ++i) {
                if (m_tab_print_nozzle[i]) {
                    m_tab_print_nozzle[i]->Reparent(m_tab_print);
                    m_tab_print_nozzle[i]->Hide();
                    // Insert at position i - after m_top_panel (pos 0), before m_tabctrl.
                    // Re-insert guard: free_sizers() clears ParamsPanel's own sizer only, so a
                    // row already sits in tab_print's main sizer across a panel rebuild.
                    if (!tab_print->get_main_sizer()->GetItem(m_tab_print_nozzle[i]))
                        tab_print->get_main_sizer()->Insert(fos_prp_row_base(tab_print) + i,
                                                           m_tab_print_nozzle[i], 0, wxEXPAND);
                }
            }
        }
    }

    if (m_tab_print_plate) {
        m_left_sizer->Add(m_tab_print_plate, 0, wxEXPAND);
    }

    if (m_tab_print_object) {
        m_left_sizer->Add( m_tab_print_object, 0, wxEXPAND );
    }

    if (m_tab_print_part) {
        m_left_sizer->Add( m_tab_print_part, 0, wxEXPAND );
    }

    if (m_tab_print_layer) {
        m_left_sizer->Add(m_tab_print_layer, 0, wxEXPAND);
    }

    if (m_tab_filament) {
        //m_filament_sizer = new wxBoxSizer( wxVERTICAL );
        //m_filament_sizer->Add( m_tab_filament, 1, wxEXPAND | wxALL, 5 );
       // m_left_sizer->Add( m_filament_sizer, 1, wxEXPAND, 5 );
        m_left_sizer->Add( m_tab_filament, 0, wxEXPAND );
    }

    if (m_tab_printer) {
        //m_printer_sizer = new wxBoxSizer( wxVERTICAL );
        //m_printer_sizer->Add( m_tab_printer, 1, wxEXPAND | wxALL, 5 );
        m_left_sizer->Add( m_tab_printer, 0, wxEXPAND );
    }

    //m_left_sizer->Add( m_printer_sizer, 1, wxEXPAND, 1 );

    //m_button_sizer = new wxBoxSizer( wxHORIZONTAL );

    //m_button_sizer->Add( m_export_to_file, 0, wxALL, 5 );

    //m_button_sizer->Add( m_import_from_file, 0, wxALL, 5 );

    //m_left_sizer->Add( m_button_sizer, 0, wxALIGN_CENTER, 5 );

    m_top_sizer->Add(m_left_sizer, 1, wxEXPAND);

    //m_right_sizer = new wxBoxSizer( wxVERTICAL );

    //m_right_sizer->Add( m_page_view, 1, wxEXPAND | wxALL, 5 );

    //m_top_sizer->Add( m_right_sizer, 1, wxEXPAND, 5 );
    // BBS: new layout
    m_left_sizer->AddSpacer(6 * em_unit(this) / 10);
#if __WXOSX__
    m_left_sizer->Add(m_tmp_panel, 1, wxEXPAND | wxALL, 0);
    m_tmp_panel->GetSizer()->Add( m_page_view, 1, wxEXPAND );
#else
    m_left_sizer->Add( m_page_view, 1, wxEXPAND );
#endif

    //this->SetSizer( m_top_sizer );
    this->Layout();
}

void ParamsPanel::rebuild_panels()
{
    refresh_tabs();
    free_sizers();
    create_layout();
}

void ParamsPanel::refresh_tabs()
{
    auto& tabs_list = wxGetApp().tabs_list;
    auto print_tech = wxGetApp().preset_bundle->printers.get_selected_preset().printer_technology();
    for (auto tab : tabs_list)
        if (tab->supports_printer_technology(print_tech))
        {
            if (tab->GetParent() != this) continue;
            switch (tab->type())
            {
                case Preset::TYPE_PRINT:
                case Preset::TYPE_SLA_PRINT:
                    m_tab_print = tab;
                    break;

                case Preset::TYPE_FILAMENT:
                case Preset::TYPE_SLA_MATERIAL:
                    m_tab_filament = tab;
                    break;

                case Preset::TYPE_PRINTER:
                    m_tab_printer = tab;
                    break;
                default:
                    break;
            }
        }
    if (m_top_panel) {
        m_tab_print_plate = wxGetApp().get_plate_tab();
        m_tab_print_object = wxGetApp().get_model_tab();
        m_tab_print_part = wxGetApp().get_model_tab(true);
        m_tab_print_layer = wxGetApp().get_layer_tab();
    }
    // FOS: one per-nozzle PRP row tab per printer nozzle (0=N1, 1=N2, ...). No fixed 4 -
    // fos_ensure_nozzle_rows() is idempotent and grows the set when a printer with more
    // nozzles is selected.
    fos_ensure_nozzle_rows(fos_nozzle_slot_count());
    return;
}

void ParamsPanel::clear_page()
{
    if (m_page_sizer)
        m_page_sizer->Clear(true);
}


void ParamsPanel::OnActivate()
{
    if (m_current_tab == NULL)
    {
        //the first time
        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": first time opened, set current tab to print");
        // BBS: open/close tab
        //m_current_tab = m_tab_print;
        set_active_tab(m_tab_print ? m_tab_print : m_tab_filament);
    }
    Tab* cur_tab = dynamic_cast<Tab *> (m_current_tab);
    if (cur_tab)
        cur_tab->OnActivate();
}

void ParamsPanel::OnToggled(wxCommandEvent& event)
{
    if (m_mode_region && m_mode_region->GetId() == event.GetId()) {
        wxWindowUpdateLocker locker(GetParent());
        set_active_tab(nullptr);
        // FOS: on Global->Objects, warn that the per-object preview color is not the
        // sliced/printed result. The first-time modal is suppressible; the lower-right
        // WARNING notification shows every time regardless of the checkbox. Deferred so
        // it appears after the update locker is released and the tab switch completes.
        if (m_mode_region->GetValue()) {
            wxGetApp().CallAfter([] {
                const wxString msg = _L("The previewed color does not accurately represent the sliced or printed result. Please refer to the sliced result.");
                if (wxGetApp().app_config->get("do_not_show_per_object_color_tip").empty()) {
                    TipsDialog dlg(wxGetApp().mainframe, _L("Per-object preview"), msg,
                                   "do_not_show_per_object_color_tip");
                    dlg.ShowModal();
                }
                if (auto* plater = wxGetApp().plater())
                    plater->get_notification_manager()->push_notification(
                        NotificationType::CustomNotification,
                        NotificationManager::NotificationLevel::WarningNotificationLevel,
                        _u8L("WARNING:") + "\n" + std::string(msg.ToUTF8().data()));
            });
        }
        event.Skip();
        return;
    }

    if (wxID_ABOUT != event.GetId()) {
        return;
    }

    // this is from tab's mode switch
    bool value = dynamic_cast<SwitchButton*>(event.GetEventObject())->GetValue();
    int mode_id;

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": Advanced mode toogle to %1%") % value;

    if (value)
    {
        //m_mode_region->SetBitmap(m_toggle_on_icon);
        mode_id = comAdvanced;
    }
    else
    {
        //m_mode_region->SetBitmap(m_toggle_off_icon);
        mode_id = comSimple;
    }

    Slic3r::GUI::wxGetApp().save_mode(mode_id);
}

// This is special, DO NOT call it from outer except from Tab
void ParamsPanel::set_active_tab(wxPanel* tab)
{
    Tab* cur_tab = dynamic_cast<Tab *> (tab);

    if (cur_tab == nullptr) {
        if (!m_mode_region->GetValue()) {
            cur_tab = (Tab*) m_tab_print;
        } else if (m_tab_print_part && ((TabPrintModel*) m_tab_print_part)->has_model_config()) {
            cur_tab = (Tab*) m_tab_print_part;
        } else if (m_tab_print_layer && ((TabPrintModel*)m_tab_print_layer)->has_model_config()) {
            cur_tab = (Tab*)m_tab_print_layer;
        } else if (m_tab_print_object && ((TabPrintModel*) m_tab_print_object)->has_model_config()) {
            cur_tab = (Tab*) m_tab_print_object;
        } else if (m_tab_print_plate && ((TabPrintPlate*)m_tab_print_plate)->has_model_config()) {
            cur_tab = (Tab*)m_tab_print_plate;
        }
        Show(cur_tab != nullptr);
        wxGetApp().sidebar().show_object_list(m_mode_region->GetValue());
        if (m_current_tab == cur_tab)
            return;
        if (cur_tab)
            cur_tab->restore_last_select_item();
        return;
    }

    m_current_tab = tab;
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": set current to %1%, type=%2%") % cur_tab % cur_tab?cur_tab->type():-1;
    update_mode();

    // BBS: open/close tab
    for (auto t : std::vector<std::pair<wxPanel*, wxStaticLine*>>({
            {m_tab_print, m_staticline_print},
            {m_tab_print_object, m_staticline_print_object},
            {m_tab_print_part, m_staticline_print_part},
            {m_tab_print_layer, nullptr},
            {m_tab_print_plate, nullptr},
            {m_tab_filament, m_staticline_filament},
            {m_tab_printer, m_staticline_printer}})) {
        if (!t.first) continue;
        t.first->Show(tab == t.first);
        if (!t.second) continue;
        t.second->Show(tab == t.first);
        //m_left_sizer->GetItem(t)->SetProportion(tab == t ? 1 : 0);
    }
    m_left_sizer->Layout();
    if (auto dialog = dynamic_cast<wxDialog*>(GetParent())) {
        wxString title = cur_tab->type() == Preset::TYPE_FILAMENT ? _L("Material settings") : _L("Printer settings");
        dialog->SetTitle(title);
    }

    auto tab_print = dynamic_cast<Tab *>(m_tab_print);
    if (cur_tab == m_tab_print) {
        if (tab_print)
            tab_print->toggle_line("print_flow_ratio", false);
    } else {
        if (tab_print)
            tab_print->toggle_line("print_flow_ratio", false);
    }
}

bool ParamsPanel::is_active_and_shown_tab(wxPanel* tab)
{
    if (m_current_tab == tab)
        return true;
    else
        return false;
}

void ParamsPanel::update_mode()
{
    int app_mode = Slic3r::GUI::wxGetApp().get_mode();
    SwitchButton * mode_view = m_current_tab ? dynamic_cast<Tab*>(m_current_tab)->m_mode_view : nullptr;
    if (mode_view == nullptr) mode_view = m_mode_view;
    if (mode_view == nullptr) return;

    //BBS: disable the mode tab and return directly when enable develop mode
    if (app_mode == comDevelop)
    {
        mode_view->Disable();
        return;
    }
    if (!mode_view->IsEnabled())
        mode_view->Enable();

    if (app_mode == comAdvanced)
    {
        mode_view->SetValue(true);
    }
    else
    {
        mode_view->SetValue(false);
    }
}

void ParamsPanel::msw_rescale()
{
    if (m_process_icon) m_process_icon->msw_rescale();
    if (m_setting_btn) m_setting_btn->msw_rescale();
    if (m_search_btn) m_search_btn->msw_rescale();
    if (m_compare_btn) m_compare_btn->msw_rescale();
    if (m_tips_arrow) m_tips_arrow->msw_rescale();
    m_left_sizer->SetMinSize(wxSize(40 * em_unit(this), -1));
    if (m_mode_sizer)
        m_mode_sizer->SetMinSize(-1, 3 * em_unit(this));
    if (m_mode_region)
        ((SwitchButton* )m_mode_region)->Rescale();
    if (m_mode_view)
        ((SwitchButton* )m_mode_view)->Rescale();
    for (auto tab : {m_tab_print, m_tab_print_plate, m_tab_print_object, m_tab_print_part, m_tab_print_layer, m_tab_filament, m_tab_printer}) {
        if (tab) dynamic_cast<Tab*>(tab)->msw_rescale();
    }
    // FOS: N2-N4 PRP rows are TabPrintNozzle tabs that removed themselves from tabs_list and
    // are NOT in the loop above, so their preset dropdowns never rescale on DPI change - they
    // stay at build-time size (N1 tracks DPI, N2-N4 stuck). Rescale each nozzle row's combo
    // directly. Do NOT call the full Tab::msw_rescale here: nozzle tabs have an empty build()
    // with no page icons, so its m_scaled_icons_list.front() would be UB.
    for (int i = 1; i < (int) m_tab_print_nozzle.size(); ++i)
        if (m_tab_print_nozzle[i])
            m_tab_print_nozzle[i]->fos_rescale_row(); // FOS: rescale whole nozzle row (combo + buttons)
    //((Button*)m_export_to_file)->Rescale();
    //((Button*)m_import_from_file)->Rescale();
}

void ParamsPanel::switch_to_global()
{
    m_mode_region->SetValue(false);
    set_active_tab(nullptr);
}

void ParamsPanel::switch_to_object(bool with_tips)
{
    m_mode_region->SetValue(true);
    set_active_tab(nullptr);
    if (with_tips) {
        m_highlighter.init(std::pair(m_tips_arrow, &m_tips_arror_blink), m_top_panel);
        m_highlighter.blink();
    }
}

void ParamsPanel::notify_object_config_changed()
{
    auto & model = wxGetApp().model();
    bool has_config = false;
    for (auto obj : model.objects) {
        if (!obj->config.empty()) {
            SettingsFactory::Bundle cat_options = SettingsFactory::get_bundle(&obj->config.get(), true);
            if (cat_options.size() > 0) {
                has_config = true;
                break;
            }
        }
        for (auto volume : obj->volumes) {
            if (!volume->config.empty()) {
                SettingsFactory::Bundle cat_options = SettingsFactory::get_bundle(&volume->config.get(), true);
                if (cat_options.size() > 0) {
                    has_config = true;
                    break;
                }
            }
        }
        if (has_config) break;
    }
    if (has_config == m_has_object_config) return;
    m_has_object_config = has_config;
    if (has_config)
        m_mode_region->SetTextColor2(StateColor(std::pair{0xfffffe, (int) StateColor::Checked}, std::pair{wxGetApp().get_label_clr_modified(), 0}));
    else
        m_mode_region->SetTextColor2(StateColor());
    m_mode_region->Rescale();
}

int ParamsPanel::fos_nozzle_slot_count() const
{
    // FOS: one PRP slot per printer nozzle. nozzle_diameter is the SINGLE source of the count -
    // the same option TabPrint sizes m_fos_slot_configs from - so the row set and the slot
    // configs cannot disagree. Never returns 0: slot 0 (N1) always exists.
    const auto* nd = wxGetApp().preset_bundle
        ? wxGetApp().preset_bundle->printers.get_edited_preset()
              .config.option<ConfigOptionFloats>("nozzle_diameter")
        : nullptr;
    return (nd && !nd->values.empty()) ? (int) nd->values.size() : 1;
}

void ParamsPanel::fos_ensure_nozzle_rows(int count)
{
    // FOS: grow the PRP row set to count rows. Idempotent - only missing rows are created, so it
    // is safe to call on every printer change. Rows are never destroyed: a printer with fewer
    // nozzles hides the surplus (update_prp_nozzle_rows), because Tab.cpp holds nozzle tabs by
    // raw pointer through get_nozzle_tab() and a destroyed row would dangle.
    if (!m_tab_print || count <= 0) return;
    auto* tab_print = dynamic_cast<Tab*>(m_tab_print);
    for (int i = (int) m_tab_print_nozzle.size(); i < count; ++i) {
        auto* t = new TabPrintNozzle(this, i);
        t->create_preset_tab();
        m_tab_print_nozzle.push_back(t);
        // Slot 0 (N1) has no row of its own - the N1 PRP row is m_tab_print. A row created
        // AFTER create_layout() ran must insert itself; create_layout() re-inserts the whole
        // set on a panel rebuild (same position-i rule, guarded against double insert).
        if (i > 0 && tab_print && tab_print->get_main_sizer()) {
            t->Reparent(m_tab_print);
            t->Hide();
            if (!tab_print->get_main_sizer()->GetItem(t))
                tab_print->get_main_sizer()->Insert(fos_prp_row_base(tab_print) + i, t, 0, wxEXPAND);
        }
    }
}

// FOS 8.6.4: the diameter token slot `slot` requires in a process preset name.
static std::string fos_slot_dia_str(int slot)
{
    auto* bundle = wxGetApp().preset_bundle;
    if (!bundle) return std::string();
    const auto* nd = bundle->printers.get_edited_preset()
        .config.option<ConfigOptionFloats>("nozzle_diameter");
    if (!nd || slot < 0 || slot >= (int) nd->values.size()) return std::string();
    return float_to_string_decimal_point(nd->values[slot], 1);
}

// FOS 8.6.4: the same diameter test the per-nozzle dropdown filter applies
// (PresetComboBoxes.cpp ~1755). Kept identical so the list and the auto-pick cannot disagree.
static bool fos_prp_name_matches_dia(const std::string& name, const std::string& dia)
{
    if (dia.empty()) return true;
    return name.find(dia + " nozzle") != std::string::npos
        || name.find("(" + dia) != std::string::npos;
}

// FOS 8.6.4: first process preset matching slot `slot`'s diameter AND this printer model.
// Empty means this model has no process preset for that diameter - callers must then leave the
// filter off rather than painting an empty dropdown.
static std::string fos_pick_prp_for_slot(int slot)
{
    auto* bundle = wxGetApp().preset_bundle;
    const std::string slot_dia = fos_slot_dia_str(slot);
    if (!bundle || slot_dia.empty()) return std::string();
    const auto* pm = bundle->printers.get_edited_preset()
        .config.option<ConfigOptionString>("printer_model");
    const std::string expected_printer = (pm && !pm->value.empty())
        ? pm->value + " (" + slot_dia + " nozzle)" : std::string();
    std::string chosen, fallback;
    for (const auto& preset : bundle->prints.get_presets()) {
        if (!preset.is_visible || preset.is_default) continue;
        if (!fos_prp_name_matches_dia(preset.name, slot_dia)) continue;
        if (fallback.empty()) fallback = preset.name;
        if (!expected_printer.empty()) {
            const auto* cp = preset.config.option<ConfigOptionStrings>("compatible_printers");
            if (cp && !cp->values.empty()) {
                for (const auto& p : cp->values)
                    if (p == expected_printer) { chosen = preset.name; break; }
            }
        }
        if (!chosen.empty()) break;
    }
    return !chosen.empty() ? chosen : fallback;
}

void ParamsPanel::update_prp_nozzle_rows(bool mixed_active)
{
    m_fos_mixed_nozzle_mode = mixed_active; // FOS: store for fos_reload_slot_config
    // FOS: the printer may have just changed to one with MORE nozzles - grow the row set before
    // showing, so every slot has a row to bind. Past the old fixed 4, get_nozzle_tab() returned
    // null and those slots silently never loaded their PRP.
    const int fos_slot_count = fos_nozzle_slot_count();
    fos_ensure_nozzle_rows(fos_slot_count);
    // FOS 8.6.4: desynced-mode notice above the Nozzle 1 row. Per-nozzle PRPs drive width and
    // speed (TabPrint::fos_resolve_nozzle_arrays), but layer height is still ONE global value in
    // 8.x - the per-nozzle layer-height arrays were MAPS plumbing and were removed, so N2-N4's
    // PRP cannot move it. Say so on screen rather than letting the rows imply otherwise.
    // Lives at main-sizer index 0; fos_prp_row_base() keeps the nozzle rows below it.
    if (auto* fos_notice_tab = dynamic_cast<Tab*>(m_tab_print)) {
        if (auto* fos_notice_sizer = fos_notice_tab->get_main_sizer()) {
            if (m_fos_prp_lh_notice == nullptr)
                m_fos_prp_lh_notice = new Label(m_tab_print, Label::Head_14,
                    _L("For now, only Nozzle 1's process preset affects layer height."),
                    LB_AUTO_WRAP);
            // Head_14 is Body_14's size in bold, matching the "Nozzle 1" row label, and red
            // because this is a limitation the per-nozzle rows otherwise imply is not there.
            m_fos_prp_lh_notice->SetForegroundColour(wxColour(0xC5, 0x1B, 0x1B));
            // Re-insert guard, same reason as the rows: a panel rebuild leaves the window
            // parented and already in this sizer.
            if (!fos_notice_sizer->GetItem(m_fos_prp_lh_notice))
                fos_notice_sizer->Insert(0, m_fos_prp_lh_notice, 0,
                    wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(SidebarProps::ContentMargin()));
            fos_notice_sizer->Show(m_fos_prp_lh_notice, mixed_active);
            fos_notice_sizer->Layout();
        }
    }
    // FOS 8.6.4: N1 has no row of its own - its PRP is the MAIN print combo - so the i>=1 loop
    // below never reached it, and the main combo takes the OTHER filter branch in
    // TabPresetComboBox::update() (PresetComboBoxes.cpp ~1778), the one governed by
    // is_compatible. Process presets carry an explicit compatible_printers NAME list, and
    // is_compatible_with_printer (Preset.cpp:686) matches on active_printer.preset.name with the
    // inherits() fallback gated on !is_system. A dirtied stock PTP keeps is_system AND its name
    // (select_preset copies the whole Preset into m_edited_preset), so on a stock PTP no amount
    // of diameter editing could make another diameter's process presets compatible: N1's list
    // stayed at the stock diameter until Save-As under a new name made the preset non-system
    // (Tab.cpp:7571 -> update_nozzle_settings(true) -> the inherits rewrite in Plater.cpp).
    // Filaments were never affected: update_compatible_internal has a TYPE_FILAMENT-only escape
    // hatch that rescans nozzle_diameter, which is why FL1 followed the diameter and PRP1 did not.
    // Fix: give slot 0 the same diameter filter + auto-pick slots 1..N already get. N1's PRP
    // follows NOZZLE 1's OWN diameter (index 0), not the outer-wall nozzle.
    if (auto* fos_n1_tab = dynamic_cast<Tab*>(m_tab_print)) {
        if (auto* fos_n1_combo = fos_n1_tab->get_combo_box()) {
            const std::string fos_n1_dia  = mixed_active ? fos_slot_dia_str(0) : std::string();
            const std::string fos_n1_pick = mixed_active ? fos_pick_prp_for_slot(0) : std::string();
            fos_n1_combo->set_nozzle_slot(fos_n1_pick.empty() ? -1 : 0);
            fos_n1_combo->update(); // FOS: repaint the list under the new slot filter
            // Re-pick ONLY when the current N1 preset does not belong to nozzle 1's diameter.
            // Picking unconditionally would clobber a hand-chosen quality tier on every refresh,
            // because the pick is "first match" and this function runs on load and on PTP switch.
            const std::string fos_n1_cur =
                wxGetApp().preset_bundle->prints.get_edited_preset().name;
            if (!fos_n1_pick.empty() && !fos_prp_name_matches_dia(fos_n1_cur, fos_n1_dia)) {
                // select_preset() may raise the unsaved-changes dialog and rebuilds the tab,
                // which re-enters this function - defer it, and re-test inside so the re-entrant
                // pass is a no-op.
                wxGetApp().CallAfter([fos_n1_pick]() {
                    if (auto* t = wxGetApp().get_tab(Preset::TYPE_PRINT))
                        if (wxGetApp().preset_bundle->prints.get_edited_preset().name != fos_n1_pick)
                            t->select_preset(fos_n1_pick);
                });
            }
        }
    }
    // FOS: i=0=N1, i=1=N2, ... all 0-based; N1 row (i=0) shown in mixed mode alongside m_tab_print
    for (int i = 1; i < (int) m_tab_print_nozzle.size(); ++i) {  // FOS: skip i=0 (N1) - N1 PRP row is m_tab_print itself
        // FOS: a row past the current nozzle count stays hidden regardless of mixed mode
        const bool row_active = mixed_active && i < fos_slot_count;
        if (m_tab_print_nozzle[i]) {
            m_tab_print_nozzle[i]->Show(row_active);
            if (row_active) m_tab_print_nozzle[i]->fos_rescale_row(); // FOS: rescale row to current DPI on show
            // FOS: re-apply nozzle slot filter so PRP dropdown reflects current PTP nozzle diameters
            if (auto* combo = m_tab_print_nozzle[i]->get_combo_box()) {
                combo->set_nozzle_slot(i);
                // FOS: auto-select first matching preset for this slot's nozzle diameter
                const auto* nd_opt = wxGetApp().preset_bundle->printers
                    .get_edited_preset().config.option<ConfigOptionFloats>("nozzle_diameter");
                if (nd_opt && i < (int)nd_opt->values.size()) {
                    std::string slot_dia = float_to_string_decimal_point(nd_opt->values[i], 1);
                    const auto& presets = wxGetApp().preset_bundle->prints.get_presets();
                    // FOS: build the same model-compatibility key the dropdown filter uses
                    // (PresetComboBoxes.cpp ~1733) so the auto-selected default matches the
                    // filtered dropdown. Diameter-only matching let non-U1 presets
                    // (@Snapmaker, @Snapmaker Artisan) get defaulted in, shifting the
                    // per-nozzle ilh array on cold load (was misdiagnosed as AP-77 race).
                    const auto* pm = wxGetApp().preset_bundle->printers
                        .get_edited_preset().config.option<ConfigOptionString>("printer_model");
                    std::string expected_printer = (pm && !pm->value.empty())
                        ? pm->value + " (" + slot_dia + " nozzle)" : std::string();
                    // FOS: restore the user's SAVED per-nozzle PRP selection (project load)
                    // before falling back to the diameter heuristic. print_filament_presets
                    // is persisted in the .3mf and loaded into project_config; slot i holds
                    // the saved name for nozzle i+1. Empty slot -> fall through to heuristic
                    // (new project / never-assigned). This fixes saved selections reverting
                    // to a diameter-default on reopen (e.g. 0.48 -> 0.24 for N4).
                    std::string saved_pick;
                    {
                        const auto* pfp = wxGetApp().preset_bundle->project_config
                            .option<ConfigOptionStrings>("print_filament_presets");
                        if (pfp && i < (int)pfp->values.size() && !pfp->values[i].empty())
                            saved_pick = pfp->values[i];
                    }
                    std::string chosen;        // FOS: model-compatible match (preferred)
                    std::string fallback;      // FOS: diameter-only match (used iff no compatible)
                    for (const auto& preset : presets) {
                        if (!preset.is_visible || preset.is_default) continue;
                        bool diameter_match =
                            preset.name.find(slot_dia + " nozzle") != std::string::npos ||
                            preset.name.find("(" + slot_dia) != std::string::npos;
                        if (!diameter_match) continue;
                        if (fallback.empty()) fallback = preset.name;
                        if (!expected_printer.empty()) {
                            const auto* cp = preset.config.option<ConfigOptionStrings>("compatible_printers");
                            // FOS: empty compatible_printers = generic, not a U1 match
                            if (cp && !cp->values.empty()) {
                                for (const auto& p : cp->values)
                                    if (p == expected_printer) { chosen = preset.name; break; }
                            }
                        }
                        if (!chosen.empty()) break;
                    }
                    // FOS 8.6.4: a saved selection wins ONLY while it still belongs to this
                    // slot's CURRENT diameter. It used to win unconditionally, which is correct
                    // for a project load but pinned the row forever afterwards: once
                    // print_filament_presets[i] held a name (written by the per-nozzle selection
                    // handler, Tab.cpp ~2546, and persisted in the .3mf), changing nozzle i+1's
                    // diameter could never move its PRP - the heuristic below was unreachable.
                    // Same rule the N1 re-pick uses: a diameter change invalidates the saved
                    // selection, nothing else does.
                    const bool saved_ok = !saved_pick.empty()
                                       && fos_prp_name_matches_dia(saved_pick, slot_dia);
                    std::string pick = saved_ok ? saved_pick
                                     : (!chosen.empty() ? chosen : fallback);
                    // When the saved name is dropped, correct project_config too - otherwise the
                    // stale name stays in the project and is written back out to the .3mf even
                    // though no row is using it any more.
                    if (!saved_ok && !saved_pick.empty() && !pick.empty()) {
                        auto* fos_pfp = wxGetApp().preset_bundle->project_config
                            .option<ConfigOptionStrings>("print_filament_presets", true);
                        if (fos_pfp) {
                            while ((int) fos_pfp->values.size() <= i) fos_pfp->values.push_back("");
                            fos_pfp->values[i] = pick;
                        }
                    }
                    if (!pick.empty()) {
                        m_tab_print_nozzle[i]->set_selected_preset_name(pick);
                        combo->set_per_nozzle_selected(pick);
                        // FOS: rebind m_fos_slot_configs[i] from the (possibly saved) pick.
                        // set_selected_preset_name only updates the combo display; without this
                        // reload the slot config keeps its stale heuristic value and the slicer
                        // reads the wrong PRP. AP-98 follow-on: project-load round-trip.
                        if (auto* tab_print = dynamic_cast<TabPrint*>(wxGetApp().get_tab(Preset::TYPE_PRINT)))
                            tab_print->fos_reload_slot_config(i);
                    }
                }
                combo->update();
            }
        }
    }
    // FOS 8.5.3: PTP-create populate. The per-slot fos_reload_slot_config() calls above BAIL
    // when the per-nozzle notebook optgroups are not built yet (they build lazily on first
    // Quality-tab open), so on PTP create slots 1-N never load and the slice used N1 width for
    // everything. Populate ALL slots from their sources + resolve ONCE here (complete-set), so
    // the create path never depends on the notebook being open.
    if (auto* fos_tp = dynamic_cast<TabPrint*>(wxGetApp().get_tab(Preset::TYPE_PRINT))) {
        fos_tp->fos_populate_all_slots(mixed_active);
        // FOS: the per-nozzle notebooks freeze their page count at build time, so a nozzle-count
        // change needs them rebuilt here - otherwise the Quality line-width and Speed notebooks
        // keep the old tab count until the user switches pages away and back.
        fos_tp->fos_sync_nozzle_notebooks();
    }
    // FOS: reduce bottom padding of row 1 when nozzle rows are shown
    auto* tab_print = dynamic_cast<Tab*>(m_tab_print);
    if (tab_print && tab_print->get_main_sizer() && tab_print->get_top_panel()) {
        auto* item = tab_print->get_main_sizer()->GetItem(tab_print->get_top_panel());
        if (item) {
            if (mixed_active) {
                item->SetFlag(wxEXPAND | wxTOP);
                item->SetBorder(FromDIP(6));
            } else {
                item->SetFlag(wxEXPAND | wxUP | wxDOWN);
                item->SetBorder(tab_print->get_em_unit());
            }
        }
        tab_print->get_main_sizer()->Layout();
    }
    if (m_left_sizer) m_left_sizer->Layout();
    Layout();
}

void ParamsPanel::switch_to_object_if_has_object_configs()
{
    if (m_has_object_config)
        m_mode_region->SetValue(true);
    set_active_tab(nullptr);
}

void ParamsPanel::free_sizers()
{
    if (m_top_sizer)
    {
        m_top_sizer->Clear(false);
        //m_top_sizer = nullptr;
    }

    m_left_sizer = nullptr;
    //m_right_sizer = nullptr;
    m_mode_sizer = nullptr;
    //m_print_sizer = nullptr;
    //m_filament_sizer = nullptr;
    //m_printer_sizer = nullptr;
    m_button_sizer = nullptr;
}

void ParamsPanel::delete_subwindows()
{
    if (m_title_label)
    {
        delete m_title_label;
        m_title_label = nullptr;
    }

    if (m_mode_region)
    {
        delete m_mode_region;
        m_mode_region = nullptr;
    }

    if (m_mode_view)
    {
        delete m_mode_view;
        m_mode_view = nullptr;
    }

    if (m_title_view)
    {
        delete m_title_view;
        m_title_view = nullptr;
    }

    if (m_search_btn)
    {
        delete m_search_btn;
        m_search_btn = nullptr;
    }

    if (m_staticline_print)
    {
        delete m_staticline_print;
        m_staticline_print = nullptr;
    }

    if (m_staticline_print_part)
    {
        delete m_staticline_print_part;
        m_staticline_print_part = nullptr;
    }

    if (m_staticline_print_object)
    {
        delete m_staticline_print_object;
        m_staticline_print_object = nullptr;
    }

    if (m_staticline_filament)
    {
        delete m_staticline_filament;
        m_staticline_filament = nullptr;
    }

    if (m_staticline_printer)
    {
        delete m_staticline_printer;
        m_staticline_printer = nullptr;
    }

    if (m_export_to_file)
    {
        delete m_export_to_file;
        m_export_to_file = nullptr;
    }

    if (m_import_from_file)
    {
        delete m_import_from_file;
        m_import_from_file = nullptr;
    }

    if (m_page_view)
    {
        delete m_page_view;
        m_page_view = nullptr;
    }
}

ParamsPanel::~ParamsPanel()
{
#if 0
    free_sizers();
    delete m_top_sizer;

    delete_subwindows();
#endif
    // BBS: fix double destruct of OG_CustomCtrl
    Tab* cur_tab = dynamic_cast<Tab*> (m_current_tab);
    if (cur_tab)
        cur_tab->clear_pages();
}

} // GUI
} // Slic3r
