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
#include "CMainDialog.h"
#include "CServerTabPanel.h"
#include "CAppSettingsTabPanel.h"
#include "CTabPanel.h"
#include "AppMsg.h"
#include "CAboutDialog.h"

#include <afxdialogex.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMainDialog dialog


CMainDialog::CMainDialog(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_MAIN, pParent)
    , m_tabPanelServer(nullptr)
{
    m_hIcon = theApp.LoadIconW(IDR_MAINFRAME);

    auto hr = CoInitializeEx(nullptr, COINIT::COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        if (hr == S_FALSE) {
            CoUninitialize();
        }
        exit(-1);
    }

}

CMainDialog::~CMainDialog()
{
    CoUninitialize();
}

// copy from https://devblogs.microsoft.com/oldnewthing/20190802-00/?p=102747
COLORREF CMainDialog::GetBrushColor(HBRUSH brush)
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

void CMainDialog::ShowNotificationIcon(BOOL bShow)
{
    if (bShow) {
        NOTIFYICONDATA nid{};
        nid.uVersion = NOTIFYICON_VERSION_4;
        nid.cbSize = sizeof(nid);
        nid.hWnd = this->GetSafeHwnd();
        nid.uID = 0;
        nid.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
        nid.hIcon = m_hIcon;
        wcscpy(nid.szTip, L"Audio Share Server");
        nid.uCallbackMessage = WM_APP_NOTIFYICON;
        Shell_NotifyIconW(NIM_ADD, &nid);
    }
    else {
        NOTIFYICONDATA nid{};
        nid.uVersion = NOTIFYICON_VERSION_4;
        nid.cbSize = sizeof(nid);
        nid.hWnd = this->GetSafeHwnd();
        nid.uID = 0;
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }
}

void CMainDialog::ShowBalloonNotification(LPCWSTR lpszInfoTitle, LPCWSTR lpszInfo)
{
    NOTIFYICONDATA nid{};
    nid.uVersion = NOTIFYICON_VERSION_4;
    nid.cbSize = sizeof(nid);
    nid.hWnd = this->GetSafeHwnd();
    nid.uID = 0;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
    wcscpy(nid.szTip, L"Audio Share Server");
    wcscpy(nid.szInfoTitle, lpszInfoTitle);
    wcscpy(nid.szInfo, lpszInfo);
    nid.dwInfoFlags = NIIF_INFO | NIIF_LARGE_ICON;
    nid.hIcon = m_hIcon;
    nid.hBalloonIcon = nid.hIcon;
    nid.uCallbackMessage = WM_APP_NOTIFYICON;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void CMainDialog::SetUpdateLink(LPCWSTR lpszUpdateLink)
{
    m_strUpdateLink = lpszUpdateLink;
}

void CMainDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TAB, m_tabCtrl);
}


BEGIN_MESSAGE_MAP(CMainDialog, CDialogEx)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_MESSAGE(WM_APP_NOTIFYICON, &CMainDialog::OnNotifyIcon)
    ON_WM_DESTROY()
    ON_WM_CTLCOLOR()
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB, &CMainDialog::OnTcnSelchangeTab)
    ON_WM_CLOSE()
    ON_COMMAND(ID_APP_SHOW, &CMainDialog::OnAppShow)
    ON_COMMAND(ID_APP_ABOUT, &CMainDialog::OnAppAbout)
    ON_COMMAND(ID_START_SERVER, &CMainDialog::OnStartServer)
    ON_UPDATE_COMMAND_UI(ID_START_SERVER, &CMainDialog::OnUpdateStartServer)
    ON_COMMAND(ID_APP_EXIT, &CMainDialog::OnAppExit)
END_MESSAGE_MAP()


// CMainDialog message handlers

BOOL CMainDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

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

    // TODO: Add extra initialization here
    ShowNotificationIcon(true);

    if (theApp.GetContextMenuManager()->AddMenu(L"SystemTray", IDR_MENU_SYSTEM_TRAY) == FALSE) {
        AfxMessageBox(L"AddMenu Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    ShowWindow(theApp.m_bHide ? SW_HIDE : SW_SHOW);

    // add tab items
    m_vecTabPanel = {
        m_tabPanelServer = new CServerTabPanel(this),
        new CAppSettingsTabPanel(this),
    };

    m_tabCtrl.SetItemSize(CSize(0, dpToPx(30)));
    m_tabCtrl.SetPadding(CSize(dpToPx(20), 0));

    CRect childRect, tabRect, itemRect;
    m_tabCtrl.GetClientRect(&childRect);
    m_tabCtrl.GetItemRect(0, &itemRect);
    m_tabCtrl.GetWindowRect(tabRect);
    this->ScreenToClient(&tabRect);
    childRect.top += tabRect.top + itemRect.Height();
    childRect.left += tabRect.left;
    for (int i = 0; auto panel : m_vecTabPanel) {
        panel->Create();
        m_tabCtrl.InsertItem(i, panel->m_strTitle);
        panel->MoveWindow(childRect);
        panel->ShowWindow(SW_HIDE);
        ++i;
    }
    m_vecTabPanel[0]->ShowWindow(SW_SHOW);

    return TRUE;  // return TRUE  unless you set the focus to a control
}

void CMainDialog::PostNcDestroy()
{
    delete this;
}

void CMainDialog::OnOK()
{
}

void CMainDialog::OnCancel()
{
}

void CMainDialog::OnClose()
{
    if (theApp.GetProfileIntW(L"App Settings", L"WhenClose", 0) == 0) {
        //https://learn.microsoft.com/en-us/cpp/mfc/destroying-the-dialog-box
        DestroyWindow();
    }
    else {
        theApp.HideApplication();
    }
}

void CMainDialog::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
        this->OnAppAbout();
    }
    else
    {
        CDialogEx::OnSysCommand(nID, lParam);
    }
}

LRESULT CMainDialog::OnNotifyIcon(WPARAM wParam, LPARAM lParam)
{
    auto event = LODWORD(lParam);
    auto icon_id = HIDWORD(lParam);
    auto pos_x = GET_X_LPARAM(wParam);
    auto pos_y = GET_Y_LPARAM(wParam);

    if (event == WM_LBUTTONDOWN) {
        this->SendMessageW(WM_COMMAND, ID_APP_SHOW);
    }
    else if (event == WM_RBUTTONDOWN) {
        // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackpopupmenu#remarks
        SetForegroundWindow();
        POINT pos{};
        GetCursorPos(&pos);
        theApp.GetContextMenuManager()->ShowPopupMenu(IDR_MENU_SYSTEM_TRAY, pos.x, pos.y, this, TRUE);
    }
    else if (event == NIN_BALLOONUSERCLICK) {
        ShellExecuteW(nullptr, nullptr, m_strUpdateLink, nullptr, nullptr, 0);
    }

    return 0;
}

void CMainDialog::OnDestroy()
{
    CDialogEx::OnDestroy();

    // TODO: Add your message handler code here
    ShowNotificationIcon(false);
}

HBRUSH CMainDialog::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

    // handle Transparent background, https://devblogs.microsoft.com/oldnewthing/20111028-00/?p=9243
    if (pWnd->GetExStyle() & WS_EX_TRANSPARENT) {
        pDC->SetBkMode(TRANSPARENT);
        hbr = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
    }

    return hbr;
}

void CMainDialog::OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult)
{
    // TODO: Add your control notification handler code here
    for (int i = 0; i < m_vecTabPanel.size(); ++i) {
        m_vecTabPanel[i]->ShowWindow(i == m_tabCtrl.GetCurSel() ? SW_SHOW : SW_HIDE);
    }

    *pResult = 0;
}

void CMainDialog::OnAppShow()
{
    ShowWindow(SW_SHOW);
    ShowWindow(SW_NORMAL);
    SetForegroundWindow();
}

void CMainDialog::OnAppAbout()
{
    CAboutDialog aboutDialog;
    aboutDialog.DoModal();
}

void CMainDialog::OnUpdateStartServer(CCmdUI* pCmdUI)
{
    CString s;
    s.LoadStringW(m_tabPanelServer->IsRunning() ? IDS_STOP_SERVER : IDS_START_SERVER);
    pCmdUI->SetText(s);
}

void CMainDialog::OnStartServer()
{
    m_tabPanelServer->SwitchServer();
}

void CMainDialog::OnAppExit()
{
    DestroyWindow();
}
