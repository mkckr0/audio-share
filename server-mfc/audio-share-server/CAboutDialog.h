#pragma once
#include "afxdialogex.h"


// CAboutDialog dialog

class CAboutDialog : public CDialogEx
{
	DECLARE_DYNAMIC(CAboutDialog)

public:
	CAboutDialog(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CAboutDialog();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	static CString GetStringFileInfo(LPCWSTR lpszName);

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

public:
	virtual BOOL OnInitDialog();
	CStatic m_staticName;
	CString m_strVersion;
	CString m_strCopyright;
	CFont m_fontName;
};
