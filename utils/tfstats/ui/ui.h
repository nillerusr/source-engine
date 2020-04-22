//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// UI.h : main header file for the UI application
//

#if !defined(AFX_UI_H__D6AFABCC_5BD5_11D3_A5CF_005004039597__INCLUDED_)
#define AFX_UI_H__D6AFABCC_5BD5_11D3_A5CF_005004039597__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CUIApp:
// See UI.cpp for the implementation of this class
//
#include <string>
#include <list>

using std::string;
using std::list;

class CUIApp : public CWinApp
{
public:
	class CTFStatsExec
	{
	public:
		bool displayMM2;
		bool useSupportDir;
		bool persistPlayerStats;
		bool displayStartupInfo;
		bool elimOldPlayers;
		int elimDays;
		string TFStatsdirectory;
		string playerdirectory;
		string playerhttp;
		string logdirectory;
		string ruledirectory;
		string outputdirectory;
		string outputsubdir;
		string supportdirectory;
		string supporthttp;
		string inputfile;
		string fullpath;
		string getExecString();
	};

	string m_OutDir;
	string m_SupportDir;
	string m_SupportHTTPPath;
	string m_RuleDir;
	string m_TFStatsDir;
	string m_PlayerDir;
	string m_PlayerHTTPPath;
	bool displayMM2;
	bool useSupportDir;
	bool persistPlayerStats;
	bool displayStartupInfo;
	bool elimOldPlayers;
	int elimDays;
	list<CTFStatsExec>* m_pLogs;
	
	void execTFStats();
	bool pause;
	int pauseSecs;

	bool FirstEverTimeRun;

public:
	CUIApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CUIApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CUIApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
extern CUIApp theApp;

/////////////////////////////////////////////////////////////////////////////
string& addSlash(string& tempbuf);
string& removeSlash(string& tempbuf);

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_UI_H__D6AFABCC_5BD5_11D3_A5CF_005004039597__INCLUDED_)


