//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_SERVICESDLG_H__FD755233_0A7A_4CBB_BA5E_A5D0B3B5F830__INCLUDED_)
#define AFX_SERVICESDLG_H__FD755233_0A7A_4CBB_BA5E_A5D0B3B5F830__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ServicesDlg.h : header file
//

#include "iphelpers.h"
#include "idle_dialog.h"
#include "utllinkedlist.h"
#include "resource.h"
#include "window_anchor_mgr.h"
#include "net_view_thread.h"
#include "vmpi_defs.h"


class CServiceInfo
{
public:

	bool IsOff() const;	// Returns true if the time since we've heard from this guy is too long.


public:

	CString	m_ComputerName;
	CString	m_MasterName;
	CString m_Password;
	int		m_iState;
	
	// Since the live time is always changing, we only update it every 10 seconds or so.
	DWORD	m_LiveTimeMS;			// How long the service has been running (in milliseconds).

	DWORD	m_WorkerAppTimeMS;		// How long the worker app has been running (0 if it's not running).
	
	DWORD	m_LastPingTimeMS;		// Last time we heard from this machine. Used to detect if the service
									// is off or not.

	// Used to detect if we need to re-sort the list.
	const char *m_pLastStatusText;
	DWORD		m_LastLiveTimeMS;
	CString		m_LastMasterName;

	int			m_CPUPercentage;
	CString		m_ExeName;
	CString		m_MapName;
	int			m_MemUsageMB;

	// Last time we updated the service in the listbox.. used to make sure we update its on/off status
	// every once in a while.
	DWORD		m_LastUpdateTime;
	
	int			m_ProtocolVersion;		// i.e. the service's VMPI_SERVICE_PROTOCOL_VERSION.
	char		m_ServiceVersion[32];	// Version string.

	CIPAddr	m_Addr;
};



/////////////////////////////////////////////////////////////////////////////
// CServicesDlg dialog

class CServicesDlg : public CIdleDialog
{
// Construction
public:
	CServicesDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CServicesDlg)
	enum { IDD = IDD_SERVICES };
	CStatic	m_NumServicesControl;
	CStatic	m_NumDisabledServicesControl;
	CStatic	m_NumWorkingServicesControl;
	CStatic	m_NumWaitingServicesControl;
	CStatic	m_NumOffServicesControl;
	CStatic	m_PasswordDisplay;
	CListCtrl	m_ServicesList;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CServicesDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	//}}AFX_VIRTUAL

// Implementation
protected:
	void BuildVMPIPingPacket( CUtlVector<char> &out, char cPacketID, unsigned char cProtocolVersion=VMPI_PROTOCOL_VERSION, bool bIgnorePassword=false );

	virtual void OnIdle();


	CServiceInfo* FindServiceByComputerName( const char *pComputerName );
	void SendToSelectedServices( const char *pData, int len );
	void UpdateServiceInListbox( CServiceInfo *pInfo );
	void UpdateServiceCountDisplay();
	void ResortItems();

	void UpdateServicesFromNetMessages();
	void UpdateServicesFromNetView();
	void BuildClipboardText( CUtlVector<char> &clipboardText );


	ISocket	*m_pServicesPingSocket;
	DWORD m_dwLastServicesPing;	// Last time we pinged all the services.

	// Restricts the password so we only see a particular set of VMPI services.
	CString m_Password;

	CUtlLinkedList<CServiceInfo*, int> m_Services;
	CNetViewThread m_NetViewThread;


	CWindowAnchorMgr	m_AnchorMgr;

	bool m_bListChanged;		// Used to detect if we need to re-sort the list.

	// Generated message map functions
	//{{AFX_MSG(CServicesDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPatchServices();
	afx_msg void OnStopServices();
	afx_msg void OnStopJobs();
	afx_msg void OnFilterByPassword();
	afx_msg void OnForcePassword();
	afx_msg void OnCopyToClipboard();
	afx_msg void OnDblclkServicesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SERVICESDLG_H__FD755233_0A7A_4CBB_BA5E_A5D0B3B5F830__INCLUDED_)
