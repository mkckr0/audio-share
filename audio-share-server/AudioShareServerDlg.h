
// audio-share-serverDlg.h : header file
//

#pragma once

#include <map>
#include <string>
#include <memory>

#include <mmdeviceapi.h>

class network_manager;

// CAudioShareServerDlg dialog
class CAudioShareServerDlg : public CDialogEx
{
// Construction
public:
	CAudioShareServerDlg(CWnd* pParent = nullptr);	// standard constructor
	~CAudioShareServerDlg();

private:
	std::map<std::wstring, std::wstring> m_endpoint_map;	// <id, name>
	std::unique_ptr<network_manager> m_network_manger;
	void EnableInputControls(bool bEnable = true);
	COLORREF GetBrushColor(HBRUSH brush);
	bool ShowNotifyIcon(bool bEnable = true);

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AUDIOSHARESERVER_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	

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
	afx_msg LRESULT OnNotifyIcon(WPARAM wParam, LPARAM lParam);
	afx_msg void OnDestroy();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

	DECLARE_MESSAGE_MAP()
public:
	CComboBox m_comboBoxHost;
	CEdit m_editPort;
	CComboBox m_comboBoxAudioEndpoint;
	CButton m_buttonServer;
};
