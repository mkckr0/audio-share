// ServerDialog.cpp : implementation file
//

#include "pch.h"
#include "CServerTabPanel.h"
#include "resource.h"
#include "audio_manager.hpp"
#include "network_manager.hpp"
#include "AudioShareServer.h"

// CServerTabPanel dialog

IMPLEMENT_DYNAMIC(CServerTabPanel, CTabPanel)

CServerTabPanel::CServerTabPanel(CWnd* pParent)
	: CTabPanel(IDD_SERVER, L"Server", pParent)
{
}

CServerTabPanel::~CServerTabPanel()
{
}

void CServerTabPanel::SwitchServer()
{
    m_buttonServer.PostMessageW(BM_CLICK);
}

bool CServerTabPanel::IsRunning()
{
    return m_network_manager->is_running();
}

void CServerTabPanel::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO_HOST, m_comboBoxHost);
    DDX_Control(pDX, IDC_EDIT_PORT, m_editPort);
    DDX_Control(pDX, IDC_COMBO_AUDIO_ENDPOINT, m_comboBoxAudioEndpoint);
    DDX_Control(pDX, IDC_BUTTON_SERVER, m_buttonServer);
    DDX_Control(pDX, IDC_BUTTON_RESET, m_buttonReset);
}


BEGIN_MESSAGE_MAP(CServerTabPanel, CTabPanel)
    ON_BN_CLICKED(IDC_BUTTON_SERVER, &CServerTabPanel::OnBnClickedStartServer)
    ON_BN_CLICKED(IDC_BUTTON_RESET, &CServerTabPanel::OnBnClickedButtonReset)
END_MESSAGE_MAP()


// CServerTabPanel message handlers

BOOL CServerTabPanel::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // TODO:  Add extra initialization here
    m_editPort.SetLimitText(5);

    // init controls data
    this->OnBnClickedButtonReset();

    // create network_manager
    m_audio_manager = std::make_shared<audio_manager>();
    m_network_manager = std::make_shared<network_manager>(m_audio_manager);

    auto nWhenAppStart = theApp.GetProfileIntW(L"App Settings", L"WhenAppStart", false);
    if (nWhenAppStart == 1 || theApp.GetProfileIntW(L"App", L"Running", false)) {
        m_buttonServer.PostMessageW(BM_CLICK);
    }

    //auto x = this->GetThisMessageMap();

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}


void CServerTabPanel::OnBnClickedButtonReset()
{
    //m_comboBoxHost.Clear();
    m_comboBoxHost.ResetContent();
    auto address_list = network_manager::get_local_addresss();
    for (auto address : address_list) {
        auto nIndex = m_comboBoxHost.AddString(address.c_str());
    }
    auto configHost = theApp.GetProfileStringW(L"Network", L"host");
    if (!configHost.IsEmpty()) {
        m_comboBoxHost.SetWindowTextW(configHost);
    }
    else {
        if (m_comboBoxHost.GetCount()) {
            m_comboBoxHost.SetCurSel(0);
        }
    }
    m_editPort.SetWindowTextW(theApp.GetProfileStringW(L"Network", L"port", L"65530"));

    //m_comboBoxAudioEndpoint.Clear();    // clear current selection
    for (int nIndex = m_comboBoxAudioEndpoint.GetCount() - 1; nIndex >= 0; --nIndex) {
        free(m_comboBoxAudioEndpoint.GetItemDataPtr(nIndex));
        //m_comboBoxAudioEndpoint.DeleteString(nIndex);
    }
    m_comboBoxAudioEndpoint.ResetContent();

    audio_manager::endpoint_list_t endpoint_list;
    int default_index = m_audio_manager->get_endpoint_list(endpoint_list);
    for (int i = 0; i < endpoint_list.size(); ++i) {
        auto&& [id, name] = endpoint_list[i];
        int nIndex = m_comboBoxAudioEndpoint.AddString(mbs_to_wchars(name).c_str());
        m_comboBoxAudioEndpoint.SetItemDataPtr(nIndex, _strdup(id.c_str()));
    }

    auto configEndpoint = theApp.GetProfileStringW(L"Audio", L"endpoint");
    for (int nIndex = 0; nIndex < m_comboBoxAudioEndpoint.GetCount(); ++nIndex) {
        CString text;
        m_comboBoxAudioEndpoint.GetLBText(nIndex, text);
        if (configEndpoint == text) {
            m_comboBoxAudioEndpoint.SetCurSel(nIndex);
            return;
        }
    }
    for (int nIndex = 0; nIndex < m_comboBoxAudioEndpoint.GetCount(); ++nIndex) {
        CString text;
        m_comboBoxAudioEndpoint.GetLBText(nIndex, text);
        if (default_index == nIndex) {
            m_comboBoxAudioEndpoint.SetCurSel(nIndex);
            return;
        }
    }
}

void CServerTabPanel::OnBnClickedStartServer()
{
    if (!m_network_manager) {
        AfxMessageBox(L"network_manager is nullptr", MB_OK | MB_ICONSTOP);
        EndDialog(-1);
        return;
    }

    CString text;
    m_buttonServer.GetWindowText(text);
    if (text == L"Start Server") {
        if (!m_comboBoxAudioEndpoint.GetCount()) {
            AfxMessageBox(L"No Audio Endpoint", MB_OK | MB_ICONSTOP);
            return;
        }

        // start
        EnableInputControls(false);
        m_buttonServer.EnableWindow(false);

        CString host_str, port_str;
        m_comboBoxHost.GetWindowText(host_str);
        m_editPort.GetWindowText(port_str);
        std::string host = wchars_to_mbs(host_str.GetString());
        std::uint16_t port = std::stoi(wchars_to_mbs(port_str.GetString()));
        auto endpoint = (const char*)m_comboBoxAudioEndpoint.GetItemDataPtr(m_comboBoxAudioEndpoint.GetCurSel());
        try {
            m_network_manager->start_server(host, port, endpoint);
        }
        catch (std::exception& e) {
            AfxMessageBox(CString(e.what()), MB_OK | MB_ICONSTOP);
            EnableInputControls(true);
            m_buttonServer.EnableWindow(true);
            m_buttonServer.SetWindowText(L"Start Server");
            m_buttonServer.SetFocus();
            return;
        }

        m_buttonServer.EnableWindow(true);
        m_buttonServer.SetWindowText(L"Stop Server");
        m_buttonServer.SetFocus();
        theApp.WriteProfileStringW(L"Network", L"host", host_str);
        theApp.WriteProfileStringW(L"Network", L"port", port_str);
        CString endpoint_name;
        m_comboBoxAudioEndpoint.GetWindowTextW(endpoint_name);
        theApp.WriteProfileStringW(L"Audio", L"endpoint", endpoint_name);
        theApp.WriteProfileInt(L"App", L"Running", true);
    }
    else {
        // stop
        m_buttonServer.EnableWindow(false);
        m_network_manager->stop_server();

        EnableInputControls(true);
        m_buttonServer.EnableWindow(true);
        m_buttonServer.SetWindowText(L"Start Server");
        m_buttonServer.SetFocus();
        theApp.WriteProfileInt(L"App", L"Running", false);
    }
}

void CServerTabPanel::EnableInputControls(bool bEnable)
{
    m_comboBoxHost.EnableWindow(bEnable);
    m_editPort.EnableWindow(bEnable);
    m_comboBoxAudioEndpoint.EnableWindow(bEnable);
    m_buttonReset.EnableWindow(bEnable);
}
