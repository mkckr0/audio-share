#include "pch.h"
#include "CTabPanel.h"
#include <resource.h>

IMPLEMENT_DYNAMIC(CTabPanel, CDialogEx)

CTabPanel::CTabPanel(UINT nIDTemplate, UINT nIDTitle, CWnd* pParent)
    : CDialogEx(nIDTemplate, pParent)
{
    (void)m_strTitle.LoadStringW(nIDTitle);
}

CTabPanel::~CTabPanel()
{
}

BOOL CTabPanel::Create()
{
    return CDialogEx::Create(m_lpszTemplateName, m_pParentWnd);
}


BEGIN_MESSAGE_MAP(CTabPanel, CDialogEx)
	ON_WM_CTLCOLOR()
END_MESSAGE_MAP()


void CTabPanel::PostNcDestroy()
{
    delete this;
}


HBRUSH CTabPanel::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (pWnd->GetDlgCtrlID() == IDC_CHECK_AUTORUN) {
        (HBRUSH)GetStockObject(WHITE_BRUSH);
    }
    HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

    // handle Transparent background, https://devblogs.microsoft.com/oldnewthing/20111028-00/?p=9243
    if (pWnd->GetExStyle() & WS_EX_TRANSPARENT) {
        pDC->SetBkMode(TRANSPARENT);
        TCHAR szTemp[32];
        ::GetClassName(pWnd->GetSafeHwnd(), szTemp, _countof(szTemp));
        if (::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, szTemp, -1, L"Button", -1) == CSTR_EQUAL) {
            hbr = (HBRUSH)GetStockObject(WHITE_BRUSH);
        }
        else {
            hbr = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
        }
    }

    return hbr;
}
