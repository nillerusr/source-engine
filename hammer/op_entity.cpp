//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "EntityHelpDlg.h"
#include "History.h"
#include "MainFrm.h"
#include "MapWorld.h"
#include "OP_Entity.h"
#include "CustomMessages.h"
#include "NewKeyValue.h"
#include "GlobalFunctions.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "ObjectProperties.h"
#include "TargetNameCombo.h"
#include "TextureBrowser.h"
#include "TextureSystem.h"
#include "ToolPickAngles.h"
#include "ToolPickEntity.h"
#include "ToolPickFace.h"
#include "ToolManager.h"
#include "SoundBrowser.h"
#include "ifilesystemopendialog.h"
#include "filesystem_tools.h"
#include "tier0/icommandline.h"
#include "HammerVGui.h"
#include "mapview3d.h"
#include "camera.h"
#include "Selection.h"
#include "options.h"
#include "op_flags.h"
#include "MapInstance.h"

extern GameData *pGD;		// current game data

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning( disable : 4355 )


#define IDC_SMARTCONTROL 1
#define IDC_SMARTCONTROL_TARGETNAME 2	// We have a different define for this because we rely 100%
										// on CTargetNameComboBox for the info about its updates.
#define IDC_SMARTCONTROL_INSTANCE_VARIABLE	3
#define IDC_SMARTCONTROL_INSTANCE_VALUE		4
#define IDC_SMARTCONTROL_INSTANCE_PARM		5

#define SPAWNFLAGS_KEYNAME	"spawnflags"

#define INSTANCE_VAR_MAP_START		-10


static WCKeyValues kvClipboard;
static BOOL bKvClipEmpty = TRUE;

// Colors used for the keyvalues list control.
static COLORREF g_BgColor_Edited		= RGB( 239, 239, 255 ); // blue
static COLORREF g_BgColor_Default		= RGB( 255, 255, 255 ); // white
static COLORREF g_BgColor_Added			= RGB( 255, 239, 239 ); // pink
static COLORREF g_BgColor_InstanceParm  = RGB( 239, 255, 239 ); // green
static COLORREF g_TextColor_Normal			= RGB( 0, 0, 0 );
static COLORREF g_TextColor_MissingTarget	= RGB( 255, 0, 0 ); // dark red

static int g_DumbEditControls[] = {IDC_DELETEKEYVALUE, IDC_KEY, IDC_VALUE, IDC_ADDKEYVALUE, IDC_KEY_LABEL, IDC_VALUE_LABEL};

//-----------------------------------------------------------------------------
// Less function for use with CString
//-----------------------------------------------------------------------------
bool CStringLessFunc(const CString &lhs, const CString &rhs)
{
	return (Q_strcmp(lhs, rhs) < 0);
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the string specifies the name of an entity in the world.
//-----------------------------------------------------------------------------
static bool IsValidTargetName( const char *pTestName )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	CMapWorld *pWorld = pDoc->GetMapWorld();
	const CMapEntityList *pList = pWorld->EntityList_GetList();

	for ( int i=0; i < pList->Count(); i++ )
	{
		CMapEntity *pEntity = pList->Element( i );
		const char *pszTargetName = pEntity->GetKeyValue("targetname");
		if ( pszTargetName && Q_stricmp( pszTargetName, pTestName ) == 0 )
			return true;
	}

	return false;
}



static CString StripDirPrefix( const char *pFilename, const char *pPrefix )
{
	int prefixLen = V_strlen( pPrefix );
	if ( V_stristr( pFilename, pPrefix ) == pFilename )
	{
		if ( pFilename[prefixLen] == '/' || pFilename[prefixLen] == '\\' )
			return pFilename + prefixLen + 1;
	}
	
	return pFilename;
}


//-----------------------------------------------------------------------------
// CColoredListCtrl implementation.
//-----------------------------------------------------------------------------

CColoredListCtrl::CColoredListCtrl( IItemColorCallback *pCallback )
{
	m_pCallback = pCallback;
}


void CColoredListCtrl::DrawItem( LPDRAWITEMSTRUCT p )
{
	CDC *pDC = CDC::FromHandle( p->hDC );

	COLORREF bgColor, txtColor;
	m_pCallback->GetItemColor( p->itemID, &bgColor, &txtColor );

	// Draw the background.
	CBrush br;
	CPen pen( PS_SOLID, 0, bgColor );
	
	// Selected? Draw a dotted border.
	LOGBRUSH logBrush;
	logBrush.lbColor = RGB(0,0,0);
	logBrush.lbHatch = HS_CROSS;
	logBrush.lbStyle = BS_SOLID;
	CPen dashedPen( PS_ALTERNATE, 1, &logBrush );
	
	if ( p->itemState & ODS_SELECTED )
		pDC->SelectObject( &dashedPen );
	else
		pDC->SelectObject( &pen );

	br.CreateSolidBrush( bgColor );
	pDC->SelectObject( &br );
	RECT rcFill = p->rcItem;
	rcFill.bottom -= 1;
	pDC->Rectangle( &rcFill );

	// Setup for drawing text.
	pDC->SetTextColor( txtColor );

	// Draw the first column.
	RECT rcItem = p->rcItem;
	rcItem.left += 3;
	pDC->DrawText( GetItemText( p->itemID, 0 ), &rcItem, DT_LEFT | DT_VCENTER );

	// Draw the second column.
	LVCOLUMN columnInfo;
	columnInfo.mask = LVCF_WIDTH;
	GetColumn( 0, &columnInfo );
	rcItem = p->rcItem;
	rcItem.left += columnInfo.cx;

	// Give our owner a chance to draw the second column.
	if ( !m_pCallback->CustomDrawItemValue( p, &rcItem ) )
	{
		rcItem.left += 3;
		pDC->DrawText( GetItemText( p->itemID, 1 ), &rcItem, DT_LEFT | DT_VCENTER );
	}
}


class CMyEdit : public CEdit
{
	public:
		void SetParentPage(COP_Entity* pPage)
		{
			m_pParent = pPage;
		}

		afx_msg void OnChar(UINT, UINT, UINT);

		COP_Entity *m_pParent;
		DECLARE_MESSAGE_MAP()
};


BEGIN_MESSAGE_MAP(CMyEdit, CEdit)
	ON_WM_CHAR()
END_MESSAGE_MAP()
	

class CMyComboBox : public CComboBox
{
	public:
		void SetParentPage(COP_Entity* pPage)
		{
			m_pParent = pPage;
		}

		afx_msg void OnChar(UINT, UINT, UINT);

		COP_Entity *m_pParent;
		DECLARE_MESSAGE_MAP()
};


BEGIN_MESSAGE_MAP(CMyComboBox, CComboBox)
	ON_WM_CHAR()
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Called by the angles picker tool when a target point is picked. This
//			stuffs the angles into the smartedit control so that the entity
//			points at the target position.
//-----------------------------------------------------------------------------
void CPickAnglesTarget::OnNotifyPickAngles(const Vector &vecPos)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (!pDoc)
	{
		return;
	}

	GetHistory()->MarkUndoPosition(pDoc->GetSelection()->GetList(), "Point At");

	//
	// Update the edit control text with the entity name. This text will be
	// stuffed into the local keyvalue storage in OnChangeSmartControl.
	//
	FOR_EACH_OBJ( *m_pDlg->m_pObjectList, pos )
	{
		CMapClass *pObject = m_pDlg->m_pObjectList->Element(pos);
		CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);
		Assert(pEntity != NULL);
		if (pEntity != NULL)
		{
			GetHistory()->Keep(pEntity);

			// Calculate the angles to point this entity at the chosen spot.
			Vector vecOrigin;
			pEntity->GetOrigin(vecOrigin);
			Vector vecForward = vecPos - vecOrigin;

			QAngle angFace;
			VectorAngles(vecForward, angFace);

			// HACK: lights negate pitch
			if (pEntity->GetClassName() && (!strnicmp(pEntity->GetClassName(), "light_", 6)))
			{
				angFace[PITCH] *= -1;
			}

			// Update the edit control with the calculated angles.
			char szAngles[80];	
			sprintf(szAngles, "%.0f %.0f %.0f", angFace[PITCH], angFace[YAW], angFace[ROLL]);
			pEntity->SetKeyValue("angles", szAngles);

			// HACK: lights have a separate "pitch" key
			if (pEntity->GetClassName() && (!strnicmp(pEntity->GetClassName(), "light_", 6)))
			{
				char szPitch[20];
				sprintf(szPitch, "%.0f", angFace[PITCH]);
				pEntity->SetKeyValue("pitch", szPitch);
			}

			// FIXME: this should be called automatically, but it isn't
			m_pDlg->OnChangeSmartcontrol();
		}
	}
	m_pDlg->StopPicking();

	GetMainWnd()->pObjectProperties->MarkDataDirty();		
}


//-----------------------------------------------------------------------------
// Purpose: Called by the entity picker tool when an entity is picked. This
//			stuffs the entity name into the smartedit control.
//-----------------------------------------------------------------------------
void CPickEntityTarget::OnNotifyPickEntity(CToolPickEntity *pTool)
{
	//
	// Update the edit control text with the entity name. This text will be
	// stuffed into the local keyvalue storage in OnChangeSmartControl.
	//
	CMapEntityList Full;
	CMapEntityList Partial;
	pTool->GetSelectedEntities(Full, Partial);
	CMapEntity *pEntity = Full.Element(0);
	if (pEntity)
	{
		const char *pszValue = pEntity->GetKeyValue(m_szKey);
		if (!pszValue)
		{
			pszValue = "";
		}

		m_pDlg->SetSmartControlText(pszValue);
	}

	m_pDlg->StopPicking();
}


//-----------------------------------------------------------------------------
// Purpose: Called by the face picker tool when the face selection changes.
// Input  : pTool - The face picker tool that is notifying us.
//-----------------------------------------------------------------------------
void CPickFaceTarget::OnNotifyPickFace(CToolPickFace *pTool)
{
	m_pDlg->UpdatePickFaceText(pTool);
}


CSmartControlTargetNameRouter::CSmartControlTargetNameRouter( COP_Entity *pDlg )
{
	m_pDlg = pDlg;
}

void CSmartControlTargetNameRouter::OnTextChanged( const char *pText )
{
	m_pDlg->OnSmartControlTargetNameChanged( pText );
}


BEGIN_MESSAGE_MAP(COP_Entity, CObjectPage)
	//{{AFX_MSG_MAP(COP_Entity)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_KEYVALUES, OnItemChangedKeyValues)
	ON_NOTIFY(NM_DBLCLK, IDC_KEYVALUES, OnDblClickKeyValues)

	ON_BN_CLICKED(IDC_ADDKEYVALUE, OnAddkeyvalue)
	ON_BN_CLICKED(IDC_REMOVEKEYVALUE, OnRemovekeyvalue)
	ON_BN_CLICKED(IDC_SMARTEDIT, OnSmartedit)
	ON_EN_CHANGE(IDC_VALUE, OnChangeKeyorValue)
	ON_BN_CLICKED(IDC_COPY, OnCopy)
	ON_BN_CLICKED(IDC_PASTE, OnPaste)
	ON_BN_CLICKED(IDC_PICKCOLOR, OnPickColor)
	ON_WM_SIZE()
	ON_EN_SETFOCUS(IDC_KEY, OnSetfocusKey)
	ON_EN_KILLFOCUS(IDC_KEY, OnKillfocusKey)
	ON_MESSAGE(ABN_CHANGED, OnChangeAngleBox)
	ON_CBN_SELCHANGE(IDC_SMARTCONTROL, OnChangeSmartcontrolSel)
	ON_CBN_EDITUPDATE(IDC_SMARTCONTROL, OnChangeSmartcontrol)
	ON_EN_CHANGE(IDC_SMARTCONTROL, OnChangeSmartcontrol)
	ON_BN_CLICKED(IDC_BROWSE, OnBrowse)
	ON_BN_CLICKED(IDC_BROWSE_INSTANCE, OnBrowseInstance)
	ON_BN_CLICKED(IDC_PLAY_SOUND, OnPlaySound)
	ON_BN_CLICKED(IDC_MARK, OnMark)
	ON_BN_CLICKED(IDC_MARK_AND_ADD, OnMarkAndAdd)
	ON_BN_CLICKED(IDC_PICK_FACES, OnPickFaces)
	ON_BN_CLICKED(IDC_ENTITY_HELP, OnEntityHelp)
	ON_BN_CLICKED(IDC_PICK_ANGLES, OnPickAngles)
	ON_BN_CLICKED(IDC_PICK_ENTITY, OnPickEntity)
	ON_BN_CLICKED(IDC_CAMERA_DISTANCE, OnCameraDistance)
	ON_EN_CHANGE(IDC_SMARTCONTROL_INSTANCE_VARIABLE, OnChangeInstanceVariableControl)
	ON_EN_CHANGE(IDC_SMARTCONTROL_INSTANCE_VALUE, OnChangeInstanceVariableControl)
	ON_EN_CHANGE(IDC_SMARTCONTROL_INSTANCE_PARM, OnChangeInstanceParmControl)
	ON_CBN_SELCHANGE(IDC_SMARTCONTROL_INSTANCE_PARM, OnChangeInstanceParmControl)
	ON_CBN_EDITUPDATE(IDC_SMARTCONTROL_INSTANCE_PARM, OnChangeInstanceParmControl)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


IMPLEMENT_DYNCREATE(COP_Entity, CObjectPage)


typedef int (CALLBACK *ColumnSortFn)( LPARAM iItem1, LPARAM iItem2, LPARAM lpParam );

int InternalSortByColumn( COP_Entity *pDlg, const char *pShortName1, const char *pShortName2, int iColumn )
{
	int i1 = pDlg->GetKeyValueRowByShortName( pShortName1 );
	int i2 = pDlg->GetKeyValueRowByShortName( pShortName2 );
	if ( i1 == -1 || i2 == -1 )
		return 0;
	
	CString str1 = pDlg->m_VarList.GetItemText( i1, iColumn );
	CString str2 = pDlg->m_VarList.GetItemText( i2, iColumn );
	
	return Q_stricmp( str1, str2 );
}

int CALLBACK SortByItemEditedState( LPARAM iItem1, LPARAM iItem2, LPARAM lpParam )
{
	COP_Entity *pDlg = (COP_Entity*)lpParam;
	if ( !pDlg->m_pDisplayClass )
		return 0;
		
	const char *pShortName1 = (const char*)iItem1;
	const char *pShortName2 = (const char*)iItem2;

	EKeyState s1, s2;
	bool b1, b2;
	pDlg->GetKeyState( pShortName1, &s1, &b1 );
	pDlg->GetKeyState( pShortName2, &s2, &b2 );
	
	bool bNew1 = (s1 == k_EKeyState_AddedManually);
	bool bNew2 = (s2 == k_EKeyState_AddedManually);
	
	return bNew1 < bNew2;
}

static int CALLBACK SortByColumn0( LPARAM iItem1, LPARAM iItem2, LPARAM lpParam )
{
	return InternalSortByColumn( (COP_Entity*)lpParam, (const char*)iItem1, (const char*)iItem2, 0 );
}

static int CALLBACK SortByColumn1( LPARAM iItem1, LPARAM iItem2, LPARAM lpParam )
{
	return InternalSortByColumn( (COP_Entity*)lpParam, (const char*)iItem1, (const char*)iItem2, 1 );
}

ColumnSortFn g_ColumnSortFunctions[] =
{
	SortByItemEditedState,
	SortByColumn0,
	SortByColumn1
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COP_Entity::COP_Entity()
	: CObjectPage(COP_Entity::IDD), 
	m_cClasses( this ), 
	m_SmartControlTargetNameRouter( this ),
	m_VarList( this ),
	m_InstanceParmData( CStringLessFunc )
{
	//{{AFX_DATA_INIT(COP_Entity)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	m_iLastClassListSolidClasses = -9999;
	m_bAllowPresentProperties = true;
	m_nPresentPropertiesCalls = 0;
	m_bClassSelectionEmpty = false;
	m_cClasses.SetOnlyProvideSuggestions( true );

	m_bPicking = false;
	m_bChangingKeyName = false;
	
	m_pSmartBrowseButton = NULL;
	m_pLastSmartControlVar = NULL;

	m_pEditInstanceVariable = NULL;
	m_pEditInstanceValue = NULL;
	m_pComboInstanceParmType = NULL;

	m_bIgnoreKVChange = false;
	m_bSmartedit = true;
	m_pSmartControl = NULL;
	m_pDisplayClass = NULL;
	m_pEditClass = NULL;
	m_eEditType = ivString;

	m_nNewKeyCount = 0;
	m_iSortColumn = -1;

	m_bEnableControlUpdate = true;

	m_pEditObjectRuntimeClass = RUNTIME_CLASS(editCEditGameClass);

	pModelBrowser = NULL;
	m_pInstanceVar = NULL;

	m_bCustomColorsLoaded = false; //Make sure they get loaded!
	memset(CustomColors, 0, sizeof(CustomColors));
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
COP_Entity::~COP_Entity(void)
{
	DestroySmartControls();

	delete pModelBrowser;
	pModelBrowser = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDX - 
//-----------------------------------------------------------------------------
void COP_Entity::DoDataExchange(CDataExchange* pDX)
{
	CObjectPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COP_Entity)
	DDX_Control(pDX, IDC_VALUE, m_cValue);
	DDX_Control(pDX, IDC_KEYVALUES, m_VarList);
	DDX_Control(pDX, IDC_KEY, m_cKey);
	DDX_Control(pDX, IDC_ENTITY_COMMENTS, m_Comments);
	DDX_Control(pDX, IDC_KEYVALUE_HELP, m_KeyValueHelpText);
	DDX_Control(pDX, IDC_PASTE, m_PasteControl);
	//}}AFX_DATA_MAP
}


//-----------------------------------------------------------------------------
// Purpose: Handles notifications..
//-----------------------------------------------------------------------------
BOOL COP_Entity::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult) 
{
	NMHDR *pHdr = (NMHDR*)lParam;
	if ( pHdr->idFrom == IDC_KEYVALUES )
	{
		if ( pHdr->code == LVN_COLUMNCLICK )
		{
			LPNMLISTVIEW pListView = (LPNMLISTVIEW)lParam;

			// Now sort by this column.
			m_iSortColumn = max( 0, min( pListView->iSubItem, ARRAYSIZE( g_ColumnSortFunctions ) - 1 ) );
			ResortItems();
		}
	}

	return CObjectPage::OnNotify(wParam, lParam, pResult);
}

//-----------------------------------------------------------------------------
// Purpose: Determine how/if this key's value has been modified relative
//          to its default in the FGD file.
//-----------------------------------------------------------------------------
void COP_Entity::GetKeyState( const char *pShortName, EKeyState *pState, bool *pMissingTarget )
{
	*pMissingTarget = false;
	
	// If we're in multiedit mode with various types of entities selected, then don't look at m_pDisplayClass.
	if ( !m_pDisplayClass )
	{
		*pState = k_EKeyState_DefaultFGDValue;
		return;
	}
	
	const char *pszCurValue = m_kv.GetValue( pShortName );
	if ( !pszCurValue )
		pszCurValue = "";
	
	// Now see if this var is even in the FGD.
	GDinputvariable *pVar = m_pDisplayClass->VarForName( pShortName );
	if ( !pVar )
	{
		if ( m_InstanceParmData.Find( pShortName ) != m_InstanceParmData.InvalidIndex() )
		{
			*pState = k_EKeyState_InstanceParm;
		}
		else
		{
			*pState = k_EKeyState_AddedManually;
		}
		return;
	}

	// Missing targetname?
	if ((pVar->GetType() == ivTargetSrc) || (pVar->GetType() == ivTargetDest))
	{
		if ( pszCurValue[0] && !IsValidTargetName( pszCurValue ) )
			*pMissingTarget = true;
	}
	
	// Now we know it's in the FGD, so see if the value has been modified from the default.
	GDinputvariable varCopy;
	varCopy = *pVar;
	MDkeyvalue tmpkv;
	varCopy.ResetDefaults();
	varCopy.ToKeyValue( &tmpkv );

	if ( Q_stricmp( pszCurValue, tmpkv.szValue ) == 0 )
		*pState = k_EKeyState_DefaultFGDValue;
	else
		*pState = k_EKeyState_Modified;
}


void COP_Entity::ResortItems()
{
	m_VarList.SortItems( g_ColumnSortFunctions[m_iSortColumn+1], (LPARAM)this );
	
	// Update m_VarMap if in smart edit mode.
	if ( m_bSmartedit )
	{
		for ( int i=0; i < m_VarList.GetItemCount(); i++ )
		{
			const char *pShortName = (const char*)m_VarList.GetItemData( i );
			if ( !pShortName )
				continue;

			int index = -1;
			if ( m_pDisplayClass && m_pDisplayClass->VarForName( pShortName, &index ) )
			{
				m_VarMap[i] = index;
			}
			else
			{
				index = m_InstanceParmData.Find( pShortName );
				if ( index != m_InstanceParmData.InvalidIndex() )
				{
					m_VarMap[i] = INSTANCE_VAR_MAP_START - index;
				}
				else
				{
					m_VarMap[i] = -1;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void DumpKeyvalues(WCKeyValues &kv)
{
	for (int i = kv.GetFirst(); i != kv.GetInvalidIndex(); i=kv.GetNext( i ) )
	{
		DBG("   %d: %s\n", i, kv.GetKey(i));
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds an object's keys to our list of keys. If a given key is already
//			in the list, it is either ignored or set to a "different" if the value
//			is different from the value in our list.
// Input  : pEdit - Object whose keys are to be added to our list.
//-----------------------------------------------------------------------------
void COP_Entity::MergeObjectKeyValues(CEditGameClass *pEdit)
{
	//VPROF_BUDGET( "COP_Entity::MergeObjectKeyValues", "Object Properties" );
	for ( int i=pEdit->GetFirstKeyValue(); i != pEdit->GetInvalidKeyValue(); i=pEdit->GetNextKeyValue( i ) )
	{
		LPCTSTR pszCurValue = m_kv.GetValue(pEdit->GetKey(i));
		if (pszCurValue == NULL)
		{
			//
			// Doesn't exist yet - set current value.
			//
			m_kv.SetValue(pEdit->GetKey(i), pEdit->GetKeyValue(i));
		}
		else if (strcmp(pszCurValue, pEdit->GetKeyValue(i)))
		{
			//
			// Already present - we need to merge the value with the existing data.
			//
			MergeKeyValue(pEdit->GetKey(i));
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Updates the dialog's keyvalue data with a given keyvalue. If the
//			data can be merged in with existing data in a meaningful manner,
//			that will be done. If not, VALUE_DIFFERENT_STRING will be set to
//			indicate that not all objects have the same value for the key.
// Input  : pszKey - 
//-----------------------------------------------------------------------------
void COP_Entity::MergeKeyValue(char const *pszKey)
{
	//VPROF_BUDGET( "COP_Entity::MergeKeyValue", "Object Properties" );

	Assert(pszKey);
	if (!pszKey)
	{
		return;
	}

	bool bHandled = false;

	if (m_pEditClass != NULL)
	{
		GDinputvariable *pVar = m_pEditClass->VarForName(pszKey);
		if (pVar != NULL)
		{
			switch (pVar->GetType())
			{
				case ivSideList:
				{
					//
					// Merging sidelist keys is a little complicated. We build a string
					// representing the merged sidelists.
					//
					CMapFaceIDList FaceIDListFull;
					CMapFaceIDList FaceIDListPartial;

					GetFaceIDListsForKey(FaceIDListFull, FaceIDListPartial, pszKey);

					char szValue[KEYVALUE_MAX_VALUE_LENGTH];
					CMapWorld::FaceID_FaceIDListsToString(szValue, sizeof(szValue), &FaceIDListFull, &FaceIDListPartial);
					m_kv.SetValue(pszKey, szValue);
			
					bHandled = true;
					break;
				}

				case ivAngle:
				{
					//
					// We can't merge angles, so set the appropriate angles control to "different".
					// We'll catch the main angles control below, since it's supported even
					// for objects of an unknown class.
					//
					if (stricmp(pVar->GetName(), "angles"))
					{
						m_SmartAngle.SetDifferent(true);
					}
					break;
				}
			}
		}
	}

	if (!bHandled)
	{
		//
		// Can't merge with current value - show a "different" string.
		//
		m_kv.SetValue(pszKey, VALUE_DIFFERENT_STRING);

		if (!stricmp(pszKey, "angles"))
		{
			// We can't merge angles, so set the main angles control to "different".
			m_Angle.SetDifferent(true);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Mode - 
//			pData - 
//-----------------------------------------------------------------------------
void COP_Entity::UpdateData( int Mode, PVOID pData, bool bCanEdit )
{
	//VPROF_BUDGET( "COP_Entity::UpdateData", "Object Properties" );

	//DBG("UpdateData\n");
	//DumpKeyvalues(m_kv);

	__super::UpdateData( Mode, pData, bCanEdit );

	if (!IsWindow(m_hWnd))
	{
		return;
	}

	if (GetFocus() == &m_cKey)
	{
		OnKillfocusKey();
	}

	if (Mode == LoadFinished)
	{
		m_kvAdded.RemoveAll();
		m_bAllowPresentProperties = true;
		PresentProperties();
		return;
	}
	else if ( Mode == LoadData || Mode == LoadFirstData )
	{
		// Wait until the LoadFinished call to create all the controls,
		// otherwise it'll do a lot of unnecessary work.
		m_bAllowPresentProperties = false;
	}

	if (!pData)
	{
		return;
	}

	CEditGameClass *pEdit = (CEditGameClass *)pData;
	char szBuf[KEYVALUE_MAX_VALUE_LENGTH];

	if (Mode == LoadFirstData)
	{
		LoadClassList();

		m_nNewKeyCount = 1;

		QAngle vecAngles;
		pEdit->GetAngles(vecAngles);
		m_Angle.SetAngles(vecAngles, false);
		m_Angle.SetDifferent(false);

		#ifdef NAMES
		if(pEdit->pClass)
		{
			sprintf(szBuf, "%s (%s)", pEdit->pClass->GetDescription(), pEdit->pClass->GetName());
		}
		else
		#endif

		V_strcpy_safe( szBuf, pEdit->GetClassName() );
		m_cClasses.AddSuggestion( szBuf );	// If we don't make sure it has this item in its list, it will do 
											// Bad Things later on. This only happens when the FGD is missing an
											// entity that is in the map file. In that case, just let it be.
											// Check For Problems should ID the problem for them.
		m_cClasses.SelectItem(szBuf);
		m_bClassSelectionEmpty = false;

		//
		// Can't change the class of the worldspawn entity.
		//
		if ( pEdit->IsClass("worldspawn") || m_bCanEdit == false )
		{
			m_cClasses.EnableWindow(FALSE);
		}
		else
		{
			m_cClasses.EnableWindow(TRUE);
		}

		//
		// Get the comments text from the entity.
		//
		m_Comments.SetWindowText(pEdit->GetComments());

		//
		// Add entity's keys to our local storage
		//
		m_kv.RemoveAll();
		for ( int i=pEdit->GetFirstKeyValue(); i != pEdit->GetInvalidKeyValue(); i=pEdit->GetNextKeyValue( i ) )
		{
			const char *pszKey = pEdit->GetKey(i);
			const char *pszValue = pEdit->GetKeyValue(i);
			if ((pszKey != NULL) && (pszValue != NULL))
			{
				m_kv.SetValue(pszKey, pszValue);
			}
		}

		UpdateEditClass(pEdit->GetClassName(), true);
		UpdateDisplayClass(pEdit->GetClassName());

		SetCurKey(m_strLastKey);
	}
	else if (Mode == LoadData)
	{
		//
		// Not first data - merge with current stuff.
		//

		//
		// Deal with class name.
		//
		CString str = m_cClasses.GetCurrentItem();
		if ( m_bClassSelectionEmpty )
			str = "";
			
		if (strcmpi(str, pEdit->GetClassName()))
		{
			//
			// Not the same - set class to be blank and 
			// disable smartedit.
			//
			m_cClasses.ForceEditControlText( "" );
			m_bClassSelectionEmpty = true;

			UpdateEditClass("", false);
			UpdateDisplayClass("");
		}
		else
		{
			LoadClassList();
			m_cClasses.SelectItem( pEdit->GetClassName() );
		}

		//
		// Mark the comments field as "(different)" if it isn't the same as this entity's
		// comments.
		//
		char szComments[1024];
		m_Comments.GetWindowText(szComments, sizeof(szComments));
		if (strcmp(szComments, pEdit->GetComments()) != 0)
		{
			m_Comments.SetWindowText(VALUE_DIFFERENT_STRING);
		}

		MergeObjectKeyValues(pEdit);
		SetCurKey(m_strLastKey);
	}
	else
	{
		Assert(FALSE);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Stops entity, face, or angles picking.
//-----------------------------------------------------------------------------
void COP_Entity::StopPicking(void)
{
	if (m_bPicking)
	{
		m_bPicking = false;
		ToolManager()->SetTool(m_ToolPrePick);

		//
		// Stop angles picking if we are doing so.
		//
		CButton *pButton = (CButton *)GetDlgItem(IDC_PICK_ANGLES);
		if (pButton)
		{
			pButton->SetCheck(0);
		}

		//
		// Stop entity picking if we are doing so.
		//
		pButton = (CButton *)GetDlgItem(IDC_PICK_ENTITY);
		if (pButton)
		{
			pButton->SetCheck(0);
		}

		//
		// Stop face picking if we are doing so.
		//
		pButton = (CButton *)GetDlgItem(IDC_PICK_FACES);
		if (pButton)
		{
			pButton->SetCheck(0);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called manually from CObjectProperties::OnApply because the Apply
//			button is implemented in a nonstandard way. I'm not sure why.
//-----------------------------------------------------------------------------
BOOL COP_Entity::OnApply(void)
{
	m_pLastSmartControlVar = NULL;	// Force it to recreate the target name combo if need be, 
									// because we might have locked in a new targetname.
	StopPicking();
	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEntity - 
//			pszKey - 
//			pszValue - 
//-----------------------------------------------------------------------------
void COP_Entity::ApplyKeyValueToObject(CEditGameClass *pObject, const char *pszKey, const char *pszValue)
{
	//VPROF_BUDGET( "COP_Entity::ApplyKeyValueToObject", "Object Properties" );

	GDclass *pClass = pObject->GetClass();
	if (pClass != NULL)
	{
		GDinputvariable *pVar = pClass->VarForName(pszKey);
		if (pVar != NULL)
		{
			if ((pVar->GetType() == ivSideList) || (pVar->GetType() == ivSide))
			{
				CMapWorld *pWorld = GetActiveWorld();

				//
				// Get the face list currently set in this keyvalue.
				//
				CMapFaceIDList CurFaceList;
				const char *pszCurVal = pObject->GetKeyValue(pszKey);
				if (pszCurVal != NULL)
				{
					pWorld->FaceID_StringToFaceIDLists(&CurFaceList, NULL, pszCurVal);
				}

				//
				// Build the face list to apply. Only include the faces that are:
				//
				// 1. In the full selection list (outside the parentheses).
				// 2. In the partial selection set (inside the parentheses) AND are in the
				//    original face list.
				//
				// FACEID TODO: Optimize so that we only call StringToFaceList once per keyvalue
				//		 instead of once per keyvalue per entity being applied to.
				CMapFaceIDList FullFaceList;
				CMapFaceIDList PartialFaceList;
				pWorld->FaceID_StringToFaceIDLists(&FullFaceList, &PartialFaceList, pszValue);
				
				CMapFaceIDList KeepFaceList;
				for (int i = 0; i < PartialFaceList.Count(); i++)
				{
					int nFace = PartialFaceList.Element(i);

					if (CurFaceList.Find(nFace) != -1)
					{	
						KeepFaceList.AddToTail(nFace);
					}
				}

				FullFaceList.AddVectorToTail(KeepFaceList);

				char szSetValue[KEYVALUE_MAX_VALUE_LENGTH];
				CMapWorld::FaceID_FaceIDListsToString(szSetValue, sizeof(szSetValue), &FullFaceList, NULL);

				pObject->SetKeyValue(pszKey, szSetValue);
				return;
			}
		}
	}

	pObject->SetKeyValue(pszKey, pszValue);
}


//-----------------------------------------------------------------------------
// Purpose: Called by the sheet to let the page remember its state before a
//			refresh of the data.
//-----------------------------------------------------------------------------
void COP_Entity::RememberState(void)
{
	GetCurKey(m_strLastKey);
}


//-----------------------------------------------------------------------------
// Purpose: Called by the object properties page to tell us that all our data
// is dirty. We don't have to do anything on most of our data because it'll 
// get regenerated, but we must clear out our class pointers because we'll
// do crazy things if they give us a new class and have us load the data
// (we'll think it just changed from one class to another and we'll wipe
// out spawnflags, for instance). So clear out the class pointers.
//-----------------------------------------------------------------------------
void COP_Entity::MarkDataDirty()
{
	UpdateDisplayClass( (GDclass*)NULL );
	m_pEditClass = NULL;
	m_VarList.DeleteAllItems();
	m_InstanceParmData.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Saves the dialog data into the objects being edited.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool COP_Entity::SaveData(void)
{
	//VPROF_BUDGET( "COP_Entity::SaveData", "Object Properties" );

	//DBG("SaveData\n");
	//DumpKeyvalues(m_kv);

	RememberState();

	CString strClassName = m_cClasses.GetCurrentItem();
	
	// If we were multiselecting entities and they haven't chosen a new classname yet, 
	// we need to know that here so we don't force them all to be the last selection in the classname combo.
	if ( m_bClassSelectionEmpty )
		strClassName = "";

	UpdateEditClass(strClassName, false);

	//
	// Apply the dialog data to all the objects being edited.
	//
	FOR_EACH_OBJ( *m_pObjectList, pos )
	{
		CMapClass *pObject = m_pObjectList->Element(pos);
		CEditGameClass *pEdit = dynamic_cast <CEditGameClass *>(pObject);
		Assert(pEdit != NULL);

		if (pEdit != NULL)
		{
			RemoveBlankKeys();

			//
			// Give keys back to object. For every key in our local storage that is
			// also found in the list of added keys, set the key value in the edit
			// object(s).
			//
			for (int i = m_kv.GetFirst(); i != m_kv.GetInvalidIndex(); i=m_kv.GetNext( i ) )
			{
				MDkeyvalue &kvCur = m_kv.GetKeyValue(i);
				const char *pszAddedKeyValue = m_kvAdded.GetValue(kvCur.szKey);							
				if (pszAddedKeyValue != NULL)
				{
					Q_FixSlashes( kvCur.szValue, '/' );	
					//
					// Don't store keys with multiple/undefined values.
					//
					if (strcmp(kvCur.szValue, VALUE_DIFFERENT_STRING))
					{
						//DBG("    apply key %s\n", kvCur.szKey);
						ApplyKeyValueToObject(pEdit, kvCur.szKey, kvCur.szValue);
					}
				}
			}
			
			//
			// All keys in the object should also be found in local storage,
			// unless the user deleted them. If there are any such extraneous
			// keys, get rid of them. Go from the top down because otherwise
			// deleting messes up our iteration.
			//
			int iNext;
			for ( int i=pEdit->GetFirstKeyValue(); i != pEdit->GetInvalidKeyValue(); i=iNext )
			{
				iNext = pEdit->GetNextKeyValue( i );
				//
				// If this key is in not in our local storage, delete it from the object.
				//
				if (!m_kv.GetValue(pEdit->GetKey(i)))
				{
					//DBG("    delete key %s\n", pEdit->GetKey(i));
					pEdit->DeleteKeyValue(pEdit->GetKey(i));
				}
			}

			//
			// Store class.
			//
			if (strClassName[0])
			{
				pEdit->SetClass(strClassName);
			}
			
			//
			// Store the entity comments.
			//
			char szComments[1024];
			szComments[0] = '\0';
			m_Comments.GetWindowText(szComments, sizeof(szComments));
			if (strcmp(szComments, VALUE_DIFFERENT_STRING) != 0)
			{
				pEdit->SetComments(szComments);
			}
		}

		pObject->PostUpdate(Notify_Changed);
	}

	UpdateDisplayClass(strClassName);
	
	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Given the short name of a key, find which row it's at in the list control.
//-----------------------------------------------------------------------------
int COP_Entity::GetKeyValueRowByShortName( const char *pShortName )
{	
	const char *pSearchString = pShortName;
	if ( m_bSmartedit )
	{
		if ( m_pDisplayClass )
		{
			GDinputvariable *pVar = m_pDisplayClass->VarForName( pShortName );
			if (pVar)
				pSearchString = pVar->GetLongName();
		}
	}
	 
	LVFINDINFO fi;
	memset( &fi, 0, sizeof( fi ) );
	fi.flags = LVFI_STRING;
	fi.psz = pSearchString;
	return m_VarList.FindItem( &fi );
}


//-----------------------------------------------------------------------------
// Purpose: Fills the values in the second column for all properties.
//-----------------------------------------------------------------------------
void COP_Entity::RefreshKVListValues( const char *pOnlyThisVar )
{
	// We match listctrl elements to their values in 2 different ways:
	// 1. In smartedit mode, the raw name of the property in the first column is matched to m_kv.
	// 2. In non-smartedit mode, we look at the lParam entry in the listctrl.
	for ( int i=0; i < m_VarList.GetItemCount(); i++ )
	{
		const char *pVarName = (const char*)m_VarList.GetItemData( i );
		const char *pValue = NULL;
		char tmpValueBuf[512];

		// If they only wanted to update one var...
		if ( pOnlyThisVar && Q_stricmp( pVarName, pOnlyThisVar ) != 0 )
			continue;
		
		if ( m_bSmartedit )
		{
			GDinputvariable *pVar = (m_pDisplayClass ? m_pDisplayClass->VarForName( pVarName ) : NULL);
			if ( pVar )
			{
				const char *pUnformattedValue = m_kv.GetValue( pVar->GetName() );
				pValue = pUnformattedValue; // Default is unformatted.
				
				if ( pUnformattedValue )
				{
					// Do special formatting for various value types.
					GDIV_TYPE eType = pVar->GetType();
					if ( eType == ivChoices )
					{
						if ( pUnformattedValue )
						{
							const char *pTestValue = pVar->ItemStringForValue( pUnformattedValue );
							if ( pTestValue )
								pValue = pTestValue;
						}
					}
					else if ( 
						(eType == ivStudioModel) || (eType == ivSprite) || (eType == ivSound) || (eType == ivDecal) ||
						(eType == ivMaterial) || (eType == ivScene) )
					{
						// It's a filename.. just show the filename and not the directory. They can look at the 
						// full filename in the smart control if they want.
						const char *pLastSlash = max( strrchr( pUnformattedValue, '\\' ), strrchr( pUnformattedValue, '/' ) );
						if ( pLastSlash )
						{
							Q_strncpy( tmpValueBuf, pLastSlash+1, sizeof( tmpValueBuf ) );
							pValue = tmpValueBuf;
						}
					}					
				}
			}
			else
			{
				pValue = m_kv.GetValue( pVarName );	// This was probably added in dumbedit mode.
			}
		}
		else
		{
			pValue = m_kv.GetValue( pVarName );
		}
		
		if ( pValue )
			m_VarList.SetItemText( i, 1, pValue );
		else
			m_VarList.SetItemText( i, 1, "" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets up the current mode (smartedit/non) after loading an object
//			or toggling the SmartEdit button.
//-----------------------------------------------------------------------------
void COP_Entity::PresentProperties()
{
	if ( !m_bAllowPresentProperties )
		return;
	
	++m_nPresentPropertiesCalls;
	
	m_VarList.SetRedraw( FALSE );
	ClearVarList();	

	if (!m_bSmartedit || !m_pDisplayClass)
	{
		RemoveBlankKeys();

		for (int i = m_kv.GetFirst(); i != m_kv.GetInvalidIndex(); i=m_kv.GetNext( i ) )
		{
			MDkeyvalue &KeyValue = m_kv.GetKeyValue(i);
			
			int iItem = m_VarList.InsertItem( i, KeyValue.szKey );
			m_VarList.SetItemData( iItem, (DWORD)KeyValue.szKey );
		}
	
		m_Angle.Enable( m_bCanEdit );
	}
	else
	{
		// Too many entity variables! Increase GD_MAX_VARIABLES if you get this assertion.
		Assert(m_pDisplayClass->GetVariableCount() <= GD_MAX_VARIABLES);

		for ( int i=0; i < ARRAYSIZE(m_VarMap); i++ )
		{
			m_VarMap[i] = -1;
		}
		
		//
		// Add all the keys from the entity's class to the listbox. If the key is not already
		// in the entity, add it to the m_kvAdded list as well.
		//
		for (int i = 0; i < m_pDisplayClass->GetVariableCount(); i++)
		{
			GDinputvariable *pVar = m_pDisplayClass->GetVariableAt(i);

			//
			// Spawnflags are handled separately - don't add that key.
			//
			if (strcmpi(pVar->GetName(), SPAWNFLAGS_KEYNAME) != 0)
			{
				int iItem = m_VarList.InsertItem( i, pVar->GetLongName() );
				m_VarList.SetItemData( iItem, (DWORD)pVar->GetName() );
			}
		}

		if ( m_pObjectList->Count() == 1 )
		{
			CMapEntity *pEntity = static_cast< CMapEntity * >( m_pObjectList->Element( 0 ) );

			CMapInstance	*pMapInstance = pEntity->GetChildOfType( ( CMapInstance * )NULL );
			if ( pMapInstance && pMapInstance->GetInstancedMap() )
			{
				CMapEntityList entityList;

				pMapInstance->GetInstancedMap()->FindEntitiesByClassName( entityList, "func_instance_parms", false );
				if ( entityList.Count() == 1 )
				{
					CMapEntity *pInstanceParmsEntity = entityList.Element( 0 );

					for ( int i = pInstanceParmsEntity->GetFirstKeyValue(); i != pInstanceParmsEntity->GetInvalidKeyValue(); i = pInstanceParmsEntity->GetNextKeyValue( i ) )
					{
						LPCTSTR	pInstanceKey = pInstanceParmsEntity->GetKey( i );
						LPCTSTR	pInstanceValue = pInstanceParmsEntity->GetKeyValue( i );

						if ( strnicmp( pInstanceKey, "parm", strlen( "parm" ) ) == 0 )
						{
							char		ValueData[ KEYVALUE_MAX_KEY_LENGTH ];
							const char *pVariable, *pReplace;

							pVariable = pReplace = "";
							if ( pInstanceValue )
							{
								strcpy( ValueData, pInstanceValue );
								pVariable = ValueData;
								char *pos = strchr( ValueData, ' ' );
								if ( pos )
								{
									*pos = 0;
									pos++;
									pReplace = pos;
								}
							}
							else
							{
								continue;
							}

							for ( int j = pEntity->GetFirstKeyValue(); j != pEntity->GetInvalidKeyValue(); j = pEntity->GetNextKeyValue( j ) )
							{
								LPCTSTR	pKey = pEntity->GetKey( j );
								LPCTSTR	pValue = pEntity->GetKeyValue( j );

								if ( strnicmp( pValue, pVariable, strlen( pVariable ) ) == 0 )
								{
									CInstanceParmData	InstanceParmData;

									InstanceParmData.m_ParmVariable = new GDinputvariable( pReplace, pVariable );
									InstanceParmData.m_ParmKey = pKey;
									InstanceParmData.m_VariableName = pVariable;
									int InsertIndex = m_InstanceParmData.Insert( InstanceParmData.m_VariableName, InstanceParmData );

									const char *ptr = m_InstanceParmData[ InsertIndex ].m_VariableName;
									int iItem = m_VarList.InsertItem( 0, ptr );
									m_VarList.SetItemData( iItem, (DWORD)ptr );
									m_kv.SetValue( pVariable, pValue + strlen( pVariable ) + 1 );
								}
							}
						}
					}
				}
			}
		}

		// rj: this must be after the instancing check above, as adding keyvalues to m_kv can cause the list to get reallocated, causing the SetItemData to have an invalid pointer
		// Also add any keyvalues they added in dumbedit mode. These will show up in red.
		for (int i = m_kv.GetFirst(); i != m_kv.GetInvalidIndex(); i=m_kv.GetNext( i ) )
		{
			MDkeyvalue &KeyValue = m_kv.GetKeyValue(i);

			if ( !m_pDisplayClass->VarForName( KeyValue.szKey ) && m_InstanceParmData.Find( KeyValue.szKey ) == m_InstanceParmData.InvalidIndex() )
			{			
				int iItem = m_VarList.InsertItem( i, KeyValue.szKey );
				m_VarList.SetItemData( iItem, (DWORD)KeyValue.szKey );
			}
		}

		//
		// If this class defines angles, enable the angles control.
		//
		if (m_pDisplayClass->VarForName("angles") != NULL)
		{
			m_Angle.Enable( m_bCanEdit );
		}
		else
		{
			m_Angle.Enable(false);
		}
	}

	RefreshKVListValues();
	ResortItems();

	SetCurKey(m_strLastKey);
	m_VarList.SetRedraw( TRUE );
	m_VarList.Invalidate( FALSE );
}


void COP_Entity::ClearVarList()
{
	m_VarList.DeleteAllItems();
	m_InstanceParmData.RemoveAll();
	
	for ( int i=0; i < ARRAYSIZE( m_VarMap ); i++ )
		m_VarMap[i] = -1;
}


//-----------------------------------------------------------------------------
// Purpose: Removes any keys with blank values from our local keyvalue storage.
//-----------------------------------------------------------------------------
void COP_Entity::RemoveBlankKeys(void)
{
	//VPROF_BUDGET( "COP_Entity::RemoveBlankKeys", "Object Properties" );

	int iNext;
	for (int i = m_kv.GetFirst(); i != m_kv.GetInvalidIndex(); i=iNext)
	{
		iNext = m_kv.GetNext( i );
		
		MDkeyvalue &KeyValue = m_kv.GetKeyValue(i);
		if (KeyValue.szValue[0] == '\0')
		{
			bool bRemove = true;

#if 0
			// Only remove keys that are blank and whose default value is not blank,
			// because Hammer assigns any missing key with the FGD's default value.
			//
			// dvs: disabled for now because deleting the value text is the currently
			//      accepted way of reverting a key to its default value.
			GDinputvariable *pVar = m_pDisplayClass->VarForName( KeyValue.szKey );
			if ( pVar )
			{
				char szDefault[MAX_KEYVALUE_LEN];
				pVar->GetDefault( szDefault );
				if ( szDefault[0] != '\0' )
				{
					bRemove = false;
				}
			}
#endif

			if ( bRemove )
			{
				m_kv.RemoveKeyAt(i);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::LoadClassList(void)
{
	CEditGameClass *pEdit = (CEditGameClass*) GetEditObject();
	const char *pWorldSpawnString = "worldspawn";
	int iSolidClasses = -1;

	if (pEdit->IsClass())
	{
		if ( pEdit->IsClass( pWorldSpawnString ) )
		{
			iSolidClasses = 2;
		}
		else if (pEdit->IsSolidClass())
		{
			iSolidClasses = 1;
		}
		else
		{
			iSolidClasses = 0;
		}
	}

	// Ok, we've already initialized the list with the same list. Don't do it over again.
	if ( m_iLastClassListSolidClasses == iSolidClasses )
		return;

	CUtlVector<CString> suggestions;
	if ( iSolidClasses == 2 )
	{
		suggestions.AddToTail( pWorldSpawnString );
	}
	else
	{
		int nCount = pGD->GetClassCount();
		CString str;
		for (int i =0; i < nCount; i++)
		{
			GDclass *pc = pGD->GetClass(i);
			if (!pc->IsBaseClass())
			{
				if (iSolidClasses == -1 || (iSolidClasses == (int)pc->IsSolidClass()))
				{
#ifdef NAMES
					str.Format("%s (%s)", pc->GetDescription(), pc->GetName());
#else
					str = pc->GetName();
#endif

					if (!pc->IsClass( pWorldSpawnString ))
					{
						suggestions.AddToTail( str );
					}
				}
			}
		}
	}

	m_iLastClassListSolidClasses = iSolidClasses;
	m_cClasses.SetSuggestions( suggestions, 0 );
	
	// Add this class' class name in case it's not in the list yet.
	m_cClasses.AddSuggestion( pEdit->GetClassNameA() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL COP_Entity::OnInitDialog(void)
{
	//VPROF_BUDGET( "COP_Entity::OnInitDialog", "Object Properties" );

	CObjectPage::OnInitDialog();

	// Sometimes it has deleted our window (and its children) from underneath us,
	// and we don't want to hang onto old window pointers.
	m_SmartControls.Purge();
	m_pDisplayClass = NULL;
	m_pEditClass = NULL;
	m_pLastSmartControlVar = NULL;

	// Clear m_kv.
	m_kv.RemoveAll();
	ClearVarList();

	// Hook up the main angles controls.
	m_Angle.SubclassDlgItem(IDC_ANGLEBOX, this);
	m_AngleEdit.SubclassDlgItem(IDC_ANGLEEDIT, this);
	m_Angle.SetEditControl(&m_AngleEdit);
	m_AngleEdit.SetAngleBox(&m_Angle);

	// Hook up the smart angles controls.
	m_SmartAngle.SubclassDlgItem(IDC_SMART_ANGLEBOX, this);
	m_SmartAngleEdit.SubclassDlgItem(IDC_SMART_ANGLEEDIT, this);
	m_SmartAngle.SetEditControl(&m_SmartAngleEdit);
	m_SmartAngleEdit.SetAngleBox(&m_SmartAngle);

	// Hook up the classes autoselect combo.
	m_cClasses.SubclassDlgItem(IDC_CLASS, this);

	// Hook up the pick color button.
	m_cPickColor.SubclassDlgItem(IDC_PICKCOLOR, this);

	LoadClassList();
	
	//m_VarList.SetTabStops(65);
	
	// Put our varlist in the right mode.
	DWORD dwStyle = GetWindowLong( m_VarList.GetSafeHwnd(), GWL_STYLE );
	dwStyle &= ~(LVS_ICON | LVS_LIST | LVS_SMALLICON);
	dwStyle |= LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDRAWFIXED;
	SetWindowLong( m_VarList.GetSafeHwnd(), GWL_STYLE, dwStyle );
	
	m_VarList.SetExtendedStyle( m_VarList.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES );
	m_VarList.InsertColumn(0, "Property Name", LVCFMT_LEFT, 200);
	m_VarList.InsertColumn(1, "Value", LVCFMT_LEFT, 150);
		 
	m_bWantSmartedit = true;
	SetSmartedit(false);

	UpdateAnchors();
	return TRUE;
}


void COP_Entity::UpdateAnchors()
{
	CAnchorDef anchorDefs[] =
	{
		CAnchorDef( IDC_KEYVALUES, k_eSimpleAnchorAllSides ),
		CAnchorDef( IDC_SMARTEDIT, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_ENTITY_HELP, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_ANGLES_LABEL, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_ANGLEEDIT, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_ANGLEBOX, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_KEY_LABEL, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_KEY, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_VALUE_LABEL, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_PICKCOLOR, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_SMART_ANGLEEDIT, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_SMART_ANGLEBOX, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_VALUE, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_KEYVALUE_HELP_GROUP, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_KEYVALUE_HELP, k_eAnchorRight, k_eAnchorTop, k_eAnchorRight, k_eAnchorBottom ),
		CAnchorDef( IDC_COMMENTS_LABEL, k_eSimpleAnchorBottomRight ),
		CAnchorDef( IDC_ENTITY_COMMENTS, k_eAnchorRight, k_eAnchorBottom, k_eAnchorRight, k_eAnchorBottom ),
		CAnchorDef( IDC_ADDKEYVALUE, k_eSimpleAnchorRightSide ),
		CAnchorDef( IDC_REMOVEKEYVALUE, k_eSimpleAnchorRightSide )
	};
	CUtlVector<CAnchorDef> defs;
	defs.CopyArray( anchorDefs, ARRAYSIZE( anchorDefs ) );
	
	for ( int i=0; i < m_SmartControls.Count(); i++ )
	{
		defs.AddToTail( CAnchorDef( m_SmartControls[i]->GetSafeHwnd(), k_eSimpleAnchorRightSide ) );
	}
	
	m_AnchorMgr.Init( GetSafeHwnd(), defs.Base(), defs.Count() );
}


int COP_Entity::GetCurVarListSelection()
{
	POSITION pos = m_VarList.GetFirstSelectedItemPosition();
	if ( pos )
	{
		return m_VarList.GetNextSelectedItem( pos );
	}
	else
	{
		return LB_ERR;
	}
}


void COP_Entity::OnItemChangedKeyValues(NMHDR* pNMHDR, LRESULT* pResult)
{
	OnSelchangeKeyvalues();
}


void COP_Entity::OnDblClickKeyValues(NMHDR* pNMHDR, LRESULT* pResult)
{
	// Do smart stuff if we're in SmartEdit mode.
	if ( !m_bSmartedit )
		return;

	int iSel = GetCurVarListSelection();
	if ( iSel == LB_ERR )
		return;

	GDinputvariable *pVar = GetVariableAt( iSel );
	if ( pVar == NULL )
	{
		return;
	}

	if ( pVar->GetType() == ivColor255 || pVar->GetType() == ivColor1 )
	{
		OnPickColor();
	}
	else if ( m_pSmartControl )
	{
		if ( m_pSmartBrowseButton )
		{
			OnBrowse();
		}
		else if ( dynamic_cast< CMyEdit* >( m_pSmartControl ) )
		{
			m_pSmartControl->SetFocus();
			m_pSmartControl->SendMessage( EM_SETSEL, 0, -1 );
		}
		else if ( dynamic_cast< CMyComboBox* >( m_pSmartControl ) || dynamic_cast< CTargetNameComboBox* >( m_pSmartControl ) )
		{
			m_pSmartControl->SetFocus();
		}
	}
}


void COP_Entity::SetCurVarListSelection( int iSel )
{
	// Deselect everything.
	POSITION pos = m_VarList.GetFirstSelectedItemPosition();
	while ( pos )
	{
		int iItem = m_VarList.GetNextSelectedItem( pos );
		m_VarList.SetItemState( iItem, 0, LVIS_SELECTED );		
	}

	// Now set selection on the item we want.
	m_VarList.SetItemState( iSel, LVIS_SELECTED, LVIS_SELECTED );		
}


//-----------------------------------------------------------------------------
// Gets the name of the currently selected key in the properties list.
//-----------------------------------------------------------------------------
void COP_Entity::GetCurKey(CString &strKey)
{
	//VPROF_BUDGET( "COP_Entity::GetCurKey", "Object Properties" );

	int iSel = GetCurVarListSelection();
	if (iSel == -1)
	{
		strKey = "";
		return;
	}
	
	strKey = (CString)(const char*)m_VarList.GetItemData( iSel );
}


//-----------------------------------------------------------------------------
// Purpose: Selects the given key in the list of keys. If the key is not in the
//			list, the first key is selected.
// Input  : pszKey - Name of key to select.
//-----------------------------------------------------------------------------
void COP_Entity::SetCurKey(LPCTSTR pszKey)
{
	//VPROF_BUDGET( "COP_Entity::SetCurKey", "Object Properties" );

	int nSel = m_VarList.GetItemCount();

	for (int i = 0; i < nSel; i++)
	{
		CString str = (CString)(const char*)m_VarList.GetItemData( i );
		if ( !Q_stricmp( str, pszKey ) )
		{
			// found it here - 
			SetCurVarListSelection( i );
			
			// FIXME: Ideally we'd only call OnSelChangeKeyvalues if the selection index
			//        actually changed, but that makes the keyvalue text not refresh properly
			//		  when multiselecting entities with a sidelist key selected. 
			if ( m_bSmartedit )
			{
				OnSelchangeKeyvalues();
			}
			return;
		}
	}

	//
	// Not found, select the first key in the list.
	//
	SetCurVarListSelection( 0 );
	OnSelchangeKeyvalues();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void COP_Entity::DestroySmartControls(void)
{
	//VPROF_BUDGET( "COP_Entity::DestroySmartControls", "Object Properties" );

	for (int i = 0; i < m_SmartControls.Count(); i++)
	{
		CWnd *pControl = m_SmartControls.Element(i);
		if (pControl != NULL)
		{
			pControl->DestroyWindow();
			delete pControl;
		}
	}
	
	m_SmartControls.RemoveAll();
	m_pSmartBrowseButton = NULL;
	m_pSmartControl = NULL;
	m_pLastSmartControlVar = NULL;

	m_pEditInstanceVariable = NULL;
	m_pEditInstanceValue = NULL;
	m_pComboInstanceParmType = NULL;

	UpdateAnchors();
}


//-----------------------------------------------------------------------------
// Purpose: Creates the smart controls based on the given type. Deletes any
//			smart controls that are not appropriate to the given type.
// Input  : eType - Type of keyvalue for which to create controls.
//-----------------------------------------------------------------------------
void COP_Entity::CreateSmartControls(GDinputvariable *pVar, CUtlVector<const char *>*pHelperType)
{
	//VPROF_BUDGET( "COP_Entity::CreateSmartControls", "Object Properties" );

	// dvs: TODO: break this monster up into smaller functions
	if (pVar == NULL)
	{
		// UNDONE: delete smart controls?
		return;
	}

	CString strValue = m_kv.GetValue( pVar->GetName() );

	// If this is the same var that our smart controls are already setup for, then don't do anything.
	if ( m_SmartControls.Count() > 0 && 
		 pVar == m_pLastSmartControlVar && 
		 Q_stricmp( strValue, m_LastSmartControlVarValue ) == 0 )
	{
		return;
	}
	
	//
	// Set this so that we don't process notifications when we set the edit text,
	// which makes us do things like assume the user changed a key when they didn't.
	//
	m_bIgnoreKVChange = true;

	DestroySmartControls();
	m_pLastSmartControlVar = pVar;
	m_LastSmartControlVarValue = strValue;
	
	//
	// Build a rectangle in which to create a new control.
	//
	CRect ctrlrect = CalculateSmartControlRect();

	GDIV_TYPE eType = pVar->GetType();

	//
	// Hide or show color button.
	//
	m_cPickColor.ShowWindow((eType == ivColor255 || eType == ivColor1) ? SW_SHOW : SW_HIDE);

	HFONT hControlFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	if (hControlFont == NULL)
	{
		hControlFont = (HFONT)GetStockObject(ANSI_VAR_FONT);
	}

	//
	// Hide or show the Smart angles controls.
	//
	bool bShowSmartAngles = false;
	if (eType == ivAngle)
	{
		CreateSmartControls_Angle( pVar, ctrlrect, hControlFont, &bShowSmartAngles );
	}

	m_SmartAngle.ShowWindow(bShowSmartAngles ? SW_SHOW : SW_HIDE);
	m_SmartAngleEdit.ShowWindow(bShowSmartAngles ? SW_SHOW : SW_HIDE);

	//
	// Choices, NPC classes and filter classes get a combo box.
	//
	if ((eType == ivChoices) || (eType == ivNPCClass) || (eType == ivFilterClass) || (eType == ivPointEntityClass) )
	{
		CreateSmartControls_Choices( pVar, ctrlrect, hControlFont );
	}
	else if ( eType == ivInstanceVariable )
	{
		CreateSmartControls_InstanceVariable( pVar, ctrlrect, hControlFont );
	}
	else if ( eType == ivInstanceParm )
	{
		CreateSmartControls_InstanceParm( pVar, ctrlrect, hControlFont );
	}
	else
	{
		if ((eType == ivTargetSrc) || (eType == ivTargetDest))
		{
			CreateSmartControls_TargetName( pVar, ctrlrect, hControlFont );
		}
		else
		{
			CreateSmartControls_BasicEditControl( pVar, ctrlrect, hControlFont, pHelperType );
		}

		//
		// Create a "Browse..." button for browsing for files.
		//
		if ((eType == ivStudioModel) || (eType == ivSprite) || (eType == ivSound) || (eType == ivDecal) ||
			(eType == ivMaterial) || (eType == ivScene) || ( eType == ivInstanceFile ) )
		{
			CreateSmartControls_BrowseAndPlayButtons( pVar, ctrlrect, hControlFont );
		}
		else if ((eType == ivTargetDest) || (eType == ivTargetNameOrClass) || (eType == ivTargetSrc) || (eType == ivNodeDest))
		{
			CreateSmartControls_MarkAndEyedropperButtons( pVar, ctrlrect, hControlFont );
		}
		else if ((eType == ivSide) || (eType == ivSideList))
		{
			CreateSmartControls_PickButton( pVar, ctrlrect, hControlFont );
		}
	}

	m_pSmartControl->ShowWindow(SW_SHOW);
	m_pSmartControl->SetWindowPos(&m_VarList, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOREDRAW | SWP_NOSIZE);

	m_bIgnoreKVChange = false;
	UpdateAnchors();
}


CRect COP_Entity::CalculateSmartControlRect()
{
	CRect ctrlrect;

	GetDlgItem(IDC_KEY)->GetWindowRect(ctrlrect);
	ScreenToClient(ctrlrect);
	int nHeight = ctrlrect.Height();
	int nRight = ctrlrect.right - 10;

	// Put it just to the right of the keyvalue list.
	m_VarList.GetWindowRect(ctrlrect);
	ScreenToClient(ctrlrect);

	ctrlrect.left = ctrlrect.right + 10;
	ctrlrect.right = nRight;
	ctrlrect.bottom = ctrlrect.top + nHeight;

	return ctrlrect;
}


void COP_Entity::CreateSmartControls_Angle( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont, bool *bShowSmartAngles )
{
	if (stricmp(pVar->GetName(), "angles"))
	{
		*bShowSmartAngles = true;

		CRect rectAngleBox;
		m_SmartAngle.GetWindowRect(&rectAngleBox);

		CRect rectAngleEdit;
		m_SmartAngleEdit.GetWindowRect(&rectAngleEdit);

		m_SmartAngle.SetWindowPos(NULL, ctrlrect.left + rectAngleEdit.Width() + 4, ctrlrect.bottom + 10, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		m_SmartAngleEdit.SetWindowPos(NULL, ctrlrect.left, (ctrlrect.bottom + rectAngleBox.Height() + 10) - rectAngleEdit.Height(), 0, 0, SWP_NOSIZE | SWP_NOZORDER);

		// Update the smart control with the current value
		LPCTSTR pszValue = m_kv.GetValue(pVar->GetName());
		if (pszValue != NULL)
		{
			if (!stricmp(pszValue, VALUE_DIFFERENT_STRING))
			{
				m_SmartAngle.SetDifferent(true);
			}
			else
			{
				m_SmartAngle.SetDifferent(false);
				m_SmartAngle.SetAngles(pszValue);
			}
		}
	}

	//
	// Create an eyedropper button for picking target angles.
	//
	CRect ButtonRect;
	if (bShowSmartAngles)
	{
		CRect rectAngleBox;
		m_SmartAngle.GetWindowRect(&rectAngleBox);
		ScreenToClient(&rectAngleBox);

		CRect rectAngleEdit;
		m_SmartAngleEdit.GetWindowRect(&rectAngleEdit);
		ScreenToClient(&rectAngleEdit);

		ButtonRect.left = rectAngleBox.right + 8;
		ButtonRect.top = rectAngleEdit.top;
		ButtonRect.bottom = rectAngleEdit.bottom;
	}
	else
	{
		ButtonRect.left = ctrlrect.left;
		ButtonRect.top = ctrlrect.bottom + 4;
		ButtonRect.bottom = ButtonRect.top + ctrlrect.Height();
	}

	CButton *pButton = new CButton;
	pButton->CreateEx(0, "Button", "Point At...", WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
		ButtonRect.left, ButtonRect.top, 58, ButtonRect.Height() + 2, GetSafeHwnd(), (HMENU)IDC_PICK_ANGLES);
	pButton->SendMessage(WM_SETFONT, (WPARAM)hControlFont);

	CWinApp *pApp = AfxGetApp();
	HICON hIcon = pApp->LoadIcon(IDI_CROSSHAIR);
	pButton->SetIcon(hIcon);

	m_SmartControls.AddToTail(pButton);
}


void COP_Entity::CreateSmartControls_Choices( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont )
{
	CMyComboBox *pCombo = new CMyComboBox;
	pCombo->SetParentPage(this);
	ctrlrect.bottom += 150;
	pCombo->Create(CBS_DROPDOWN | CBS_HASSTRINGS | WS_TABSTOP | WS_CHILD | WS_BORDER | WS_VSCROLL | CBS_AUTOHSCROLL | ((pVar->GetType() != ivChoices) ? CBS_SORT : 0), ctrlrect, this, IDC_SMARTCONTROL);
  	pCombo->SendMessage(WM_SETFONT, (WPARAM)hControlFont);
	pCombo->SetDroppedWidth(150);

	//
	// If we are editing multiple entities, start with the "(different)" string.
	//
	if (IsMultiEdit())
	{
		pCombo->AddString(VALUE_DIFFERENT_STRING);
	}

	//
	// If this is a choices field, give combo box text choices from GameData variable
	//
	if (pVar->GetType() == ivChoices)
	{
		for (int i = 0; i < pVar->GetChoiceCount(); i++)
		{
			pCombo->AddString(pVar->GetChoiceCaption(i));
		}
	}
	//
	// For filterclass display a list of all the names of filters that are in the map
	//
	else if (pVar->GetType() == ivFilterClass)
	{
		CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
		CMapWorld *pWorld = pDoc->GetMapWorld();
		const CMapEntityList *pEntityList = pWorld->EntityList_GetList();

		
		FOR_EACH_OBJ( *pEntityList, pos )
		{
			CMapEntity *pEntity = pEntityList->Element(pos);
			GDclass *pClass = pEntity->GetClass();
			if (pClass && pClass->IsFilterClass())
			{
				const char *pString = pEntity->GetKeyValue("targetname");
				if (pString)
				{
					pCombo->AddString(pString);
				}
			}
		}
	}
	//
	// For npcclass fields, fill with all the NPC classes from the FGD.
	//
	else if (pVar->GetType() == ivNPCClass)
	{
		if (pGD != NULL)
		{
			int nCount = pGD->GetClassCount();
			for (int i = 0; i < nCount; i++)
			{
				GDclass *pClass = pGD->GetClass(i);
				if (pClass->IsNPCClass())
				{
					pCombo->AddString(pClass->GetName());
				}
			}
		}
	}
	//
	// For pointentity fields, fill with all the point entity classes from the FGD.
	//
	else
	{
		if (pGD != NULL)
		{
			int nCount = pGD->GetClassCount();
			for (int i = 0; i < nCount; i++)
			{
				GDclass *pClass = pGD->GetClass(i);
				if (pClass->IsPointClass())
				{
					pCombo->AddString(pClass->GetName());
				}
			}
		}
	}

	//
	// If the current value is the "different" string, display that in the combo box.
	//
	LPCTSTR pszValue = m_kv.GetValue(pVar->GetName());
	if (pszValue != NULL)
	{
		if (strcmp(pszValue, VALUE_DIFFERENT_STRING) == 0)
		{
			pCombo->SelectString(-1, VALUE_DIFFERENT_STRING);
		}
		else
		{
			const char *p = NULL;
			
			//
			// If this is a choices field and the current value corresponds to one of our
			// choices, display the friendly name of the choice in the edit control.
			//
			if (pVar->GetType() == ivChoices)
			{
				p = pVar->ItemStringForValue(pszValue);
			}

			if (p != NULL)
			{
				pCombo->SelectString(-1, p);
			}
			//
			// Otherwise, just display the value as-is.
			//
			else
			{
				pCombo->SetWindowText(pszValue);
			}
		}
	}

	m_pSmartControl = pCombo;
	m_SmartControls.AddToTail(pCombo);
}


void COP_Entity::CreateSmartControls_TargetName( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont )
{
	//
	// Create a combo box for picking target names.
	//
	CRect ComboRect = ctrlrect;
	ComboRect.bottom += 150;
	CTargetNameComboBox *pCombo = CTargetNameComboBox::Create( &m_SmartControlTargetNameRouter, CBS_DROPDOWN | WS_TABSTOP | WS_CHILD | WS_BORDER | CBS_AUTOHSCROLL | WS_VSCROLL | CBS_SORT, ComboRect, this, IDC_SMARTCONTROL_TARGETNAME);
  	pCombo->SendMessage(WM_SETFONT, (WPARAM)hControlFont);
	pCombo->SetDroppedWidth(150);

	m_pSmartControl = pCombo;
	m_SmartControls.AddToTail(m_pSmartControl);

	//
	// Attach the world's entity list to the add dialog.
	//
	// HACK: This is ugly. It should be passed in from outside.
	//
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	CMapWorld *pWorld = pDoc->GetMapWorld();
	pCombo->SetEntityList(pWorld->EntityList_GetList());
	pCombo->SelectItem( m_kv.GetValue(pVar->GetName()) );
	
	if (pVar->IsReadOnly())
	{
		pCombo->EnableWindow(FALSE);
	}
}


void COP_Entity::CreateSmartControls_BasicEditControl( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont, CUtlVector<const char *> *pHelperType )
{
	//
	// Create an edit control for inputting the keyvalue as text.
	//
	CMyEdit *pEdit = new CMyEdit;
	pEdit->SetParentPage(this);
	ctrlrect.bottom += 2;
	pEdit->CreateEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_TABSTOP | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 
		ctrlrect.left, ctrlrect.top, ctrlrect.Width(), ctrlrect.Height(), GetSafeHwnd(), HMENU(IDC_SMARTCONTROL));
  	pEdit->SendMessage(WM_SETFONT, (WPARAM)hControlFont);

	const char *pValue = m_kv.GetValue( pVar->GetName() );
	if ( pValue )
		pEdit->SetWindowText( pValue );

	if (pVar->IsReadOnly())
	{
		pEdit->EnableWindow(FALSE);
	}

	for ( int i = 0; i < pHelperType->Count(); i++ )
	{
		if ( !Q_strcmp( pHelperType->Element(i), "sphere" ) )
		{
			CRect ButtonRect = ctrlrect;
			ButtonRect.top = ctrlrect.bottom + 4;
			ButtonRect.bottom = ctrlrect.bottom + ctrlrect.Height() + 4;
			ButtonRect.right = ButtonRect.left + 32;	
			CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();		
			CMapView3D *pView = pDoc->GetFirst3DView();					
			int disabled = 0;
			if ( !pView )
			{
				disabled = WS_DISABLED;
			}  
			
			CButton *pButton = new CButton;
			pButton->CreateEx(0, "Button", "", WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_ICON | BS_PUSHLIKE | disabled,
				ButtonRect.left, ButtonRect.top, ButtonRect.Width(), ButtonRect.Height(),
				GetSafeHwnd(), (HMENU)IDC_CAMERA_DISTANCE);
			ButtonRect.left += ButtonRect.Width() + 4;
			CWinApp *pApp = AfxGetApp();
			HICON hIcon = pApp->LoadIcon(IDI_CAMERA);
			pButton->SetIcon(hIcon);

			m_SmartControls.AddToTail(pButton);
		}
	}

	m_pSmartControl = pEdit;
	m_SmartControls.AddToTail(pEdit);
}


void COP_Entity::CreateSmartControls_BrowseAndPlayButtons( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont )
{
	CRect ButtonRect = ctrlrect;

	ButtonRect.top = ctrlrect.bottom + 4;
	ButtonRect.bottom = ctrlrect.bottom + ctrlrect.Height() + 4;
	ButtonRect.right = ButtonRect.left + 54;

	HMENU message = (HMENU)IDC_BROWSE;
	if ( pVar->GetType() == ivInstanceFile )
	{
		message = (HMENU)IDC_BROWSE_INSTANCE;
	}

	CButton *pButton = new CButton;
	pButton->CreateEx(0, "Button", "Browse...", WS_TABSTOP | WS_CHILD | WS_VISIBLE, 
		ButtonRect.left, ButtonRect.top, ButtonRect.Width(), ButtonRect.Height(), 
		GetSafeHwnd(), message);
	pButton->SendMessage(WM_SETFONT, (WPARAM)hControlFont);
	m_pSmartBrowseButton = pButton;

	m_SmartControls.AddToTail(pButton);

	if ( pVar->GetType() == ivSound || pVar->GetType() == ivScene )
	{
		ButtonRect.left = ButtonRect.right + 8;
		ButtonRect.right = ButtonRect.left + 54;

		pButton = new CButton;
		pButton->CreateEx(0, "Button", "Play", WS_TABSTOP | WS_CHILD | WS_VISIBLE, 
			ButtonRect.left, ButtonRect.top, ButtonRect.Width(), ButtonRect.Height(), 
			GetSafeHwnd(), (HMENU)IDC_PLAY_SOUND);
		pButton->SendMessage(WM_SETFONT, (WPARAM)hControlFont);

		m_SmartControls.AddToTail(pButton);
	}
}


void COP_Entity::CreateSmartControls_MarkAndEyedropperButtons( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont )
{
	CRect ButtonRect = ctrlrect;
	ButtonRect.top = ctrlrect.bottom + 4;
	ButtonRect.bottom = ctrlrect.bottom + ctrlrect.Height() + 4;
	CButton *pButton = NULL;

	GDIV_TYPE eType = pVar->GetType();

	if ((eType == ivTargetDest) || (eType == ivTargetNameOrClass) || (eType == ivTargetSrc))
	{
		//
		// Create a "Mark" button for finding target entities.
		//
		ButtonRect.right = ButtonRect.left + 48;

		pButton = new CButton;
		pButton->CreateEx(0, "Button", "Mark", WS_TABSTOP | WS_CHILD | WS_VISIBLE, 
			ButtonRect.left, ButtonRect.top, ButtonRect.Width(), ButtonRect.Height(),
			GetSafeHwnd(), (HMENU)IDC_MARK);
		pButton->SendMessage(WM_SETFONT, (WPARAM)hControlFont);

		ButtonRect.left += ButtonRect.Width() + 4;

		m_SmartControls.AddToTail(pButton);

		//
		// Create a "Mark+Add" button for finding target entities.
		//
		ButtonRect.right = ButtonRect.left + 64;

		pButton = new CButton;
		pButton->CreateEx(0, "Button", "Mark+Add", WS_TABSTOP | WS_CHILD | WS_VISIBLE,
			ButtonRect.left, ButtonRect.top, ButtonRect.Width(), ButtonRect.Height(),
			GetSafeHwnd(), (HMENU)IDC_MARK_AND_ADD);
		pButton->SendMessage(WM_SETFONT, (WPARAM)hControlFont);

		ButtonRect.left += ButtonRect.Width() + 4;

		m_SmartControls.AddToTail(pButton);
	}

	//
	// Create an eyedropper button for picking entities.
	//
	ButtonRect.right = ButtonRect.left + 32;

	pButton = new CButton;
	pButton->CreateEx(0, "Button", "", WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_ICON | BS_AUTOCHECKBOX | BS_PUSHLIKE,
		ButtonRect.left, ButtonRect.top, ButtonRect.Width(), ButtonRect.Height(),
		GetSafeHwnd(), (HMENU)IDC_PICK_ENTITY);

	ButtonRect.left += ButtonRect.Width() + 4;

	CWinApp *pApp = AfxGetApp();
	HICON hIcon = pApp->LoadIcon(IDI_EYEDROPPER);
	pButton->SetIcon(hIcon);

	m_SmartControls.AddToTail(pButton);
}


void COP_Entity::CreateSmartControls_PickButton( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont )
{
	//
	// Create a "Pick" button for clicking on brush sides.
	//
	CRect ButtonRect = ctrlrect;

	ButtonRect.top = ctrlrect.bottom + 4;
	ButtonRect.bottom = ctrlrect.bottom + ctrlrect.Height() + 4;
	ButtonRect.right = ButtonRect.left + 54;

	CButton *pButton = new CButton;
	pButton->CreateEx(0, "Button", "Pick...", WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
		ButtonRect.left, ButtonRect.top, ButtonRect.Width(), ButtonRect.Height(), 
		GetSafeHwnd(), (HMENU)IDC_PICK_FACES);
	pButton->SendMessage(WM_SETFONT, (WPARAM)hControlFont);

	m_SmartControls.AddToTail(pButton);
}


//-----------------------------------------------------------------------------
// Purpose: this function will set up the smart controls for an instance variable
// Input  : pVar - the kv pair
//			ctrlrect - the rect space of this area
//			hControlFont - the standard font
//-----------------------------------------------------------------------------
void COP_Entity::CreateSmartControls_InstanceVariable( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont )
{
	const char *pValue = m_kv.GetValue( pVar->GetName() );
	char		ValueData[ KEYVALUE_MAX_KEY_LENGTH ];
	const char *pVariable, *pReplace;
	const int	VariableLimit = 50;

	pVariable = pReplace = "";
	if ( pValue )
	{
		strcpy( ValueData, pValue );
		pVariable = ValueData;
		char *pos = strchr( ValueData, ' ' );
		if ( pos )
		{
			*pos = 0;
			pos++;
			pReplace = pos;
		}
	}

	if ( m_pObjectList->Count() == 1 )
	{
		CMapEntity *pEntity = static_cast< CMapEntity * >( m_pObjectList->Element( 0 ) );

		CMapInstance	*pMapInstance = pEntity->GetChildOfType( ( CMapInstance * )NULL );
		if ( pMapInstance != NULL && pMapInstance->GetInstancedMap() != NULL )
		{
			CMapEntityList entityList;

			pMapInstance->GetInstancedMap()->FindEntitiesByClassName( entityList, "func_instance_parms", false );
			if ( entityList.Count() == 1 )
			{
				CMapEntity *pInstanceParmsEntity = entityList.Element( 0 );

				for ( int i = pInstanceParmsEntity->GetFirstKeyValue(); i != pInstanceParmsEntity->GetInvalidKeyValue(); i = pInstanceParmsEntity->GetNextKeyValue( i ) )
				{
					LPCTSTR	pKey = pInstanceParmsEntity->GetKey( i );
					LPCTSTR	psValue = pInstanceParmsEntity->GetKeyValue( i );

					if ( strnicmp( pKey, "parm", strlen( "parm" ) ) == 0 )
					{
						if ( strnicmp( psValue, pVariable, strlen( pVariable ) ) == 0 )
						{
							m_kv.SetValue( "temp_parm_value", pReplace );
							m_kv.SetValue( "temp_parm_name", pVar->GetName() );
							m_kv.SetValue( "temp_parm_field", pVariable );

							m_pInstanceVar = new GDinputvariable( "color255", "temp_parm_value" );
							CUtlVector<const char *>helperNames;

							m_pDisplayClass->GetHelperForGDVar( m_pInstanceVar, &helperNames );

							//
							// Update the keyvalue help text control with this variable's help info.
							//
							m_KeyValueHelpText.SetWindowText(m_pInstanceVar->GetDescription());

							CreateSmartControls(m_pInstanceVar, &helperNames);
							m_eEditType = m_pInstanceVar->GetType();
							return;
						}
					}
				}
			}
		}
	}

	CStatic		*pStaticInstanceVariable;
	pStaticInstanceVariable = new CStatic;
	pStaticInstanceVariable->CreateEx( WS_EX_LEFT, "STATIC", "Variable:", WS_CHILD | WS_VISIBLE | SS_LEFT, ctrlrect.left, ctrlrect.top, 50, 24, GetSafeHwnd(), HMENU( IDC_STATIC ) );
	pStaticInstanceVariable->SendMessage( WM_SETFONT, ( WPARAM )hControlFont );

	m_pEditInstanceVariable = new CEdit;
	ctrlrect.bottom += 2;
	m_pEditInstanceVariable->CreateEx( WS_EX_CLIENTEDGE, "EDIT", "", WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 
		ctrlrect.left + 50, ctrlrect.top, ctrlrect.Width() - 50, 24, GetSafeHwnd(), HMENU( IDC_SMARTCONTROL_INSTANCE_VARIABLE ) );
	m_pEditInstanceVariable->SendMessage( WM_SETFONT, ( WPARAM )hControlFont );
	m_pEditInstanceVariable->SetWindowText( pVariable );
	m_pEditInstanceVariable->SetLimitText( VariableLimit );
	
	if ( pVar->IsReadOnly() )
	{
		m_pEditInstanceVariable->EnableWindow( FALSE );
	}

	ctrlrect.top += 26;
	ctrlrect.bottom += 26;

	CStatic		*pStaticInstanceValue;
	pStaticInstanceValue = new CStatic;
	pStaticInstanceValue->CreateEx( WS_EX_LEFT, "STATIC", "Value:", WS_CHILD | WS_VISIBLE | SS_LEFT, ctrlrect.left, ctrlrect.top, 50, 24, GetSafeHwnd(), HMENU( IDC_STATIC ) );
	pStaticInstanceValue->SendMessage( WM_SETFONT, ( WPARAM )hControlFont );

	m_pEditInstanceValue = new CEdit;
	m_pEditInstanceValue->CreateEx( WS_EX_CLIENTEDGE, "EDIT", "", WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 
		ctrlrect.left + 50, ctrlrect.top, ctrlrect.Width() - 50, 24, GetSafeHwnd(), HMENU( IDC_SMARTCONTROL_INSTANCE_VALUE ) );
	m_pEditInstanceValue->SendMessage( WM_SETFONT, ( WPARAM )hControlFont );
	m_pEditInstanceValue->SetWindowText( pReplace );
	m_pEditInstanceVariable->SetLimitText( KEYVALUE_MAX_KEY_LENGTH - VariableLimit - 2 );	// to account for null and space in between

	if ( pVar->IsReadOnly() )
	{
		m_pEditInstanceValue->EnableWindow( FALSE );
	}

	m_pSmartControl = m_pEditInstanceVariable;
	m_SmartControls.AddToTail( m_pEditInstanceVariable );
	m_SmartControls.AddToTail( m_pEditInstanceValue );
	m_SmartControls.AddToTail( pStaticInstanceVariable );
	m_SmartControls.AddToTail( pStaticInstanceValue );
}


//-----------------------------------------------------------------------------
// Purpose: this function will set up the smart controls for an instance parameter
// Input  : pVar - the kv pair
//			ctrlrect - the rect space of this area
//			hControlFont - the standard font
//-----------------------------------------------------------------------------
void COP_Entity::CreateSmartControls_InstanceParm( GDinputvariable *pVar, CRect &ctrlrect, HFONT hControlFont )
{
	const char *pValue = m_kv.GetValue( pVar->GetName() );
	char		ValueData[ KEYVALUE_MAX_KEY_LENGTH ];
	const char *pVariable, *pType;
	const int	VariableLimit = 50;

	pVariable = pType = "";
	if ( pValue )
	{
		strcpy( ValueData, pValue );
		pVariable = ValueData;
		char *pos = strchr( ValueData, ' ' );
		if ( pos )
		{
			*pos = 0;
			pos++;
			pType = pos;
		}
	}

	CStatic		*pStaticInstanceVariable;
	pStaticInstanceVariable = new CStatic;
	pStaticInstanceVariable->CreateEx( WS_EX_LEFT, "STATIC", "Variable:", WS_CHILD | WS_VISIBLE | SS_LEFT, ctrlrect.left, ctrlrect.top, 50, 24, GetSafeHwnd(), HMENU( IDC_STATIC ) );
	pStaticInstanceVariable->SendMessage( WM_SETFONT, ( WPARAM )hControlFont );

	m_pEditInstanceVariable = new CEdit;
	ctrlrect.bottom += 2;
	m_pEditInstanceVariable->CreateEx( WS_EX_CLIENTEDGE, "EDIT", "", WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 
		ctrlrect.left + 50, ctrlrect.top, ctrlrect.Width() - 50, 24, GetSafeHwnd(), HMENU( IDC_SMARTCONTROL_INSTANCE_PARM ) );
	m_pEditInstanceVariable->SendMessage( WM_SETFONT, ( WPARAM )hControlFont );
	m_pEditInstanceVariable->SetWindowText( pVariable );
	m_pEditInstanceVariable->SetLimitText( VariableLimit );

	if ( pVar->IsReadOnly() )
	{
		m_pEditInstanceVariable->EnableWindow( FALSE );
	}

	ctrlrect.top += 26;
	ctrlrect.bottom += 26;

	CStatic		*pStaticInstanceValue;
	pStaticInstanceValue = new CStatic;
	pStaticInstanceValue->CreateEx( WS_EX_LEFT, "STATIC", "Value:", WS_CHILD | WS_VISIBLE | SS_LEFT, ctrlrect.left, ctrlrect.top, 50, 24, GetSafeHwnd(), HMENU( IDC_STATIC ) );
	pStaticInstanceValue->SendMessage( WM_SETFONT, ( WPARAM )hControlFont );

	m_pComboInstanceParmType = new CMyComboBox;
	m_pComboInstanceParmType->SetParentPage( this );
	ctrlrect.bottom += 150;
	ctrlrect.left += 50;
	m_pComboInstanceParmType->Create( CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | CBS_AUTOHSCROLL | CBS_SORT, ctrlrect, this, IDC_SMARTCONTROL_INSTANCE_PARM );
	m_pComboInstanceParmType->SendMessage( WM_SETFONT, ( WPARAM )hControlFont );
	m_pComboInstanceParmType->SetDroppedWidth( 150 );

	if ( pVar->IsReadOnly() )
	{
		m_pComboInstanceParmType->EnableWindow( FALSE );
	}

	for( int i = 0; i < ivMax; i++ )
	{
		m_pComboInstanceParmType->AddString( GDinputvariable::GetVarTypeName( ( GDIV_TYPE )i ) );
	}
	
	m_pComboInstanceParmType->SelectString( -1, pType );

	m_pSmartControl = m_pEditInstanceVariable;
	m_SmartControls.AddToTail( m_pEditInstanceVariable );
	m_SmartControls.AddToTail( m_pComboInstanceParmType );
	m_SmartControls.AddToTail( pStaticInstanceVariable );
	m_SmartControls.AddToTail( pStaticInstanceValue );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void COP_Entity::SetSmartControlText(const char *pszText)
{
	// dvs: HACK: the smart control should be completely self-contained, the dialog
	//		should only set the type of the edited variable, then just set & get text
	CTargetNameComboBox *pCombo = dynamic_cast <CTargetNameComboBox *>(m_pSmartControl);
	if (pCombo)
	{
		pCombo->SelectItem(pszText);
	}
	else
	{
		m_pSmartControl->SetWindowText(pszText);
	}

	// FIXME: this should be called automatically, but it isn't
	OnChangeSmartcontrol();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnSelchangeKeyvalues(void)
{
	//VPROF_BUDGET( "COP_Entity::OnSelchangeKeyvalues", "Object Properties" );

	//
	// Load new selection's key/values into the key/value 
	// edit controls.
	//
	if (m_VarList.m_hWnd == NULL)
	{
		return;
	}

	if (m_bSmartedit)
	{
		//
		// Stop face or entity picking if we are doing so.
		//
		StopPicking();
	}

	int iSel = GetCurVarListSelection();
	if (iSel == LB_ERR)
	{
		if (!m_bSmartedit)
		{
			// No selection; clear our the key and value controls.
			m_cKey.SetWindowText("");
			m_cValue.SetWindowText("");
		}

		return;
	}

	if (!m_bSmartedit)
	{
		CString str, val;
		str = m_VarList.GetItemText( iSel, 0 );
		val = m_VarList.GetItemText( iSel, 1 );

		//
		// Set the edit control text, but ignore the notifications caused by that.
		//
		m_bIgnoreKVChange = true;
		m_cKey.SetWindowText( str );
		m_cValue.SetWindowText( val );
		m_bChangingKeyName = false;
		m_bIgnoreKVChange = false;
	}
	else
	{
		GDinputvariable *pVar = GetVariableAt( iSel );
		if ( pVar == NULL || !m_pDisplayClass )
		{
			// This is a var added in dumbedit mode.
			DestroySmartControls();
			m_KeyValueHelpText.SetWindowText( "" );
		}
		else
		{
			CUtlVector<const char *>helperNames;

			m_pDisplayClass->GetHelperForGDVar( pVar, &helperNames );

			//
			// Update the keyvalue help text control with this variable's help info.
			//
			m_KeyValueHelpText.SetWindowText(pVar->GetDescription());
			
			CreateSmartControls(pVar, &helperNames);
			m_eEditType = pVar->GetType();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Used only in standard (non-SmartEdit) mode. Called when the contents
//			of the value edit control change. This is where the edit control
//			contents are converted to keyvalue data in standard mode.
//-----------------------------------------------------------------------------
void COP_Entity::OnChangeKeyorValue(void) 
{
	if (m_bIgnoreKVChange)
	{
		return;
	}

	int iSel = GetCurVarListSelection();
	if (iSel == LB_ERR)
	{
		return;
	}

	char szKey[KEYVALUE_MAX_KEY_LENGTH];
	char szValue[KEYVALUE_MAX_VALUE_LENGTH];

	m_cKey.GetWindowText(szKey, sizeof(szKey));
	m_cValue.GetWindowText(szValue, sizeof(szValue));

	UpdateKeyValue(szKey, szValue);

	//
	// Save it in our local kv storage.
	//
	m_kv.SetValue(szKey, szValue);

	// If they changed spawnflags, notify the flags page so its changes don't overwrite ours later.
	if ( V_stricmp( szKey, SPAWNFLAGS_KEYNAME ) == 0 )
	{
		unsigned long value;
		sscanf( szValue, "%lu", &value );
		m_pFlagsPage->OnUpdateSpawnFlags( value );
	}
	
	if (m_bEnableControlUpdate)
	{
		// Update any controls that are displaying the same data as the edit control.

		// This code should only be hit as a result of user input in the edit control!
		// If they changed the "angles" key, update the main angles control.
		if (!stricmp(szKey, "angles"))
		{
			m_Angle.SetDifferent(false);
			m_Angle.SetAngles(szValue, true);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnAddkeyvalue(void)
{
	//VPROF_BUDGET( "COP_Entity::OnAddkeyvalue", "Object Properties" );

	// create a new keyvalue at the end of the list
	CNewKeyValue newkv;
	newkv.m_Key.Format("newkey%d", m_nNewKeyCount++);
	newkv.m_Value = "value";
	if(newkv.DoModal() == IDCANCEL)
		return;

	// if the key we're adding is already in the list, do some
	//  stuff to make it unique
	if(m_kv.GetValue(newkv.m_Key))
	{
		CString strTemp;
		for(int i = 1; ; i++)
		{
			strTemp.Format("%s#%d", newkv.m_Key.GetBuffer(), i);
			if(!m_kv.GetValue(strTemp))
				break;
		}
		newkv.m_Key = strTemp;
	}

	m_kvAdded.SetValue( newkv.m_Key, "1" );
	m_kv.SetValue( newkv.m_Key, newkv.m_Value );
	
	PresentProperties();
	SetCurKey( newkv.m_Key );
}


//-----------------------------------------------------------------------------
// Purpose: Deletes the selected keyvalue.
//-----------------------------------------------------------------------------
void COP_Entity::OnRemovekeyvalue(void) 
{
	int iSel = GetCurVarListSelection();
	if (iSel == LB_ERR)
	{
		return;
	}

	CString strBuf;
	strBuf = (CString)(const char*)m_VarList.GetItemData(iSel);

	//
	// Remove the keyvalue from local storage.
	//
	m_kv.RemoveKey( strBuf );
	m_VarList.DeleteItem(iSel);
	if (iSel == m_VarList.GetItemCount())
	{
		SetCurVarListSelection( iSel - 1 );
	}

	ResortItems();
	OnSelchangeKeyvalues();
}

//-----------------------------------------------------------------------------
// Returns a mask indicating which flags have the same caption and bit value
// between the two classes.
//-----------------------------------------------------------------------------
static unsigned long GetMatchingFlagsMask( GDinputvariable *pVar1, GDinputvariable *pVar2 )
{
	unsigned long nMatchingMask = 0;

	for ( int i=0; i < pVar1->GetFlagCount(); i++ )
	{
		for ( int j=0; j < pVar2->GetFlagCount(); j++ )
		{
			if ( pVar1->GetFlagMask( i ) == pVar2->GetFlagMask( j ) )
			{
				if ( V_stricmp( pVar1->GetFlagCaption( i ), pVar2->GetFlagCaption( j ) ) == 0 )
				{
					unsigned long iMask = (unsigned long)pVar1->GetFlagMask( i );
					nMatchingMask |= iMask;
					break;
				}
			}
		}
	}
	
	return nMatchingMask;
}

//-----------------------------------------------------------------------------
// Assign default values to keys that are in the FGD but missing from the entity.
//-----------------------------------------------------------------------------
void COP_Entity::AssignClassDefaults(GDclass *pClass, GDclass *pOldClass)
{
	//VPROF_BUDGET( "COP_Entity::AssignClassDefaults", "Object Properties" );
	
	if (!pClass)
		return;

	for (int i = 0; i < pClass->GetVariableCount(); i++)
	{
		GDinputvariable *pVar = pClass->GetVariableAt(i);

		int iIndex;
		LPCTSTR p = m_kv.GetValue(pVar->GetName(), &iIndex);
		
		// Always reset spawnflags.
		if (!strcmpi(pVar->GetName(), SPAWNFLAGS_KEYNAME))
		{
			unsigned long nOriginalFlagsValue = 0;
			if (p)
			{
				sscanf( p, "%lu", &nOriginalFlagsValue );
			}
			
			unsigned long nCurrent = nOriginalFlagsValue;
			if ( pOldClass && (pOldClass != pClass) )
			{
				// First, just use the defaults for the new class.
				int defaultValue;
				pVar->GetDefault( &defaultValue );
				nCurrent = (unsigned long)defaultValue;

				// But.. if the old class and the new class have any flags with the same name and value,
				// then keep the current value for that flag.
				GDinputvariable *pOldVar = pOldClass->VarForName( SPAWNFLAGS_KEYNAME );
				if ( p && pOldVar )
				{
					unsigned long mask = GetMatchingFlagsMask( pOldVar, pVar );
					nCurrent &= ~mask;							// Get rid of the default value.
					nCurrent |= (nOriginalFlagsValue & mask);	// Add back in the current values.
				}

				// Notify the flags page so it'll have the right data if they tab to it.
				// It'll also SAVE the right data when they save.
				m_pFlagsPage->OnUpdateSpawnFlags( nCurrent );
			}
			else
			{
				unsigned long nMask = 0;
				int nCount = pVar->GetFlagCount();
				for (int j = 0; j < nCount; j++)
				{
					nMask |= (unsigned int)pVar->GetFlagMask(j);
				}
				
				// Mask off any bits that aren't defined in the FGD.			
				nCurrent &= nMask;
			}
			
			char szValue[128];
			Q_snprintf( szValue, sizeof( szValue ), "%lu", nCurrent );

			// Remember that we added or changed this key.
			if (!p || Q_stricmp(p, szValue) != 0)
			{
				m_kvAdded.SetValue(SPAWNFLAGS_KEYNAME, "1");
			}

			m_kv.SetValue(SPAWNFLAGS_KEYNAME, szValue);
		}
		else if (!p)
		{
			MDkeyvalue newkv;
			pVar->ResetDefaults();
			pVar->ToKeyValue(&newkv);
			m_kv.SetValue(newkv.szKey, newkv.szValue);
			
			// Remember that we added this key.
			m_kvAdded.SetValue(newkv.szKey, "1");
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Updates the dialog based on a change to the entity class name.
//-----------------------------------------------------------------------------
void COP_Entity::UpdateEditClass(const char *pszClass, bool bForce)
{
	//VPROF_BUDGET( "COP_Entity::UpdateClass", "Object Properties" );
	
	GDclass *pOldEditClass = m_pEditClass;
	m_pEditClass = pGD->ClassForName(pszClass);

	if (!bForce && (m_pEditClass == pOldEditClass))
		return;

	//DBG("UpdateEditClass - BEFORE PRUNE\n");
	//DumpKeyvalues(m_kv);
	
	//
	// Remove unused keyvalues.
	//
	if (m_pEditClass != pOldEditClass && m_pEditClass && pOldEditClass && strcmpi(pszClass, "multi_manager"))
	{
		int iNext;
		for ( int i=m_kv.GetFirst(); i != m_kv.GetInvalidIndex(); i=iNext )
		{
			iNext = m_kv.GetNext( i );
			
			MDkeyvalue &KeyValue = m_kv.GetKeyValue(i);
			if (m_pEditClass->VarForName(KeyValue.szKey) == NULL)
			{
				m_kv.RemoveKey(KeyValue.szKey);
			}
		}
	}

	//DBG("UpdateEditClass - AFTER PRUNE\n");
	//DumpKeyvalues(m_kv);

	AssignClassDefaults(m_pEditClass, pOldEditClass);
	PresentProperties();

	SetReadOnly( ( m_pDisplayClass != m_pEditClass ) || ( m_bCanEdit == false ) );
}


//-----------------------------------------------------------------------------
// Purpose: Called when a keyvalue is modified by the user.
// Input  : szKey - The key name.
//			szValue - The new value.
//-----------------------------------------------------------------------------
void COP_Entity::UpdateKeyValue(const char *szKey, const char *szValue)
{
	//VPROF_BUDGET( "COP_Entity::UpdateKeyValue", "Object Properties" );

	m_kvAdded.SetValue(szKey, "1");
	m_kv.SetValue(szKey, szValue);

	int index = m_InstanceParmData.Find( szKey );
	
	if ( index != m_InstanceParmData.InvalidIndex() )
	{
		CString NewValue = m_InstanceParmData[ index ].m_VariableName + " " + szValue;

		m_kvAdded.SetValue( m_InstanceParmData[ index ].m_ParmKey, "1" );
		m_kv.SetValue( m_InstanceParmData[ index ].m_ParmKey, NewValue );
		RefreshKVListValues( m_InstanceParmData[ index ].m_ParmKey );
	}
	
	RefreshKVListValues( szKey );
}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables SmartEdit mode, hiding showing the appropriate
//			dialog controls.
// Input  : b - TRUE to enable, FALSE to disable SmartEdit.
//-----------------------------------------------------------------------------
void COP_Entity::SetSmartedit(bool bSet)
{
	// Nothing to do?
	if ( m_bSmartedit == bSet )
		return;

	m_bSmartedit = bSet;

	//
	// If disabling smartedit, remove any smartedit-specific controls that may
	// or may not be currently visible.
	//
	if (!m_bSmartedit)
	{
		m_cPickColor.ShowWindow(SW_HIDE);
		m_SmartAngle.ShowWindow(SW_HIDE);
		m_SmartAngleEdit.ShowWindow(SW_HIDE);

		DestroySmartControls();
	}

	m_KeyValueHelpText.ShowWindow(m_bSmartedit ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_KEYVALUE_HELP_GROUP)->ShowWindow(m_bSmartedit ? SW_SHOW : SW_HIDE);

	//
	// Hide or show all controls after and including "delete kv" button.
	//
	for (int i = 0; i < ARRAYSIZE(g_DumbEditControls); i++)
	{
		CWnd *pWnd = GetDlgItem(g_DumbEditControls[i]);
		if ( pWnd )
			pWnd->ShowWindow(m_bSmartedit ? SW_HIDE : SW_SHOW);
	}

	((CButton*)GetDlgItem(IDC_SMARTEDIT))->SetCheck(m_bSmartedit);

	PresentProperties();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void COP_Entity::SetReadOnly(bool bReadOnly)
{
	m_VarList.EnableWindow(bReadOnly ? FALSE : TRUE);
	m_cPickColor.EnableWindow(bReadOnly ? FALSE : TRUE);
	m_SmartAngle.EnableWindow(bReadOnly ? FALSE : TRUE);
	m_SmartAngleEdit.EnableWindow(bReadOnly ? FALSE : TRUE);
	m_PasteControl.EnableWindow(bReadOnly ? FALSE : TRUE);

	m_KeyValueHelpText.EnableWindow(bReadOnly ? FALSE : TRUE);
	GetDlgItem(IDC_KEYVALUE_HELP_GROUP)->EnableWindow(bReadOnly ? FALSE : TRUE);

	//
	// Hide or show all controls after and including "delete kv" button.
	//
	for (int i = 0; i < ARRAYSIZE(g_DumbEditControls); i++)
	{
		CWnd *pWnd = GetDlgItem(g_DumbEditControls[i]);
		if ( pWnd )
			pWnd->EnableWindow( !bReadOnly );
	}
	
	for (int i = 0; i < m_SmartControls.Count(); i++)
	{
		if (m_SmartControls.Element(i) != NULL)
		{
			m_SmartControls.Element(i)->EnableWindow(bReadOnly ? FALSE : TRUE);
		}
	}

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnSmartedit(void) 
{
	m_iSortColumn = -1;	// Go back to FGD sorting.
	SetSmartedit(!m_bSmartedit);
	m_bWantSmartedit = m_bSmartedit;
}


//-----------------------------------------------------------------------------
// Updates the UI to show properties for the given FGD class. If the class
// differs from the actual class of the edited objects, the UI is set to be
// read only until the Apply button is pressed.
//-----------------------------------------------------------------------------
void COP_Entity::UpdateDisplayClass(const char *szClass)
{
	UpdateDisplayClass( pGD->ClassForName( szClass ) );
}

void COP_Entity::UpdateDisplayClass(GDclass *pClass)
{
	// The outer check here is lame, but somewhere along the line, all the controls get enabled
	// behind our backs. So verify that our state information is sane. If it's not, then we'll redo some state in here.
	bool bForceRefresh = false;
	if ( GetDlgItem(IDC_SMARTEDIT)->IsWindowEnabled() != (m_pDisplayClass != 0) )
		bForceRefresh = true;
		
	if ( pClass == m_pDisplayClass && !bForceRefresh )
	{
		return;
	}

	int lastNumPresentPropertiesCalls = m_nPresentPropertiesCalls;

	bool bNeedsSetupForMode = true;
	m_pDisplayClass = pClass;

	// In case we're not allowed to present the properties yet, make sure nobody uses m_VarMap.
	if ( !m_bAllowPresentProperties )
	{
		ClearVarList();
	}

	if (!m_pDisplayClass)
	{
		//
		// Object has no known class - get rid of smartedit.
		//
		if (m_bSmartedit || bForceRefresh)
		{
			m_bSmartedit = true; // In case bForceRefresh was on, force it to refresh the controls.
			SetSmartedit(false);
			bNeedsSetupForMode = false;
		}

		GetDlgItem(IDC_SMARTEDIT)->EnableWindow(FALSE);
	}    
	else
	{
		CEntityHelpDlg::SetEditGameClass(m_pDisplayClass);

		//
		// Known class - enable smartedit.
		//
		GetDlgItem(IDC_SMARTEDIT)->EnableWindow(TRUE);

		if (bForceRefresh)
			m_bSmartedit = !m_bWantSmartedit;
		
		SetSmartedit(m_bWantSmartedit);
	}

	// No need to call PresentProperties an extra time if it was called because of SetSmartedit..
	if ( bNeedsSetupForMode && (m_nPresentPropertiesCalls == lastNumPresentPropertiesCalls) )
	{
		PresentProperties();
	}

	SetReadOnly( ( m_pDisplayClass != m_pEditClass ) || ( m_bCanEdit == false ) );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool COP_Entity::BrowseModels( char *szModelName, int length, int &nSkin )
{
	bool bChanged = false;

	if (pModelBrowser == NULL)
	{
		pModelBrowser = new CModelBrowser( GetMainWnd() );
	}
	else
	{
		pModelBrowser->Show(); 
	}

	pModelBrowser->SetModelName( szModelName );
    pModelBrowser->SetSkin( nSkin );

	if (pModelBrowser->DoModal() == IDOK)
	{
		pModelBrowser->GetModelName( szModelName, length );
		pModelBrowser->GetSkin( nSkin );
		bChanged = true;
	}

	pModelBrowser->Hide();

	return bChanged;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void COP_Entity::BrowseTextures( const char *szFilter, bool bSprite )
{
	// browse for textures
	int iSel = GetCurVarListSelection();

	GDinputvariable * pVar = GetVariableAt( iSel );
	if ( pVar == NULL )
	{
		return;
	}

	// get current texture
	char szInitialTexture[128];
	m_pSmartControl->GetWindowText(szInitialTexture, 128);
	
	// create a texture browser and set it to browse decals
	CTextureBrowser *pBrowser = new CTextureBrowser(GetMainWnd());

	// setup filter - if any
	if( szFilter[0] != '\0' )
	{
		pBrowser->SetFilter( szFilter );
	}

	pBrowser->SetInitialTexture(szInitialTexture);
	
	if (pBrowser->DoModal() == IDOK)
	{
		IEditorTexture *pTex = g_Textures.FindActiveTexture(pBrowser->m_cTextureWindow.szCurTexture);

		char szName[MAX_PATH];
		if (pTex)
		{
			pTex->GetShortName(szName);
		}
		else
		{
			szName[0] = '\0';
		}

		if (bSprite && g_pGameConfig->GetTextureFormat() == tfVMT)
		{
			char sprExt[4];
			Q_snprintf(sprExt, 4, ".vmt");
			Q_snprintf(szName, MAX_PATH, "%s.vmt", szName);
			//Strcat is being zee stupido. I prolly have to strip the other string or something.
			//Q_strcat( szName, sprExt, MAX_PATH );
		}


		m_pSmartControl->SetWindowText(szName);

		// also set variable
		m_kv.SetValue(pVar->GetName(), szName);
		m_kvAdded.SetValue(pVar->GetName(), "1");
	}

	delete pBrowser;
}


void COP_Entity::OnChangeSmartcontrol(void)
{
	if ( m_pSmartControl )
	{
		char szValue[KEYVALUE_MAX_VALUE_LENGTH];
		m_pSmartControl->GetWindowText(szValue, sizeof(szValue));
		InternalOnChangeSmartcontrol( szValue );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Used only in SmartEdit mode. Called when the contents of the value
//			edit control change. This is where the edit control contents are
//			converted to keyvalue data in SmartEdit mode.
//-----------------------------------------------------------------------------
void COP_Entity::InternalOnChangeSmartcontrol( const char *szValue )
{
	//VPROF_BUDGET( "COP_Entity::OnChangeSmartcontrol", "Object Properties" );

	//
	// We only respond to this message when it is due to user input.
	// Don't do anything if we're creating the smart control.
	//
	if (m_bIgnoreKVChange)
	{
		return;
	}

	m_LastSmartControlVarValue = szValue;

	int iSel = GetCurVarListSelection();
	GDinputvariable * pVar = GetVariableAt( iSel );
	if ( pVar == NULL )
	{
		return;
	}

	char szKey[KEYVALUE_MAX_KEY_LENGTH];
	V_strncpy(szKey, pVar->GetName(), sizeof( szKey ));
	
	CString strValue = szValue;

	if (pVar->GetType() == ivChoices) 
	{
		//
		// If a choicelist, change buffer to the string value of what we chose.
		//
		const char *pszValueString = pVar->ItemValueForString(szValue);
		if (pszValueString != NULL)
		{
			strValue = pszValueString;
		}
	}

	UpdateKeyValue(szKey, strValue);

	if (m_bEnableControlUpdate)
	{
		// Update any controls that are displaying the same data as the edit control.
		// This code should only be hit as a result of user input in the edit control!
		if (pVar->GetType() == ivAngle)
		{
			// If they changed the "angles" key, update the main angles control.
			if (!stricmp(pVar->GetName(), "angles"))
			{
				m_Angle.SetDifferent(false);
				m_Angle.SetAngles(strValue, true);
			}
			// Otherwise update the Smart angles control.
			else
			{
				m_SmartAngle.SetDifferent(false);
				m_SmartAngle.SetAngles(strValue, true);
			}
		}
	}
	
	// We have to do this because it's an owner draw control and we're redoing the background colors.
	// Normally, Windows will only invalidate the second column.
	RECT rc;
	if ( m_VarList.GetItemRect( iSel, &rc, LVIR_BOUNDS ) )
		m_VarList.InvalidateRect( &rc, FALSE );
}


//-----------------------------------------------------------------------------
// Purpose: this function is called whenever the instance variable or value is changed
//-----------------------------------------------------------------------------
void COP_Entity::OnChangeInstanceVariableControl( void )
{
	if ( m_pEditInstanceVariable && m_pEditInstanceValue )
	{
		char szVariable[ KEYVALUE_MAX_VALUE_LENGTH ], szValue[ KEYVALUE_MAX_VALUE_LENGTH ];
		m_pEditInstanceVariable->GetWindowText( szVariable, sizeof( szVariable ) );
		m_pEditInstanceValue->GetWindowText( szValue, sizeof( szValue ) );

		if ( szValue[ 0 ] )
		{
			strcat( szVariable, " " );
			strcat( szVariable, szValue );
		}

		int iSel = GetCurVarListSelection();
		GDinputvariable * pVar = GetVariableAt( iSel );
		if ( pVar == NULL )
		{
			return;
		}

		char szKey[ KEYVALUE_MAX_KEY_LENGTH ];
		V_strncpy( szKey, pVar->GetName(), sizeof( szKey ) );

		UpdateKeyValue( szKey, szVariable );
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function is called whenever the instance parameter is changed
//-----------------------------------------------------------------------------
void COP_Entity::OnChangeInstanceParmControl( void )
{
	if ( m_pEditInstanceVariable && m_pComboInstanceParmType )
	{
		char szVariable[ KEYVALUE_MAX_VALUE_LENGTH ], szValue[ KEYVALUE_MAX_VALUE_LENGTH ];
		m_pEditInstanceVariable->GetWindowText( szVariable, sizeof( szVariable ) );

		int iSmartsel = m_pComboInstanceParmType->GetCurSel();
		if ( iSmartsel != LB_ERR )
		{
			// found a selection - now get the text
			m_pComboInstanceParmType->GetLBText( iSmartsel, szValue );
		}
		else
		{
			m_pComboInstanceParmType->GetWindowText( szValue, sizeof( szValue ) );
		}

		if ( szValue[ 0 ] )
		{
			strcat( szVariable, " " );
			strcat( szVariable, szValue );
		}

		int iSel = GetCurVarListSelection();
		GDinputvariable * pVar = GetVariableAt( iSel );
		if ( pVar == NULL )
		{
			return;
		}

		char szKey[ KEYVALUE_MAX_KEY_LENGTH ];
		V_strncpy( szKey, pVar->GetName(), sizeof( szKey ) );

		UpdateKeyValue( szKey, szVariable );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnChangeSmartcontrolSel(void)
{
	// update current value with this
	int iSel = GetCurVarListSelection();
	if ( !m_pDisplayClass )
		return;

	GDinputvariable * pVar = GetVariableAt( iSel );
	if ( pVar == NULL )
	{
		return;
	}

	if ((pVar->GetType() != ivTargetSrc) &&
		(pVar->GetType() != ivTargetDest) &&
		(pVar->GetType() != ivTargetNameOrClass) &&
		(pVar->GetType() != ivChoices) &&
		(pVar->GetType() != ivNPCClass) &&
		(pVar->GetType() != ivFilterClass) &&
		(pVar->GetType() != ivPointEntityClass))
	{
		return;
	}

	CComboBox *pCombo = (CComboBox *)m_pSmartControl;

	char szBuf[128];

	// get current selection
	int iSmartsel = pCombo->GetCurSel();
	if (iSmartsel != LB_ERR)
	{
		// found a selection - now get the text
		pCombo->GetLBText(iSmartsel, szBuf);
	}
	else
	{
		// just get the text from the combo box (no selection)
		pCombo->GetWindowText(szBuf, 128);
	}

	if (pVar->GetType() == ivChoices)
	{
		const char *pszValue = pVar->ItemValueForString(szBuf);
		if (pszValue != NULL)
		{
			strcpy(szBuf, pszValue);
		}
	}
	
	m_LastSmartControlVarValue = szBuf;

	m_kvAdded.SetValue(pVar->GetName(), "1");
	m_kv.SetValue(pVar->GetName(), szBuf);
	RefreshKVListValues( pVar->GetName() );

	// We have to do this because it's an owner draw control and we're redoing the background colors.
	// Normally, Windows will only invalidate the second column.
	RECT rc;
	if ( m_VarList.GetItemRect( iSel, &rc, LVIR_BOUNDS ) )
		m_VarList.InvalidateRect( &rc, FALSE );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cmd - 
//-----------------------------------------------------------------------------
void COP_Entity::SetNextVar(int cmd)
{
	int iSel = GetCurVarListSelection();
	int nCount = m_VarList.GetItemCount();
	if(iSel == LB_ERR)
		return;	// no!
	iSel += cmd;
	if(iSel == nCount)
		--iSel;
	if(iSel == -1)
		++iSel;
	SetCurVarListSelection( iSel );
	OnSelchangeKeyvalues();
}


//-----------------------------------------------------------------------------
// Purpose: Set flags page
//-----------------------------------------------------------------------------
void COP_Entity::SetFlagsPage( COP_Flags *pFlagsPage )
{
	m_pFlagsPage = pFlagsPage;
}


//-----------------------------------------------------------------------------
// Purpose: Plays the sound
//-----------------------------------------------------------------------------
void COP_Entity::OnPlaySound(void)
{
	if ( m_eEditType != ivSound && m_eEditType != ivScene )
		return;

	// Get the name of the sound or VCD.
	char szCurrentSound[256];
	m_pSmartControl->GetWindowText(szCurrentSound, 256);
	if (!szCurrentSound[0])
		return;
	
	// Get rid of "scenes/" for scenes.
	CString filename = szCurrentSound;
	if ( m_eEditType == ivScene )
		filename = StripDirPrefix( szCurrentSound, "scenes" );

	// Now play the sound..
	SoundType_t type;
	int nIndex;
	if ( g_Sounds.FindSoundByName( filename, &type, &nIndex ) )
		g_Sounds.Play( type, nIndex );
}


// Filesystem dialog module wrappers.
CSysModule *g_pFSDialogModule = 0;
CreateInterfaceFn g_FSDialogFactory = 0;

void LoadFileSystemDialogModule()
{
	Assert( !g_pFSDialogModule );

	// Load the module with the file system open dialog.
	const char *pDLLName = "FileSystemOpenDialog.dll";
	g_pFSDialogModule = Sys_LoadModule( pDLLName );
	if ( !g_pFSDialogModule || 
		 (g_FSDialogFactory = Sys_GetFactory( g_pFSDialogModule )) == NULL
		 )
	{
		if ( g_pFSDialogModule )
		{
			Sys_UnloadModule( g_pFSDialogModule );
			g_pFSDialogModule = 0;
		}

		char str[512];
		Q_snprintf( str, sizeof( str ), "Can't load %s.\n", pDLLName );
		AfxMessageBox( str, MB_OK );
	}
}

void UnloadFileSystemDialogModule()
{
	if ( g_pFSDialogModule )
	{
		Sys_UnloadModule( g_pFSDialogModule );
		g_pFSDialogModule = 0;
	}
}	


//-----------------------------------------------------------------------------
// Purpose: Brings up the file browser in the appropriate default directory
//			based on the type of file being browsed for.
//-----------------------------------------------------------------------------
void COP_Entity::OnBrowse(void)
{
	// handle browsing for .fgd "material" type
	if( m_eEditType == ivMaterial )
	{
		BrowseTextures( "\0" );
		return;
	}

	if ( m_eEditType == ivStudioModel && Options.IsVGUIModelBrowserEnabled() )
	{
		char szCurrentModel[512];
		char szCurrentSkin[512];
		
		const char *result = m_kv.GetValue( "skin" );
		int nSkin = ( result ) ? Q_atoi( result ) : 0;

		m_pSmartControl->GetWindowText( szCurrentModel, sizeof(szCurrentModel) );
		
		if ( BrowseModels( szCurrentModel, sizeof(szCurrentModel), nSkin ) )
		{
			// model was changed
			m_pSmartControl->SetWindowText( szCurrentModel );
			UpdateKeyValue("skin", itoa( nSkin, szCurrentSkin, 10 ));			
		}
		return;
	}

	// handle browsing for .fgd "decal" type
	if(m_eEditType == ivDecal)
	{
		if (g_pGameConfig->GetTextureFormat() == tfVMT)
		{
			BrowseTextures("decals/");
		}
		else
		{
			BrowseTextures("{");
		}		
		return;		
	}

	if ( m_eEditType == ivSprite )
	{
		BrowseTextures( "sprites/", true);
		return;
	}

	if ( m_eEditType == ivInstanceFile )
	{
		OnBrowseInstance();
		return;
	}

	char *pszInitialDir = 0;

	
	// Instantiate a dialog.
	if ( !g_FSDialogFactory )
		return;

	IFileSystemOpenDialog *pDlg;
	pDlg = (IFileSystemOpenDialog*)g_FSDialogFactory( FILESYSTEMOPENDIALOG_VERSION, NULL );
	if ( !pDlg )
	{
		char str[512];
		Q_snprintf( str, sizeof( str ), "Can't create %s interface.", FILESYSTEMOPENDIALOG_VERSION );
		AfxMessageBox( str, MB_OK );
		return;
	}
	pDlg->Init( g_Factory, NULL );
	
	
	const char *pPathID = "GAME";

	//
	// Based on the type of file that we are picking, set up the default extension,
	// default directory, and filters. Each type of file remembers its last directory.
	//
	switch (m_eEditType)
	{
		case ivStudioModel:
		{
			static char szInitialDir[MAX_PATH] = "models";
			pszInitialDir = szInitialDir;

			pDlg->AddFileMask( "*.jpg" );
			pDlg->AddFileMask( "*.mdl" );
			pDlg->SetInitialDir( pszInitialDir, pPathID );
			pDlg->SetFilterMdlAndJpgFiles( true );
			break;
		}

		case ivSound:
		{
			CString currentValue;
			m_pSmartControl->GetWindowText( currentValue );

			CSoundBrowser soundDlg( currentValue );
			if ( soundDlg.m_SoundType != SOUND_TYPE_RAW && soundDlg.m_SoundType != SOUND_TYPE_GAMESOUND )
				soundDlg.m_SoundType = SOUND_TYPE_GAMESOUND;

			int nRet = soundDlg.DoModal();
			if ( nRet == IDOK )
			{
				m_pSmartControl->SetWindowText(soundDlg.GetSelectedSound());
			}
			goto Cleanup;
		}

		case ivScene:
		{
			CString currentValue;
			m_pSmartControl->GetWindowText( currentValue );
			CString stripped = StripDirPrefix( currentValue, "scenes" );
			
			CSoundBrowser soundDlg( stripped );
			soundDlg.m_SoundType = SOUND_TYPE_SCENE;
			int nRet = soundDlg.DoModal();
			if ( nRet == IDOK )
			{
				m_pSmartControl->SetWindowText(CString("scenes\\") + soundDlg.GetSelectedSound());
			}
			goto Cleanup;
		}

		default:
		{
			pDlg->AddFileMask( "*.*" );
			pDlg->SetInitialDir( ".", pPathID );
			break;
		}
	}

	//
	// If they picked a file and hit OK, put everything after the last backslash
	// into the SmartEdit control. If there is no backslash, put the whole filename.
	//
	int ret;
	if ( 1/*g_pFullFileSystem->IsSteam()*/ || CommandLine()->FindParm( "-NewDialogs" ) )
		ret = pDlg->DoModal();
	else
		ret = pDlg->DoModal_WindowsDialog();

	if ( ret == IDOK )
	{
		//
		// Save the default folder for next time.
		//
		pDlg->GetFilename( pszInitialDir, MAX_PATH );
		char *pchSlash = strrchr(pszInitialDir, '\\');
		if (pchSlash != NULL)
		{
			*pchSlash = '\0';
		}

		if (m_pSmartControl != NULL)
		{
			//
			// Reverse the slashes, because the engine expects them that way.
			//
			char szTemp[MAX_PATH];
			pDlg->GetFilename( szTemp, sizeof( szTemp ) );
			for (unsigned int i = 0; i < strlen(szTemp); i++)
			{
				if (szTemp[i] == '\\')
				{
					szTemp[i] = '/';
				}
			}

			m_pSmartControl->SetWindowText(szTemp);
		}
	}

Cleanup:;
	pDlg->Release();
}


//-----------------------------------------------------------------------------
// Purpose: this function will display a file dialog to locate an instance vmf.
//-----------------------------------------------------------------------------
void COP_Entity::OnBrowseInstance(void)
{
	const char	*pszMapPath = "\\maps\\";
	CString		MapFileName;
	char		FileName[ MAX_PATH ];
	CString		currentValue;
	CMapDoc		*activeDoc = CMapDoc::GetActiveMapDoc();

	m_pSmartControl->GetWindowText( currentValue );

	MapFileName = activeDoc->GetPathName();

	CMapInstance::DeterminePath( MapFileName, currentValue, FileName );

	CFileDialog dlg( 
		true,								// open dialog?
		".vmf",								// default file extension
		FileName,							// initial filename
		OFN_ENABLESIZING,					// flags
		"Valve Map Files (*.vmf)|*.vmf||",
		this );

	if ( dlg.DoModal() == IDOK )
	{
		strcpy( FileName, dlg.GetPathName() );
		V_RemoveDotSlashes( FileName );
		V_FixDoubleSlashes( FileName );
		V_strlower( FileName );

		char *pos = strstr( FileName, pszMapPath );
		if ( pos )
		{
			pos += strlen( pszMapPath );
			*( pos - 1 ) = 0;
		}
		else if ( pos == NULL )
		{
			const char *pszInstancePath = CMapInstance::GetInstancePath();

			if ( pszInstancePath[ 0 ] != 0 )
			{
				pos = strstr( FileName, pszInstancePath );
				if ( pos )
				{
					pos += strlen( pszInstancePath );
					*( pos - 1 ) = 0;
				}
			}
		}

		if ( pos )
		{
			m_pSmartControl->SetWindowText( pos );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnCopy(void)
{
	// copy entity keyvalues
	kvClipboard.RemoveAll();
	bKvClipEmpty = FALSE;
	for ( int i=m_kv.GetFirst(); i != m_kv.GetInvalidIndex(); i=m_kv.GetNext( i ) )
	{
		if (stricmp(m_kv.GetKey(i), "origin"))
		{
			kvClipboard.SetValue(m_kv.GetKey(i), m_kv.GetValue(i));
		}
	}

	CString strClass = m_cClasses.GetCurrentItem();
	kvClipboard.SetValue("xxxClassxxx", strClass);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnEntityHelp(void)
{
	CEntityHelpDlg::ShowEntityHelpDialog();
	CEntityHelpDlg::SetEditGameClass(m_pDisplayClass);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnKillfocusKey(void)
{
	if (!m_bChangingKeyName)
		return;

	m_bChangingKeyName = false;
	CString strOutput;

	m_cKey.GetWindowText(strOutput);

	if (strOutput.IsEmpty())
	{
		AfxMessageBox("Use the delete button to remove key/value pairs.");
		return;
	}

	strOutput.MakeLower();

	// No change
	if (strOutput == m_szOldKeyName)
		return;

	char szSaveValue[KEYVALUE_MAX_VALUE_LENGTH];
	memset(szSaveValue, 0, sizeof(szSaveValue));
	V_strcpy_safe(szSaveValue, m_kv.GetValue(m_szOldKeyName, NULL));

	int iSel = GetCurVarListSelection();
	if (iSel == LB_ERR)
		return;
	
	// Get rid of the old key.
	CString strBuf;
	strBuf = (CString)(const char*)m_VarList.GetItemData(iSel);
	m_kv.RemoveKey( strBuf );	// remove from local kv storage
	m_VarList.DeleteItem(iSel);

	// Add a new key with the new keyname + old value.
	CNewKeyValue newkv;
	newkv.m_Key   = strOutput;
	newkv.m_Value = szSaveValue;

	m_kvAdded.SetValue(newkv.m_Key, "1");
	m_kv.SetValue(newkv.m_Key, newkv.m_Value);

	PresentProperties();

	// Select this property.
	SetCurVarListSelection( GetKeyValueRowByShortName( newkv.m_Key ) );
	OnSelchangeKeyvalues();
}


//-----------------------------------------------------------------------------
// Does the dirty marking deed
//-----------------------------------------------------------------------------
void COP_Entity::PerformMark( const char *szTargetName, bool bClear, bool bNameOrClass )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	if (pDoc != NULL)
	{
		if (szTargetName[0] != '\0')
		{
			CMapEntityList Found;
			
			pDoc->FindEntitiesByName(Found, szTargetName, false);
			if ((Found.Count() == 0) && bNameOrClass)
			{
				pDoc->FindEntitiesByClassName(Found, szTargetName, false);
			}

			if (Found.Count() != 0)
			{
				CMapObjectList Select;
				FOR_EACH_OBJ( Found, pos )
				{
					CMapEntity *pEntity = Found.Element(pos);
					Select.AddToTail(pEntity);
				}

				if ( bClear )
				{
					// clear & safe previous selection
					pDoc->SelectObjectList(&Select);
				}
				else
				{
					// don't save changes and add object to selection
					pDoc->SelectObjectList(&Select, scSelect );
				}
				
				pDoc->Center2DViewsOnSelection();
			}
			else
			{
				MessageBox("No entities were found with that targetname.", "No entities found", MB_ICONINFORMATION | MB_OK);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Marks all entities whose targetnames match the currently selected
//			key value.
//-----------------------------------------------------------------------------
void COP_Entity::OnMark(void)
{
	int iSel = GetCurVarListSelection();
	GDinputvariable * pVar = GetVariableAt( iSel );
	if ( pVar == NULL )
	{
		return;
	}

	char szTargetName[MAX_IO_NAME_LEN];
	m_pSmartControl->GetWindowText(szTargetName, sizeof(szTargetName));

	bool bNameOrClass = false;
	if (pVar && (pVar->GetType() == ivTargetNameOrClass))
	{
		bNameOrClass = true;
	}

	PerformMark( szTargetName, true, bNameOrClass );
}


//-----------------------------------------------------------------------------
// Add the mark to the selection
//-----------------------------------------------------------------------------
void COP_Entity::OnMarkAndAdd(void)
{
	int iSel = GetCurVarListSelection();
	GDinputvariable * pVar = GetVariableAt( iSel );
	if ( pVar == NULL )
	{
		return;
	}
	
	// Build a temporary list of all the currently selected objects because this
	// process will change the selected objects.
	CMapEntity **pTemp = (CMapEntity**)stackalloc( m_pObjectList->Count() * sizeof(CMapEntity*) );
	CUtlVector<CMapEntity*> temp( pTemp, m_pObjectList->Count() );
	
	FOR_EACH_OBJ( *m_pObjectList, pos )
	{
		CMapEntity *pEntity = static_cast<CMapEntity *>(m_pObjectList->Element(pos));
		temp.AddToTail( pEntity );
	}

	// Mark all the entities referred to by the current keyvalue in the selected entities.
	bool bNameOrClass = false;
	if (pVar && (pVar->GetType() == ivTargetNameOrClass))
	{
		bNameOrClass = true;
	}

	for ( int i = 0; i < temp.Count(); ++i )
	{
		const char *pTargetName = temp[i]->GetKeyValue( pVar->GetName() );
		if ( pTargetName )
		{
			PerformMark( pTargetName, false, bNameOrClass );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnPaste(void)
{
	if(bKvClipEmpty)
		return;

	CString str;
	GetCurKey(str);

	// copy entity keyvalues
	for (int i = kvClipboard.GetFirst(); i != kvClipboard.GetInvalidIndex(); i=kvClipboard.GetNext( i ) )
	{
		if (!strcmp(kvClipboard.GetKey(i), "xxxClassxxx"))
		{
			m_cClasses.SelectItem( kvClipboard.GetValue(i) );
			UpdateEditClass(kvClipboard.GetValue(i), false);
			UpdateDisplayClass(kvClipboard.GetValue(i));
			continue;
		}

		m_kv.SetValue(kvClipboard.GetKey(i), kvClipboard.GetValue(i));
		m_kvAdded.SetValue(kvClipboard.GetKey(i), "1");
	}

	PresentProperties();
	SetCurKey(str);
}


//-----------------------------------------------------------------------------
// Purpose: For the given key name, builds a list of faces that are common
//			to all entitie being edited and a list of faces that are found in at
//			least one entity being edited.
// Input  : FullFaces - 
//			PartialFaces - 
//			pszKey - the name of the key.
//-----------------------------------------------------------------------------
void COP_Entity::GetFaceIDListsForKey(CMapFaceIDList &FullFaces, CMapFaceIDList &PartialFaces, const char *pszKey)
{
	CMapWorld *pWorld = GetActiveWorld();

	if ((m_pObjectList != NULL) && (pWorld != NULL))
	{
		bool bFirst = true;
		
		FOR_EACH_OBJ( *m_pObjectList, pos )
		{
			CMapClass *pObject = m_pObjectList->Element(pos);
			CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);
			if (pEntity != NULL)
			{
				const char *pszValue = pEntity->GetKeyValue(pszKey);

				if (bFirst)
				{
					pWorld->FaceID_StringToFaceIDLists(&FullFaces, NULL, pszValue);
					bFirst = false;
				}
				else
				{
					CMapFaceIDList TempFaces;
					pWorld->FaceID_StringToFaceIDLists(&TempFaces, NULL, pszValue);

					CMapFaceIDList TempFullFaces = FullFaces;
					FullFaces.RemoveAll();

					TempFaces.Intersect(TempFullFaces, FullFaces, PartialFaces);
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: For the given key name, builds a list of faces that are common
//			to all entitie being edited and a list of faces that are found in at
//			least one entity being edited.
// Input  : FullFaces - 
//			PartialFaces - 
//			pszKey - the name of the key.
//-----------------------------------------------------------------------------
// dvs: FIXME: try to eliminate this function
void COP_Entity::GetFaceListsForKey(CMapFaceList &FullFaces, CMapFaceList &PartialFaces, const char *pszKey)
{
	CMapWorld *pWorld = GetActiveWorld();

	if ((m_pObjectList != NULL) && (pWorld != NULL))
	{
		bool bFirst = true;
		
		FOR_EACH_OBJ( *m_pObjectList, pos )
		{
			CMapClass *pObject = m_pObjectList->Element(pos);
			CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);
			if (pEntity != NULL)
			{
				const char *pszValue = pEntity->GetKeyValue(pszKey);

				if (bFirst)
				{
					pWorld->FaceID_StringToFaceLists(&FullFaces, NULL, pszValue);
					bFirst = false;
				}
				else
				{
					CMapFaceList TempFaces;
					pWorld->FaceID_StringToFaceLists(&TempFaces, NULL, pszValue);

					CMapFaceList TempFullFaces = FullFaces;
					FullFaces.RemoveAll();

					TempFaces.Intersect(TempFullFaces, FullFaces, PartialFaces);
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnPickFaces(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	Assert(pDoc != NULL);

	if (pDoc == NULL)
	{
		return;
	}

	CButton *pButton = (CButton *)GetDlgItem(IDC_PICK_FACES);
	Assert(pButton != NULL);

	if (pButton != NULL)
	{
		if (pButton->GetCheck())
		{
			int nSel = GetCurVarListSelection();
			Assert(nSel != LB_ERR);

			if (nSel != LB_ERR )
			{
				GDinputvariable * pVar = GetVariableAt( nSel );
				if ( pVar != NULL )
				{
					Assert((pVar->GetType() == ivSideList) || (pVar->GetType() == ivSide));

					// FACEID TODO: make the faces highlight even when the tool is not active

					//
					// Build the list of faces that are in all selected entities, and a list
					// of faces that are in at least one selected entity, so that we can do
					// multiselect properly.
					//
					CMapFaceList FullFaces;
					CMapFaceList PartialFaces;
					GetFaceListsForKey(FullFaces, PartialFaces, pVar->GetName());

					// Save the old tool so we can reset to the correct tool when we stop picking.
					m_ToolPrePick = ToolManager()->GetActiveToolID();
					m_bPicking = true;

					//
					// Activate the face picker tool. It will handle the picking of faces.
					//
					ToolManager()->SetTool(TOOL_PICK_FACE);

					CToolPickFace *pTool = (CToolPickFace *)ToolManager()->GetToolForID(TOOL_PICK_FACE);
					pTool->SetSelectedFaces(FullFaces, PartialFaces);
					m_PickFaceTarget.AttachEntityDlg(this);
					pTool->Attach(&m_PickFaceTarget);
					pTool->AllowMultiSelect(pVar->GetType() == ivSideList);
				}
			}
		}
		else
		{
			//
			// Get the face IDs from the face picker tool.
			//
			m_bPicking = false;
			CToolPickFace *pTool = (CToolPickFace *)ToolManager()->GetToolForID(TOOL_PICK_FACE);
			UpdatePickFaceText(pTool);
			ToolManager()->SetTool(TOOL_POINTER);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnPickAngles(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	Assert(pDoc != NULL);
	if (pDoc == NULL)
	{
		return;
	}

	CButton *pButton = (CButton *)GetDlgItem(IDC_PICK_ANGLES);
	Assert(pButton != NULL);

	if (pButton != NULL)
	{
		if (pButton->GetCheck())
		{
			int nSel = GetCurVarListSelection();
			Assert(nSel != LB_ERR);

			if (nSel != LB_ERR)
			{
				GDinputvariable * pVar = GetVariableAt( nSel );
				if ( pVar != NULL )
				{
					Assert(pVar->GetType() == ivAngle);

					// Save the old tool so we can reset to the correct tool when we stop picking.
					m_ToolPrePick = ToolManager()->GetActiveToolID();
					m_bPicking = true;

					//
					// Activate the angles picker tool.
					//
					CToolPickAngles *pTool = (CToolPickAngles *)ToolManager()->GetToolForID(TOOL_PICK_ANGLES);
					m_PickAnglesTarget.AttachEntityDlg(this);
					pTool->Attach(&m_PickAnglesTarget);

					ToolManager()->SetTool(TOOL_PICK_ANGLES);
				}
			}
		}
		else
		{
			StopPicking();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnPickEntity(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	Assert(pDoc != NULL);
	if (pDoc == NULL)
	{
		return;
	}

	CButton *pButton = (CButton *)GetDlgItem(IDC_PICK_ENTITY);
	Assert(pButton != NULL);

	if (pButton != NULL)
	{
		if (pButton->GetCheck())
		{
			int nSel = GetCurVarListSelection();
			Assert(nSel != LB_ERR);

			if (nSel != LB_ERR)
			{
				GDinputvariable * pVar = GetVariableAt( nSel );
				if ( pVar != NULL )
				{
					// Save the old tool so we can reset to the correct tool when we stop picking.
					m_ToolPrePick = ToolManager()->GetActiveToolID();
					m_bPicking = true;

					//
					// Activate the entity picker tool.
					//
					CToolPickEntity *pTool = (CToolPickEntity *)ToolManager()->GetToolForID(TOOL_PICK_ENTITY);
					m_PickEntityTarget.AttachEntityDlg(this);

					switch (pVar->GetType())
					{
						case ivTargetDest:
						case ivTargetNameOrClass:
						case ivTargetSrc:
						{
							m_PickEntityTarget.SetKeyToRetrieve("targetname");
							break;
						}

						case ivNodeDest:
						{
							m_PickEntityTarget.SetKeyToRetrieve("nodeid");
							break;
						}

						default:
						{
							Assert(false);
						}
					}

					pTool->Attach(&m_PickEntityTarget);

					ToolManager()->SetTool(TOOL_PICK_ENTITY);
				}
			}
		}
		else
		{
			StopPicking();
		}
	}
}
//-----------------------------------------------------------------------------
// Purpose: Load custom colors 
//-----------------------------------------------------------------------------
void COP_Entity::LoadCustomColors()
{
	if (m_bCustomColorsLoaded)
		return;

	char szRootDir[MAX_PATH];
	char szFullPath[MAX_PATH];
	APP()->GetDirectory(DIR_PROGRAM, szRootDir);
	Q_MakeAbsolutePath( szFullPath, MAX_PATH, "customcolors.dat", szRootDir ); 
	std::ifstream file(szFullPath, std::ios::in | std::ios::binary);
	
	if(!file.is_open())
	{
		//Nothing to load, but don't keep trying every time the dialog pops up.
		m_bCustomColorsLoaded = true;
		return;
	}
	file.read((char*)CustomColors, sizeof(CustomColors));
	file.close();
	m_bCustomColorsLoaded = true;
}
//-----------------------------------------------------------------------------
// Purpose: Save custom colors out to a file
//-----------------------------------------------------------------------------
void COP_Entity::SaveCustomColors()
{
	char szRootDir[MAX_PATH];
	char szFullPath[MAX_PATH];
	APP()->GetDirectory(DIR_PROGRAM, szRootDir);
	Q_MakeAbsolutePath( szFullPath, MAX_PATH, "customcolors.dat", szRootDir ); 
	std::ofstream file( szFullPath, std::ios::out | std::ios::binary );

	file.write((char*)CustomColors, sizeof(CustomColors));
	file.close();
}


//-----------------------------------------------------------------------------
// Purpose: this function will attempt to look up a variable from the variable map
// Input  : index - non-negative, it is the index into the variable map.
//					-1 = invalid
//					negative starting at the INSTANCE_VAR_MAP_START indicates it is a 
//						custom instance parameter.
// Output : 
//-----------------------------------------------------------------------------
GDinputvariable *COP_Entity::GetVariableAt( int index )
{
	if ( m_VarMap[ index ] == -1 )
	{
		return NULL;
	}

	if ( m_VarMap[ index ] <= INSTANCE_VAR_MAP_START )
	{
		return m_InstanceParmData[ INSTANCE_VAR_MAP_START - m_VarMap[ index ] ].m_ParmVariable;
	}

	return m_pDisplayClass->GetVariableAt( m_VarMap[ index ] );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnPickColor(void)
{
	int iSel = GetCurVarListSelection();
	GDinputvariable * pVar = GetVariableAt( iSel );
	if ( pVar == NULL )
	{
		return;
	}

	if (!m_bCustomColorsLoaded)
		LoadCustomColors();

	// find current color
	COLORREF clr;
	BYTE r = 255, g = 255, b = 255;
	DWORD brightness = 0xffffffff;

	char szTmp[128], *pTmp;
	m_pSmartControl->GetWindowText(szTmp, sizeof szTmp);

	pTmp = strtok(szTmp, " ");
	int iCurToken = 0;
	while(pTmp)
	{
		if(pTmp[0])
		{
			if(iCurToken == 3)
			{
				brightness = atol(pTmp);
			}
			else if(pVar->GetType() == ivColor255)
			{
				if(iCurToken == 0)
					r = BYTE(atol(pTmp));
				if(iCurToken == 1)
					g = BYTE(atol(pTmp));
				if(iCurToken == 2)
					b = BYTE(atol(pTmp));
			}
			else
			{
				if(iCurToken == 0)
					r = BYTE(atof(pTmp) * 255.f);
				if(iCurToken == 1)
					g = BYTE(atof(pTmp) * 255.f);
				if(iCurToken == 2)
					b = BYTE(atof(pTmp) * 255.f);
			}
			++iCurToken;
		}
		pTmp = strtok(NULL, " ");
	}

	clr = RGB(r, g, b);

	CColorDialog dlg(clr, CC_FULLOPEN);
	dlg.m_cc.lpCustColors = CustomColors;
	if(dlg.DoModal() != IDOK)
		return;

	SaveCustomColors();

	r = GetRValue(dlg.m_cc.rgbResult);
	g = GetGValue(dlg.m_cc.rgbResult);
	b = GetBValue(dlg.m_cc.rgbResult);

	// set back in field
	if(pVar->GetType() == ivColor255)
	{
		sprintf(szTmp, "%d %d %d", r, g, b);
	}
	else
	{
		sprintf(szTmp, "%.3f %.3f %.3f", float(r) / 255.f,
			float(g) / 255.f, float(b) / 255.f);
	}

	if(brightness != 0xffffffff)
		sprintf(szTmp + strlen(szTmp), " %d", brightness);

	m_pSmartControl->SetWindowText(szTmp);
	RefreshKVListValues();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Entity::OnSetfocusKey(void)
{
	m_cKey.GetWindowText(m_szOldKeyName);

	if (m_szOldKeyName.IsEmpty())
		return;

	m_szOldKeyName.MakeLower();

	m_bChangingKeyName = true;
}


//-----------------------------------------------------------------------------
// Purpose: Called whenever we are hidden or shown.
// Input  : bShow - TRUE if we are being shown, FALSE if we are being hidden.
//			nStatus - 
//-----------------------------------------------------------------------------
void COP_Entity::OnShowPropertySheet(BOOL bShow, UINT nStatus)
{
	if (bShow)
	{
		//
		// Being shown. Make sure the data in the smartedit control is correct.
		//
		OnSelchangeKeyvalues();
	}
	else
	{
		//
		// Being hidden. Abort face picking if we are doing so.
		//
		StopPicking();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nChar - 
//			nRepCnt - 
//			nFlags - 
//-----------------------------------------------------------------------------
void CMyEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	CEdit::OnChar(nChar, nRepCnt, nFlags);
	return;

	if(nChar == 1)	// ctrl+a
	{
		m_pParent->SetNextVar(1);
	}
	else if(nChar == 11)	// ctrl+q
	{
		m_pParent->SetNextVar(-1);
	}
	else
	{
		CEdit::OnChar(nChar, nRepCnt, nFlags);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nChar - 
//			nRepCnt - 
//			nFlags - 
//-----------------------------------------------------------------------------
void CMyComboBox::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	CComboBox::OnChar(nChar, nRepCnt, nFlags);
	return;

	if(nChar == 1)	// ctrl+a
	{
		m_pParent->SetNextVar(1);
	}
	else if(nChar == 11)	// ctrl+q
	{
		m_pParent->SetNextVar(-1);
	}
	else
	{
		CComboBox::OnChar(nChar, nRepCnt, nFlags);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets the new face ID list from the pick face tool and updates the
//			contents of the edit control with space-delimited face IDs.
//-----------------------------------------------------------------------------
void COP_Entity::UpdatePickFaceText(CToolPickFace *pTool)
{
	char szList[KEYVALUE_MAX_VALUE_LENGTH];
	szList[0] = '\0';

	CMapFaceList FaceListFull;
	CMapFaceList FaceListPartial;

	pTool->GetSelectedFaces(FaceListFull, FaceListPartial);
	if (!CMapWorld::FaceID_FaceListsToString(szList, sizeof(szList), &FaceListFull, &FaceListPartial))
	{
		MessageBox("Too many faces selected for this keyvalue to hold. Deselect some faces.", "Error", MB_OK);
	}

	//
	// Update the edit control text with the new face IDs. This text will be
	// stuffed into the local keyvalue storage in OnChangeSmartControl.
	//
	m_pSmartControl->SetWindowText(szList);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the message sent by the angles custom control when the user
//			changes the angle via the angle box or edit combo.
// Input  : WPARAM - The ID of the control that sent the message.
//			LPARAM - Unused.
//-----------------------------------------------------------------------------
LRESULT COP_Entity::OnChangeAngleBox(WPARAM nID, LPARAM)
{
	CString strKey;
	GetCurKey(strKey);

	char szValue[KEYVALUE_MAX_VALUE_LENGTH];
	bool bUpdateControl = false;
	if ((nID == IDC_ANGLEBOX) || (nID == IDC_ANGLEEDIT))
	{
		// From the main "angles" box.
		m_Angle.GetAngles(szValue);

		// Only update the edit control text if the "angles" key is selected.
		if (!strKey.CompareNoCase("angles"))
		{
			bUpdateControl = true;
		}

		// Slam "angles" into the key name since that's the key we're modifying.
		strKey = "angles";
	}
	else
	{
		// From the secondary angles box that edits the selected keyvalue.
		m_SmartAngle.GetAngles(szValue);
		bUpdateControl = true;
	}

	// Commit the change to our local storage.
	UpdateKeyValue(strKey, szValue);

	if (bUpdateControl)
	{
		if (m_bSmartedit)
		{
			// Reflect the change in the SmartEdit control.
			Assert(m_pSmartControl);
			if (m_pSmartControl)
			{
				m_bEnableControlUpdate = false;
				m_pSmartControl->SetWindowText(szValue);
				m_bEnableControlUpdate = true;
			}
		}
		else
		{
			// Reflect the change in the keyvalue control.
			m_bEnableControlUpdate = false;
			m_cValue.SetWindowText(szValue);
			m_bEnableControlUpdate = true;
		}
	}

	return 0;
}

void COP_Entity::OnCameraDistance(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	CMapView3D *pView = pDoc->GetFirst3DView();
	if ( !pView )
		return;
	const CCamera *camera = pView->GetCamera();
	Vector cameraPos;
	camera->GetViewPoint( cameraPos );
	Assert(pDoc != NULL);
	if (pDoc == NULL)
	{
		return;
	}

	int nSel = GetCurVarListSelection();
	Assert(nSel != LB_ERR);

	if (nSel != LB_ERR)
	{		
		GDinputvariable * pVar = GetVariableAt( nSel );
		if ( pVar == NULL )
		{
			return;
		}

		Vector objectPos;
		const CMapObjectList *pSelection = pDoc->GetSelection()->GetList();
		int iSelectionCount = pSelection->Count();
		if ( iSelectionCount == 1 )
		{
			// Only 1 entity selected.. we can just set our SmartControl text and the change will get applied 
			// when they close the properties dialog or click Apply.
			CMapClass *selectedObject = pSelection->Element(iSelectionCount - 1);
			selectedObject->GetOrigin( objectPos );
			int distance = VectorLength( cameraPos - objectPos );	
			char buf[255];
			itoa( distance, buf, 10 );			
			m_pSmartControl->SetWindowText(buf);	
		}
		else
		{
			// Multiple entities selected. We have to apply the current set of changes, 
			// Set the value in each entity and set the kv text to VALUE_DIFFERENT_STRING so it doesn't overwrite anything when we Apply().
			int index = m_kv.FindByKeyName( pVar->GetName() );
			if ( index == m_kv.GetInvalidIndex() )
				return;
				
			// First set VALUE_DIFFERENT_STRING in our smart control and in m_kv.
			m_pSmartControl->SetWindowText( VALUE_DIFFERENT_STRING );
			MDkeyvalue &kvCur = m_kv.GetKeyValue( index );
			V_strncpy( kvCur.szValue, VALUE_DIFFERENT_STRING, sizeof( kvCur.szValue ) );

			// Get the list of objects we'll apply this to.
			CMapObjectList objectList;
			FOR_EACH_OBJ( *m_pObjectList, pos )
			{
				CMapClass *pObject = m_pObjectList->Element(pos);
				if ( pObject && !IsWorldObject( pObject ) && dynamic_cast <CEditGameClass *>(pObject) )
					objectList.AddToTail( pObject );
			}
			
			// Now set the distance property directly on the selected entities.
			if ( objectList.Count() > 0 )
			{
				// Setup undo stuff.
				GetHistory()->MarkUndoPosition( pDoc->GetSelection()->GetList(), "Change Properties");
				GetHistory()->Keep( &objectList );

				FOR_EACH_OBJ( objectList, pos )
				{
					CMapClass *pObject = m_pObjectList->Element(pos);
					CEditGameClass *pEdit = dynamic_cast <CEditGameClass *>(pObject);
					Assert( pObject && pEdit );

					pObject->GetOrigin( objectPos );
					int distance = VectorLength( cameraPos - objectPos );	
					char buf[255];
					itoa( distance, buf, 10 );			

					pEdit->SetKeyValue( pVar->GetName(), buf );
				}
			}
		}
	}
}


void COP_Entity::OnTextChanged( const char *pText )
{
	m_bClassSelectionEmpty = false;
	UpdateDisplayClass( pText );
}


bool COP_Entity::OnUnknownEntry( const char *pText )
{
	// They entered a classname we don't recognize. Get rid of all keys.
	// It's about to call OnTextChanged, and we'll null m_pDisplayClass, disable SmartEdit, etc.
	m_kv.RemoveAll();
	return true;
}

void COP_Entity::OnSmartControlTargetNameChanged( const char *pText )
{
	CFilteredComboBox *pCombo = dynamic_cast<CFilteredComboBox*>( m_pSmartControl );
	Assert( pCombo );
	if ( pCombo )
	{
		InternalOnChangeSmartcontrol( pCombo->GetCurrentItem() );
	}
}


void COP_Entity::GetItemColor( int iItem, COLORREF *pBackgroundColor, COLORREF *pTextColor )
{
	// Setup the background color.
	EKeyState eState;
	bool bMissingTarget;
	GetKeyState( (const char*)m_VarList.GetItemData( iItem ), &eState, &bMissingTarget );
	
	if ( eState == k_EKeyState_Modified )
		*pBackgroundColor = g_BgColor_Edited;
	else if ( eState == k_EKeyState_AddedManually )
		*pBackgroundColor = g_BgColor_Added;
	else if ( eState == k_EKeyState_InstanceParm )
		*pBackgroundColor = g_BgColor_InstanceParm;
	else
		*pBackgroundColor = g_BgColor_Default;

	// Setup the text color.
	if ( bMissingTarget )
		*pTextColor = g_TextColor_MissingTarget;
	else
		*pTextColor = g_TextColor_Normal;
}


bool COP_Entity::CustomDrawItemValue( const LPDRAWITEMSTRUCT p, const RECT *pRect )
{
	if ( !m_bSmartedit || p->itemID < 0 || p->itemID >= ARRAYSIZE(m_VarMap) || m_VarMap[p->itemID] < 0 )
		return false;

	if ( !m_pDisplayClass )
		return false;
	
	GDinputvariable * pVar = GetVariableAt( p->itemID );
	if ( pVar == NULL )
	{
		return false;
	}
	if ( pVar && (pVar->GetType() == ivColor255 || pVar->GetType() == ivColor1) )
	{
		const char *pValue = m_kv.GetValue( pVar->GetName() );
		if ( pValue )
		{
			int r, g, b;
			if ( pVar->GetType() == ivColor255 )
			{
				sscanf( pValue, "%d %d %d", &r, &g, &b );
			}
			else
			{
				float fr, fg, fb;
				sscanf( pValue, "%f %f %f", &fr, &fg, &fb );
				r = (int)(fr * 255.0);
				g = (int)(fg * 255.0);
				b = (int)(fb * 255.0);
			}

			HBRUSH hBrush = CreateSolidBrush( RGB( r, g, b ) );
			HPEN hPen = CreatePen( PS_SOLID, 0, RGB(0,0,0) );
			SelectObject( p->hDC, hBrush );
			SelectObject( p->hDC, hPen );

			RECT rc = *pRect;
			Rectangle( p->hDC, rc.left+6, rc.top+2, rc.right-6, rc.bottom-2 );

			return true;
		}
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: The flags page calls this whenever a spawnflag changes in it.
// Input  : preserveMask tells the bits you should NOT change in the spawnflags value.
//          newValues is the values of the other bits.
//-----------------------------------------------------------------------------
void COP_Entity::OnUpdateSpawnFlags( unsigned long preserveMask, unsigned long newValues )
{
	const char *p = m_kv.GetValue( SPAWNFLAGS_KEYNAME );
	if ( !p )
		return;
	
	unsigned long oldValue = 0;
	sscanf( p, "%lu", &oldValue );
	
	unsigned long newValue = (oldValue & preserveMask) | (newValues & ~preserveMask);
	
	// We print the string ourselves here because the int version of SetValue will show a negative number
	// if we exceed 1<<31.
	char str[512];
	V_snprintf( str, sizeof( str ), "%lu", newValue );
	m_kv.SetValue( SPAWNFLAGS_KEYNAME, str );
	
	RefreshKVListValues( SPAWNFLAGS_KEYNAME );
	OnSelchangeKeyvalues(); // Refresh the control with its value in case it's selected currently.
}


void COP_Entity::OnSize( UINT nType, int cx, int cy )
{
	m_AnchorMgr.OnSize();
}


