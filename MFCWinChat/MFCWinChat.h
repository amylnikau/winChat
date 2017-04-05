#pragma once

#ifndef __AFXWIN_H__
#error 
#endif

#include "resource.h"

class CWinChatApp : public CWinApp
{
public:
	CWinChatApp();

public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};

extern CWinChatApp theApp;