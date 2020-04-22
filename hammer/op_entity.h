//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef OP_ENTITY_H
#define OP_ENTITY_H
#ifdef _WIN32
#pragma once
#endif

#include "AutoSelCombo.h"
#include "ChunkFile.h"
#include "ListBoxEx.h"
#include "AngleBox.h"
#include "fgdlib/WCKeyValues.h"
#include "MapFace.h"
#include "ObjectPage.h"
#include "ToolPickAngles.h"
#include "ToolPickEntity.h"
#include "ToolPickFace.h"
#include "FilteredComboBox.h"
#include "AnchorMgr.h"
#include "ModelBrowser.h"

class CEditGameClass;
class COP_Entity;
class COP_Flags;
class CMyComboBox;

//-----------------------------------------------------------------------------
// Owner-draw list control that uses cool colors to show 
// the state of items.
//-----------------------------------------------------------------------------
class CColoredListCtrl : public CListCtrl
{
public:

	class IItemColorCallback
	{
	public:
		// This is called for every item to get its colors.
		virtual void GetItemColor( int iItem, COLORREF *pBackgroundColor, COLORREF *pTextColor ) = 0;
		
		// This is called for every item so you can draw custom stuff in its value column.
		// The RECT inside the DRAWITEMSTRUCT contains the whole row, and pRect contains the rect for the value column only.
		// Return true if you don't want CColoredListControl to draw its value.
		virtual bool CustomDrawItemValue( const LPDRAWITEMSTRUCT p, const RECT *pRect ) = 0;
	};

public:
	CColoredListCtrl( IItemColorCallback *pCallback );
	
	virtual void DrawItem( LPDRAWITEMSTRUCT p );

private:
	IItemColorCallback *m_pCallback;
};


//-----------------------------------------------------------------------------
// Purpose: A little glue object that connects the angles picker tool to our dialog.
//-----------------------------------------------------------------------------
class CPickAnglesTarget : public IPickAnglesTarget
{
	public:

		void AttachEntityDlg(COP_Entity *pDlg) { m_pDlg = pDlg; }
		void OnNotifyPickAngles(const Vector &vecPos);
	
	private:

		COP_Entity *m_pDlg;
};


//-----------------------------------------------------------------------------
// Purpose: A little glue object that connects the entity picker tool to our dialog.
//			Currently this gets the value of a given key and puts that into the smart
//			control.
//-----------------------------------------------------------------------------
class CPickEntityTarget : public IPickEntityTarget
{
	public:

		inline CPickEntityTarget();
		void AttachEntityDlg(COP_Entity *pDlg) { m_pDlg = pDlg; }
		inline void SetKeyToRetrieve(const char *pszKey);
		void OnNotifyPickEntity(CToolPickEntity *pTool);
	
	private:

		char m_szKey[MAX_KEYVALUE_LEN];		// The name of the key we are going to slurp out of the entity.
		COP_Entity *m_pDlg;					// The dialog to receive the key value.
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPickEntityTarget::CPickEntityTarget()
{
	m_szKey[0] = '\0';
	m_pDlg = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszKey - 
//-----------------------------------------------------------------------------
void CPickEntityTarget::SetKeyToRetrieve(const char *pszKey)
{
	strncpy(m_szKey, pszKey, sizeof(m_szKey) - 1);
}


//-----------------------------------------------------------------------------
// Purpose: A little glue object that connects the face picker tool to our dialog.
//-----------------------------------------------------------------------------
class CPickFaceTarget : public IPickFaceTarget
{
	public:

		void AttachEntityDlg(COP_Entity *pDlg) { m_pDlg = pDlg; }
		void OnNotifyPickFace(CToolPickFace *pTool);
	
	private:

		COP_Entity *m_pDlg;
};


enum EKeyState
{
	k_EKeyState_DefaultFGDValue=0,	// This key is unmodified from its default value in the FGD.
	k_EKeyState_Modified=1,			// This key is in the FGD, and its value has been modified.
	k_EKeyState_AddedManually,		// This key was added manually (i.e. it does not exist in the FGD).
	k_EKeyState_InstanceParm,
};


class CInstanceParmData
{
public:
	GDinputvariable		*m_ParmVariable;
	CString				m_ParmKey;
	CString				m_VariableName;
};


// This class just routes the OnTextChanged call into COP_Entity.
class CSmartControlTargetNameRouter : public CFilteredComboBox::ICallbacks
{
public:
	CSmartControlTargetNameRouter( COP_Entity *pDlg );

	virtual void OnTextChanged( const char *pText );
	
private:
	COP_Entity *m_pDlg;
};


class COP_Entity : public CObjectPage, CFilteredComboBox::ICallbacks, public CColoredListCtrl::IItemColorCallback
{
	DECLARE_DYNCREATE(COP_Entity)
	typedef CObjectPage BaseClass;
	
	friend int InternalSortByColumn( COP_Entity *pDlg, const char *pShortName1, const char *pShortName2, int iColumn );
	friend int CALLBACK SortByItemEditedState( LPARAM iItem1, LPARAM iItem2, LPARAM lpParam );
	friend class CColoredListCtrl;

	// Construction
	public:

		COP_Entity();
		~COP_Entity();

		virtual void MarkDataDirty();

		//
		// Interface for property sheet.
		//
		virtual bool SaveData(void);
		virtual void UpdateData( int Mode, PVOID pData, bool bCanEdit );
		virtual void RememberState(void);

		//
		// Interface for custom edit control.
		//
		void SetNextVar(int cmd);

		void SetFlagsPage( COP_Flags *pFlagsPage );
		void OnUpdateSpawnFlags( unsigned long preserveMask, unsigned long newValues );
		
		//{{AFX_DATA(COP_Entity)
		enum { IDD = IDD_OBJPAGE_ENTITYKV };
		CAngleCombo m_AngleEdit;
		CAngleCombo m_SmartAngleEdit;
		CEdit m_cValue;
		CColoredListCtrl m_VarList;
		CEdit m_cKey;
		CFilteredComboBox m_cClasses;
		CEdit m_Comments;
		CEdit m_KeyValueHelpText;
		CButton	m_PasteControl;
		//}}AFX_DATA

		// ClassWizard generate virtual function overrides
		//{{AFX_VIRTUAL(COP_Entity)
		protected:
		virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
		virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
		//}}AFX_VIRTUAL

	protected:

	// Implementation of CFilteredComboBox::ICallbacks for m_cClasses.
	
		virtual void OnTextChanged( const char *pText );
		virtual bool OnUnknownEntry( const char *pText );

	// This gets routed from m_pSmartControl (for target names).
		
		virtual void OnSmartControlTargetNameChanged( const char *pText );

	// Implementation of CColoredListCtrl::IItemColorCallback.

		virtual void GetItemColor( int iItem, COLORREF *pBackgroundColor, COLORREF *pTextColor );
		virtual bool CustomDrawItemValue( const LPDRAWITEMSTRUCT p, const RECT *pRect );


	// Other functions.

		// If pMissingTarget is set to true, then it is a 
		void GetKeyState( const char *pShortName, EKeyState *pState, bool *pMissingTarget );
		void ResortItems();
		void LoadClassList();
		void SetSmartedit(bool bSet);
		void RemoveBlankKeys();

		void EnableAnglesControl(bool bEnable);

		void CreateSmartControls(GDinputvariable *pVar, CUtlVector<const char *> *pHelperType);
		void DestroySmartControls(void);

		CRect CalculateSmartControlRect();
		void CreateSmartControls_Angle( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont, bool *bShowSmartAngles );
		void CreateSmartControls_Choices( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont );
		void CreateSmartControls_TargetName( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont );
		void CreateSmartControls_BasicEditControl( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont, CUtlVector<const char *> *pHelperType );
		void CreateSmartControls_BrowseAndPlayButtons( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont );
		void CreateSmartControls_MarkAndEyedropperButtons( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont );
		void CreateSmartControls_PickButton( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont );
		void CreateSmartControls_InstanceVariable( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont );
		void CreateSmartControls_InstanceParm( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont );

		void UpdateDisplayClass(const char *pszClass);
		void UpdateDisplayClass(GDclass *pClass);

		void UpdateEditClass(const char *pszClass, bool bForce);
		void UpdateKeyValue(const char *szKey, const char *szValue);

		virtual void UpdatePickFaceText(CToolPickFace *pTool);
		void GetFaceIDListsForKey(CMapFaceIDList &FullFaces, CMapFaceIDList &PartialFaces, const char *pszKey);
		void GetFaceListsForKey(CMapFaceList &FullFaces, CMapFaceList &PartialFaces, const char *pszKey);
		void ApplyKeyValueToObject(CEditGameClass *pObject, const char *pszKey, const char *pszValue);

		void InternalOnChangeSmartcontrol( const char *szValue );

		// Generated message map functions
		//{{AFX_MSG(COP_Entity)
		afx_msg void OnAddkeyvalue();
		afx_msg BOOL OnApply(void);
		afx_msg void OnBrowse(void);
		afx_msg void OnBrowseInstance(void);
		afx_msg void OnPlaySound(void);
		virtual BOOL OnInitDialog();
		afx_msg void OnSelchangeKeyvalues();
		afx_msg void OnRemovekeyvalue();
		afx_msg void OnSelChangeAngleEdit(void);
		afx_msg void OnChangeAngleedit();
		afx_msg void OnSmartedit();
		afx_msg void OnChangeKeyorValue();
		afx_msg void OnCopy();
		afx_msg void OnPaste();
		afx_msg void OnSetfocusKey();
		afx_msg void OnKillfocusKey();
		afx_msg LRESULT OnChangeAngleBox(WPARAM, LPARAM);
		afx_msg void OnChangeSmartcontrol();
		afx_msg void OnChangeSmartcontrolSel();
		afx_msg void OnChangeInstanceVariableControl();
		afx_msg void OnChangeInstanceParmControl();
		afx_msg void OnPickFaces(void);
		afx_msg void OnPickColor();
		afx_msg void OnMark();
		afx_msg void OnSize( UINT nType, int cx, int cy );
		afx_msg void OnMarkAndAdd();
		afx_msg void OnEntityHelp(void);
		afx_msg void OnPickAngles(void);
		afx_msg void OnPickEntity(void);
		afx_msg void OnCameraDistance(void);
		afx_msg void OnItemChangedKeyValues(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnDblClickKeyValues(NMHDR* pNMHDR, LRESULT* pResult);
		//}}AFX_MSG

		void BrowseTextures( const char *szFilter, bool bIsSprite = false ); 
		bool BrowseModels( char *szModelName, int length, int &nSkin );
		void MergeObjectKeyValues(CEditGameClass *pEdit);
		void MergeKeyValue(char const *pszKey);
		void SetCurKey(LPCTSTR pszKey);
		void GetCurKey(CString& strKey);

		void SetCurVarListSelection( int iSel );
		int GetCurVarListSelection();

		void OnShowPropertySheet(BOOL bShow, UINT nStatus);
		void StopPicking(void);

		DECLARE_MESSAGE_MAP()

	private:

		void UpdateAnchors();
		void AssignClassDefaults(GDclass *pClass, GDclass *pOldClass);
		
		int GetKeyValueRowByShortName( const char *pShortName );		// Find the row in the listctrl that the var is at. Returns -1 if not found.
		
		void RefreshKVListValues( const char *pOnlyThisVar = NULL );
		void PresentProperties();
		void ClearVarList();
		void SetReadOnly(bool bReadOnly);

		void SetSmartControlText(const char *pszText);
		void PerformMark(const char *pTargetName, bool bClear, bool bNameOrClass);

		void LoadCustomColors();
		void SaveCustomColors();
		
		GDinputvariable *GetVariableAt( int index );

	private:
	
		CAnchorMgr m_AnchorMgr;
		
		CString m_szOldKeyName;
		bool m_bWantSmartedit;
		bool m_bEnableControlUpdate;	// Whether to reflect changes to the edit control into other controls.

		CAngleBox m_Angle;
		CAngleBox m_SmartAngle;

		CButton m_cPickColor;
		bool m_bSmartedit;
		int m_nNewKeyCount;

		CEdit		*m_pEditInstanceVariable, *m_pEditInstanceValue;
		CMyComboBox	*m_pComboInstanceParmType;

		// Used to prevent unnecessary calls to PresentProperties.
		int m_nPresentPropertiesCalls;
		bool m_bAllowPresentProperties;

		GDclass *m_pDisplayClass;		// The class that the dialog is showing. Can be different from m_pEditClass
										// until the user hits Apply.
		GDinputvariable *m_pInstanceVar;

		short m_VarMap[GD_MAX_VARIABLES];
		
		CWnd *m_pSmartControl;			// current smartedit control
		CButton *m_pSmartBrowseButton;
		CUtlVector<CWnd *> m_SmartControls;
		
		// The last variable we setup smart controls for.
		GDinputvariable *m_pLastSmartControlVar;
		CString m_LastSmartControlVarValue;
		
		CString m_strLastKey;			// Active key when SaveData was called.

		GDclass *m_pEditClass;			// The class of the object that we are editing.
		WCKeyValues m_kv;				// Our kv storage. Holds merged keyvalues for multiselect.
		WCKeyValues m_kvAdded;			// Corresponding keys set to value "1" if they were added

		GDIV_TYPE m_eEditType;			// The type of the currently selected key when SmartEdit is enabled.

		bool	 m_bIgnoreKVChange;			// Set to ignore Windows notifications when setting up controls.
		bool	 m_bChangingKeyName;
		
		int		m_iLastClassListSolidClasses;	// Used to prevent reinitializing the class list unnecessarily.

		bool m_bPicking;					// A picking tool is currently active.
		ToolID_t m_ToolPrePick;				// The tool that was active before we activated the picking tool.

		int m_iSortColumn;					// Which column we're sorting the keyvalues by.

		CPickAnglesTarget m_PickAnglesTarget;
		CPickEntityTarget m_PickEntityTarget;
		CPickFaceTarget m_PickFaceTarget;
		
		COP_Flags *m_pFlagsPage;
		
		CSmartControlTargetNameRouter m_SmartControlTargetNameRouter;

		CUtlMap<CString, CInstanceParmData> m_InstanceParmData; 
		
		// Used when multiselecting classes to remember whether they've selected a class
		// or not yet.
		bool m_bClassSelectionEmpty;
		CModelBrowser *pModelBrowser; 

	friend class CPickAnglesTarget;
	friend class CPickEntityTarget;
	friend class CPickFaceTarget;
	friend class CSmartControlTargetNameRouter;

	COLORREF CustomColors[16];
	bool m_bCustomColorsLoaded;
};

// These are used to load the filesystem open dialog.
void LoadFileSystemDialogModule();
void UnloadFileSystemDialogModule();

#endif // OP_ENTITY_H
