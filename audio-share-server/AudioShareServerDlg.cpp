
// audio-share-serverDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "AudioShareServer.h"
#include "AudioShareServerDlg.h"
#include "afxdialogex.h"
#include "audio_manager.hpp"
#include "network_manager.hpp"
#include "common.hpp"

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


// CaudioshareserverDlg dialog



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
	m_ComboBoxHost.EnableWindow(bEnable);
	m_EditPort.EnableWindow(bEnable);
	m_ComboBoxAudioEndpoint.EnableWindow(bEnable);
}

void CAudioShareServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_HOST, m_ComboBoxHost);
	DDX_Control(pDX, IDC_EDIT_PORT, m_EditPort);
	DDX_Control(pDX, IDC_COMBO_AUDIO_ENDPOINT, m_ComboBoxAudioEndpoint);
	DDX_Control(pDX, IDC_STATIC_SERVER, m_ButtonServer);
}

BEGIN_MESSAGE_MAP(CAudioShareServerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_STATIC_SERVER, &CAudioShareServerDlg::OnBnClickedStartServer)
END_MESSAGE_MAP()


// CaudioshareserverDlg message handlers

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

	ShowWindow(SW_SHOW);

	// TODO: Add extra initialization here

	// set default host and port
	auto address_list = network_manager::get_local_addresss();
	for (auto address : address_list) {
		auto nIndex = m_ComboBoxHost.AddString(address.c_str());
	}
	if (m_ComboBoxHost.GetCount()) {
		m_ComboBoxHost.SetCurSel(0);
	}

	m_EditPort.SetLimitText(5);
	m_EditPort.SetWindowTextW(L"65530");

	// init endpoint list
	m_endpoint_map = audio_manager::get_audio_endpoint_map();
	for (auto&& [id, name] : m_endpoint_map) {
		int nIndex = m_ComboBoxAudioEndpoint.AddString(name.c_str());
		m_ComboBoxAudioEndpoint.SetItemDataPtr(nIndex, (void*)&id);
	}
	
	auto default_id = audio_manager::get_default_endpoint();
	for (int nIndex = 0; nIndex < m_ComboBoxAudioEndpoint.GetCount(); ++nIndex) {
		auto endpoint = *(std::wstring*)m_ComboBoxAudioEndpoint.GetItemDataPtr(nIndex);
		if (endpoint == default_id) {
			m_ComboBoxAudioEndpoint.SetCurSel(nIndex);
			break;
		}
	}
	
	// create network_manager
	m_network_manger = std::make_unique<network_manager>();

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
	if (!m_network_manger) {
		AfxMessageBox(L"network_manager is nullptr", MB_OK | MB_ICONSTOP);
		EndDialog(-1);
		return;
	}

	CString text;
	m_ButtonServer.GetWindowText(text);
	if (text == L"Start Server") {
		// start
		EnableInputControls(false);
		m_ButtonServer.EnableWindow(false);

		CString host_str, port_str;
		m_ComboBoxHost.GetWindowText(host_str);
		m_EditPort.GetWindowText(port_str);
		std::string host = wchars_to_mbs(host_str);
		std::uint16_t port = std::stoi(wchars_to_mbs(port_str.GetString()));
		auto endpoint = *(std::wstring*)m_ComboBoxAudioEndpoint.GetItemDataPtr(m_ComboBoxAudioEndpoint.GetCurSel());
		try {
			m_network_manger->start_server(host, port, endpoint);
		}
		catch (std::exception& e) {
			AfxMessageBox(CString(e.what()), MB_OK | MB_ICONSTOP);
			EnableInputControls(true);
			m_ButtonServer.EnableWindow(true);
			m_ButtonServer.SetWindowText(L"Start Server");
			return;
		}

		m_ButtonServer.EnableWindow(true);
		m_ButtonServer.SetWindowText(L"Stop Server");
	}
	else {
		// stop
		m_ButtonServer.EnableWindow(false);
		m_network_manger->stop_server();
		
		EnableInputControls(true);
		m_ButtonServer.EnableWindow(true);
		m_ButtonServer.SetWindowText(L"Start Server");
	}
}
