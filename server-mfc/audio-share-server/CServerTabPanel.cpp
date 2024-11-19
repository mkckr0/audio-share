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
    DDX_Control(pDX, IDC_COMBO_ENCODING, m_comboEncoding);
    DDX_Control(pDX, IDC_BUTTON_SOUND_PANEL, m_buttonSoundPanel);
}


BEGIN_MESSAGE_MAP(CServerTabPanel, CTabPanel)
    ON_BN_CLICKED(IDC_BUTTON_SERVER, &CServerTabPanel::OnBnClickedStartServer)
    ON_BN_CLICKED(IDC_BUTTON_RESET, &CServerTabPanel::OnBnClickedButtonReset)
    ON_BN_CLICKED(IDC_BUTTON_SOUND_PANEL, &CServerTabPanel::OnBnClickedButtonSoundPanel)
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

    auto nWhenAppStart = theApp.GetProfileIntW(L"App Settings", L"WhenAppStart", 0);
    if (nWhenAppStart == 1 || nWhenAppStart == 2 && theApp.GetProfileIntW(L"App", L"Running", false)) {
        m_buttonServer.PostMessageW(BM_CLICK);
    }

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}


void CServerTabPanel::OnBnClickedButtonReset()
{
    // host list
    {
        m_comboBoxHost.ResetContent();
        auto address_list = network_manager::get_address_list();
        for (auto address : address_list) {
            auto nIndex = m_comboBoxHost.AddString(mbs_to_wchars(address).c_str());
        }
        auto default_address = network_manager::get_default_address();
        auto configHost = theApp.GetProfileStringW(L"Network", L"host", default_address.empty() ? nullptr : mbs_to_wchars(default_address).c_str());
        m_comboBoxHost.SetWindowTextW(configHost);
    }

    // port
    {
        m_editPort.SetWindowTextW(theApp.GetProfileStringW(L"Network", L"port", L"65530"));
    }

    // audio endpoint list
    {
        for (int nIndex = m_comboBoxAudioEndpoint.GetCount() - 1; nIndex >= 0; --nIndex) {
            free(m_comboBoxAudioEndpoint.GetItemDataPtr(nIndex));
        }
        m_comboBoxAudioEndpoint.ResetContent();

        auto nIndex = m_comboBoxAudioEndpoint.AddString(L"Default");
        m_comboBoxAudioEndpoint.SetItemDataPtr(nIndex, _wcsdup(L"default"));
        
        audio_manager::endpoint_list_t endpoint_list = m_audio_manager->get_endpoint_list();
        for (auto&& [id, name] : endpoint_list) {
            int nIndex = m_comboBoxAudioEndpoint.AddString(mbs_to_wchars(name).c_str());
            m_comboBoxAudioEndpoint.SetItemDataPtr(nIndex, _wcsdup(mbs_to_wchars(id).c_str()));
        }

        auto configEndpoint = theApp.GetProfileStringW(L"Capture", L"endpoint", L"default");
        for (int nIndex = 0; nIndex < m_comboBoxAudioEndpoint.GetCount(); ++nIndex) {
            if (configEndpoint == (LPCWSTR)m_comboBoxAudioEndpoint.GetItemDataPtr(nIndex)) {
                m_comboBoxAudioEndpoint.SetCurSel(nIndex);
                break;
            }
        }
    }

    // encoding list
    {
        m_comboEncoding.ResetContent();
        using encoding_t = audio_manager::encoding_t;
        std::vector<std::pair<encoding_t, std::wstring>> array = {
            { encoding_t::encoding_default, L"Default" },
            { encoding_t::encoding_f32, L"32 bit floating-point PCM" },
            { encoding_t::encoding_s8, L"8 bit integer PCM" },
            { encoding_t::encoding_s16, L"16 bit integer PCM" },
            { encoding_t::encoding_s24, L"24 bit integer PCM" },
            { encoding_t::encoding_s32, L"32 bit integer PCM" },
        };
        for (auto&& [encoding, name] : array) {
            auto nIndex = m_comboEncoding.AddString(name.c_str());
            m_comboEncoding.SetItemData(nIndex, (int)encoding);
        }
        
        // select
        auto configEncoding = (encoding_t)theApp.GetProfileIntW(L"Capture", L"encoding", (int)encoding_t::encoding_default);
        for (int nIndex = 0; nIndex < m_comboEncoding.GetCount(); ++nIndex) {
            if (configEncoding == (encoding_t)m_comboEncoding.GetItemData(nIndex)) {
                m_comboEncoding.SetCurSel(nIndex);
                break;
            }
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
        if (m_comboBoxAudioEndpoint.GetCount() == 1) {
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
        audio_manager::capture_config config;
        config.endpoint_id = wchars_to_mbs((LPCWSTR)m_comboBoxAudioEndpoint.GetItemDataPtr(m_comboBoxAudioEndpoint.GetCurSel()));
        config.encoding = (audio_manager::encoding_t)m_comboEncoding.GetItemData(m_comboEncoding.GetCurSel());
        try {
            m_network_manager->start_server(host, port, config);
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
        theApp.WriteProfileStringW(L"Capture", L"endpoint", mbs_to_wchars(config.endpoint_id).c_str());
        theApp.WriteProfileInt(L"Capture", L"encoding", (int)config.encoding);
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
    m_comboEncoding.EnableWindow(bEnable);
    m_buttonSoundPanel.EnableWindow(bEnable);
}


void CServerTabPanel::OnBnClickedButtonSoundPanel()
{
    STARTUPINFO si{};
    PROCESS_INFORMATION pi{};

    LPWSTR lpCommandLine = _wcsdup(L"control.exe /name Microsoft.Sound");
    auto hr = CreateProcessW(nullptr, lpCommandLine, nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pi);
    
    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    free(lpCommandLine);
}
