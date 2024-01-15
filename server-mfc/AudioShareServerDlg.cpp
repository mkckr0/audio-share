/*
   Copyright 2022-2024 mkckr0 <https://github.com/mkckr0>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/


// audio-share-serverDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "AudioShareServer.h"
#include "AudioShareServerDlg.h"

#include <afxdialogex.h>

#include "audio_manager.hpp"
#include "network_manager.hpp"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
    CAboutDlg();

    // Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ABOUTBOX };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

    // Implementation
protected:
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CAudioShareServerDlg dialog

#define WM_APP_NOTIFYICON (WM_APP + 1)


CAudioShareServerDlg::CAudioShareServerDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_AUDIOSHARESERVER_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    auto hr = CoInitializeEx(nullptr, COINIT::COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        if (hr == S_FALSE) {
            CoUninitialize();
        }
        exit(-1);
    }
}

CAudioShareServerDlg::~CAudioShareServerDlg()
{
    CoUninitialize();
}

void CAudioShareServerDlg::EnableInputControls(bool bEnable)
{
    m_comboBoxHost.EnableWindow(bEnable);
    m_editPort.EnableWindow(bEnable);
    m_comboBoxAudioEndpoint.EnableWindow(bEnable);
    m_buttonRefresh.EnableWindow(bEnable);
}

// copy from https://devblogs.microsoft.com/oldnewthing/20190802-00/?p=102747
COLORREF CAudioShareServerDlg::GetBrushColor(HBRUSH brush)
{
    LOGBRUSH lbr;
    if (GetObject(brush, sizeof(lbr), &lbr) != sizeof(lbr)) {
        // Not even a brush!
        return CLR_NONE;
    }
    if (lbr.lbStyle != BS_SOLID) {
        // Not a solid color brush.
        return CLR_NONE;
    }
    return lbr.lbColor;
}

bool CAudioShareServerDlg::ShowNotifyIcon(bool bEnable)
{
    if (bEnable) {
        NOTIFYICONDATA nid{};
        nid.uVersion = NOTIFYICON_VERSION_4;
        nid.cbSize = sizeof(nid);
        nid.hWnd = GetSafeHwnd();
        nid.uID = m_uNotifyIconID;
        nid.uFlags = NIF_ICON | NIF_MESSAGE;
        LoadIconMetric(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MAINFRAME), LIM_SMALL, &(nid.hIcon));
        nid.uCallbackMessage = WM_APP_NOTIFYICON;
        return Shell_NotifyIcon(NIM_ADD, &nid);
    }
    else {
        NOTIFYICONDATA nid = {};
        nid.uVersion = NOTIFYICON_VERSION_4;
        nid.cbSize = sizeof(nid);
        nid.hWnd = GetSafeHwnd();
        nid.uID = m_uNotifyIconID;
        nid.uFlags = NIF_GUID;
        return Shell_NotifyIcon(NIM_DELETE, &nid);
    }
}

void CAudioShareServerDlg::SetAutoRun(bool bEnable)
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

void CAudioShareServerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO_HOST, m_comboBoxHost);
    DDX_Control(pDX, IDC_EDIT_PORT, m_editPort);
    DDX_Control(pDX, IDC_COMBO_AUDIO_ENDPOINT, m_comboBoxAudioEndpoint);
    DDX_Control(pDX, IDC_BUTTON_SERVER, m_buttonServer);
    DDX_Control(pDX, IDC_BUTTON_REFRESH, m_buttonRefresh);
    DDX_Control(pDX, IDC_CHECK_AUTORUN, m_buttonAutoRun);
}

void CAudioShareServerDlg::PostNcDestroy()
{
    delete this;
}

void CAudioShareServerDlg::OnCancel()
{
    // https://learn.microsoft.com/en-us/cpp/mfc/destroying-the-dialog-box
    DestroyWindow();
}

BEGIN_MESSAGE_MAP(CAudioShareServerDlg, CDialogEx)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BUTTON_SERVER, &CAudioShareServerDlg::OnBnClickedStartServer)
    ON_BN_CLICKED(IDC_BUTTON_HIDE, &CAudioShareServerDlg::OnBnClickedButtonHide)
    ON_MESSAGE(WM_APP_NOTIFYICON, &CAudioShareServerDlg::OnNotifyIcon)
    ON_WM_DESTROY()
    ON_WM_CTLCOLOR()
    ON_BN_CLICKED(IDC_BUTTON_REFRESH, &CAudioShareServerDlg::OnBnClickedButtonRefresh)
    ON_BN_CLICKED(IDC_CHECK_AUTORUN, &CAudioShareServerDlg::OnBnClickedCheckAutoRun)
END_MESSAGE_MAP()


// CAudioShareServerDlg message handlers

BOOL CAudioShareServerDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Add "About..." menu item to system menu.

    // IDM_ABOUTBOX must be in the system command range.
    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != nullptr)
    {
        BOOL bNameValid;
        CString strAboutMenu;
        bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
        ASSERT(bNameValid);
        if (!strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);			// Set big icon

    ShowNotifyIcon();

    if (theApp.GetContextMenuManager()->AddMenu(L"SystemTray", IDR_MENU_SYSTEM_TRAY) == FALSE) {
        AfxMessageBox(L"AddMenu Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    // TODO: Add extra initialization here
    ShowWindow(theApp.m_bHide ? SW_HIDE : SW_SHOW);

    // set default host and port
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

    m_editPort.SetLimitText(5);
    m_editPort.SetWindowTextW(theApp.GetProfileStringW(L"Network", L"port", L"65530"));

    // init endpoint list
    this->OnBnClickedButtonRefresh();

    CPngImage pngImage;
    pngImage.Load(IDB_PNG_REFRESH);
    m_buttonRefresh.SetBitmap(pngImage);

    // create network_manager
    m_audio_manager = std::make_shared<audio_manager>();
    m_network_manager = std::make_shared<network_manager>(m_audio_manager);

    bool configAutoRun = theApp.GetProfileIntW(L"App", L"AutoRun", false);
    SetAutoRun(configAutoRun);
    m_buttonAutoRun.SetCheck(configAutoRun);

    if (theApp.GetProfileIntW(L"App", L"Running", false)) {
        m_buttonServer.PostMessageW(BM_CLICK);
    }

    return TRUE;  // return TRUE  unless you set the focus to a control
}

void CAudioShareServerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }
    else
    {
        CDialogEx::OnSysCommand(nID, lParam);
    }
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CAudioShareServerDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
    }
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CAudioShareServerDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}


void CAudioShareServerDlg::OnBnClickedStartServer()
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
            return;
        }

        m_buttonServer.EnableWindow(true);
        m_buttonServer.SetWindowText(L"Stop Server");
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
        theApp.WriteProfileInt(L"App", L"Running", false);
    }
}

void CAudioShareServerDlg::OnBnClickedButtonHide()
{
    ShowWindow(SW_HIDE);
}

void CAudioShareServerDlg::OnBnClickedButtonRefresh()
{
    m_comboBoxAudioEndpoint.Clear();    // clear current selection
    for (int nIndex = m_comboBoxAudioEndpoint.GetCount() - 1; nIndex >= 0; --nIndex) {
        free(m_comboBoxAudioEndpoint.GetItemDataPtr(nIndex));
        m_comboBoxAudioEndpoint.DeleteString(nIndex);
    }

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

LRESULT CAudioShareServerDlg::OnNotifyIcon(WPARAM wParam, LPARAM lParam)
{
    auto event = LODWORD(lParam);
    auto icon_id = HIDWORD(lParam);
    auto pos_x = GET_X_LPARAM(wParam);
    auto pos_y = GET_Y_LPARAM(wParam);

    if (event == WM_LBUTTONDOWN) {
        ShowWindow(SW_SHOW);
        ShowWindow(SW_NORMAL);
        SetForegroundWindow();
    }
    else if (event == WM_RBUTTONDOWN) {
        // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackpopupmenu#remarks
        SetForegroundWindow();
        POINT pos{};
        GetCursorPos(&pos);
        theApp.GetContextMenuManager()->ShowPopupMenu(IDR_MENU_SYSTEM_TRAY, pos.x, pos.y, this, TRUE);
    }

    return 0;
}

void CAudioShareServerDlg::OnDestroy()
{
    ShowNotifyIcon(false);
}

HBRUSH CAudioShareServerDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

    // handle Transparent background, https://devblogs.microsoft.com/oldnewthing/20111028-00/?p=9243
    if (pWnd->GetExStyle() & WS_EX_TRANSPARENT) {
        pDC->SetBkMode(TRANSPARENT);
    }

    if (nCtlColor == CTLCOLOR_DLG || nCtlColor == CTLCOLOR_STATIC) {
        if (HBRUSH sysBrush = GetSysColorBrush(COLOR_WINDOW)) {
            hbr = sysBrush;
        }
    }

    return hbr;
}


void CAudioShareServerDlg::OnBnClickedCheckAutoRun()
{
    SetAutoRun(m_buttonAutoRun.GetCheck());
    theApp.WriteProfileInt(L"App", L"AutoRun", m_buttonAutoRun.GetCheck());
}
