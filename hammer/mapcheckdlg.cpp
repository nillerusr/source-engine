//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "GameConfig.h"
#include "GlobalFunctions.h"
#include "History.h"
#include "MainFrm.h"
#include "MapCheckDlg.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapSolid.h"
#include "MapWorld.h"
#include "Options.h"
#include "ToolManager.h"
#include "VisGroup.h"
#include "hammer.h"
#include "MapOverlay.h"
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


// ********
// NOTE: Make sure the order matches g_MapErrorStringIDs below!
// ********
typedef enum
{
	ErrorNoPlayerStart,
	ErrorMixedFace,
	ErrorDuplicatePlanes,
	ErrorMissingTarget,
	ErrorInvalidTexture,
	ErrorSolidStructure,
	ErrorUnusedKeyvalues,
	ErrorEmptyEntity,
	ErrorDuplicateKeys,
	ErrorSolidContents,
	ErrorInvalidTextureAxes,
	ErrorDuplicateFaceIDs,
	ErrorDuplicateNodeIDs,
	ErrorBadConnections,
	ErrorHiddenGroupHiddenChildren,
	ErrorHiddenGroupVisibleChildren,
	ErrorHiddenGroupMixedChildren,
	ErrorHiddenObjectNoVisGroup,
	ErrorHiddenChildOfEntity,
	ErrorIllegallyHiddenObject,
	ErrorKillInputRaceCondition,
	ErrorOverlayFaceList,
} MapErrorType;

// ********
// NOTE: Make sure the order matches MapErrorType above!
// ********
struct
{
	int	m_StrResourceID;
	int m_DescriptionResourceID;
} g_MapErrorStrings[] =
{
	{IDS_NOPLAYERSTART,					IDS_NOPLAYERSTART_DESC},
	{IDS_MIXEDFACES,					IDS_MIXEDFACES_DESC},
	{IDS_DUPLICATEPLANES,				IDS_DUPLICATEPLANES_DESC},
	{IDS_UNMATCHEDTARGET,				IDS_UNMATCHEDTARGET_DESC},
	{IDS_INVALIDTEXTURE,				IDS_INVALIDTEXTURE_DESC},
	{IDS_SOLIDSTRUCTURE,				IDS_SOLIDSTRUCTURE_DESC},
	{IDS_UNUSEDKEYVALUES,				IDS_UNUSEDKEYVALUES_DESC},
	{IDS_EMPTYENTITY,					IDS_EMPTYENTITY_DESC},
	{IDS_DUPLICATEKEYS,					IDS_DUPLICATEKEYS_DESC},
	{IDS_SOLIDCONTENT,					IDS_SOLIDCONTENT_DESC},
	{IDS_INVALIDTEXTUREAXES,			IDS_INVALIDTEXTUREAXES_DESC},
	{IDS_DUPLICATEFACEID,				IDS_DUPLICATEFACEID_DESC},
	{IDS_DUPLICATE_NODE_ID,				IDS_DUPLICATE_NODE_ID_DESC},
	{IDS_BAD_CONNECTIONS,				IDS_BAD_CONNECTIONS_DESC},
	{IDS_HIDDEN_GROUP_HIDDEN_CHILDREN,	IDS_HIDDEN_GROUP_HIDDEN_CHILDREN_DESC},
	{IDS_HIDDEN_GROUP_VISIBLE_CHILDREN, IDS_HIDDEN_GROUP_VISIBLE_CHILDREN_DESC},
	{IDS_HIDDEN_GROUP_MIXED_CHILDREN,	IDS_HIDDEN_GROUP_MIXED_CHILDREN_DESC},
	{IDS_HIDDEN_NO_VISGROUP,			IDS_HIDDEN_NO_VISGROUP_DESC},
	{IDS_HIDDEN_CHILD_OF_ENTITY,		IDS_HIDDEN_CHILD_OF_ENTITY_DESC},
	{IDS_HIDDEN_ILLEGALLY,				IDS_HIDDEN_ILLEGALLY_DESC},
	{IDS_KILL_INPUT_RACE_CONDITION,		IDS_KILL_INPUT_RACE_CONDITION_DESC},
	{IDS_BAD_OVERLAY,					IDS_DAB_OVERLAY_DESC}
};



typedef enum
{
	CantFix,
	NeedsFix,
	Fixed,
} FIXCODE;


struct MapError
{
	CMapClass *pObjects[3];
	MapErrorType Type;
	DWORD dwExtra;
	FIXCODE Fix;
};


//
// Fix functions.
//
static void FixDuplicatePlanes(MapError *pError);
static void FixSolidStructure(MapError *pError);
static void FixInvalidTexture(MapError *pError);
static void FixInvalidTextureAxes(MapError *pError);
static void FixUnusedKeyvalues(MapError *pError);
static void FixEmptyEntity(MapError *pError);
static void FixBadConnections(MapError *pError);
static void FixInvalidContents(MapError *pError);
static void FixDuplicateFaceIDs(MapError *pError);
static void FixDuplicateNodeIDs(MapError *pError);
static void FixMissingTarget(MapError *pError);
void FixHiddenObject(MapError *pError);
static void FixKillInputRaceCondition(MapError *pError);
static void FixOverlayFaceList(MapError *pError);


CMapCheckDlg *s_pDlg = NULL;


BEGIN_MESSAGE_MAP(CMapCheckDlg, CDialog)
	//{{AFX_MSG_MAP(CMapCheckDlg)
	ON_BN_CLICKED(IDC_GO, OnGo)
	ON_LBN_SELCHANGE(IDC_ERRORS, OnSelchangeErrors)
	ON_LBN_DBLCLK(IDC_ERRORS, OnDblClkErrors)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDC_FIX, OnFix)
	ON_BN_CLICKED(IDC_FIXALL, OnFixall)
	ON_WM_DESTROY()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_CHECK_VISIBLE_ONLY, OnCheckVisibleOnly)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Visibility check
//-----------------------------------------------------------------------------
inline bool IsCheckVisible( CMapClass *pClass )
{
	return (Options.general.bCheckVisibleMapErrors == FALSE) || pClass->IsVisible();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapCheckDlg::CheckForProblems(CWnd *pwndParent)
{
	if (!s_pDlg)
	{
		s_pDlg = new CMapCheckDlg;
		s_pDlg->Create(IDD, pwndParent);
	}

	if (!s_pDlg->DoCheck())
	{
		// Found problems.
		s_pDlg->ShowWindow(SW_SHOW);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pParent - 
//-----------------------------------------------------------------------------
CMapCheckDlg::CMapCheckDlg(CWnd *pParent)
	: CDialog(CMapCheckDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CMapCheckDlg)
	m_bCheckVisible = FALSE;
	//}}AFX_DATA_INIT

	m_bCheckVisible = Options.general.bCheckVisibleMapErrors;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDX - 
//-----------------------------------------------------------------------------
void CMapCheckDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	//{{AFX_DATA_MAP(CMapCheckDlg)
	DDX_Control(pDX, IDC_FIXALL, m_cFixAll);
	DDX_Control(pDX, IDC_FIX, m_Fix);
	DDX_Control(pDX, IDC_GO, m_Go);
	DDX_Control(pDX, IDC_DESCRIPTION, m_Description);
	DDX_Control(pDX, IDC_ERRORS, m_Errors);
	DDX_Check(pDX, IDC_CHECK_VISIBLE_ONLY, m_bCheckVisible);
	//}}AFX_DATA_MAP

	if ( pDX->m_bSaveAndValidate )
	{
		Options.general.bCheckVisibleMapErrors = m_bCheckVisible;
	}
}


//-----------------------------------------------------------------------------
// Checkbox indicating whether we should check visible errors
//-----------------------------------------------------------------------------
void CMapCheckDlg::OnCheckVisibleOnly()
{
	UpdateData( TRUE );
	DoCheck();
}


//-----------------------------------------------------------------------------
// Purpose: Selects the current error objects and centers the views on it.
//-----------------------------------------------------------------------------
void CMapCheckDlg::OnGo() 
{
	GotoSelectedErrors();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapCheckDlg::GotoSelectedErrors()
{
	int iSel = m_Errors.GetCurSel();
	if (iSel == LB_ERR)
	{
		return;
	}

	ToolManager()->SetTool(TOOL_POINTER);

	CMapObjectList Objects;
	for (int i = 0; i < m_Errors.GetCount(); i++)
	{
		if (m_Errors.GetSel(i) > 0)
		{
			MapError *pError = (MapError *)m_Errors.GetItemDataPtr(i);
			if (pError)
			{
				Objects.AddToTail(pError->pObjects[0]);
			}
		}
	}

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	pDoc->SelectObjectList(&Objects);
	pDoc->CenterViewsOnSelection();
}


//-----------------------------------------------------------------------------
// Purpose: Fixes all the selected errors.
//-----------------------------------------------------------------------------
void CMapCheckDlg::OnFix() 
{
	int iSel = m_Errors.GetCurSel();
	if (iSel == LB_ERR)
	{
		return;
	}

	UpdateBox ub;
	CMapObjectList Objects;
	ub.Objects = &Objects;

	for (int i = 0; i < m_Errors.GetCount(); i++)
	{
		if (m_Errors.GetSel(i) > 0)
		{
			MapError *pError = (MapError *)m_Errors.GetItemDataPtr(i);
			if (pError)
			{
				Fix(pError, ub);
			}
		}
	}

	OnSelchangeErrors();
	CMapDoc::GetActiveMapDoc()->UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pError - 
//			ub - 
//-----------------------------------------------------------------------------
void CMapCheckDlg::Fix(MapError *pError, UpdateBox &ub)
{
	CMapDoc::GetActiveMapDoc()->SetModifiedFlag();

	if (pError->Fix != NeedsFix)
	{
		// should never get here because this button is supposed 
		//  to be disabled if the error cannot be fixed
		return;
	}

	//
	// Expand the bounds of the update region to include the broken objects.
	//
	for (int i = 0; i < 2; i++)
	{
		if (!pError->pObjects[i])
		{
			continue;
		}

		ub.Objects->AddToTail(pError->pObjects[i]);

		Vector mins;
		Vector maxs;
		pError->pObjects[i]->GetRender2DBox(mins, maxs);
		ub.Box.UpdateBounds(mins, maxs);
	}

	//
	// Perform the fix.
	//
	switch (pError->Type)
	{
		case ErrorDuplicatePlanes:
		{
			FixDuplicatePlanes(pError);
			break;
		}
		case ErrorDuplicateFaceIDs:
		{
			FixDuplicatePlanes(pError);
			break;
		}
		case ErrorDuplicateNodeIDs:
		{
			FixDuplicateNodeIDs(pError);
			break;
		}
		case ErrorMissingTarget:
		{
			FixMissingTarget(pError);
			break;
		}
		case ErrorSolidStructure:
		{
			FixSolidStructure(pError);
			break;
		}
		case ErrorSolidContents:
		{
			FixInvalidContents(pError);
			break;
		}
		case ErrorInvalidTexture:
		{
			FixInvalidTexture(pError);
			break;
		}
		case ErrorInvalidTextureAxes:
		{
			FixInvalidTextureAxes(pError);
			break;
		}
		case ErrorUnusedKeyvalues:
		{
			FixUnusedKeyvalues(pError);
			break;
		}
		case ErrorBadConnections:
		{
			FixBadConnections(pError);
			break;
		}
		case ErrorEmptyEntity:
		{
			FixEmptyEntity(pError);
			break;
		}
		case ErrorHiddenGroupVisibleChildren:
		case ErrorHiddenGroupMixedChildren:
		case ErrorHiddenGroupHiddenChildren:
		case ErrorHiddenObjectNoVisGroup:
		case ErrorHiddenChildOfEntity:
		case ErrorIllegallyHiddenObject:
		{
			FixHiddenObject(pError);
			break;
		}
		case ErrorKillInputRaceCondition:
		{
			FixKillInputRaceCondition(pError);
			break;
		}
		case ErrorOverlayFaceList:
		{
			FixOverlayFaceList( pError );
			break;
		}
	}

	pError->Fix = Fixed;

	//
	// Expand the bounds of the update region to include the fixed objects.
	//
	for (int i = 0; i < 2; i++)
	{
		if (!pError->pObjects[i])
		{
			continue;
		}

		Vector mins;
		Vector maxs;
		pError->pObjects[i]->GetRender2DBox(mins, maxs);
		ub.Box.UpdateBounds(mins, maxs);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapCheckDlg::OnFixall() 
{
	int iSel = m_Errors.GetCurSel();
	if (iSel == LB_ERR)
	{
		return;
	}

	MapError *pError = (MapError *) m_Errors.GetItemDataPtr(iSel);

	if (pError->Fix == CantFix)
	{
		// should never get here because this button is supposed 
		//  to be disabled if the error cannot be fixed
		return;
	}

	UpdateBox ub;
	CMapObjectList Objects;
	ub.Objects = &Objects;

	// For every selected error...
	for (int i = 0; i < m_Errors.GetCount(); i++)
	{
		if (m_Errors.GetSel(i) > 0)
		{
			pError = (MapError *)m_Errors.GetItemDataPtr(i);
			if ((pError) && (pError->Fix == NeedsFix))
			{
				// Find and fix every error of the same type.
				for (int j = 0; j < m_Errors.GetCount(); j++)
				{
					MapError *pError2 = (MapError *)m_Errors.GetItemDataPtr(j);
					if ((pError2->Type != pError->Type) || (pError2->Fix != NeedsFix))
					{
						continue;
					}

					Fix(pError2, ub);
				}
			}
		}
	}

	OnSelchangeErrors();
	CMapDoc::GetActiveMapDoc()->UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapCheckDlg::OnSelchangeErrors() 
{
	// change description to match error
	int iSel = m_Errors.GetCurSel();

	if(iSel == LB_ERR)
	{
		m_Fix.EnableWindow(FALSE);
		m_cFixAll.EnableWindow(FALSE);
		m_Go.EnableWindow(FALSE);
	}

	CString str;
	MapError *pError;
	pError = (MapError*) m_Errors.GetItemDataPtr(iSel);

	// Figure out which error string we're using.
	int iErrorStr = (int)pError->Type;
	iErrorStr = clamp( iErrorStr, 0, ARRAYSIZE( g_MapErrorStrings ) - 1 );
	Assert( iErrorStr == (int)pError->Type );
	
	str.LoadString(g_MapErrorStrings[iErrorStr].m_DescriptionResourceID);
	m_Description.SetWindowText(str);

	m_Go.EnableWindow(pError->pObjects[0] != NULL);

	// set state of fix button
	m_Fix.EnableWindow(pError->Fix == NeedsFix);
	m_cFixAll.EnableWindow(pError->Fix != CantFix);

	// set text of fix button
	switch (pError->Fix)
	{
		case NeedsFix:
			m_Fix.SetWindowText("&Fix");
			break;
		case CantFix:
			m_Fix.SetWindowText("Can't fix");
			break;
		case Fixed:
			m_Fix.SetWindowText("(fixed)");
			break;
	}

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	pDoc->GetSelection()->SetMode(selectObjects);
	
	if (pError->pObjects[0])
	{
		pDoc->SelectObject(pError->pObjects[0], scClear|scSelect|scSaveChanges );
	}
	else
	{
		pDoc->SelectObject(NULL, scClear|scSaveChanges );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapCheckDlg::OnDblClkErrors() 
{
	GotoSelectedErrors();	
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapCheckDlg::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapCheckDlg::KillErrorList()
{
	// delete items in list.. their data ptrs are allocated objects
	int iSize = m_Errors.GetCount();
	for(int i = 0; i < iSize; i++)
	{
		MapError *pError = (MapError*) m_Errors.GetItemDataPtr(i);
		delete pError;
	}

	m_Errors.ResetContent();
}


//-----------------------------------------------------------------------------
// Purpose: Builds the listbox string for the given error and adds it to the list.
//-----------------------------------------------------------------------------
static void AddErrorToListBox(CListBox *pList, MapError *pError)
{
	CString str;

	// Figure out which error string we're using.
	int iErrorStr = (int)pError->Type;
	iErrorStr = clamp( iErrorStr, 0, ARRAYSIZE( g_MapErrorStrings ) - 1 );
	Assert( iErrorStr == (int)pError->Type );
	
	str.LoadString(g_MapErrorStrings[iErrorStr].m_StrResourceID);

	if (str.Find('%') != -1)
	{
		if (pError->Type == ErrorUnusedKeyvalues)
		{
			// dwExtra has the name of the string in it
			CString str2 = str;
			CMapEntity *pEntity = (CMapEntity *)pError->pObjects[0];
			str.Format(str2, pEntity->GetClassName(), pError->dwExtra);
		}
		else
		{
			CString str2 = str;
			str.Format(str2, pError->dwExtra);
		}
	}

	int iIndex = pList->AddString(str);
	pList->SetItemDataPtr(iIndex, (PVOID)pError);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pList - 
//			Type - 
//			dwExtra - 
//			... - 
//-----------------------------------------------------------------------------
static void AddError(CListBox *pList, MapErrorType Type, DWORD dwExtra, ...)
{
	MapError *pError = new MapError;
	memset(pError, 0, sizeof(MapError));

	pError->Type = Type;
	pError->dwExtra = dwExtra;
	pError->Fix = CantFix;

	va_list vl;
	va_start(vl, dwExtra);

	//
	// Get the object pointer from the variable argument list.
	//
	switch (Type)
	{
		case ErrorNoPlayerStart:
		{
			// no objects.
			break;
		}

		case ErrorMixedFace:
		case ErrorMissingTarget:
		case ErrorDuplicatePlanes:
		case ErrorDuplicateFaceIDs:
		case ErrorDuplicateNodeIDs:
		case ErrorSolidStructure:
		case ErrorSolidContents:
		case ErrorInvalidTexture:
		case ErrorUnusedKeyvalues:
		case ErrorBadConnections:
		case ErrorEmptyEntity:
		case ErrorDuplicateKeys:
		case ErrorInvalidTextureAxes:
		case ErrorHiddenGroupHiddenChildren:
		case ErrorHiddenGroupVisibleChildren:
		case ErrorHiddenGroupMixedChildren:
		case ErrorHiddenObjectNoVisGroup:
		case ErrorHiddenChildOfEntity:
		case ErrorIllegallyHiddenObject:
		case ErrorOverlayFaceList:
		{
			pError->pObjects[0] = va_arg(vl, CMapClass *);
			break;
		}

		case ErrorKillInputRaceCondition:
		{
			pError->pObjects[0] = va_arg(vl, CMapClass *);
			pError->dwExtra = (DWORD)va_arg(vl, CEntityConnection *);
			break;
		}
	}

	//
	// Set the can fix flag.
	//
	switch (Type)
	{
		case ErrorSolidContents:
		case ErrorDuplicatePlanes:
		case ErrorDuplicateFaceIDs:
		case ErrorDuplicateNodeIDs:
		case ErrorSolidStructure:
		case ErrorInvalidTexture:
		case ErrorUnusedKeyvalues:
		case ErrorMissingTarget:
		case ErrorBadConnections:
		case ErrorEmptyEntity:
		case ErrorDuplicateKeys:
		case ErrorInvalidTextureAxes:
		case ErrorHiddenGroupHiddenChildren:
		case ErrorHiddenGroupVisibleChildren:
		case ErrorHiddenGroupMixedChildren:
		case ErrorHiddenObjectNoVisGroup:
		case ErrorHiddenChildOfEntity:
		case ErrorIllegallyHiddenObject:
		case ErrorKillInputRaceCondition:
		case ErrorOverlayFaceList:
		{
			pError->Fix = NeedsFix;
			break;
		}
	}

	va_end(vl);

	AddErrorToListBox(pList, pError);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//			DWORD - 
// Output : 
//-----------------------------------------------------------------------------
static BOOL FindPlayer(CMapEntity *pObject, DWORD)
{
	if ( !IsCheckVisible( pObject ) )
		return TRUE;

	if (pObject->IsPlaceholder() && pObject->IsClass("info_player_start"))
	{
		return(FALSE);
	}
	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pList - 
//			pWorld - 
//-----------------------------------------------------------------------------
static void CheckRequirements(CListBox *pList, CMapWorld *pWorld)
{
	// ensure there's a player start .. 
	if (pWorld->EnumChildren((ENUMMAPCHILDRENPROC)FindPlayer, 0, MAPCLASS_TYPE(CMapEntity)))
	{
		// if rvl is !0, it was not stopped prematurely.. which means there is 
		// NO player start.
		AddError(pList, ErrorNoPlayerStart, 0);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pSolid - 
//			pList - 
// Output : 
//-----------------------------------------------------------------------------
static BOOL _CheckMixedFaces(CMapSolid *pSolid, CListBox *pList)
{
	if ( !IsCheckVisible( pSolid ) )
		return TRUE;

	// run thru faces..
	int iFaces = pSolid->GetFaceCount();
	int iSolid = 2;	// start off ambivalent
	int i;
	for(i = 0; i < iFaces; i++)
	{
		CMapFace *pFace = pSolid->GetFace(i);

		char ch = pFace->texture.texture[0];
		if((ch == '*' && iSolid == 1) || (ch != '*' && iSolid == 0))
		{
			break;
		}
		else iSolid = (ch == '*') ? 0 : 1;
	}

	if(i == iFaces)	// all ok
		return TRUE;

	// NOT ok
	AddError(pList, ErrorMixedFace, 0, pSolid);

	return TRUE;
}


static void CheckMixedFaces(CListBox *pList, CMapWorld *pWorld)
{
	pWorld->EnumChildren((ENUMMAPCHILDRENPROC)_CheckMixedFaces, (DWORD)pList, MAPCLASS_TYPE(CMapSolid));
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if there is another node entity in the world with the
//			same node ID as the given entity.
// Input  : pNode - 
//			pWorld - 
//-----------------------------------------------------------------------------
bool FindDuplicateNodeID(CMapEntity *pNode, CMapWorld *pWorld)
{
	if ( !IsCheckVisible( pNode ) )
		return false;

	EnumChildrenPos_t pos;
	CMapClass *pChild = pWorld->GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pChild);
		if (pEntity && IsCheckVisible( pEntity ) && (pEntity != pNode) && pEntity->IsNodeClass())
		{
			int nNodeID1 = pNode->GetNodeID();
			int nNodeID2 = pEntity->GetNodeID();
			if ((nNodeID1 != 0) && (nNodeID2 != 0) && (nNodeID1 == nNodeID2))
			{
				return true;
			}
		}
		
		pChild = pWorld->GetNextDescendent(pos);
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Checks for node entities with the same node ID.
//-----------------------------------------------------------------------------
static void CheckDuplicateNodeIDs(CListBox *pList, CMapWorld *pWorld)
{
	EnumChildrenPos_t pos;
	CMapClass *pChild = pWorld->GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pChild);
		if (pEntity && pEntity->IsNodeClass())
		{
			if (FindDuplicateNodeID(pEntity, pWorld))
			{
				AddError(pList, ErrorDuplicateNodeIDs, (DWORD)pWorld, pEntity);
			}
		}
		
		pChild = pWorld->GetNextDescendent(pos);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Checks for faces with identical face normals in this solid object.
// Input  : pSolid - Solid to check for duplicate faces.
// Output : Returns TRUE if the face contains at least one duplicate face,
//			FALSE if the solid contains no duplicate faces.
//-----------------------------------------------------------------------------
BOOL DoesContainDuplicates(CMapSolid *pSolid)
{
	int iFaces = pSolid->GetFaceCount();
	for (int i = 0; i < iFaces; i++)
	{
		CMapFace *pFace = pSolid->GetFace(i);
		Vector& pts1 = pFace->plane.normal;

		for (int j = 0; j < iFaces; j++)
		{
			// Don't check self.
			if (j == i)
			{
				continue;
			}

			CMapFace *pFace2 = pSolid->GetFace(j);
			Vector& pts2 = pFace2->plane.normal;

			if (pts1 == pts2)
			{
				return(TRUE);
			}
		}
	}
	
	return(FALSE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pSolid - 
//			pList - 
// Output : 
//-----------------------------------------------------------------------------
static BOOL _CheckDuplicatePlanes(CMapSolid *pSolid, CListBox *pList)
{
	if ( !IsCheckVisible( pSolid ) )
		return TRUE;

	if (DoesContainDuplicates(pSolid))
	{
		AddError(pList, ErrorDuplicatePlanes, 0, pSolid);
	}

	return(TRUE);
}


static void CheckDuplicatePlanes(CListBox *pList, CMapWorld *pWorld)
{
	pWorld->EnumChildren((ENUMMAPCHILDRENPROC)_CheckDuplicatePlanes, (DWORD)pList, MAPCLASS_TYPE(CMapSolid));
}


struct FindDuplicateFaceIDs_t
{
	CMapFaceList All;					// Collects all the face IDs in this map.
	CMapFaceList Duplicates;			// Collects the duplicate face IDs in this map.
};


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pSolid - 
//			pData - 
// Output : Returns TRUE to continue enumerating.
//-----------------------------------------------------------------------------
static BOOL _CheckDuplicateFaceIDs(CMapSolid *pSolid, FindDuplicateFaceIDs_t *pData)
{
	if ( !IsCheckVisible( pSolid ) )
		return TRUE;

	int nFaceCount = pSolid->GetFaceCount();
	for (int i = 0; i < nFaceCount; i++)
	{
		CMapFace *pFace = pSolid->GetFace(i);
		if (pData->All.FindFaceID(pFace->GetFaceID()) != -1)
		{
			if (pData->Duplicates.FindFaceID(pFace->GetFaceID()) != -1)
			{
				pData->Duplicates.AddToTail(pFace);
			}
		}
		else
		{
			pData->All.AddToTail(pFace);
		}
	}

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Reports errors for all faces with duplicate face IDs.
// Input  : pList - 
//			pWorld -  
//-----------------------------------------------------------------------------
static void CheckDuplicateFaceIDs(CListBox *pList, CMapWorld *pWorld)
{
	FindDuplicateFaceIDs_t Lists;
	Lists.All.SetGrowSize(128);
	Lists.Duplicates.SetGrowSize(128);

	pWorld->EnumChildren((ENUMMAPCHILDRENPROC)_CheckDuplicateFaceIDs, (DWORD)&Lists, MAPCLASS_TYPE(CMapSolid));

	for (int i = 0; i < Lists.Duplicates.Count(); i++)
	{
		CMapFace *pFace = Lists.Duplicates.Element(i);
		AddError(pList, ErrorDuplicateFaceIDs, (DWORD)pFace, (CMapSolid *)pFace->GetParent());
	}
}


//-----------------------------------------------------------------------------
// Checks if a particular target is valid.
//-----------------------------------------------------------------------------
static void CheckValidTarget(CMapEntity *pEntity, const char *pFieldName, const char *pTargetName, CListBox *pList, bool bCheckClassNames)
{
	if (!pTargetName)
		return;

	// These procedural names are always assumed to exist.
	if (!stricmp(pTargetName, "!activator") || !stricmp(pTargetName, "!caller") || !stricmp(pTargetName, "!player") || !stricmp(pTargetName, "!self"))
		return;

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	// Search by name first.
	CMapEntityList Found;
	bool bFound = pDoc->FindEntitiesByName(Found, pTargetName, (Options.general.bCheckVisibleMapErrors == TRUE));
	if (!bFound && bCheckClassNames)
	{
		// Not found, search by classname.
		bFound = pDoc->FindEntitiesByClassName(Found, pTargetName, (Options.general.bCheckVisibleMapErrors == TRUE));
	}

	if (!bFound)
	{
		// No dice, flag it as an error.
		AddError(pList, ErrorMissingTarget, (DWORD)pFieldName, pEntity);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Checks the given entity for references by name to nonexistent entities.
// Input  : pEntity - 
//			pList - 
// Output : Returns TRUE to keep enumerating.
//-----------------------------------------------------------------------------
static BOOL _CheckMissingTargets(CMapEntity *pEntity, CListBox *pList)
{
	if ( !IsCheckVisible( pEntity ) )
		return TRUE;

	GDclass *pClass = pEntity->GetClass();
	if (!pClass)
	{
		// Unknown class -- just check for target references.
		static char *pszTarget = "target";
		const char *pszValue = pEntity->GetKeyValue(pszTarget);
		CheckValidTarget(pEntity, pszTarget, pszValue, pList, false);
	}
	else
	{
		// Known class -- check all target_destination and target_name_or_class keyvalues.
		for (int i = 0; i < pClass->GetVariableCount(); i++)
		{
			GDinputvariable *pVar = pClass->GetVariableAt(i);
			if ((pVar->GetType() != ivTargetDest) && (pVar->GetType() != ivTargetNameOrClass))
				continue;

			const char *pszValue = pEntity->GetKeyValue(pVar->GetName());
			CheckValidTarget(pEntity, pVar->GetName(), pszValue, pList, (pVar->GetType() == ivTargetNameOrClass));
		}
	}

	return TRUE;
}


static void CheckMissingTargets(CListBox *pList, CMapWorld *pWorld)
{
	pWorld->EnumChildren((ENUMMAPCHILDRENPROC)_CheckMissingTargets, (DWORD)pList, MAPCLASS_TYPE(CMapEntity));
}


//-----------------------------------------------------------------------------
// Purpose: Determines whether a solid is good or bad.
// Input  : pSolid - Solid to check.
//			pList - List box into which to place errors.
// Output : Always returns TRUE to continue enumerating.
//-----------------------------------------------------------------------------
static BOOL _CheckSolidIntegrity(CMapSolid *pSolid, CListBox *pList)
{
	if ( !IsCheckVisible( pSolid ) )
		return TRUE;

	CCheckFaceInfo cfi;
	int nFaces = pSolid->GetFaceCount();
	for (int i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = pSolid->GetFace(i);

		//
		// Reset the iPoint member so results from previous faces don't carry over.
		//
		cfi.iPoint = -1;

		//
		// Check the face.
		//
		if (!pFace->CheckFace(&cfi))
		{
			AddError(pList, ErrorSolidStructure, 0, pSolid);
			break;
		}
	}

	return(TRUE);
}


static void CheckSolidIntegrity(CListBox *pList, CMapWorld *pWorld)
{
	pWorld->EnumChildren((ENUMMAPCHILDRENPROC)_CheckSolidIntegrity, (DWORD)pList, MAPCLASS_TYPE(CMapSolid));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pSolid - 
//			pList - 
// Output : 
//-----------------------------------------------------------------------------
static BOOL _CheckSolidContents(CMapSolid *pSolid, CListBox *pList)
{
	if ( !IsCheckVisible( pSolid ) )
		return TRUE;

	CCheckFaceInfo cfi;
	int nFaces = pSolid->GetFaceCount();
	CMapFace *pFace = pSolid->GetFace(0);
	DWORD dwContents = pFace->texture.q2contents;

	for (int i = 1; i < nFaces; i++)
	{
		pFace = pSolid->GetFace(i);
		if (pFace->texture.q2contents == dwContents)
		{
			continue;
		}
		AddError(pList, ErrorSolidContents, 0, pSolid);
		break;
	}

	return TRUE;
}


static void CheckSolidContents(CListBox *pList, CMapWorld *pWorld)
{
	if (CMapDoc::GetActiveMapDoc() && CMapDoc::GetActiveMapDoc()->GetGame() && CMapDoc::GetActiveMapDoc()->GetGame()->mapformat == mfQuake2)
	{
		pWorld->EnumChildren((ENUMMAPCHILDRENPROC)_CheckSolidContents, (DWORD)pList, MAPCLASS_TYPE(CMapSolid));
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determines if there are any invalid textures or texture axes on any
//			face of this solid. Adds an error message to the list box for each
//			error found.
// Input  : pSolid - Solid to check.
//			pList - Pointer to the error list box.
// Output : Returns TRUE.
//-----------------------------------------------------------------------------
static BOOL _CheckInvalidTextures(CMapSolid *pSolid, CListBox *pList)
{
	if ( !IsCheckVisible( pSolid ) )
		return TRUE;

	int nFaces = pSolid->GetFaceCount();
	for(int i = 0; i < nFaces; i++)
	{
		const CMapFace *pFace = pSolid->GetFace(i);

		IEditorTexture *pTex = pFace->GetTexture();
		if (pTex->IsDummy())
		{
			AddError(pList, ErrorInvalidTexture, (DWORD)pFace->texture.texture, pSolid);
			return TRUE;
		}

		if (!pFace->IsTextureAxisValid())
		{
			AddError(pList, ErrorInvalidTextureAxes, i, pSolid);
			return(TRUE);
		}
	}
	
	return(TRUE);
}


static void CheckInvalidTextures(CListBox *pList, CMapWorld *pWorld)
{
	pWorld->EnumChildren((ENUMMAPCHILDRENPROC)_CheckInvalidTextures, (DWORD)pList, MAPCLASS_TYPE(CMapSolid));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEntity - 
//			pList - 
// Output : 
//-----------------------------------------------------------------------------
static BOOL _CheckUnusedKeyvalues(CMapEntity *pEntity, CListBox *pList)
{
	if ( !IsCheckVisible( pEntity ) )
		return TRUE;

	if (!pEntity->IsClass() || pEntity->IsClass("multi_manager"))
	{
		return(TRUE);	// can't check if no class associated
	}

	GDclass *pClass = pEntity->GetClass();

	for (int i = pEntity->GetFirstKeyValue(); i != pEntity->GetInvalidKeyValue(); i=pEntity->GetNextKeyValue( i ) )
	{
		if (pClass->VarForName(pEntity->GetKey(i)) == NULL)
		{
			AddError(pList, ErrorUnusedKeyvalues, (DWORD)pEntity->GetKey(i), pEntity);
			return(TRUE);
		}
	}
	
	return(TRUE);
}


static void CheckUnusedKeyvalues(CListBox *pList, CMapWorld *pWorld)
{
	pWorld->EnumChildren((ENUMMAPCHILDRENPROC)_CheckUnusedKeyvalues, (DWORD)pList, MAPCLASS_TYPE(CMapEntity));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEntity - 
//			pList - 
// Output : 
//-----------------------------------------------------------------------------
static BOOL _CheckEmptyEntities(CMapEntity *pEntity, CListBox *pList)
{
	if ( !IsCheckVisible( pEntity ) )
		return TRUE;

	if(!pEntity->IsPlaceholder() && !pEntity->GetChildCount())
	{
		AddError(pList, ErrorEmptyEntity, (DWORD)pEntity->GetClassName(), pEntity);
	}
	
	return(TRUE);
}


static void CheckEmptyEntities(CListBox *pList, CMapWorld *pWorld)
{
	pWorld->EnumChildren((ENUMMAPCHILDRENPROC)_CheckEmptyEntities, (DWORD)pList, MAPCLASS_TYPE(CMapEntity));
}


//-----------------------------------------------------------------------------
// Purpose: Checks the entity for bad I/O connections.
// Input  : pEntity - the entity to check
//			pList - list box that tracks the errors
// Output : Returns TRUE to keep enumerating.
//-----------------------------------------------------------------------------
static BOOL _CheckBadConnections(CMapEntity *pEntity, CListBox *pList)
{
	if ( !IsCheckVisible( pEntity ) )
		return TRUE;

	if (CEntityConnection::ValidateOutputConnections(pEntity, (Options.general.bCheckVisibleMapErrors == TRUE)) == CONNECTION_BAD)
	{
		AddError(pList, ErrorBadConnections, (DWORD)pEntity->GetClassName(), pEntity);
	}

	// TODO: Check for a "Kill" input with the same output, target, and delay as another input. This
	//		 creates a race condition in the game where the order of arrival is not guaranteed
	//int nConnCount = pEntity->Connections_GetCount();
	//for (int i = 0; i < nConnCount; i++)
	//{
	//	CEntityConnection *pConn = pEntity->Connections_Get(i);
	//	if (!stricmp(pConn->GetInputName(), "kill"))
	//	{
	//	}
	//}
	
	return TRUE;
}


static void CheckBadConnections(CListBox *pList, CMapWorld *pWorld)
{
	pWorld->EnumChildren((ENUMMAPCHILDRENPROC)_CheckBadConnections, (DWORD)pList, MAPCLASS_TYPE(CMapEntity));
}


static bool HasVisGroupHiddenChildren(CMapClass *pObject)
{
	const CMapObjectList *pChildren = pObject->GetChildren();

	FOR_EACH_OBJ( *pChildren, pos )
	{
		if (!pChildren->Element(pos)->IsVisGroupShown())
			return true;
	}

	return false;
}


static bool HasVisGroupShownChildren(CMapClass *pObject)
{
	const CMapObjectList *pChildren = pObject->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		if (pChildren->Element(pos)->IsVisGroupShown())
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Makes sure that the visgroup assignments are valid.
//-----------------------------------------------------------------------------
static BOOL _CheckVisGroups(CMapClass *pObject, CListBox *pList)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	// dvs: FIXME: not working yet, revisit
	// Entities cannot have hidden children.
	//CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);
	//if (pEntity && HasVisGroupHiddenChildren(pEntity))
	//{
	//	AddError(pList, ErrorHiddenChildOfEntity, 0, pEntity);
	//	return TRUE;
	//}

	// Check the validity of any object that claims to be hidden by visgroups.
	if (!pObject->IsVisGroupShown())
	{
		// Groups cannot be hidden by visgroups.
		if (pObject->IsGroup())
		{
			bool bHidden = HasVisGroupHiddenChildren(pObject);
			bool bVisible = HasVisGroupShownChildren(pObject);

			if (bHidden && !bVisible)
			{
				AddError(pList, ErrorHiddenGroupHiddenChildren, 0, pObject);
			}
			else if (!bHidden && bVisible)
			{
				AddError(pList, ErrorHiddenGroupVisibleChildren, 0, pObject);
			}
			else
			{
				AddError(pList, ErrorHiddenGroupMixedChildren, 0, pObject);
			}

			return TRUE;
		}

		// Check for unanticipated objects that are hidden but forbidden from visgroup membership.
		if (!pDoc->VisGroups_ObjectCanBelongToVisGroup(pObject))
		{
			AddError(pList, ErrorIllegallyHiddenObject, 0, pObject);
			return TRUE;
		}
		
		// Hidden objects must belong to at least one visgroup.
		if (pObject->GetVisGroupCount() == 0)
		{
			AddError(pList, ErrorHiddenObjectNoVisGroup, 0, pObject);
			return TRUE;
		}
	}

	return TRUE;
}


static void CheckVisGroups(CListBox *pList, CMapWorld *pWorld)
{
	pWorld->EnumChildrenRecurseGroupsOnly((ENUMMAPCHILDRENPROC)_CheckVisGroups, (DWORD)pList);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static BOOL _CheckOverlayFaceList( CMapEntity *pEntity, CListBox *pList )
{
	if ( !IsCheckVisible( pEntity ) )
		return TRUE;

	const CMapObjectList *pChildren = pEntity->GetChildren();

	FOR_EACH_OBJ( *pChildren, pos )
	{
		CMapOverlay *pOverlay = dynamic_cast<CMapOverlay*>( pChildren->Element(pos) );
		if ( pOverlay )
		{
			// Check to see if the overlay has assigned faces.
			if ( pOverlay->GetFaceCount() <= 0 )
			{
				AddError( pList, ErrorOverlayFaceList, 0, pEntity );
				return TRUE;
			}
		}
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void CheckOverlayFaceList( CListBox *pList, CMapWorld *pWorld )
{
	pWorld->EnumChildren( ( ENUMMAPCHILDRENPROC )_CheckOverlayFaceList, ( DWORD )pList, MAPCLASS_TYPE( CMapEntity ));
}

//
// ** FIX FUNCTIONS
//
static void FixDuplicatePlanes(MapError *pError)
{
	// duplicate planes in pObjects[0]
	// run thru faces..

	CMapSolid *pSolid = (CMapSolid*) pError->pObjects[0];

ReStart:
	int iFaces = pSolid->GetFaceCount();
	for(int i = 0; i < iFaces; i++)
	{
		CMapFace *pFace = pSolid->GetFace(i);
		Vector& pts1 = pFace->plane.normal;
		for (int j = 0; j < iFaces; j++)
		{
			// Don't check self
			if (j == i)
			{
				continue;
			}

			CMapFace *pFace2 = pSolid->GetFace(j);
			Vector& pts2 = pFace2->plane.normal;
			if (pts1 == pts2)
			{
				pSolid->DeleteFace(j);
				goto ReStart;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Repairs an invalid solid.
// Input  : pError - Contains information about the error.
//-----------------------------------------------------------------------------
static void FixSolidStructure(MapError *pError)
{
	CMapSolid *pSolid = (CMapSolid *)pError->pObjects[0];

	//
	// First make sure all the faces are good.
	//
	int nFaces = pSolid->GetFaceCount();
	for (int i = nFaces - 1; i >= 0; i--)
	{
		CMapFace *pFace = pSolid->GetFace(i);
		if (!pFace->CheckFace(NULL))
		{
			pFace->Fix();
		}
		//
		// If the face has no points, just remove it from the solid.
		//
		if (pFace->GetPointCount() == 0)
		{
			pSolid->DeleteFace(i);
		}
	}

	//
	// Rebuild the solid from the planes.
	//
	pSolid->CreateFromPlanes();
	pSolid->PostUpdate(Notify_Changed);
}


LPCTSTR GetDefaultTextureName(); // dvs: BAD!


//-----------------------------------------------------------------------------
// Purpose: Replaces any missing textures with the default texture.
// Input  : pError - 
//-----------------------------------------------------------------------------
static void FixInvalidTexture(MapError *pError)
{
	CMapSolid *pSolid = (CMapSolid *)pError->pObjects[0];

	int nFaces = pSolid->GetFaceCount();
	for (int i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = pSolid->GetFace(i);
		if (pFace != NULL)
		{
			IEditorTexture *pTex = pFace->GetTexture();
			if (pTex != NULL)
			{
				if (pTex->IsDummy())
				{
					pFace->SetTexture(GetDefaultTextureName());
				}
			}
		}
	}
}


static void FixInvalidTextureAxes(MapError *pError)
{
	CMapSolid *pSolid = (CMapSolid *)pError->pObjects[0];

	int nFaces = pSolid->GetFaceCount();
	for (int i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = pSolid->GetFace(i);
		if (!pFace->IsTextureAxisValid())
		{
			pFace->InitializeTextureAxes(Options.GetTextureAlignment(), INIT_TEXTURE_FORCE | INIT_TEXTURE_AXES);
		}
	}
}


static void FixInvalidContents(MapError *pError)
{
	CMapSolid *pSolid = (CMapSolid *)pError->pObjects[0];

	CMapFace *pFace = pSolid->GetFace(0);
	DWORD dwContents = pFace->texture.q2contents;

	int nFaces = pSolid->GetFaceCount();
	for (int i = 1; i < nFaces; i++)
	{
		pFace = pSolid->GetFace(i);
		pFace->texture.q2contents = dwContents;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Fixes duplicate face IDs by assigning the face a unique ID within
//			the world.
// Input  : pError - Holds the world and the face that is in error.
//-----------------------------------------------------------------------------
static void FixDuplicateFaceIDs(MapError *pError)
{
	CMapWorld *pWorld = (CMapWorld *)pError->pObjects[0];
	CMapFace *pFace = (CMapFace *)pError->dwExtra;

	pFace->SetFaceID(pWorld->FaceID_GetNext());
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pError - 
//-----------------------------------------------------------------------------
static void FixUnusedKeyvalues(MapError *pError)
{
	CMapEntity *pEntity = (CMapEntity*) pError->pObjects[0];

	GDclass *pClass = pEntity->GetClass();
	if (!pClass)
	{
		return;
	}

	int iNext;
	for ( int i=pEntity->GetFirstKeyValue(); i != pEntity->GetInvalidKeyValue(); i = iNext )
	{
		iNext = pEntity->GetNextKeyValue( i );
		if (pClass->VarForName(pEntity->GetKey(i)) == NULL)
		{
			pEntity->DeleteKeyValue(pEntity->GetKey(i));
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes any bad connections from the entity associated with the error.
// Input  : pError - 
//-----------------------------------------------------------------------------
static void FixBadConnections(MapError *pError)
{
	CMapEntity *pEntity = (CMapEntity *)pError->pObjects[0];
	CEntityConnection::FixBadConnections(pEntity, (Options.general.bCheckVisibleMapErrors == TRUE));
}


//-----------------------------------------------------------------------------
// Purpose: Fixes a race condition caused by a Kill input being triggered at the
//			same instant as another input.
// Input  : pError - 
//-----------------------------------------------------------------------------
static void FixKillInputRaceCondition(MapError *pError)
{
	CEntityConnection *pConn = (CEntityConnection *)pError->pObjects[1];

	// Delay the Kill command so that it arrives after the other command,
	// solving the race condition.
	pConn->SetDelay(pConn->GetDelay() + 0.01);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pError - 
//-----------------------------------------------------------------------------
static void FixOverlayFaceList( MapError *pError )
{
	CMapEntity *pEntity = static_cast<CMapEntity*>( pError->pObjects[0] );
	if ( !pEntity )
		return;

	const CMapObjectList *pChildren = pEntity->GetChildren();

	FOR_EACH_OBJ( *pChildren, pos )
	{
		CMapOverlay *pOverlay = dynamic_cast<CMapOverlay*>( pChildren->Element(pos) );
		if ( pOverlay )
		{
			// Destroy itself.
			CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
			pDoc->RemoveObjectFromWorld( pEntity, true );
			GetHistory()->KeepForDestruction( pEntity );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pError - 
//-----------------------------------------------------------------------------
static void FixEmptyEntity(MapError *pError)
{
	CMapClass *pKillMe = pError->pObjects[0];

	if (pKillMe->GetParent() != NULL)
	{
		GetHistory()->KeepForDestruction(pKillMe);
		pKillMe->GetParent()->RemoveChild(pKillMe);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Fixes duplicate node IDs by assigning the entity a unique node ID.
// Input  : pError - Holds the world and the entity that is in error.
//-----------------------------------------------------------------------------
static void FixDuplicateNodeIDs(MapError *pError)
{
	CMapEntity *pEntity = (CMapEntity *)pError->pObjects[0];
	pEntity->AssignNodeID();
}


//-----------------------------------------------------------------------------
// Purpose: Clears a bad target reference from the given entity.
// Input  : pError - 
//-----------------------------------------------------------------------------
static void FixMissingTarget(MapError *pError)
{
	CMapEntity *pEntity = (CMapEntity *)pError->pObjects[0];
	const char *pszKey = (const char *)pError->dwExtra;
	pEntity->SetKeyValue(pszKey, NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Fix a an invalid visgroup state. This is either:
//			1) A group that is hidden
//			2) An object that is hidden but not in any visgroups
//-----------------------------------------------------------------------------
void FixHiddenObject(MapError *pError)
{
	CMapClass *pObject = pError->pObjects[0];

	// Tweak the object's visgroup state directly to avoid changing the
	// hidden/shown state of the object's children.
	pObject->m_bVisGroupShown = true;
	pObject->m_bVisGroupAutoShown = true;
	pObject->m_VisGroups.RemoveAll();

	// Create a new visgroup to out the objects in (for hiding or inspection/deletion).
	CMapObjectList Objects;
	Objects.AddToTail(pObject);
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	if ((pError->Type == ErrorHiddenGroupHiddenChildren) ||
		(pError->Type == ErrorHiddenObjectNoVisGroup))
	{
		// The objects aren't in the compile, so just hide them.
		pDoc->VisGroups_CreateNamedVisGroup(Objects, "_hidden by Check for Problems", true, false);
	}
	else if (pError->Type == ErrorIllegallyHiddenObject)
	{
		// Do nothing, the object is now shown.
	}
	else
	{
		// ErrorHiddenGroupVisibleChildren
		// ErrorHiddenGroupMixedChildren
		// ErrorHiddenChildOfEntity

		// The objects either ARE in the compile, or they can't be hidden in a visgroup.
		// Don't hide them, just stick them in a visgroup for inspection
		pDoc->VisGroups_CreateNamedVisGroup(Objects, "found by Check for Problems", false, false);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Checks the map for problems. Returns true if the map is okay,
//			false if problems were found.
//-----------------------------------------------------------------------------
bool CMapCheckDlg::DoCheck(void)
{
	CMapWorld *pWorld = GetActiveWorld();

	// Clear error list
	KillErrorList();

	// Map validation
	CheckRequirements(&m_Errors, pWorld);

	// Solid validation
	CheckMixedFaces(&m_Errors, pWorld);
	//CheckDuplicatePlanes(&m_Errors, pWorld);
	CheckDuplicateFaceIDs(&m_Errors, pWorld);
	CheckDuplicateNodeIDs(&m_Errors, pWorld);
	CheckSolidIntegrity(&m_Errors, pWorld);
	CheckSolidContents(&m_Errors, pWorld);
	CheckInvalidTextures(&m_Errors, pWorld);

	// Entity validation
	CheckUnusedKeyvalues(&m_Errors, pWorld);
	CheckEmptyEntities(&m_Errors, pWorld);
	CheckMissingTargets(&m_Errors, pWorld);
	CheckBadConnections(&m_Errors, pWorld);

	CheckVisGroups(&m_Errors, pWorld);

	CheckOverlayFaceList(&m_Errors, pWorld);

	if (!m_Errors.GetCount())
	{
		AfxMessageBox("No errors were found.");
		EndDialog(IDOK);
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapCheckDlg::OnOK()
{
	DestroyWindow();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapCheckDlg::OnClose()
{
	DestroyWindow();
}


//-----------------------------------------------------------------------------
// Purpose: Called when our window is being destroyed.
//-----------------------------------------------------------------------------
void CMapCheckDlg::OnDestroy()
{
	delete this;
	s_pDlg = NULL;
}
