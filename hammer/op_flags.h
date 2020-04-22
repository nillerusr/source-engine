//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#pragma once


#include "ObjectPage.h"
#include "AnchorMgr.h"

class GDclass;
class CEditGameClass;
class COP_Entity;

struct CheckListItem
{
	const char *pszItemString;
	int nItemBit;
	int state;
	inline bool operator==( const CheckListItem &first ) const
	{
		return ( ( first.nItemBit == nItemBit ) && ( !Q_strcmp( first.pszItemString, pszItemString ) ) );
	}
};

class COP_Flags : public CObjectPage
{
	DECLARE_DYNCREATE(COP_Flags)

// Construction
public:
	COP_Flags();
	~COP_Flags();
	
	// This needs to be set because we have to notify the entity page when the value changes.
	void SetEntityPage( COP_Entity *pEntityPage );

	virtual bool SaveData(void);
	virtual void UpdateData( int Mode, PVOID pData, bool bCanEdit );
	void UpdateForClass(CEditGameClass* pObj);
	void MergeForClass(CEditGameClass* pObj);
	void CreateCheckList(void);
	
	// Called when the entity tab changes the spawnflags, which renders our data obsolete.
	void OnUpdateSpawnFlags( unsigned long value );
	
	GDclass *pObjClass;

// Dialog Data
	//{{AFX_DATA(COP_Flags)
	enum { IDD = IDD_OBJPAGE_FLAGS };
	CCheckListBox m_CheckList;

		// NOTE - ClassWizard will add data members here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COP_Flags)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COP_Flags)
	virtual BOOL OnInitDialog();
	virtual void OnCheckListChange();
	afx_msg void OnSize( UINT nType, int cx, int cy );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CAnchorMgr m_AnchorMgr;

	CUtlVector <CheckListItem> m_CheckListItems;
	int m_nNumSelectedObjects;
	
	COP_Entity *m_pEntityPage;
};
