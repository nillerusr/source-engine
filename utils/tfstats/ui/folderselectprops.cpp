//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// FolderSelectProps.cpp : implementation file
//

#include "stdafx.h"
#include "UI.h"
#include "FolderSelectProps.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CFolderSelectProps dialog


CFolderSelectProps::CFolderSelectProps(CWnd* pParent /*=NULL*/)
	: CPropertyPage(CFolderSelectProps::IDD),
	m_StrSupportHTTPPath("SupportHTTPPath"),
	m_StrPlayerHTTPPath("PlayerHTTPPath"),
	m_StrPlayerDir("PlayerDir"),
	m_StrTFStatsDir("TFStatsDir"),
	m_StrSupportDir("SupportDir"),
	m_StrRuleDir("RuleDir"),
	m_StrOutDir("OutputDir"),
	m_BoolSupportDefault("DefaultSupportDir"),
	m_BoolTFStatsDefault("DefaultTFStatsDir"),
	m_BoolOutputDefault("DefaultOutputDir"),
	m_BoolRuleDefault("DefaultRuleDir"),
	m_BoolPlayerDefault("DefaultPlayerDir")
{
	//{{AFX_DATA_INIT(CFolderSelectProps)
	//}}AFX_DATA_INIT
	m_psp.dwFlags &= ~PSP_HASHELP;

	theApp.m_OutDir=m_StrOutDir.toString();
	theApp.m_TFStatsDir=m_StrTFStatsDir.toString();
	theApp.m_RuleDir=m_StrRuleDir.toString();
	theApp.m_SupportDir=m_StrSupportDir.toString();
	theApp.m_SupportHTTPPath=m_StrSupportHTTPPath.toString();
	theApp.m_PlayerDir=m_StrPlayerDir.toString();
	theApp.m_PlayerHTTPPath=m_StrPlayerHTTPPath.toString();

	alreadyAcknowledged=false;
	lockOutDir=lockTFSDir=false;
	windowInitted=false;
}


void CFolderSelectProps::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFolderSelectProps)
	DDX_Control(pDX, IDC_DEFTFSTATS, m_DefTFStats);
	DDX_Control(pDX, IDC_DEFSUPPORT, m_DefSupport);
	DDX_Control(pDX, IDC_DEFRULE, m_DefRule);
	DDX_Control(pDX, IDC_DEFPLAYER, m_DefPlayer);
	DDX_Control(pDX, IDC_DEFOUTPUT, m_DefOutput);
	DDX_Control(pDX, IDC_SUPPORTHTTPPATH, m_SupportHTTPPath);
	DDX_Control(pDX, IDC_PLAYERHTTPPATH, m_PlayerHTTPPath);
	DDX_Control(pDX, IDC_PLAYERDIR, m_PlayerDir);
	DDX_Control(pDX, IDC_TFSTATSDIR, m_TFStatsDir);
	DDX_Control(pDX, IDC_SUPPORTDIR, m_SupportDir);
	DDX_Control(pDX, IDC_RULEDIR, m_RuleDir);
	DDX_Control(pDX, IDC_OUTPUTDIR, m_OutDir);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFolderSelectProps, CDialog)
	//{{AFX_MSG_MAP(CFolderSelectProps)
	ON_BN_CLICKED(IDC_DEFOUTPUT, OnDefaultCheckBoxClick)
	ON_EN_CHANGE(IDC_TFSTATSDIR, OnChangeTfstatsdir)
	ON_BN_CLICKED(IDC_DEFPLAYER, OnDefaultCheckBoxClick)
	ON_BN_CLICKED(IDC_DEFRULE, OnDefaultCheckBoxClick)
	ON_BN_CLICKED(IDC_DEFSUPPORT, OnDefaultCheckBoxClick)
	ON_BN_CLICKED(IDC_DEFTFSTATS, OnDefaultCheckBoxClick)
	ON_EN_CHANGE(IDC_OUTPUTDIR, OnChangeOutputdir)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CFolderSelectProps message handlers
#include "propsht.h"
#include <winerror.h>
BOOL CFolderSelectProps::OnSetActive()
{
		//call superclass
	BOOL bRes=this->CPropertyPage::OnSetActive();

	if (theApp.FirstEverTimeRun && !alreadyAcknowledged)
	{
		alreadyAcknowledged=true;
		m_DefSupport.SetCheck(1);
		m_DefPlayer.SetCheck(1);
		m_DefOutput.SetCheck(1);
		m_DefTFStats.SetCheck(1);
		m_DefRule.SetCheck(1);
		
		m_BoolSupportDefault=1;
		m_BoolTFStatsDefault=1;
		m_BoolOutputDefault=1;
		m_BoolRuleDefault=1;
		m_BoolPlayerDefault=1;
	}

	
	m_TFStatsDir.EnableWindow(!m_DefTFStats.GetCheck());

	m_SupportHTTPPath.EnableWindow(!m_DefSupport.GetCheck());
	m_SupportDir.EnableWindow(!m_DefSupport.GetCheck());

	m_RuleDir.EnableWindow(!m_DefRule.GetCheck());

	m_PlayerHTTPPath.EnableWindow(!m_DefPlayer.GetCheck());
	m_PlayerDir.EnableWindow(!m_DefPlayer.GetCheck());

	m_OutDir.EnableWindow(!m_DefOutput.GetCheck());


	m_DefSupport.SetCheck(m_BoolSupportDefault.toBool());
	m_DefTFStats.SetCheck(m_BoolTFStatsDefault.toBool());
	m_DefOutput.SetCheck(m_BoolOutputDefault.toBool());
	m_DefRule.SetCheck(m_BoolRuleDefault.toBool());
	m_DefPlayer.SetCheck(m_BoolPlayerDefault.toBool());

	m_SupportHTTPPath.SetWindowText(m_StrSupportHTTPPath.toChars());
	m_PlayerHTTPPath.SetWindowText(m_StrPlayerHTTPPath.toChars());
	m_PlayerDir.SetWindowText(m_StrPlayerDir.toChars());
	m_TFStatsDir.SetWindowText(m_StrTFStatsDir.toChars());
	m_SupportDir.SetWindowText(m_StrSupportDir.toChars());
	m_RuleDir.SetWindowText(m_StrRuleDir.toChars());
	m_OutDir.SetWindowText(m_StrOutDir.toChars());

	char buf[500];
	string sbuf;
	m_TFStatsDir.GetWindowText(buf,500);
	sbuf=buf;
	addSlash(sbuf);
	m_TFStatsDir.SetWindowText(sbuf.c_str());

	OnDefaultCheckBoxClick() ;	
	
	windowInitted=true;
	UpdateFolders();
	return bRes;
}



BOOL CFolderSelectProps::OnKillActive()
{
		//call superclass
	BOOL bRes=this->CPropertyPage::OnKillActive();

	m_BoolSupportDefault=m_DefSupport.GetCheck();
	m_BoolTFStatsDefault=m_DefTFStats.GetCheck();
	m_BoolOutputDefault=m_DefOutput.GetCheck();
	m_BoolRuleDefault=m_DefRule.GetCheck();
	m_BoolPlayerDefault=m_DefPlayer.GetCheck();

	

	

	DWORD numbytes;
	char tempbuf[1000];
	numbytes=m_OutDir.GetWindowText(tempbuf,1000);
	m_StrOutDir=theApp.m_OutDir=tempbuf;
		
	numbytes=m_TFStatsDir.GetWindowText(tempbuf,1000);
	m_StrTFStatsDir=theApp.m_TFStatsDir=(tempbuf);
		
	numbytes=m_SupportDir.GetWindowText(tempbuf,1000);
	m_StrSupportDir=theApp.m_SupportDir=(tempbuf);
	

	numbytes=m_SupportHTTPPath.GetWindowText(tempbuf,1000);
	m_StrSupportHTTPPath=theApp.m_SupportHTTPPath=(tempbuf);
	
	
	numbytes=m_RuleDir.GetWindowText(tempbuf,1000);
	m_StrRuleDir=theApp.m_RuleDir=(tempbuf);
	
	numbytes=m_PlayerDir.GetWindowText(tempbuf,1000);
	m_StrPlayerDir=theApp.m_PlayerDir=(tempbuf);
	
		
	numbytes=m_PlayerHTTPPath.GetWindowText(tempbuf,1000);
	m_StrPlayerHTTPPath=theApp.m_PlayerHTTPPath=(tempbuf);
	

	windowInitted=false;
	return bRes;

}

void CFolderSelectProps::OnDefaultCheckBoxClick() 
{
	
UpdateFolders();
}

void CFolderSelectProps::UpdateFolders(bool safe)
{
	
	if (!windowInitted)
		return;
	

	char buf[1000];

	m_TFStatsDir.EnableWindow(!m_DefTFStats.GetCheck());
	//m_SupportHTTPPath.EnableWindow(!m_DefSupport.GetCheck());
	//m_SupportDir.EnableWindow(!m_DefSupport.GetCheck());
	m_RuleDir.EnableWindow(!m_DefRule.GetCheck());
	//m_PlayerHTTPPath.EnableWindow(!m_DefPlayer.GetCheck());
	//m_PlayerDir.EnableWindow(!m_DefPlayer.GetCheck());
	m_OutDir.EnableWindow(!m_DefOutput.GetCheck());

	this->m_PlayerDir.EnableWindow(theApp.persistPlayerStats && !m_DefPlayer.GetCheck());
	this->m_PlayerHTTPPath.EnableWindow(theApp.persistPlayerStats && !m_DefPlayer.GetCheck());
	this->m_SupportDir.EnableWindow(theApp.useSupportDir && !m_DefSupport.GetCheck());
	this->m_SupportHTTPPath.EnableWindow(theApp.useSupportDir && !m_DefSupport.GetCheck());



	string basedir;	
	if (m_DefTFStats.GetCheck())
	{
		//find in registry
		CPersistentString cps("InstallPath","Software\\Valve\\Half-Life");
		basedir=addSlash(cps.toString());
		basedir+="tfc\\TFStats\\";
		if (!lockTFSDir)
			m_TFStatsDir.SetWindowText(basedir.c_str());

	}
	else
	{
		m_TFStatsDir.GetWindowText(buf,1000);
		basedir=buf;
		addSlash(basedir);
	}
	
	
	string outputdir;
	if (m_DefOutput.GetCheck())
	{
		outputdir=basedir+"output\\";
		if (!lockOutDir)
			m_OutDir.SetWindowText(outputdir.c_str());
	}
	else 
	{
		m_OutDir.GetWindowText(buf,1000);
		outputdir=buf;
		addSlash(outputdir);
	}
	
	string supportdir=outputdir+"support\\";
	string playerdir=outputdir+"players\\";
	

		
	if (m_DefSupport.GetCheck())
	{
		m_SupportDir.SetWindowText(supportdir.c_str());
	//if (m_DefSupportHTTP.GetCheck())
		m_SupportHTTPPath.SetWindowText("../support");
	}
	if (m_DefRule.GetCheck())
		m_RuleDir.SetWindowText(basedir.c_str());
	if (m_DefPlayer.GetCheck())
	{
		m_PlayerDir.SetWindowText(playerdir.c_str());
	//if (m_DefPlayerHTTP.GetCheck())
		m_PlayerHTTPPath.SetWindowText("../players");
	}
}
void CFolderSelectProps::OnChangeTfstatsdir() 
{
	// TODO: Add your control notification handler code here
	if (!lockTFSDir)
	{
		lockTFSDir=true;
		UpdateFolders();
		lockTFSDir=false;
	}
	
}

void CFolderSelectProps::OnChangeOutputdir() 
{

	// TODO: Add your control notification handler code here
	if (!lockOutDir)
	{
		lockOutDir=true;
		UpdateFolders();
		lockOutDir=false;
	}
}
