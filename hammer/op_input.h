//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OP_INPUT_H
#define OP_INPUT_H
#pragma once

#include "ObjectPage.h"
#include "Resource.h"
#include "MapEntity.h"
#include "AnchorMgr.h"

#define OUTPUT_LIST_NUM_COLUMNS		6


enum SortDirection_t;

// A list of connections and entites that send them
class CInputConnection
{
public:
	CMapEntity*			m_pEntity;
	CEntityConnection*	m_pConnection;
	bool				m_bIsValid;
};


// #############################################################################
class COP_Input : public CObjectPage
{
	public:
		static CImageList *m_pImageList;

	public:

		DECLARE_DYNCREATE(COP_Input)

		// Construction
		COP_Input(void);
		~COP_Input(void);

		void UpdateData( int Mode, PVOID pData, bool bCanEdit );
		void SetSelectedConnection(CEntityConnection *pConnection);

	protected:

		void AddEntityConnections(const char *pTargetName, CMapEntity *pTestEntity);
		void UpdateConnectionList(void);
		void UpdateEntityList(void);
		void RemoveAllEntityConnections(void);

		void SortListByColumn(int nColumn, SortDirection_t eDirection);
		void SetSortColumn(int nColumn, SortDirection_t eDirection);
		void UpdateColumnHeaderText(int nColumn, bool bIsSortColumn, SortDirection_t eDirection);

		// Connection validity
		void UpdateItemValidity(int nItem);
		bool ValidateConnections(int nItem);

	
	protected:
	
		CAnchorMgr m_AnchorMgr;

		CMapEntityList  *m_pEntityList;			// Object list filtered for entities
		CEditGameClass	*m_pEditGameClass;
		CMapEntity		*m_pEntity;

		CMapEntityList			*m_pTargetEntityList;	// List of entites that target me
		CEntityConnectionList	*m_pConnectionList;		// List of all the connections that target me
		bool					m_bMultipleTargetNames; // Entities with multiple target names selected
		//
		// Cached data for sorting the list view.
		//
		int m_nSortColumn;												// Current column used for sorting.
		SortDirection_t m_eSortDirection[OUTPUT_LIST_NUM_COLUMNS];		// Last sort direction per column.

		//{{AFX_DATA(COP_Input)
		enum { IDD = IDD_OBJPAGE_INPUT };
		CListCtrl m_ListCtrl;
		//}}AFX_DATA

		// ClassWizard generate virtual function overrides
		//{{AFX_VIRTUAL(COP_Input)
		virtual void DoDataExchange(CDataExchange* pDX);
		virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT *pResult);
		//}}AFX_VIRTUAL

		// Generated message map functions
		//{{AFX_MSG(COP_Input)
		afx_msg void OnMark(void);
		afx_msg void OnSize( UINT nType, int cx, int cy );
		virtual BOOL OnInitDialog(void);
		virtual void OnDestroy(void);
		//}}AFX_MSG

		DECLARE_MESSAGE_MAP()
};

#endif // OP_INPUT_H
