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

// audio-share-server.cpp : Defines the class behaviors for the application.
//

#include "pch.h"
#include "framework.h"
#include "AudioShareServer.h"
#include "CMainDialog.h"
#include "AppMsg.h"

#include <wil/resource.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>

// CAudioShareServerApp

BEGIN_MESSAGE_MAP(CAudioShareServerApp, CWinApp)
    ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CAudioShareServerApp construction

CAudioShareServerApp::CAudioShareServerApp()
    : m_bHide(false)
{
    EnsureSingleton();

    // TODO: add construction code here,
    // Place all significant initialization in InitInstance
    m_pszAppName = _wcsdup(L"Audio Share Server");

    WCHAR lpFileName[1024];
    GetModuleFileNameW(nullptr, lpFileName, sizeof(lpFileName));
    m_exePath = lpFileName;
    auto exe_dir = std::filesystem::path(m_exePath).parent_path();

    auto basic_logger = spdlog::basic_logger_mt("server", (exe_dir / "server.log").string());
    spdlog::set_default_logger(basic_logger);
#ifdef DEBUG
    spdlog::flush_every(std::chrono::seconds(1));
    spdlog::set_level(spdlog::level::trace);
#endif // DEBUG

    free((void*)m_pszProfileName);
    m_pszProfileName = _wcsdup((exe_dir / "config.ini").c_str());
}

CAudioShareServerApp::~CAudioShareServerApp()
{
}


// The one and only CAudioShareServerApp object

CAudioShareServerApp theApp;


// CAudioShareServerApp initialization

BOOL CAudioShareServerApp::InitInstance()
{
    // InitCommonControlsEx() is required on Windows XP if an application
    // manifest specifies use of ComCtl32.dll version 6 or later to enable
    // visual styles.  Otherwise, any window creation will fail.
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    // Set this to include all the common control classes you want to use
    // in your application.
    InitCtrls.dwICC = ICC_STANDARD_CLASSES | ICC_USEREX_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    CWinAppEx::InitInstance();

    //AfxEnableControlContainer();

    // Create the shell manager, in case the dialog contains
    // any shell tree view or shell list view controls.
    //CShellManager *pShellManager = new CShellManager;

    // Activate "Windows Native" visual manager for enabling themes in MFC controls
    CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

    // Standard initialization
    // If you are not using these features and wish to reduce the size
    // of your final executable, you should remove from the following
    // the specific initialization routines you do not need
    // Change the registry key under which our settings are stored
    // TODO: You should modify this string to be something appropriate
    // such as the name of your company or organization
    //SetRegistryKey(_T("Local AppWizard-Generated Applications"));

    int nConfigVersion = 1;
    if (nConfigVersion != this->GetProfileIntW(L"App", L"configVersion", 0)) {
        CFileStatus fileStatus;
        if (CFile::GetStatus(m_pszProfileName, fileStatus)) {
            CFile::Remove(m_pszProfileName);
        }
        this->WriteProfileInt(L"App", L"configVersion", nConfigVersion);
    }

    auto configLang = this->GetProfileStringW(L"App", L"language", L"default");
    if (configLang != "default") {
        std::wstring lang(configLang.GetString());
        lang.push_back(L'\0');
        ULONG ulNumLanguages = 1;
        SetProcessPreferredUILanguages(MUI_LANGUAGE_NAME, lang.c_str(), &ulNumLanguages);
    }

    struct MyCCommandLineInfo : CCommandLineInfo {
        virtual void ParseParam(const TCHAR* pszParam, BOOL bFlag, BOOL bLast)
        {
            if (bFlag) {
                if (CString(pszParam) == L"hide") {
                    theApp.m_bHide = true;
                }
            }
        }
    };
    MyCCommandLineInfo cmdInfo;
    ParseCommandLine(cmdInfo);

    if (InitContextMenuManager() == FALSE) {
        TRACE(traceAppMsg, 0, "InitContextMenuManager Error\n");
        return FALSE;
    }

    auto dlg = new CMainDialog;
    dlg->Create(IDD_MAIN);
    m_pMainWnd = dlg;

    //CMainDialog dlg;
    //m_pMainWnd = &dlg;
    //INT_PTR nResponse = dlg.DoModal();
    //if (nResponse == IDOK)
    //{
    //	// TODO: Place code here to handle when the dialog is
    //	//  dismissed with OK
    //}
    //else if (nResponse == IDCANCEL)
    //{
    //	// TODO: Place code here to handle when the dialog is
    //	//  dismissed with Cancel
    //}
    //else if (nResponse == -1)
    //{
    //	TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
    //	TRACE(traceAppMsg, 0, "Warning: if you are using MFC controls on the dialog, you cannot #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS.\n");
    //}

    // Delete the shell manager created above.
    //if (pShellManager != nullptr)
    //{
    //	delete pShellManager;
    //}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
    ControlBarCleanUp();
#endif

    // Since the dialog has been closed, return FALSE so that we exit the
    //  application, rather than start the application's message pump.
    //return FALSE;
    return TRUE;
}

CMainDialog* CAudioShareServerApp::GetMainDialog()
{
    return static_cast<CMainDialog*>(GetMainWnd());
}

void CAudioShareServerApp::EnsureSingleton()
{
    std::thread([this]() {
        constexpr auto mailSlotName = R"(\\.\mailslot\AudioShareServer\938609DF-0FF0-49C9-83BB-ABB6F391F544)";
        wil::unique_handle hMailSlot{
            CreateMailslotA(mailSlotName, 0, MAILSLOT_WAIT_FOREVER, nullptr)
        };
        if (!hMailSlot.is_valid()) {
            // mail slot already exits
            wil::unique_handle hFile{
                CreateFileA(mailSlotName, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)
            };
            if (!hFile.is_valid()) {
                return;
            }
            int cmd{ 1 };
            DWORD bytesWritten{};
            WriteFile(hFile.get(), &cmd, sizeof(cmd), &bytesWritten, nullptr);
            ExitProcess(0);
        }
        else {
            while (true) {
                int cmd{};
                DWORD bytesRead{};
                (void)ReadFile(hMailSlot.get(), &cmd, sizeof(cmd), &bytesRead, nullptr);
                this->GetMainDialog()->SendMessageW(WM_COMMAND, ID_APP_SHOW);
                TRACE(traceAppMsg, 0, "read loop");
            }
        }
    }).detach();
}

