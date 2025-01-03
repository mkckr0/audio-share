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


// AudioShareServer.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols

#include <string>

class CMainDialog;

// CAudioShareServerApp:
// See AudioShareServer.cpp for the implementation of this class
//

class CAudioShareServerApp : public CWinAppEx
{
public:
	CAudioShareServerApp();
	~CAudioShareServerApp();

// Overrides
public:
	virtual BOOL InitInstance();
	CMainDialog* GetMainDialog();

// Implementation
private:
	void EnsureSingleton();

public:
	bool m_bHide;
	std::wstring m_exePath;

	DECLARE_MESSAGE_MAP()
};

extern CAudioShareServerApp theApp;
