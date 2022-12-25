
// AudioShareServer.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CAudioShareServerApp:
// See AudioShareServer.cpp for the implementation of this class
//

class CAudioShareServerApp : public CWinAppEx
{
public:
	CAudioShareServerApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CAudioShareServerApp theApp;
