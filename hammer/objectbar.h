//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OBJECTBAR_H
#define OBJECTBAR_H
#ifdef _WIN32
#pragma once
#endif

#include "AutoSelCombo.h"
#include "HammerBar.h"
#include "FilteredComboBox.h"


class CMapView;
class BoundBox;
class CMapClass;
class Vector;
class CPrefab;

#define MAX_PREV_SEL 12


class CObjectBar : public CHammerBar, public CFilteredComboBox::ICallbacks
{
public:
	
	CObjectBar();
	BOOL Create(CWnd *pParentWnd);
	
	static LPCTSTR GetDefaultEntityClass(void);

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	
	void UpdateListForTool(int iTool);
	void SetupForBlockTool();
	void DoHideControls();
	CMapClass *CreateInBox(BoundBox *pBox, CMapView *pView = NULL);
	BOOL GetPrefabBounds(BoundBox *pBox);
	
	// If this is on, then it'll randomize the yaw when entities are placed.
	bool UseRandomYawOnEntityPlacement();

	void DoDataExchange(CDataExchange *pDX);
	
	bool IsEntityToolCreatingPrefab( void );
	bool IsEntityToolCreatingEntity( void );
	CMapClass *BuildPrefabObjectAtPoint( Vector const &HitPos );


// CFilteredComboBox::ICallbacks implementation.

	virtual void OnTextChanged( const char *pText );


private:
	
	enum 
	{
		listPrimitives,
		listPrefabs,
		listEntities
	} ListType;

	//{{AFX_DATA(CMapViewBar)
	enum { IDD = IDD_OBJECTBAR };
	//}}AFX_DATA
	
	CFilteredComboBox	m_CreateList;				// this should really be m_ItemList
	CComboBox			m_CategoryList;
	CEdit				m_Faces;
	CSpinButtonCtrl		m_FacesSpin; 

	CPrefab* FindPrefabByName( const char *pName );

	void LoadBlockCategories( void );
	void LoadEntityCategories( void );
	void LoadPrefabCategories( void );	

	void LoadBlockItems( void );
	void LoadEntityItems( void );
	void LoadPrefabItems( void );

	int UpdatePreviousSelection( int iTool );

	int GetPrevSelIndex(DWORD dwGameID, int *piNewIndex = NULL);
	BOOL EnableFaceControl(CWnd *pWnd, BOOL bModifyWnd);
	
	int iEntitySel;
	int iBlockSel;
	
	// previous selections:
	DWORD m_dwPrevGameID;
	struct tagprevsel
	{
		DWORD dwGameID;
		struct tagblock
		{
			CString strItem;
			CString strCategory;
		} block;
		struct tagentity
		{
			CString strItem;
			CString strCategory;
		} entity;
	} m_PrevSel[MAX_PREV_SEL];
	int m_iLastTool;
	
protected:
	
	afx_msg void UpdateControl(CCmdUI*);
	afx_msg void UpdateFaceControl(CCmdUI*);
	afx_msg void OnCategorylistSelchange();
	afx_msg void OnChangeCategory();
	
	DECLARE_MESSAGE_MAP()
};

#endif // OBJECTBAR_H
