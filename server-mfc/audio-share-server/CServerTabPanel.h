#pragma once
#include <memory>

#include "CTabPanel.h"

class audio_manager;
class network_manager;

// CServerTabPanel dialog

class CServerTabPanel : public CTabPanel
{
	DECLARE_DYNAMIC(CServerTabPanel)
public:
	CServerTabPanel(CWnd* pParent);
	virtual ~CServerTabPanel();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SERVER };
#endif

public:
	void SwitchServer();
	bool IsRunning();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedButtonReset();
	afx_msg void OnBnClickedStartServer();
	afx_msg void OnBnClickedButtonSoundPanel();
	void EnableInputControls(bool bEnable = true);

public:
	std::shared_ptr<audio_manager> m_audio_manager;
	std::shared_ptr<network_manager> m_network_manager;
	CComboBox m_comboBoxHost;
	CEdit m_editPort;
	CComboBox m_comboBoxAudioEndpoint;
	CButton m_buttonServer;
	CButton m_buttonReset;
	CComboBox m_comboEncoding;
	CButton m_buttonSoundPanel;
	bool m_bStarted;
};
