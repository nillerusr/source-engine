//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// LogSelectProps.cpp : implementation file
//

#include "stdafx.h"
#include "UI.h"
#include "LogSelectProps.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CLogSelectProps dialog


CLogSelectProps::CLogSelectProps(CWnd* pParent /*=NULL*/)
	: CPropertyPage(CLogSelectProps::IDD),
	m_persistLastDirectory(string("LastInputDirectory"))
{
	//{{AFX_DATA_INIT(CLogSelectProps)
	//}}AFX_DATA_INIT
	m_psp.dwFlags &= ~PSP_HASHELP;
	alreadyAcknowledged=false;
}


void CLogSelectProps::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLogSelectProps)
	DDX_Control(pDX, IDC_SELECT, m_SelectButton);
	DDX_Control(pDX, IDC_REMOVELOG, m_RemoveButton);
	DDX_Control(pDX, IDC_LOGS2DO, m_Logs2Do);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLogSelectProps, CDialog)
	//{{AFX_MSG_MAP(CLogSelectProps)
	ON_LBN_SELCHANGE(IDC_LOGS2DO, OnSelchangeLogs2do)
	ON_BN_CLICKED(IDC_REMOVELOG, OnRemovelog)
	ON_BN_CLICKED(IDC_SELECT, OnSelect)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLogSelectProps message handlers

void CLogSelectProps::OnSelect() 
{
	char* fileNameBuf= new char[10000];
	memset(fileNameBuf,0,10000);
	CFileDialog cfd(TRUE,".log",NULL,0,"Log Files (*.log)|*.log|Text Files (*.txt)|*.txt|All Files (*.*)|*.*||");
	cfd.m_ofn.Flags|=OFN_ALLOWMULTISELECT;
	cfd.m_ofn.lpstrFile=fileNameBuf;
	cfd.m_ofn.nMaxFile=10000;
	
	if (m_persistLastDirectory.toString()!="")
		cfd.m_ofn.lpstrInitialDir=m_persistLastDirectory.toString().c_str();

	if (cfd.DoModal()==IDOK)
	{
		POSITION pos=cfd.GetStartPosition();
		while(pos)
		{
			CUIApp::CTFStatsExec* pc=new CUIApp::CTFStatsExec;
			
			CString wow=cfd.GetNextPathName(pos);
			//void _splitpath( const char *path, char *drive, char *dir, char *fname, char *ext );
			pc->fullpath=wow;
			char drvbuf[5];
			char dirbuf[10000];
			char namebuf[10000];
			char extbuf[10000];
			char fname[10000];
			char inpdir[10000];
			_splitpath(pc->fullpath.c_str(),drvbuf,dirbuf,namebuf,extbuf);
			sprintf(inpdir,"%s%s",drvbuf,dirbuf);
			sprintf(fname,"%s%s",namebuf,extbuf);
			pc->outputsubdir=namebuf;
			pc->inputfile=fname;
			pc->logdirectory=inpdir;
			
			m_persistLastDirectory=inpdir;

			int idx=m_Logs2Do.AddString(pc->fullpath.c_str());
			m_Logs2Do.SetItemDataPtr(idx,pc);

			
		}
	}

	delete [] fileNameBuf;
	
}
	
list<CUIApp::CTFStatsExec>* CLogSelectProps::getList()
{
	list<CUIApp::CTFStatsExec>* pRetList=new list<CUIApp::CTFStatsExec>;
	int numItems=m_Logs2Do.GetCount();
	for (int i=0;i<numItems;i++)
	{
		CUIApp::CTFStatsExec * pExec=(CUIApp::CTFStatsExec *)m_Logs2Do.GetItemDataPtr(i);
		pRetList->push_back(*pExec);
	}

	return pRetList;
}

void CLogSelectProps::OnSelchangeLogs2do() 
{
	if (m_Logs2Do.GetSelCount() > 0)
		m_RemoveButton.EnableWindow(TRUE);
	else
		m_RemoveButton.EnableWindow(FALSE);
}

void CLogSelectProps::OnRemovelog() 
{
	int* selitems=new int[500];
	int numselected=m_Logs2Do.GetSelItems(500,selitems);

	//have to do it backwards to account for indices being changed by the actual deletions
	for (int i=numselected-1;i>=0;i--)
	{
		//delete m_Logs2Do.GetItemDataPtr(selitems[i]);
		//m_Logs2Do.SetItemData(selitems[i],0);
		CString wow;
		m_Logs2Do.GetText(selitems[i],wow);
		m_Logs2Do.DeleteString(selitems[i]);
	}
	delete [] selitems;
	

	m_Logs2Do.SetSel(-1,FALSE);
}

#include "propsht.h"
BOOL CLogSelectProps::OnSetActive()
{
	//call superclass
	BOOL bRes=this->CPropertyPage::OnSetActive();

	if (theApp.FirstEverTimeRun && !alreadyAcknowledged)
	{
		alreadyAcknowledged=true;
		CPersistentString cps("InstallPath","Software\\Valve\\Half-Life");
		string basedir=addSlash(cps.toString());
		basedir+="tfc\\logs";
		m_persistLastDirectory=basedir;
	}

	
	return bRes;
}

BOOL CLogSelectProps::OnKillActive()
{
	//call superclass
	BOOL bRes=this->CPropertyPage::OnKillActive();

	UpdateAppList();

	return bRes;
}

void CLogSelectProps::UpdateAppList()
{
	if (theApp.m_pLogs != NULL)
		delete(theApp.m_pLogs);

	theApp.m_pLogs=getList();
}
