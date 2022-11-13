
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

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AUDIOSHARESERVER_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CComboBox m_ComboBoxHost;
	CEdit m_EditPort;
	CComboBox m_ComboBoxAudioEndpoint;
	CButton m_ButtonServer;
	afx_msg void OnBnClickedStartServer();
};
