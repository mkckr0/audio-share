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


// audio-share-serverDlg.h : header file
//

#pragma once

#include <map>
#include <string>
#include <memory>
#include <vector>

class audio_manager;
class network_manager;
class CTabPanel;
class CServerTabPanel;

// CMainDialog dialog
class CMainDialog : public CDialogEx
{
// Construction
public:
	CMainDialog(CWnd* pParent = nullptr);	// standard constructor
	~CMainDialog();

private:
	COLORREF GetBrushColor(HBRUSH brush);

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MAIN };
#endif

public:
	void ShowNotificationIcon(BOOL bShow);
	void ShowBalloonNotification(LPCWSTR lpszInfoTitle, LPCWSTR lpszInfo);
	void SetUpdateLink(LPCWSTR lpszUpdateLink);

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;	// DDX/DDV support

// Implementation
protected:

	// Generated message map functions
	DECLARE_MESSAGE_MAP()

	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnClose();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg LRESULT OnNotifyIcon(WPARAM wParam, LPARAM lParam);
	afx_msg void OnDestroy();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAppShow();
	afx_msg void OnAppAbout();
	afx_msg void OnStartServer();
	afx_msg void OnUpdateStartServer(CCmdUI* pCmdUI);
	afx_msg void OnAppExit();

public:
	CString m_strUpdateLink;
	HICON m_hIcon;
	CTabCtrl m_tabCtrl;
	std::vector<CTabPanel*> m_vecTabPanel;
	CServerTabPanel* m_tabPanelServer;
};
