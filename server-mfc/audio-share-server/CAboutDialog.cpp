// CAboutDialog.cpp : implementation file
//

#include "pch.h"
#include "afxdialogex.h"
#include "CAboutDialog.h"
#include "resource.h"

#pragma comment(lib, "Version.lib")

// CAboutDialog dialog

IMPLEMENT_DYNAMIC(CAboutDialog, CDialogEx)

CAboutDialog::CAboutDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_ABOUTBOX, pParent)
	, m_strVersion(_T(""))
	, m_strCopyright(_T(""))
{

}

CAboutDialog::~CAboutDialog()
{
}

CString CAboutDialog::GetStringFileInfo(LPCWSTR lpszName)
{
	CString value;

	HINSTANCE hModule = AfxFindResourceHandle(MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
	if (hModule == nullptr) {
		return value;
	}
	HRSRC hResInfo = FindResourceW(hModule, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
	if (hResInfo == nullptr) {
		return value;
	}
	HGLOBAL hResData = LoadResource(hModule, hResInfo);
	if (hResData == nullptr) {
		return value;
	}
	LPVOID pBlock = LockResource(hResData);
	if (pBlock == nullptr) {
		FreeResource(hResData);
		return value;
	}

	CString strSubBlock = L"\\StringFileInfo\\040904B0\\";
	strSubBlock.Append(lpszName);

	// https://learn.microsoft.com/en-us/windows/win32/api/winver/nf-winver-verqueryvaluew
	LPCWSTR lpBuffer{};
	UINT uLen{};
	if (!VerQueryValueW(pBlock, strSubBlock, (LPVOID*)&lpBuffer, &uLen)) {
		FreeResource(hResData);
		return value;
	}
	value = lpBuffer;

	FreeResource(hResData);

	return value;
}

void CAboutDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_NAME, m_staticName);
	DDX_Text(pDX, IDC_STATIC_VERSION, m_strVersion);
	DDX_Text(pDX, IDC_STATIC_COPYRIGHT, m_strCopyright);
}


BEGIN_MESSAGE_MAP(CAboutDialog, CDialogEx)
END_MESSAGE_MAP()


// CAboutDialog message handlers


BOOL CAboutDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  Add extra initialization here

	m_staticName.SetWindowTextW(GetStringFileInfo(L"ProductName"));
	m_fontName.CreatePointFont(240, L"Segoe UI");
	m_staticName.SetFont(&m_fontName);
	
	m_strVersion = GetStringFileInfo(L"ProductVersion");
	m_strCopyright = GetStringFileInfo(L"LegalCopyright");

	this->UpdateData(false);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
