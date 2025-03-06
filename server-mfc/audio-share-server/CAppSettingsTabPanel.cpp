// CAppSettingsTabPanel.cpp : implementation file
//

#include "pch.h"
#include <afxdialogex.h>
#include "CAppSettingsTabPanel.h"
#include "resource.h"
#include "AudioShareServer.h"
#include "audio_manager.hpp"
#include "CAboutDialog.h"
#include "util.hpp"
#include "AppMsg.h"
#include "CMainDialog.h"

#include <netfw.h>

#include <wil/resource.h>
#include <wil/com.h>
#include <wil/win32_result_macros.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr int check_update_interval = 3 * 60 * 60 * 1000;

// CAppSettingsTabPanel dialog

IMPLEMENT_DYNAMIC(CAppSettingsTabPanel, CTabPanel)

CAppSettingsTabPanel::CAppSettingsTabPanel(CWnd* pParent)
    : CTabPanel(IDD_APP_SETTINGS, IDS_APP_SETTINGS, pParent)
    , m_lpszSection(L"App Settings")
    , m_bAutoRun(FALSE)
    , m_nWhenAppStart(0)
    , m_nWhenClose(0)
    , m_bAutoUpdate(FALSE)
{
}

CAppSettingsTabPanel::~CAppSettingsTabPanel()
{
}

void CAppSettingsTabPanel::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_BUTTON_REPPAIR_FIREWALL, m_buttonRepairFirewall);
    DDX_Radio(pDX, IDC_RADIO_NONE, m_nWhenAppStart);
    DDX_Radio(pDX, IDC_RADIO_EXIT, m_nWhenClose);
    DDX_Check(pDX, IDC_CHECK_AUTORUN, m_bAutoRun);
    DDX_Check(pDX, IDC_CHECK_AUTO_UPDATE, m_bAutoUpdate);
    DDX_Control(pDX, IDC_COMBO_LANGUAGE, m_comboLanguage);
}


BEGIN_MESSAGE_MAP(CAppSettingsTabPanel, CTabPanel)
    ON_BN_CLICKED(IDC_CHECK_AUTORUN, &CAppSettingsTabPanel::OnBnClickedCheckAutoRun)
    ON_BN_CLICKED(IDC_BUTTON_REPPAIR_FIREWALL, &CAppSettingsTabPanel::OnBnClickedButtonReppairFirewall)
    ON_BN_CLICKED(IDC_BUTTON_HIDE, &CAppSettingsTabPanel::OnBnClickedButtonHide)
    ON_BN_CLICKED(IDC_CHECK_AUTO_UPDATE, &CAppSettingsTabPanel::OnBnClickedCheckAutoUpdate)
    ON_COMMAND_RANGE(IDC_RADIO_NONE, IDC_RADIO_KEEP, CAppSettingsTabPanel::OnBnClickedWhenAppStart)
    ON_COMMAND_RANGE(IDC_RADIO_EXIT, IDC_RADIO_MINIMIZE, CAppSettingsTabPanel::OnBnClickedWhenCloseButton)
    ON_BN_CLICKED(IDC_BUTTON_UPDATE, &CAppSettingsTabPanel::OnBnClickedButtonUpdate)
    ON_WM_TIMER()
    ON_CBN_SELCHANGE(IDC_COMBO_LANGUAGE, &CAppSettingsTabPanel::OnCbnSelchangeComboLanguage)
END_MESSAGE_MAP()


// CAppSettingsTabPanel message handlers


BOOL CAppSettingsTabPanel::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // TODO:  Add extra initialization here
    SHSTOCKICONINFO sii{};
    sii.cbSize = sizeof(sii);
    HRESULT hr = SHGetStockIconInfo(SIID_SHIELD, SHGSI_ICON | SHGSI_SMALLICON, &sii);
    m_buttonRepairFirewall.SetIcon(sii.hIcon);
    DestroyIcon(sii.hIcon);

    m_bAutoRun = theApp.GetProfileIntW(m_lpszSection, L"AutoRun", false);
    SetAutoRun(m_bAutoRun);

    m_nWhenAppStart = theApp.GetProfileIntW(m_lpszSection, L"WhenAppStart", 0);
    m_nWhenClose = theApp.GetProfileIntW(m_lpszSection, L"WhenClose", 0);

    m_bAutoUpdate = theApp.GetProfileIntW(m_lpszSection, L"AutoUpdate", false);
    if (m_bAutoUpdate) {
        this->SetTimer(TIMER_ID_CHECK_UPDATE, check_update_interval, nullptr);
    }

    // language list
    CString s;
    std::vector<std::pair<std::wstring, std::wstring>> array = {
        { L"default", ((void)s.LoadStringW(IDS_DEFAULT), s.GetString()) },
        { L"en-US", ((void)s.LoadStringW(IDS_ENGLISH), s.GetString())},
        { L"zh-CN", ((void)s.LoadStringW(IDS_SIMPLIFIED_CHINESE), s.GetString())},
    };
    for (auto&& lang : array) {
        auto nIndex = m_comboLanguage.AddString(lang.second.c_str());
        m_comboLanguage.SetItemDataPtr(nIndex, _wcsdup(lang.first.c_str()));
    }
    auto configLanguage = theApp.GetProfileStringW(L"App", L"language", L"default");
    for (int nIndex = 0; nIndex < m_comboLanguage.GetCount(); ++nIndex) {
        if (configLanguage == (LPCWSTR)m_comboLanguage.GetItemDataPtr(nIndex)) {
            m_comboLanguage.SetCurSel(nIndex);
            break;
        }
    }

    this->UpdateData(false);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CAppSettingsTabPanel::OnBnClickedCheckAutoRun()
{
    this->UpdateData();
    SetAutoRun(m_bAutoRun);
    theApp.WriteProfileInt(m_lpszSection, L"AutoRun", m_bAutoRun);
}

void CAppSettingsTabPanel::OnBnClickedButtonReppairFirewall()
{
    try {
        auto pNetFwPolicy2 = wil::CoCreateInstance<INetFwPolicy2>(__uuidof(NetFwPolicy2));

        wil::unique_cotaskmem_string clsid;
        THROW_IF_FAILED(StringFromCLSID(__uuidof(NetFwPolicy2), &clsid));

        // https://learn.microsoft.com/en-us/windows/win32/secauthz/developing-applications-that-require-administrator-privilege
        // https://learn.microsoft.com/en-us/windows/win32/com/the-com-elevation-moniker
        BIND_OPTS3 bo{};
        bo.cbStruct = sizeof(bo);
        bo.hwnd = nullptr;
        bo.dwClassContext = CLSCTX_LOCAL_SERVER;
        auto moniker = fmt::format(L"Elevation:Administrator!new:{}", clsid.get());
        spdlog::info(L"moniker: {}", moniker);
        THROW_IF_FAILED(CoGetObject(moniker.c_str(), &bo, IID_PPV_ARGS(&pNetFwPolicy2)));

        wil::com_ptr<INetFwRules> pNetFwRules;
        THROW_IF_FAILED(pNetFwPolicy2->get_Rules(&pNetFwRules));

        long count{};
        THROW_IF_FAILED(pNetFwRules->get_Count(&count));

        spdlog::info("rule count: {}", count);

        wil::com_ptr<IUnknown> pEnumerator;
        pNetFwRules->get__NewEnum(&pEnumerator);

        auto pVariant = pEnumerator.query<IEnumVARIANT>();

        std::filesystem::path exe_path(theApp.m_exePath);

        // https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ics/c-enumerating-firewall-rules
        while (true) {
            wil::unique_variant var;
            ULONG cFecthed = 0;
            if (pVariant->Next(1, &var, &cFecthed) != S_OK) {
                break;
            }

            wil::com_ptr<INetFwRule> pNetFwRule;
            var.pdispVal->QueryInterface(__uuidof(INetFwRule), (void**)&pNetFwRule);

            _bstr_t app_name;
            pNetFwRule->get_ApplicationName(app_name.GetAddress());
            std::error_code ec{};
            if (!app_name || !std::filesystem::equivalent(exe_path, (wchar_t*)app_name, ec)) {
                continue;
            }

            pNetFwRule->put_Name(app_name);
            pNetFwRules->Remove(app_name);
            spdlog::info(L"remove firewall rule: {}", (wchar_t*)app_name);
        }

        long profileTypesBitmask{};
        THROW_IF_FAILED(pNetFwPolicy2->get_CurrentProfileTypes(&profileTypesBitmask));
        if ((profileTypesBitmask & NET_FW_PROFILE2_PUBLIC) && (profileTypesBitmask != NET_FW_PROFILE2_PUBLIC)) {
            profileTypesBitmask ^= NET_FW_PROFILE2_PUBLIC;
        }

        auto pNetFwRule = wil::CoCreateInstance<INetFwRule3>(__uuidof(NetFwRule));
        pNetFwRule->put_Enabled(VARIANT_TRUE);
        pNetFwRule->put_Action(NET_FW_ACTION_ALLOW);
        pNetFwRule->put_ApplicationName(_bstr_t(exe_path.c_str()));
        pNetFwRule->put_Profiles(profileTypesBitmask);

        pNetFwRule->put_Name(_bstr_t(L"Audio Share Server(TCP-In)"));
        pNetFwRule->put_Description(_bstr_t(L"Audio Share Server(TCP-In)"));
        pNetFwRule->put_Protocol(NET_FW_IP_PROTOCOL_TCP);
        THROW_IF_FAILED((pNetFwRules->Add(pNetFwRule.get())));

        pNetFwRule->put_Name(_bstr_t(L"Audio Share Server(UDP-In)"));
        pNetFwRule->put_Description(_bstr_t(L"Audio Share Server(UDP-In)"));
        pNetFwRule->put_Protocol(NET_FW_IP_PROTOCOL_UDP);
        THROW_IF_FAILED((pNetFwRules->Add(pNetFwRule.get())));

        AfxMessageBox(IDS_SUCCESS, MB_OK | MB_ICONINFORMATION);
    }
    catch (...) {
        auto err_msg = wstr_win_err(wil::Win32ErrorFromCaughtException());
        AfxMessageBox(err_msg.c_str(), MB_OK | MB_ICONSTOP);
    }
}

void CAppSettingsTabPanel::SetAutoRun(bool bEnable)
{
    HKEY key;
    RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &key);
    if (bEnable) {
        auto value = theApp.m_exePath + L" /hide";
        RegSetValueExW(key, L"AudioShareServer", 0, REG_SZ, (const BYTE*)value.c_str(), (DWORD)(value.length() + 1) * sizeof(wchar_t));
    }
    else {
        RegDeleteValueW(key, L"AudioShareServer");
    }
    RegCloseKey(key);
}

void CAppSettingsTabPanel::OnBnClickedButtonHide()
{
    theApp.HideApplication();
}

void CAppSettingsTabPanel::OnBnClickedCheckAutoUpdate()
{
    this->UpdateData();
    theApp.WriteProfileInt(m_lpszSection, L"AutoUpdate", m_bAutoUpdate);
    if (m_bAutoUpdate) {
        this->SetTimer(TIMER_ID_CHECK_UPDATE, check_update_interval, nullptr);
    }
    else {
        this->KillTimer(TIMER_ID_CHECK_UPDATE);
    }
}

void CAppSettingsTabPanel::OnBnClickedWhenAppStart(UINT nID)
{
    this->UpdateData();
    theApp.WriteProfileInt(m_lpszSection, L"WhenAppStart", m_nWhenAppStart);
}

void CAppSettingsTabPanel::OnBnClickedWhenCloseButton(UINT nID)
{
    this->UpdateData();
    theApp.WriteProfileInt(m_lpszSection, L"WhenClose", m_nWhenClose);
}

void CAppSettingsTabPanel::OnBnClickedButtonUpdate()
{
    this->CheckForUpdate(true);
}

void CAppSettingsTabPanel::CheckForUpdate(bool bPromptError)
{
    std::thread([=] {
        try
        {
            CInternetSession session(
                L"Audio Share Server",
                INTERNET_NO_CALLBACK,
                INTERNET_OPEN_TYPE_DIRECT,
                0, 0,
                INTERNET_FLAG_DONT_CACHE
            );
            auto httpFile = (CHttpFile*)session.OpenURL(
                L"https://api.github.com/repos/mkckr0/audio-share/releases/latest",
                INTERNET_NO_CALLBACK,
                INTERNET_FLAG_TRANSFER_BINARY | INTERNET_FLAG_RELOAD
            );
            if (!httpFile) {
                return;
            }
            auto cleanup = wil::scope_exit([&] {
                httpFile->Close();
            });

            std::string response;
            char buf[1024];
            while (int len = httpFile->Read(buf, sizeof(buf))) {
                response.append(buf, len);
            }

            auto res = json::parse(response);
            std::string version("v");
            version += CW2A(CAboutDialog::GetStringFileInfo(L"ProductVersion"));
            std::string tag_name = res["tag_name"];
            if (util::is_newer_version(tag_name, version)) {
                auto pMainDialog = theApp.GetMainDialog();
                pMainDialog->SetUpdateLink(CA2W(std::string(res["html_url"]).c_str()));
                CString s;
                (void)s.LoadStringW(IDS_NEW_VERSION);
                pMainDialog->ShowBalloonNotification(s, CA2W(tag_name.c_str()));
            }
            else {
                if (bPromptError) {
                    AfxMessageBox(IDS_NO_UPDATE, MB_OK | MB_ICONINFORMATION);
                }
            }
        }
        catch (const std::exception& e) {
            spdlog::error("CAppSettingsTabPanel::OnBnClickedButtonUpdate: {}", e.what());
            if (bPromptError) {
                AfxMessageBox(CA2W(e.what()), MB_OK | MB_ICONSTOP);
            }
        }
        catch (CException* e) {
            WCHAR lpszError[512];
            UINT nHelpContext;
            e->GetErrorMessage(lpszError, _countof(lpszError), &nHelpContext);
            spdlog::error(L"CAppSettingsTabPanel::OnBnClickedButtonUpdate: {}", lpszError);
            if (bPromptError) {
                AfxMessageBox(lpszError, MB_OK | MB_ICONSTOP);
            }
        }
    }).detach();
}

void CAppSettingsTabPanel::OnTimer(UINT_PTR nIDEvent)
{
    // TODO: Add your message handler code here and/or call default
    if (nIDEvent == TIMER_ID_CHECK_UPDATE) {
        CheckForUpdate(false);
    }

    CTabPanel::OnTimer(nIDEvent);
}


void CAppSettingsTabPanel::OnCbnSelchangeComboLanguage()
{
    auto lang = (LPCWSTR)m_comboLanguage.GetItemDataPtr(m_comboLanguage.GetCurSel());
    theApp.WriteProfileStringW(L"App", L"language", lang);
}
