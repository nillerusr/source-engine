//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef OP_OUTPUT_H
#define OP_OUTPUT_H
#pragma once

#include "ObjectPage.h"
#include "Resource.h"
#include "TargetNameCombo.h"
#include "MapEntity.h"
#include "ToolPickEntity.h"
#include "AutoSelCombo.h"
#include "AnchorMgr.h"


#define OUTPUT_LIST_NUM_COLUMNS		6


class COP_Output;


enum SortDirection_t;


//-----------------------------------------------------------------------------
// Purpose: A little glue object that connects the entity picker tool to our dialog.
//-----------------------------------------------------------------------------
class COP_OutputPickEntityTarget : public IPickEntityTarget
{
	public:

		void AttachEntityDlg(COP_Output *pDlg) { m_pDlg = pDlg; }
		void OnNotifyPickEntity(CToolPickEntity *pTool);
		void AttachDlgItem( int nItem ) { m_nDlgItem = nItem; }

	private:
		int m_nDlgItem;
		COP_Output *m_pDlg;
};


//-----------------------------------------------------------------------------
// Purpose: A list of connections and entites that send them.
//-----------------------------------------------------------------------------
class COutputConnection
{
public:
	CEntityConnectionList*	m_pConnList;
	CMapEntityList*			m_pEntityList;
	bool					m_bIsValid;
	bool					m_bOwnedByAll;	// Connection used by all selected entities
};


class COP_Output : public CObjectPage, public CFilteredComboBox::ICallbacks
{
	public:
		static CImageList *m_pImageList;
		static CEntityConnectionList*	m_pConnectionBuffer;	// Stores connections for copy/pasting
		static void EmptyCopyBuffer(void);

	public:

		DECLARE_DYNCREATE(COP_Output)

		// Construction
		COP_Output(void);
		~COP_Output(void);

		virtual void UpdateData( int Mode, PVOID pData, bool bCanEdit );
		void SetSelectedConnection(CEntityConnection *pConnection);
		bool ShouldShowHiddenTargets();

	protected:

		virtual void OnTextChanged( const char *pText );

		void AddEntityConnections(CMapEntity *pEntity, bool bFirst);
		void AddConnection(CEntityConnection *pConnection);
		void RemoveAllEntityConnections(void);
		void UpdateConnectionList(void);
		void ResizeColumns(void);

		void SetSelectedItem(int nItem);
		void SetSelectedConnections(CEntityConnectionList &List);

		// Validation functions
		bool ValidateConnections(COutputConnection *pOutputConn, bool bVisibilityCheck);
		void UpdateValidityButton(void);

		// Edit controls
		void UpdateEditControls(void);
		void EnableEditControls(bool bValue = true);
		void UpdateItemValidity(int nItem);
		void EnableTarget(bool bEnable);
		void EnableParam(bool bEnable);
		bool bSkipEditControlRefresh;

		// Functions for updating edited connections
		void UpdateEditedInputs(void);
		void UpdateEditedOutputs(void);
		void UpdateEditedTargets(void);
		void UpdateEditedDelays(void);
		void UpdateEditedFireOnce(void);
		void UpdateEditedParams(void);

		// Fuctions for reacting to combo box changes
		void OutputChanged(void);
		void InputChanged(void);

		void SortListByColumn(int nColumn, SortDirection_t eDirection);
		void SetSortColumn(int nColumn, SortDirection_t eDirection);
		void UpdateColumnHeaderText(int nColumn, bool bIsSortColumn, SortDirection_t eDirection);

		CMapEntityList			m_EntityList;			// Filtered m_pObjectList that only includes map entities
		const CMapEntityList*	m_pMapEntityList;		// List of all entities in the map.
		CEntityConnectionList	m_EditList;				// List of selected connections being edited 

		void UpdateEntityList();						// Generates m_EntityList from m_pObjectList

	protected:
	
		CAnchorMgr m_AnchorMgr;
		
		CEditGameClass	*m_pEditGameClass;
		CMapEntity		*m_pEntity;
		bool			m_bNoParamEdit;

		//
		// Cached data for sorting the list view.
		//
		int m_nSortColumn;												// Current column used for sorting.
		SortDirection_t m_eSortDirection[OUTPUT_LIST_NUM_COLUMNS];		// Last sort direction per column.

		bool m_bPickingEntities;

		// ########################################
		//  Message editing
		// ########################################
		void SetConnection(CEntityConnectionList *pConnectionList);

		void SetMapEntityList(const CMapEntityList *pMapEntityList);

		void AddEntityOutputs(CMapEntity *pEntity);

		void FillInputList(void);
		void FillOutputList(void);
		void FillTargetList(void);

		void FilterInputList(void);
		void FilterOutputList(void);
		void FilterTargetList(void);
		void FilterEntityOutputs(CMapEntity *pEntity);

		void StopPicking(void);

		CClassInput*	GetInput(char *szInput, int nSize);
		CClassOutput*	GetOutput(char *szOutput, int nSize);
		CMapEntityList*	GetTarget(char *szTarget, int nSize);

		void UpdateCombosForSelectedInput(CClassInput *pInput);
		void UpdateCombosForSelectedOutput(CClassOutput *pOutput);

		// #########################################

		//{{AFX_DATA(COP_Output)
		enum { IDD = IDD_OBJPAGE_OUTPUT };
		CListCtrl m_ListCtrl;
		CAutoSelComboBox	m_ComboOutput;
		CTargetNameComboBox m_ComboTarget;
		CAutoSelComboBox	m_ComboInput;
		CButton				m_CheckBoxFireOnce;
		CButton				m_ctlShowHiddenTargetsAsBroken;
		CButton				m_AddControl;
		CButton				m_PasteControl;
		CButton				m_DeleteControl;

		CString	m_strOutput;
		CString	m_strTarget;
		CString	m_strInput;
		CString	m_strParam;
		float m_fDelay;
		BOOL m_bFireOnce;
		//}}AFX_DATA

		// ClassWizard generate virtual function overrides
		//{{AFX_VIRTUAL(COP_Output)
		virtual void DoDataExchange(CDataExchange* pDX);
		virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT *pResult);
		virtual BOOL OnInitDialog(void);
		virtual void OnDestroy(void);
		//}}AFX_VIRTUAL

		// Generated message map functions
		//{{AFX_MSG(COP_Output)
		afx_msg void OnAdd(void);
		afx_msg void OnCopy(void);
		afx_msg void OnPaste(void);
		afx_msg void OnSize( UINT nType, int cx, int cy );
		afx_msg void OnDelete(void);
		afx_msg void OnMark(void);
		afx_msg void OnPickEntity(void);
		afx_msg void OnPickEntityParam(void);
		afx_msg void OnSelChangeInput(void);
		afx_msg void OnEditUpdateInput(void);
		afx_msg void OnSelChangeOutput(void);
		afx_msg void OnEditUpdateOutput(void);
		afx_msg void OnSelChangeParam(void);
		afx_msg void OnEditUpdateParam(void);
		afx_msg void OnEditDelay(void);
		afx_msg void OnFireOnce(void);
		afx_msg void OnShowHiddenTargetsAsBroken();
		//}}AFX_MSG

		DECLARE_MESSAGE_MAP()

	private:
		bool m_bEntityParamTarget;
		COP_OutputPickEntityTarget m_PickEntityTarget;
		CString m_strLastParam;
		bool m_bIgnoreTextChanged;

	friend class COP_OutputPickEntityTarget;
public:
};

#endif // OP_OUTPUT_H
