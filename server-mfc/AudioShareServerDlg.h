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

#include <mmdeviceapi.h>

class audio_manager;
class network_manager;

// CAudioShareServerDlg dialog
class CAudioShareServerDlg : public CDialogEx
{
// Construction
public:
	CAudioShareServerDlg(CWnd* pParent = nullptr);	// standard constructor
	~CAudioShareServerDlg();

private:
	std::shared_ptr<audio_manager> m_audio_manager;
	std::shared_ptr<network_manager> m_network_manager;

	void EnableInputControls(bool bEnable = true);
	COLORREF GetBrushColor(HBRUSH brush);
	bool ShowNotifyIcon(bool bEnable = true);
	void SetAutoRun(bool bEnable);

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AUDIOSHARESERVER_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;	// DDX/DDV support
	virtual void PostNcDestroy() override;
	virtual void OnCancel() override;

// Implementation
protected:
	HICON m_hIcon;
	const static UINT m_uNotifyIconID = 0;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedStartServer();
	afx_msg void OnBnClickedButtonHide();
	afx_msg void OnBnClickedButtonRefresh();
	afx_msg LRESULT OnNotifyIcon(WPARAM wParam, LPARAM lParam);
	afx_msg void OnDestroy();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnBnClickedCheckAutoRun();

	DECLARE_MESSAGE_MAP()
public:
	CComboBox m_comboBoxHost;
	CEdit m_editPort;
	CComboBox m_comboBoxAudioEndpoint;
	CButton m_buttonServer;
	CButton m_buttonRefresh;
	CButton m_buttonAutoRun;
};
