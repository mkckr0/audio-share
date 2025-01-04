#pragma once

#include <afxdialogex.h>

// CTabPanel
// only for modaless dialog
class CTabPanel : public CDialogEx
{
    DECLARE_DYNAMIC(CTabPanel)

public:
    CTabPanel(UINT nIDTemplate, UINT nIDTitle, CWnd* pParent);
    virtual ~CTabPanel();

// Attributes:
public:
    CString m_strTitle;

// Operations
    BOOL Create();

// Overrides
public:
    

// Implementation
protected:
    DECLARE_MESSAGE_MAP()
    virtual void PostNcDestroy();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};

