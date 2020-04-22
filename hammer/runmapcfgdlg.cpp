//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// RunMapCfgDlg.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "RunMapCfgDlg.h"
#include "StrDlg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CRunMapCfgDlg dialog


CRunMapCfgDlg::CRunMapCfgDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRunMapCfgDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CRunMapCfgDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	m_pApp = (CHammer*) AfxGetApp();
}


void CRunMapCfgDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRunMapCfgDlg)
	DDX_Control(pDX, IDC_CONFIGURATIONS, m_cConfigurations);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CRunMapCfgDlg, CDialog)
	//{{AFX_MSG_MAP(CRunMapCfgDlg)
	ON_BN_CLICKED(IDC_NEW, OnNew)
	ON_BN_CLICKED(IDC_REMOVE, OnRemove)
	ON_BN_CLICKED(IDC_RENAME, OnRename)
	ON_BN_CLICKED(IDC_COPY, OnCopy)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRunMapCfgDlg message handlers

void CRunMapCfgDlg::AddSequenceToList(int iIndex, CCommandSequence *pSeq)
{
	iIndex = m_cConfigurations.InsertString(iIndex, pSeq->m_szName);
	m_cConfigurations.SetItemDataPtr(iIndex, PVOID(pSeq));
	m_cConfigurations.SetCurSel(iIndex);
}

void CRunMapCfgDlg::OnNew() 
{
	// add a new sequence
	CStrDlg dlg(0, "", "Name:", "New Configuration");
	if(dlg.DoModal() == IDCANCEL)
		return;

	// add it to the list in the app
	CCommandSequence *pSeq = new CCommandSequence;
	strcpy(pSeq->m_szName, dlg.m_string);
	m_pApp->m_CmdSequences.Add(pSeq);

	AddSequenceToList(-1, pSeq);
}

void CRunMapCfgDlg::OnRemove() 
{
	int iSel = m_cConfigurations.GetCurSel();
	if(iSel == LB_ERR)
		return;	// nothing selected
	if(AfxMessageBox("Do you want to remove this configuration?",
		MB_YESNO) == IDNO)
		return;	// don't want to
	CCommandSequence *pSeq = (CCommandSequence*) 
		m_cConfigurations.GetItemDataPtr(iSel);

	// find it in the app's array
	for(int i = 0; i < m_pApp->m_CmdSequences.GetSize(); i++)
	{
		if(pSeq == m_pApp->m_CmdSequences[i])
		{
			delete pSeq;
			m_pApp->m_CmdSequences.RemoveAt(i);
			m_cConfigurations.DeleteString(iSel);
			return;	// done
		}
	}

	// shouldn't reach here -
	Assert(0);
}

void CRunMapCfgDlg::OnRename() 
{
	int iSel = m_cConfigurations.GetCurSel();
	if(iSel == LB_ERR)
		return;	// nothing selected
	CCommandSequence *pSeq = (CCommandSequence*) 
		m_cConfigurations.GetItemDataPtr(iSel);

	CStrDlg dlg(0, pSeq->m_szName, "Name:", "Rename Configuration");
	if(dlg.DoModal() == IDCANCEL)
		return;

	strcpy(pSeq->m_szName, dlg.m_string);

	m_cConfigurations.DeleteString(iSel);
	AddSequenceToList(iSel, pSeq);
}

BOOL CRunMapCfgDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	// add the configurations into the list
	int iSize = m_pApp->m_CmdSequences.GetSize();
	for(int i = 0; i < iSize; i++)
	{
		CCommandSequence *pSeq = m_pApp->m_CmdSequences[i];
		int iIndex = m_cConfigurations.AddString(pSeq->m_szName);
		m_cConfigurations.SetItemDataPtr(iIndex, PVOID(pSeq));
	}

	return TRUE;
}

void CRunMapCfgDlg::OnCopy() 
{
	int iSel = m_cConfigurations.GetCurSel();
	if(iSel == LB_ERR)
		return;	// nothing selected

	// add a new sequence
	CStrDlg dlg(0, "", "Name:", "Copy Configuration");
	if(dlg.DoModal() == IDCANCEL)
		return;

	// add it to the list in the app
	CCommandSequence *pSeq = new CCommandSequence;
	strcpy(pSeq->m_szName, dlg.m_string);
	m_pApp->m_CmdSequences.Add(pSeq);

	CCommandSequence *pSrcSeq = (CCommandSequence*) 
		m_cConfigurations.GetItemDataPtr(iSel);

	pSeq->m_Commands.Append(pSrcSeq->m_Commands);

	AddSequenceToList(-1, pSeq);
}
