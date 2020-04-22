//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// SwitchProps.cpp : implementation file
//

#include "stdafx.h"
#include "UI.h"
#include "SwitchProps.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSwitchProps dialog


CSwitchProps::CSwitchProps(CWnd* pParent /*=NULL*/)
	: CPropertyPage(CSwitchProps::IDD),
	m_persistDefall("AllSwitchesDefault"),
	m_persistDisplayMM2("DefaultMM2"),
	m_persistDisplayStartupInfo("DefaultStartupInfo"),
	m_persistPersistPlayerStats("DefaultPersistPlayerStats"),
	m_persistUseSupportDir("DefaultUseSupportDir"),
	m_persistElimPlayers("DefaultElimPlayers"),
	m_persistElimDays("DefaultElimDays"),
	m_persistPause("DefaultPause"),
	m_persistPauseSecs("DefaultPauseSecs")
{
	//{{AFX_DATA_INIT(CSwitchProps)
	//}}AFX_DATA_INIT
	m_psp.dwFlags &= ~PSP_HASHELP;
	alreadyAcknowledged=false;
}


void CSwitchProps::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSwitchProps)
	DDX_Control(pDX, IDC_STATIC2, m_OnlyHereToBeDisabledToo);
	DDX_Control(pDX, IDC_STATICLBL, m_OnlyHereToBeDisabled);
	DDX_Control(pDX, IDC_PAUSE, m_Pause);
	DDX_Control(pDX, IDC_PAUSESECS, m_PauseSecs);
	DDX_Control(pDX, IDC_DEFALL, m_Defall);
	DDX_Control(pDX, IDC_STARTUPINFO, m_DisplayStartupInfo);
	DDX_Control(pDX, IDC_ELIMDAYS, m_elimDays);
	DDX_Control(pDX, IDC_ELIMINATEOLDPLRS, m_ElimOldPlrs);
	DDX_Control(pDX, IDC_USESUPPORT, m_UseSupportDir);
	DDX_Control(pDX, IDC_PLRPERSIST, m_PersistPlayerStats);
	DDX_Control(pDX, IDC_DISPLAYMM2, m_DisplayMM2);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSwitchProps, CDialog)
	//{{AFX_MSG_MAP(CSwitchProps)
	ON_BN_CLICKED(IDC_DEFALL, OnDefall)
	ON_BN_CLICKED(IDC_PLRPERSIST, OnPlrpersist)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSwitchProps message handlers


#include "propsht.h"
BOOL CSwitchProps::OnKillActive()
{
	//call superclass
	BOOL bRes=this->CPropertyPage::OnKillActive();
	
	m_persistDefall=m_Defall.GetCheck();
	m_persistPause=theApp.pause=m_Pause.GetCheck();
	
	char buf[100];
	m_PauseSecs.GetWindowText(buf,100);
	m_persistPauseSecs=theApp.pauseSecs=atoi(buf);


	m_persistDisplayMM2=theApp.displayMM2=m_DisplayMM2.GetCheck()==1;
	m_persistPersistPlayerStats=theApp.persistPlayerStats=m_PersistPlayerStats.GetCheck()==1;
	m_persistUseSupportDir=theApp.useSupportDir=m_UseSupportDir.GetCheck()==1;
	m_persistDisplayStartupInfo=theApp.displayStartupInfo=m_DisplayStartupInfo.GetCheck()==1;
	m_persistElimPlayers=theApp.elimOldPlayers=m_ElimOldPlrs.GetCheck()==1;
	
	
	m_elimDays.GetWindowText(buf,100);
	m_persistElimDays=theApp.elimDays=atoi(buf);
	
	
	return bRes;

}

BOOL CSwitchProps::OnSetActive()
{

	//call superclass
	BOOL bRes=this->CPropertyPage::OnSetActive();

	if (theApp.FirstEverTimeRun && !alreadyAcknowledged)
	{
		alreadyAcknowledged=true;
		m_persistDefall=1;
		m_Defall.SetCheck(1);
		OnDefall();		
		return bRes;
	}
	
	
	bool temp;
	
	temp=m_persistDefall.toBool();
	m_Defall.SetCheck(temp);

	temp=theApp.pause=m_persistPause.toBool();
	m_Pause.SetCheck(temp);

	theApp.pauseSecs=m_persistPauseSecs.toInt();
	m_PauseSecs.SetWindowText(m_persistPauseSecs.toChars());

	temp=theApp.displayMM2=m_persistDisplayMM2.toBool();
	m_DisplayMM2.SetCheck(temp);
	
	temp=theApp.persistPlayerStats=m_persistPersistPlayerStats.toBool();
	m_PersistPlayerStats.SetCheck(temp);
	
	temp=theApp.useSupportDir=m_persistUseSupportDir.toBool();
	m_UseSupportDir.SetCheck(temp);

	temp=theApp.displayStartupInfo=m_persistDisplayStartupInfo.toBool();
	m_DisplayStartupInfo.SetCheck(temp);
	
	temp=theApp.elimOldPlayers=m_persistElimPlayers.toBool();
	m_ElimOldPlrs.SetCheck(temp);
	
	theApp.elimDays=m_persistElimDays.toInt();
	m_elimDays.SetWindowText(m_persistElimDays.toChars());

	
	OnDefall();
	OnPlrpersist();
	return bRes;

}


void CSwitchProps::OnDefall() 
{
	bool defall=m_Defall.GetCheck()!=0;
	
	if (defall)
	{
		m_DisplayMM2.SetCheck(0);
		m_DisplayStartupInfo.SetCheck(0);
		m_elimDays.SetWindowText("7");
		m_ElimOldPlrs.SetCheck(1);
		m_Pause.SetCheck(1);
		m_PauseSecs.SetWindowText("2");
		m_PersistPlayerStats.SetCheck(1);
		m_UseSupportDir.SetCheck(1);

	}
		m_DisplayMM2.EnableWindow(!defall);
		m_DisplayStartupInfo.EnableWindow(!defall);
		m_elimDays.EnableWindow(!defall);
		m_ElimOldPlrs.EnableWindow(!defall);
		m_Pause.EnableWindow(!defall);
		m_PauseSecs.EnableWindow(!defall);
		m_PersistPlayerStats.EnableWindow(!defall);
		m_UseSupportDir.EnableWindow(!defall);
		m_OnlyHereToBeDisabled.EnableWindow(!defall);
		m_OnlyHereToBeDisabledToo.EnableWindow(!defall);
}

void CSwitchProps::OnPlrpersist() 
{
	m_ElimOldPlrs.EnableWindow(m_PersistPlayerStats.GetCheck() && !m_Defall.GetCheck());
	m_elimDays.EnableWindow(m_PersistPlayerStats.GetCheck() && !m_Defall.GetCheck());
	m_OnlyHereToBeDisabledToo.EnableWindow(m_PersistPlayerStats.GetCheck() && !m_Defall.GetCheck());
}
