//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// UI.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "propsht.h"
#include "UI.h"
#include "UIDlg.h"
#include "PersistentString.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CUIApp

BEGIN_MESSAGE_MAP(CUIApp, CWinApp)
	//{{AFX_MSG_MAP(CUIApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CUIApp construction

CUIApp::CUIApp()
{
	// Place all significant initialization in InitInstance
	pause=false;
	pauseSecs=0;
	
	CPersistentString cps("RunBefore");
	FirstEverTimeRun=!cps.toBool();
	cps=true;

}

/////////////////////////////////////////////////////////////////////////////
// The one and only CUIApp object

CUIApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CUIApp initialization

BOOL CUIApp::InitInstance()
{
	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif
	persistPlayerStats=false;
	useSupportDir=true;
	displayMM2=false;

	CAllControlsSheet dlg("TFStats");
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
		execTFStats();
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

void reporterror31337(LONG i)
{
	LPVOID lpMsgBuf;
FormatMessage( 
    FORMAT_MESSAGE_ALLOCATE_BUFFER | 
    FORMAT_MESSAGE_FROM_SYSTEM | 
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    i,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
    (LPTSTR) &lpMsgBuf,
    0,
    NULL 
);
// Process any inserts in lpMsgBuf.
// ...
// Display the string.
MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
// Free the buffer.
LocalFree( lpMsgBuf );
 
}



string& addSlash(string& tempbuf)
{
	if (tempbuf!="")
	{
		int buflen=tempbuf.length();
		if (tempbuf.at(buflen-1) != '\\')
			tempbuf+= '\\';
	}
	return tempbuf;
}


string& removeSlash(string& tempbuf)
{
	int buflen=tempbuf.length();
	if (buflen > 0 && tempbuf.at(buflen-1) == '\\')
		tempbuf.erase(tempbuf.length()-1,1);
	return tempbuf;
}

string CUIApp::CTFStatsExec::getExecString()
{
	string retval("\"");
	retval+=addSlash(TFStatsdirectory);
	retval+="TFStatsRT.exe\"  \"" ;
	retval+=fullpath;
	retval+="\" outputDir=\"";
	retval+=addSlash(outputdirectory);
	retval+=removeSlash(outputsubdir);
	retval+="\" ";
	
	retval+="ruleDir=\"";
	retval+=removeSlash(ruledirectory);
	retval+="\" ";

	retval+=" useSupportDir=";
	retval+=useSupportDir?"yes":"no";
	if (useSupportDir)
	{
		retval+=" supportDir=\"";
		retval+=removeSlash(supportdirectory);
		retval+="\" supportHTTPPath=\"";
		retval+=removeSlash(supporthttp);
		retval+="\"";
	}
	retval+=" persistPlayerStats=";
	retval+=persistPlayerStats?"yes":"no";
	if (persistPlayerStats)
	{
		retval+=" playerDir=\"";
		retval+=removeSlash(playerdirectory);
		retval+="\" ";
		retval+=" playerHTTPPath=\"";
		retval+=removeSlash(playerhttp);
		retval+="\"";
	}
	
	retval+=" eliminateOldPlayers=";
	retval+=elimOldPlayers?"yes":"no";
	if (elimOldPlayers)
	{
		retval+=" oldPlayerCutoff=";
		char buf[100];
		itoa(elimDays,buf,10);
		retval+=buf;
		retval+=" ";
	}


	
	retval+=" displayMM2=";
	retval+=displayMM2?"yes":"no";
	retval+=" displayStartUpInfo=";
	retval+=displayStartupInfo?"yes":"no";
	

	return retval;
}


#include <list>
void CUIApp::execTFStats()
{
	std::list<CUIApp::CTFStatsExec>::iterator it;
	
	AllocConsole();
	for (it=m_pLogs->begin();it!=m_pLogs->end();++it)
	{
		CUIApp::CTFStatsExec& c=*it;
		it->TFStatsdirectory=m_TFStatsDir;
		it->outputdirectory=m_OutDir;
		it->ruledirectory=m_RuleDir;
		it->supportdirectory=m_SupportDir;
		it->playerdirectory=m_PlayerDir;
		it->displayMM2=displayMM2;
		it->persistPlayerStats=persistPlayerStats;
		it->useSupportDir=useSupportDir;
		it->supporthttp=m_SupportHTTPPath;
		it->playerhttp=m_PlayerHTTPPath;
		it->displayStartupInfo=displayStartupInfo;
		it->elimOldPlayers=elimOldPlayers;
		it->elimDays=elimDays;

		
		string exec=it->getExecString();
		//create process takes a non-const char buffer?
		char createProcBuf[4000];
		memset(createProcBuf,0,4000);
		exec.copy(createProcBuf,4000);

		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		memset(&si,0,sizeof(si));
		si.cb=sizeof(si);
		char buffer[500];
		sprintf(buffer,"TFStats is creating a Match report for %s",it->inputfile.c_str());
		si.lpTitle=buffer;
		SetConsoleTitle(buffer);

		BOOL result=CreateProcess(NULL,createProcBuf,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi);
			
		
		if (pi.hProcess && result)
		{
			//wait for this one to finish before running next one!
			DWORD exitCode=STILL_ACTIVE;
			do
			{
				GetExitCodeProcess(pi.hProcess,&exitCode);
				
				//check every 10th of a second
				if (exitCode==STILL_ACTIVE)
					Sleep(100);

				if (pause)
					Sleep(pauseSecs*1000);

			} while (exitCode==STILL_ACTIVE);
		}
		else
		{
			string badmojo("***ERROR: Could not run \"");
			badmojo+=addSlash(it->TFStatsdirectory);
			badmojo+="TFStatsRT.exe\"\n\n";
			HANDLE hConsOutput=GetStdHandle(STD_OUTPUT_HANDLE);
			WriteConsole(hConsOutput,badmojo.c_str(),badmojo.length(),NULL,0);
			Sleep(4000);
		}	
	}
	FreeConsole();
}
