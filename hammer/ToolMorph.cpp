//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "GlobalFunctions.h"
#include "History.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "MainFrm.h"
#include "MapDefs.h"
#include "MapDoc.h"
#include "MapSolid.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "Material.h"
#include "ObjectProperties.h"
#include "ToolManager.h"
#include "ToolMorph.h"
#include "Options.h"
#include "Render2D.h"
#include "StatusBarIDs.h"
#include "hammer.h"
#include "mathlib/vmatrix.h"
#include "vgui/Cursor.h"
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Callback function to add objects to the morph selection.
// Input  : pSolid - Solid to add.
//			pMorph - Morph tool.
// Output : Returns TRUE to continue enumerating.
//-----------------------------------------------------------------------------
static BOOL AddToMorph(CMapSolid *pSolid, Morph3D *pMorph)
{
	pMorph->SelectObject(pSolid, scSelect);
	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Morph3D::Morph3D(void)
{
	m_SelectedType = shtNothing;
	m_HandleMode = hmBoth;
	m_bBoxSelecting = false;
	m_bScaling = false;
	m_pOrigPosList = NULL;
	
	m_vLastMouseMovement.Init();

	m_bHit = false;
	m_bUpdateOrg = false;
	m_bLButtonDownControlState = false;

	SetDrawColors(Options.colors.clrToolHandle, Options.colors.clrToolMorph);

	memset(&m_DragHandle, 0, sizeof(m_DragHandle));
	m_bMorphing = false;
	m_bMovingSelected = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Morph3D::~Morph3D(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Morph3D::OnActivate()
{
	if (IsActiveTool())
	{
		//
		// Already active - change modes and redraw views.
		//
		ToggleMode();
	}
	else
	{
		//
		// Put all selected objects into morph
		//
		const CMapObjectList *pSelection = m_pDocument->GetSelection()->GetList();
		for (int i = 0; i < pSelection->Count(); i++)
		{
			CMapClass *pobj = pSelection->Element(i);
			if (pobj->IsMapClass(MAPCLASS_TYPE(CMapSolid)))
			{
				SelectObject((CMapSolid *)pobj, scSelect);
			}
			pobj->EnumChildren((ENUMMAPCHILDRENPROC)AddToMorph, (DWORD)this, MAPCLASS_TYPE(CMapSolid));
		}

		m_pDocument->SelectObject(NULL, scClear|scSaveChanges );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Can we deactivate this tool.
//-----------------------------------------------------------------------------
bool Morph3D::CanDeactivate( void )
{
	return CanDeselectList();
}

//-----------------------------------------------------------------------------
// Purpose: Called when the tool is deactivated.
// Input  : eNewTool - The ID of the tool that is being activated.
//-----------------------------------------------------------------------------
void Morph3D::OnDeactivate()
{
	if (IsScaling())
	{
		OnScaleCmd();
	}

	if ( !IsEmpty() )
	{
		CUtlVector <CMapClass *>List;
		GetMorphingObjects(List);

		// Empty morph tool (Save contents).
		SetEmpty();

		//
		// Select the solids that we were morphing.
		//
		int nObjectCount = List.Count();
		for (int i = 0; i < nObjectCount; i++)
		{
			CMapClass *pSolid = List.Element(i);
			CMapClass *pSelect = pSolid->PrepareSelection(m_pDocument->GetSelection()->GetMode());
			if (pSelect)
			{
				m_pDocument->SelectObject(pSelect, scSelect);
			}
		}

		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the given solid is being morphed, false if not.
// Input  : pSolid - The solid.
//			pStrucSolidRvl - The corresponding structured solid.
//-----------------------------------------------------------------------------
BOOL Morph3D::IsMorphing(CMapSolid *pSolid, CSSolid **pStrucSolidRvl)
{
	FOR_EACH_OBJ( m_StrucSolids, pos )
	{
		CSSolid *pSSolid = m_StrucSolids.Element(pos);
		if(pSSolid->m_pMapSolid == pSolid)
		{
			if(pStrucSolidRvl)
				pStrucSolidRvl[0] = pSSolid;
			return TRUE;
		}
	}

	return FALSE;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the bounding box of the objects being morphed.
// Input  : bReset - 
//-----------------------------------------------------------------------------
void Morph3D::GetMorphBounds(Vector &mins, Vector &maxs, bool bReset)
{
	mins = m_MorphBounds.bmins;
	maxs = m_MorphBounds.bmaxs;

	if (bReset)
	{
		m_MorphBounds.ResetBounds();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Can we deslect the list?  All current SSolid with displacements 
//          are valid.
//-----------------------------------------------------------------------------
bool Morph3D::CanDeselectList( void )
{
	FOR_EACH_OBJ( m_StrucSolids, pos )
	{
		CSSolid *pSSolid = m_StrucSolids.Element( pos );
		if ( pSSolid )
		{
			if ( !pSSolid->IsValidWithDisps() )
			{
				// Ask
				if( AfxMessageBox( "Invalid solid, destroy displacement(s)?", MB_YESNO ) == IDYES )
				{
					// Destroy the displacement data.
					pSSolid->DestroyDisps();
				}
				else
				{
					return false;
				}
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Selects a solid for vertex manipulation. An SSolid class is created
//			and the CMapSolid is attached to it. The map solid is removed from
//			the views, since it will be represented by the structured solid
//			until vertex manipulation is finished.
// Input  : pSolid - Map solid to select.
//			cmd - scClear, scToggle, scUnselect
//-----------------------------------------------------------------------------
void Morph3D::SelectObject(CMapSolid *pSolid, UINT cmd)
{
	// construct temporary list to pass to document functions:
	CMapObjectList List;
	List.AddToTail( pSolid );

	if( cmd & scClear )
		SetEmpty();

	CSSolid *pStrucSolid;
	if ( IsMorphing( pSolid, &pStrucSolid ) )
	{
		if ( cmd & scToggle || cmd & scUnselect )
		{
			// stop morphing solid
			Vector mins,maxs;
			pSolid->GetRender2DBox(mins, maxs);
			m_MorphBounds.UpdateBounds(mins, maxs);
			pStrucSolid->Convert(FALSE);
			pSolid->GetRender2DBox(mins, maxs);
			m_MorphBounds.UpdateBounds(mins, maxs);

			pStrucSolid->Detach();
			m_pDocument->SetModifiedFlag();
			
			// want to draw in 2d views again
			pSolid->SetVisible2D(true);

			// remove from linked list
			m_StrucSolids.FindAndRemove(pStrucSolid);
			
			// make sure none of its handles are selected
			for(int i = m_SelectedHandles.Count()-1; i >=0; i--)
			{
				if(m_SelectedHandles[i].pStrucSolid == pStrucSolid)
				{
					m_SelectedHandles.Remove(i);
				}
			}

			delete pStrucSolid;
		
			pSolid->SetSelectionState(SELECT_NONE);
		}

		return;
	}

	pStrucSolid = new CSSolid;
	
	// convert to structured solid
	pStrucSolid->Attach(pSolid);
	pStrucSolid->Convert();

	pStrucSolid->ShowHandles(m_HandleMode & hmVertex, m_HandleMode & hmEdge);

	// don't draw this solid in the 2D views anymore
	pSolid->SetVisible2D(false);
	pSolid->SetSelectionState(SELECT_MORPH);

	// add to list of structured solids
	m_StrucSolids.AddToTail(pStrucSolid);
}


int Morph3D::HitTest(CMapView *pView, const Vector2D &ptClient, bool bTestHandles )
{
	if (m_bBoxSelecting)
	{
		return Box3D::HitTest( pView, ptClient, bTestHandles);
	}

	return FALSE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CompareMorphHandles(const MORPHHANDLE &mh1, const MORPHHANDLE &mh2)
{
	return ((mh1.pMapSolid == mh2.pMapSolid) && 
			(mh1.pStrucSolid == mh2.pStrucSolid) && 
			(mh1.ssh == mh2.ssh));
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether or not the given morph handle is selected.
//-----------------------------------------------------------------------------
bool Morph3D::IsSelected(MORPHHANDLE &mh)
{
	for (int i = 0; i < m_SelectedHandles.Count(); i++)
	{
		if (CompareMorphHandles(m_SelectedHandles[i], mh))
		{
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Hit tests against all the handles. Sets the mouse cursor if we are
//			over a handle.
// Input  : pt - 
//			pInfo - 
// Output : Returns TRUE if the mouse cursor is over a handle, FALSE if not.
//-----------------------------------------------------------------------------
bool Morph3D::MorphHitTest(CMapView *pView, const Vector2D &vPoint, MORPHHANDLE *pInfo)
{
	SSHANDLE hnd = 0;

	bool bIs2D = pView->IsOrthographic();

	if ( pInfo )
	{
		memset(pInfo, 0, sizeof(MORPHHANDLE));
	}

	// check scaling position first
	if ( bIs2D && m_bScaling && pInfo)
	{
		if ( HitRect( pView, vPoint, m_ScaleOrg, 8 )  )
		{
			pInfo->ssh = SSH_SCALEORIGIN;
			return true;
		}
	}

	FOR_EACH_OBJ( m_StrucSolids, pos )
	{
		CSSolid *pStrucSolid = m_StrucSolids.Element(pos);

		// do a hit test on all handles:
		if (m_HandleMode & hmVertex)
		{
			for(int i = 0; i < pStrucSolid->m_nVertices; i++)
			{
				CSSVertex &v = pStrucSolid->m_Vertices[i];

				if( HitRect( pView, vPoint, v.pos, HANDLE_RADIUS ) )
				{
					hnd = v.id;
					break;
				}
			}
		}

		if (!hnd && (m_HandleMode & hmEdge))
		{
			for (int i = 0; i < pStrucSolid->m_nEdges; i++)
			{
				CSSEdge &e = pStrucSolid->m_Edges[i];

				if( HitRect( pView, vPoint, e.ptCenter, HANDLE_RADIUS ) )
				{
					hnd = e.id;
					break;
				}
			}
		}

		if (hnd)
		{
			if ( bIs2D )
			{
				SSHANDLEINFO hi;
				pStrucSolid->GetHandleInfo(&hi, hnd);

				// see if there is a 2d match that is already selected - if
				//  there is, select that instead
				SSHANDLE hMatch = Get2DMatches( dynamic_cast<CMapView2D*>(pView), pStrucSolid, hi);

				if(hMatch)
					hnd = hMatch;
			}

			if(pInfo)
			{
				pInfo->pMapSolid = pStrucSolid->m_pMapSolid;
				pInfo->pStrucSolid = pStrucSolid;
				pInfo->ssh = hnd;
			}
			break;
		}
	}

	return hnd != 0;
}

void Morph3D::GetHandlePos(MORPHHANDLE *pInfo, Vector& pt)
{
	SSHANDLEINFO hi;
	pInfo->pStrucSolid->GetHandleInfo(&hi, pInfo->ssh);
	pt = hi.pos;
}


//-----------------------------------------------------------------------------
// Purpose: Fills out a list of handles in the given solid that are at the same
//			position as the given handle in the current 2D view.
// Input  : pStrucSolid - 
//			hi - 
//			hAddSimilarList - 
//			pnAddSimilar - 
// Output : Returns a selected handle at the same position as the given handle,
//			if one exists, otherwise returns 0.
//-----------------------------------------------------------------------------
SSHANDLE Morph3D::Get2DMatches( CMapView2D *pView, CSSolid *pStrucSolid, SSHANDLEINFO &hi, CUtlVector<SSHANDLE>*pSimilarList)
{
	SSHANDLE hNewMoveHandle = 0;

	int axHorz = pView->axHorz;
	int axVert = pView->axVert;

	if(hi.Type == shtVertex)
	{
		for(int i = 0; i < pStrucSolid->m_nVertices; i++)
		{
			CSSVertex & v = pStrucSolid->m_Vertices[i];
	
			// YWB Fixme, scale olerance to zoom amount?
			if( (fabs(hi.pos[axHorz] - v.pos[axHorz]) < 0.5) && 
				(fabs(hi.pos[axVert] - v.pos[axVert]) < 0.5) )
			{
				if(v.m_bSelected)
				{
					hNewMoveHandle = v.id;
				}

				// add it to the array to select
				if( pSimilarList )
					pSimilarList->AddToTail(v.id);
			}
		}
	}
	else if(hi.Type == shtEdge)
	{
		for(int i = 0; i < pStrucSolid->m_nEdges; i++)
		{
			CSSEdge& e = pStrucSolid->m_Edges[i];
	
			if( (fabs(hi.pos[axHorz] - e.ptCenter[axHorz]) < 0.5) && 
				(fabs(hi.pos[axVert] - e.ptCenter[axVert]) < 0.5) )
			{
				if(e.m_bSelected)
				{
					hNewMoveHandle = e.id;
				}

				//  add it to the array to select
				if( pSimilarList )
					pSimilarList->AddToTail(e.id);
			}
		}
	}

	return hNewMoveHandle;
}


//-----------------------------------------------------------------------------
// Purpose: Selects all the handles that are of the same type and at the same
//			position in the current 2D projection as the given handle.
// Input  : pInfo - 
//			cmd - 
// Output : Returns the number of handles that were selected by this call.
//-----------------------------------------------------------------------------
void Morph3D::SelectHandle2D( CMapView2D *pView, MORPHHANDLE *pInfo, UINT cmd)
{
	SSHANDLEINFO hi;

	if ( !pInfo )
		return;

	if( pInfo->ssh == SSH_SCALEORIGIN )
		return;
	
	if (!pInfo->pStrucSolid->GetHandleInfo(&hi, pInfo->ssh))
	{
		// Can't find the handle info, bail.
		DeselectHandle(pInfo);
		return;
	}

	//
	// Check to see if there is a same type handle at the same
	// 2d coordinates.
	//
	CUtlVector<SSHANDLE> addSimilarList;

	Get2DMatches( pView, pInfo->pStrucSolid, hi, &addSimilarList );

	for (int i = 0; i < addSimilarList.Count(); i++)
	{
		MORPHHANDLE mh;
		mh.ssh = addSimilarList[i];
		mh.pStrucSolid = pInfo->pStrucSolid;
		mh.pMapSolid = pInfo->pStrucSolid->m_pMapSolid;

		SelectHandle(&mh, cmd);

		if (i == 0)
		{
			cmd &= ~scClear;
		}
	}

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );

	return;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pInfo - 
//-----------------------------------------------------------------------------
void Morph3D::DeselectHandle(MORPHHANDLE *pInfo)
{
	for (int i = 0; i <m_SelectedHandles.Count(); i++)
	{
		if (!memcmp(&m_SelectedHandles[i], pInfo, sizeof(*pInfo)))
		{
			m_SelectedHandles.Remove(i);
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pInfo - 
//			cmd - 
//-----------------------------------------------------------------------------
void Morph3D::SelectHandle(MORPHHANDLE *pInfo, UINT cmd)
{
	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );

	if( pInfo && pInfo->ssh == SSH_SCALEORIGIN )
		return;

	if(cmd & scSelectAll)
	{
		MORPHHANDLE mh;

		FOR_EACH_OBJ( m_StrucSolids, pos )
		{	
			CSSolid *pStrucSolid = m_StrucSolids.Element(pos);

			for(int i = 0; i < pStrucSolid->m_nVertices; i++)
			{
				CSSVertex& v = pStrucSolid->m_Vertices[i];
				mh.ssh = v.id;
				mh.pStrucSolid = pStrucSolid;
				mh.pMapSolid = pStrucSolid->m_pMapSolid;
				SelectHandle(&mh, scSelect);
			}
		}

		return;
	}

	if(cmd & scClear)
	{
		// clear handles first
		while( m_SelectedHandles.Count()>0)
		{
			SelectHandle(&m_SelectedHandles[0], scUnselect);
		}
	}

	if(cmd == scClear)
	{
		if(m_bScaling)
			OnScaleCmd(TRUE);	// update scaling

		return;	// nothing else to do here
	}
	
	SSHANDLEINFO hi;
	if (!pInfo->pStrucSolid->GetHandleInfo(&hi, pInfo->ssh))
	{
		// Can't find the handle info, bail.
		DeselectHandle(pInfo);
		return;
	}

	if(hi.Type != m_SelectedType)
		SelectHandle(NULL, scClear);	// clear selection first

	m_SelectedType = hi.Type;

	bool bAlreadySelected = (hi.p2DHandle->m_bSelected == TRUE);
	bool bChanged = false;

	// toggle selection:
	if(cmd & scToggle)
	{
		cmd &= ~scToggle;
		cmd |= bAlreadySelected ? scUnselect : scSelect;
	}

	if(cmd & scSelect && !(hi.p2DHandle->m_bSelected))
	{
		hi.p2DHandle->m_bSelected = TRUE;
		bChanged = true;
	}
	else if(cmd & scUnselect && hi.p2DHandle->m_bSelected)
	{
		hi.p2DHandle->m_bSelected = FALSE;
		bChanged = true;
	}

	if(!bChanged)
		return;

	if(hi.p2DHandle->m_bSelected)
	{
		m_SelectedHandles.AddToTail(*pInfo);
	}
	else
	{
		DeselectHandle(pInfo);
	}

	if(m_bScaling)
		OnScaleCmd(TRUE);
}


void Morph3D::MoveSelectedHandles(const Vector &Delta)
{

	FOR_EACH_OBJ( m_StrucSolids, pos )
	{
		CSSolid *pStrucSolid = m_StrucSolids.Element(pos);
		pStrucSolid->MoveSelectedHandles(Delta);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void Morph3D::RenderTool2D(CRender2D *pRender)
{
	pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_SQUARE );

	for (int nPass = 0; nPass < 2; nPass++)
	{

		FOR_EACH_OBJ( m_StrucSolids, pos )
		{
			CSSolid *pStrucSolid = m_StrucSolids.Element(pos);

			//
			// Draw the edges.
			//
			for (int i = 0; i < pStrucSolid->m_nEdges; i++)
			{
				CSSEdge *pEdge = & pStrucSolid->m_Edges[i];

				if (((pEdge->m_bSelected) && (nPass == 0)) ||
					((!pEdge->m_bSelected) && (nPass == 1)))
				{
					continue;
				}

				pRender->SetDrawColor( 255, 0, 0 );

				SSHANDLEINFO hi1;
				SSHANDLEINFO hi2;
				pStrucSolid->GetHandleInfo(&hi1, pEdge->hvStart);
				pStrucSolid->GetHandleInfo(&hi2, pEdge->hvEnd);
				
				pRender->DrawLine(hi1.pos, hi2.pos);

				if (!(m_HandleMode & hmEdge))
				{
					// Don't draw edge handles.
					continue;
				}

				// Draw the edge center handle.
												
				if (pEdge->m_bSelected)
				{
					pRender->SetHandleColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );
				}
				else
				{
					pRender->SetHandleColor( 255,255,0 ) ;
				}

				pRender->DrawHandle( pEdge->ptCenter );
			}

			if (!(m_HandleMode & hmVertex))
			{
				// Don't draw vertex handles.
				continue;
			}

			//
			// Draw vertex handles.
			bool bClientSpace = pRender->BeginClientSpace();
		
			for (int i = 0; i < pStrucSolid->m_nVertices; i++)
			{
				CSSVertex &v = pStrucSolid->m_Vertices[i];

				if (((v.m_bSelected) && (nPass == 0)) ||
					((!v.m_bSelected) && (nPass == 1)))
				{
					continue;
				}

				if (v.m_bSelected)
				{
					pRender->SetHandleColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );
				}
				else
				{
					pRender->SetHandleColor( GetRValue(Options.colors.clrToolHandle), GetGValue(Options.colors.clrToolHandle), GetBValue(Options.colors.clrToolHandle));
				}

				pRender->DrawHandle( v.pos );
			}

			if ( bClientSpace )
				pRender->EndClientSpace();

		}
	}

	//
	// Draw scaling point.
	//
	if (m_bScaling && m_SelectedHandles.Count() )
	{
		pRender->SetHandleStyle( 8, CRender::HANDLE_CIRCLE );
		pRender->SetHandleColor( GetRValue(Options.colors.clrToolHandle), GetGValue(Options.colors.clrToolHandle), GetBValue(Options.colors.clrToolHandle) );
		pRender->DrawHandle( m_ScaleOrg );
	}

	if ( m_bBoxSelecting )
	{
		Box3D::RenderTool2D(pRender);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Finishes the morph, committing changes made to the selected objects.
//-----------------------------------------------------------------------------
void Morph3D::SetEmpty()
{
	GetHistory()->MarkUndoPosition(NULL, "Morphing");

	while(m_StrucSolids.Count()>0)
	{
		// keep getting the head position because SelectObject (below)
		//  removes the object from the list.
		
		CSSolid *pStrucSolid = m_StrucSolids[0];
		
		//
		// Save this solid. BUT, before doing so, set it as visible in the 2D views.
		// Otherwise, it will vanish if the user does an "Undo Morphing".
		//
		pStrucSolid->m_pMapSolid->SetVisible2D(true);
		GetHistory()->Keep(pStrucSolid->m_pMapSolid);
		pStrucSolid->m_pMapSolid->SetVisible2D(false);

		// calling SelectObject with scUnselect SAVES the contents
		//  of the morph.
		SelectObject(pStrucSolid->m_pMapSolid, scUnselect);
	}
}


// 3d translation --
void Morph3D::StartTranslation( CMapView *pView, const Vector2D &vPoint, MORPHHANDLE *pInfo )
{
	if(m_bScaling)
	{
		// back to 1
		m_bScaling = false;	// don't want it to update here
		m_ScaleDlg.m_cScale.SetWindowText("1.0");
		m_bScaling = true;
	}

	if(pInfo->ssh == SSH_SCALEORIGIN)
		m_OrigHandlePos = m_ScaleOrg;
	else

		GetHandlePos(pInfo, m_OrigHandlePos);
	
	Vector vOrigin, vecHorz, vecVert, vecThird;
	pView->GetBestTransformPlane( vecHorz, vecVert, vecThird );
	SetTransformationPlane(  m_OrigHandlePos, vecHorz, vecVert, vecThird );

	// align translation plane to world origin
	ProjectOnTranslationPlane( vec3_origin, vOrigin, 0 );

	// set transformation plane
	SetTransformationPlane(vOrigin, vecHorz, vecVert, vecThird );

	Tool3D::StartTranslation( pView, vPoint, false );

	m_DragHandle = *pInfo;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt - 
//			uFlags - 
//			& - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
bool Morph3D::UpdateTranslation(const Vector &vUpdate, UINT uFlags)
{
	if (m_bBoxSelecting)
	{
		return Box3D::UpdateTranslation(vUpdate, uFlags);
	}

	if ( !Tool3D::UpdateTranslation( vUpdate, uFlags) )
		return false;

	bool bSnap =  uFlags & constrainSnap;
	
	if (m_DragHandle.ssh == SSH_SCALEORIGIN)
	{
		m_ScaleOrg = m_OrigHandlePos + vUpdate;

		if (bSnap)
		{
			m_pDocument->Snap( m_ScaleOrg, uFlags );
		}

		m_bUpdateOrg = false;

		return true;
	}

	//
	// Get the current handle position.
	//

	Vector vCurPos;
	GetHandlePos(&m_DragHandle, vCurPos);
	
	// We don't want to snap edge handles to the grid, because they don't
	// necessarily belong on the grid in the first place.
	if ( uFlags!=0 )
	{
		ProjectOnTranslationPlane( m_OrigHandlePos+m_vTranslation, m_vTranslation, uFlags );
		m_vTranslation -= m_OrigHandlePos;
	}

	Vector vDelta = (m_OrigHandlePos+m_vTranslation)-vCurPos;

	//
	// Create delta and determine if it is large enough to warrant an update.
	//

	if ( vDelta.Length() < 0.5 )
	{
		return false;	// no need to update.
	}

	MoveSelectedHandles( vDelta );

	return true;
}

bool Morph3D::StartBoxSelection(CMapView *pView, const Vector2D &vPoint, const Vector& vStart)
{
	m_bBoxSelecting = true;

	SetDrawColors(RGB(255, 255, 255), RGB(50, 255, 255));

	Box3D::StartNew( pView, vPoint, vStart, Vector(0,0,0) );

	return true;
}


void Morph3D::SelectInBox()
{
	if(!m_bBoxSelecting)
		return;

	// select all vertices within the box, and finish box
	//  selection.

	EndBoxSelection();	// may as well do it here

	// expand box along 0-depth axes
	int countzero = 0;
	for(int i = 0; i < 3; i++)
	{
		if(bmaxs[i] - bmins[i] == 0)
		{
			bmaxs[i] = COORD_NOTINIT;
			bmins[i] = -COORD_NOTINIT;
			countzero++;
		}
	}
	if(countzero > 1)
		return;

	FOR_EACH_OBJ( m_StrucSolids, pos )
	{	
		CSSolid *pStrucSolid = m_StrucSolids.Element(pos);

		for(int i = 0; i < pStrucSolid->m_nVertices; i++)
		{
			CSSVertex& v = pStrucSolid->m_Vertices[i];
			int i2;
			for(i2 = 0; i2 < 3; i2++)
			{
				if(v.pos[i2] < bmins[i2] || v.pos[i2] > bmaxs[i2])
					break;
			}
			
			if(i2 == 3)
			{
				// completed loop - intersects - select handle
				MORPHHANDLE mh;
				mh.ssh = v.id;
				mh.pStrucSolid = pStrucSolid;
				mh.pMapSolid = pStrucSolid->m_pMapSolid;
				SelectHandle(&mh, scSelect);
			}
		}
	}
}

void Morph3D::EndBoxSelection()
{
	m_bBoxSelecting = false;
	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bSave - 
//-----------------------------------------------------------------------------
void Morph3D::FinishTranslation(bool bSave)
{
	if (m_bBoxSelecting)
	{
		Box3D::FinishTranslation(bSave);
		return;
	}
	else if (bSave && m_DragHandle.ssh != SSH_SCALEORIGIN)
	{
		// figure out all the affected solids
		CUtlVector<CSSolid*> Affected;
				
		FOR_EACH_OBJ( m_StrucSolids, pos )
		{
			CSSolid *pStrucSolid = m_StrucSolids.Element(pos);
			if(Affected.Find(pStrucSolid) == -1)
				Affected.AddToTail(pStrucSolid);
		}

		int iConfirm = -1;
		FOR_EACH_OBJ( Affected, pos )
		{
			CSSolid *pStrucSolid = Affected.Element(pos);
			if(pStrucSolid->CanMergeVertices() && iConfirm != 0)
			{
				if(iConfirm == -1)
				{
					// ask
					if(AfxMessageBox("Merge vertices?", MB_YESNO) == IDYES)
						iConfirm = 1;
					else
						iConfirm = 0;
				}
				if(iConfirm == 1)
				{
					int nDeleted;
					SSHANDLE *pDeleted = pStrucSolid->MergeSameVertices(nDeleted);
					// ensure deleted handles are not marked
					for(int i = 0; i < nDeleted; i++)
					{
						MORPHHANDLE mh;
						mh.ssh = pDeleted[i];
						mh.pStrucSolid = pStrucSolid;
						mh.pMapSolid = pStrucSolid->m_pMapSolid;
						SelectHandle(&mh, scUnselect);
					}
				}
			}
//			pStrucSolid->CheckFaces();
		}
	}

	Tool3D::FinishTranslation(bSave);

	if(!bSave)
	{
		// move back to original positions
		Vector curpos;
		GetHandlePos(&m_DragHandle, curpos);
		MoveSelectedHandles(m_OrigHandlePos - curpos);
	}
	else if(m_bScaling)
	{
		OnScaleCmd(TRUE);
	}
}


bool Morph3D::SplitFace()
{
	if(!CanSplitFace())
		return false;

	if(m_SelectedHandles[0].pStrucSolid->SplitFace(m_SelectedHandles[0].ssh,
		m_SelectedHandles[1].ssh))
	{
		// unselect those invalid edges
		if(m_SelectedType == shtVertex)
		{
			// proper deselection
			SelectHandle(NULL, scClear);
		}
		else	// selection is invalid; set count to 0
			m_SelectedHandles.RemoveAll();

		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );

		return false;
	}

	return true;
}


bool Morph3D::CanSplitFace()
{
	// along two edges.
	if(m_SelectedHandles.Count() != 2 || (m_SelectedType != shtEdge && 
		m_SelectedType != shtVertex))
		return false;

	// make sure same solid.
	if(m_SelectedHandles[0].pStrucSolid != m_SelectedHandles[1].pStrucSolid)
		return false;

	return true;
}


void Morph3D::ToggleMode()
{
	if(m_HandleMode == hmBoth)
		m_HandleMode = hmVertex;
	else if(m_HandleMode == hmVertex)
		m_HandleMode = hmEdge;
	else
		m_HandleMode = hmBoth;

	// run through selected solids and tell them the new mode
	FOR_EACH_OBJ( m_StrucSolids, pos )
	{
		CSSolid *pStrucSolid = m_StrucSolids.Element(pos);
		pStrucSolid->ShowHandles(m_HandleMode & hmVertex, m_HandleMode & hmEdge);
	}

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the center of the morph selection.
// Input  : pt - Point at the center of the selection or selected handles.
//-----------------------------------------------------------------------------
void Morph3D::GetSelectedCenter(Vector& pt)
{
	BoundBox box;

	//
	// If we have selected handles, our bounds center is the center of those handles.
	//
	if (m_SelectedHandles.Count() > 0)
	{
		SSHANDLEINFO hi;

		for (int i = 0; i < m_SelectedHandles.Count(); i++)
		{
			MORPHHANDLE *mh = &m_SelectedHandles[i];
			mh->pStrucSolid->GetHandleInfo(&hi, mh->ssh);

			box.UpdateBounds(hi.pos);
		}
	}
	//
	// If no handles are selected, our bounds center is the center of all selected solids.
	//
	else
	{
		FOR_EACH_OBJ( m_StrucSolids, pos )
		{
			CSSolid *pStrucSolid = m_StrucSolids.Element(pos);
			for (int nVertex = 0; nVertex < pStrucSolid->m_nVertices; nVertex++)
			{
				CSSVertex &v = pStrucSolid->m_Vertices[nVertex];
				box.UpdateBounds(v.pos);
			}
		}
	}

	box.GetBoundsCenter(pt);
}


//-----------------------------------------------------------------------------
// Purpose: Fills out a list of the objects selected for morphing.
//-----------------------------------------------------------------------------
void Morph3D::GetMorphingObjects(CUtlVector<CMapClass *> &List)
{
	FOR_EACH_OBJ( m_StrucSolids, pos )
	{
		CSSolid *pStrucSolid = m_StrucSolids.Element(pos);
		List.AddToTail(pStrucSolid->m_pMapSolid);
	}
}


void Morph3D::OnScaleCmd(BOOL bReInit)
{
	if(m_pOrigPosList)
	{
		delete[] m_pOrigPosList;
		m_pOrigPosList = NULL;
	}

	if(m_bScaling && !bReInit)
	{
		m_ScaleDlg.ShowWindow(SW_HIDE);
		m_ScaleDlg.DestroyWindow();
		m_bScaling = false;
		return;
	}

	// start scaling
	if(!bReInit)
	{
		m_ScaleDlg.Create(IDD_SCALEVERTICES);
		CPoint pt;
		GetCursorPos(&pt);
		m_bUpdateOrg = true;

		m_ScaleDlg.SetWindowPos(NULL, pt.x, pt.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);
	}
	else
	{
		m_bScaling = false;	// don't want an update
		m_ScaleDlg.m_cScale.SetWindowText("1.0");
		m_bScaling = true;
	}

	if(m_SelectedHandles.Count()==0)
	{
		m_bScaling = true;
		return;
	}

	m_pOrigPosList = new Vector[m_SelectedHandles.Count()];

	BoundBox box;

	// save original positions of vertices
	for(int i = 0; i < m_SelectedHandles.Count(); i++)
	{
		MORPHHANDLE &hnd = m_SelectedHandles[i];
		SSHANDLEINFO hi;
		hnd.pStrucSolid->GetHandleInfo(&hi, hnd.ssh);

		if(hi.Type != shtVertex)
			continue;

		m_pOrigPosList[i] = hi.pos;
		box.UpdateBounds(hi.pos);
	}

	// center is default origin
	if(m_bUpdateOrg)
		box.GetBoundsCenter(m_ScaleOrg);

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );

	m_bScaling = true;
}


void Morph3D::UpdateScale()
{
	// update scale with data in dialog box
	if(!m_bScaling)
		return;

	float fScale = m_ScaleDlg.m_fScale;

	// match up selected vertices to original position in m_pOrigPosList.
	int iMoved = 0;
	for(int i = 0; i < m_SelectedHandles.Count(); i++)
	{
		MORPHHANDLE &hnd = m_SelectedHandles[i];
		SSHANDLEINFO hi;
		hnd.pStrucSolid->GetHandleInfo(&hi, hnd.ssh);

		if(hi.Type != shtVertex)
			continue;

		// ** scale **
		Vector& pOrigPos = m_pOrigPosList[iMoved++];
		Vector newpos;
		for(int d = 0; d < 3; d++)
		{
			float delta = pOrigPos[d] - m_ScaleOrg[d];
			// YWB rounding
			newpos[d] = /*V_rint*/(m_ScaleOrg[d] + (delta * fScale));
		}

		hnd.pStrucSolid->SetVertexPosition(hi.iIndex, newpos[0], 
			newpos[1], newpos[2]);

		// find edge that references this vertex
		int nEdges;
		CSSEdge **pEdges = hnd.pStrucSolid->FindAffectedEdges(&hnd.ssh, 1, 
			nEdges);
		for(int e = 0; e < nEdges; e++)
		{
			hnd.pStrucSolid->CalcEdgeCenter(pEdges[e]);
		}
	}

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
}



//-----------------------------------------------------------------------------
// Purpose: Renders an object that is currently selected for morphing. The
//			object is rendered in three passes:
//
//			1. Flat shaded grey with transparency.
//			2. Wireframe in white.
//			3. Edges and/or vertices are rendered as boxes.
//
// Input  : pSolid - The structured solid to render.
//-----------------------------------------------------------------------------
void Morph3D::RenderSolid3D(CRender3D *pRender, CSSolid *pSolid)
{
	VMatrix ViewMatrix;
	Vector	ViewPos;
	bool bClientSpace = false;
	
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	for (int nPass = 1; nPass <= 3; nPass++)
	{
		if (nPass == 1)
		{
			pRender->PushRenderMode( RENDER_MODE_SELECTION_OVERLAY );
		}
		else if (nPass == 2)
		{
			pRender->PushRenderMode( RENDER_MODE_WIREFRAME );
		}
		else
		{
			pRender->PushRenderMode( RENDER_MODE_FLAT_NOZ );
			pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_SQUARE );
			bClientSpace = pRender->BeginClientSpace();
			pRender->GetCamera()->GetViewMatrix( ViewMatrix );
		}

		IMesh* pMesh = pRenderContext->GetDynamicMesh();
		CMeshBuilder meshBuilder;

		int nFaceCount = pSolid->GetFaceCount();
		for (int nFace = 0; nFace < nFaceCount; nFace++)
		{
			CSSFace *pFace = pSolid->GetFace(nFace);

			int nEdgeCount = pFace->GetEdgeCount();

			unsigned char color[4];
			if (nPass == 1)
			{
				meshBuilder.Begin( pMesh, MATERIAL_POLYGON, nEdgeCount );
				color[0] = color[1] = color[2] = color[3] = 128; 
			}
			else if (nPass == 2)
			{
				meshBuilder.Begin( pMesh, MATERIAL_LINE_LOOP, nEdgeCount );
				color[0] = color[1] = color[2] = color[3] = 255; 
			}

			for (int nEdge = 0; nEdge < nEdgeCount; nEdge++)
			{
				//
				// Calc next edge so we can see which is the next clockwise point.
				//
				int nEdgeNext = nEdge + 1;
				if (nEdgeNext == nEdgeCount)
				{
					nEdgeNext = 0;
				}

				SSHANDLE hEdge = pFace->GetEdgeHandle(nEdge);
				CSSEdge *pEdgeCur = (CSSEdge *)pSolid->GetHandleData(hEdge);

				SSHANDLE hEdgeNext = pFace->GetEdgeHandle(nEdgeNext);
				CSSEdge *pEdgeNext = (CSSEdge *)pSolid->GetHandleData(hEdgeNext);

				if (!pEdgeCur || !pEdgeNext)
				{
					return;
				}

				if ((nPass == 1) || (nPass == 2))
				{
					SSHANDLE hVertex = pSolid->GetConnectionVertex(pEdgeCur, pEdgeNext);

					if (!hVertex)
					{
						return;
					}

					CSSVertex *pVertex = (CSSVertex *)pSolid->GetHandleData(hVertex);

					if (!pVertex)
					{
						return;
					}

					Vector Vertex;
					pVertex->GetPosition(Vertex);
					meshBuilder.Position3f(Vertex[0], Vertex[1], Vertex[2]);
					meshBuilder.Color4ubv( color );
					meshBuilder.AdvanceVertex();
				}
				else
				{
					if (pSolid->ShowEdges())
					{
						//
						// Project the edge midpoint into screen space.
						//
						Vector CenterPoint;
						pEdgeCur->GetCenterPoint(CenterPoint);

						ViewMatrix.V3Mul( CenterPoint, ViewPos );

						if (ViewPos[2] < 0)
						{
							Vector2D ClientPos;
							pRender->TransformPoint(ClientPos, CenterPoint);

							pEdgeCur->m_bVisible = TRUE;
							pEdgeCur->m_r.left = ClientPos.x - HANDLE_RADIUS;
							pEdgeCur->m_r.top = ClientPos.y - HANDLE_RADIUS;
							pEdgeCur->m_r.right = ClientPos.x + HANDLE_RADIUS + 1;
							pEdgeCur->m_r.bottom = ClientPos.y + HANDLE_RADIUS + 1;

							if (pEdgeCur->m_bSelected)
							{
								color[0] = 220; color[1] = color[2] = 0; color[3] = 255;
							}
							else
							{
								color[0] = color[1] = 255; color[2] = 0; color[3] = 255;

							}

							//
							// Render the edge handle as a box.
							//
							pRender->SetHandleColor( color[0], color[1], color[2] );
							pRender->DrawHandle( CenterPoint );
						}
						else
						{
							pEdgeCur->m_bVisible = FALSE;
						}
					}

					if (pSolid->ShowVertices())
					{
						SSHANDLE hVertex = pSolid->GetConnectionVertex(pEdgeCur, pEdgeNext);
						CSSVertex *pVertex = (CSSVertex *)pSolid->GetHandleData(hVertex);

						//
						// Project the vertex into screen space.
						//
						Vector vPoint;

						pVertex->GetPosition(vPoint);

						ViewMatrix.V3Mul( vPoint, ViewPos );
						
						if (ViewPos[2] < 0)
						{
							Vector2D ClientPos;
							pRender->TransformPoint(ClientPos, vPoint);
							
							pVertex->m_bVisible = TRUE;
							pVertex->m_r.left = ClientPos.x - HANDLE_RADIUS;
							pVertex->m_r.top = ClientPos.y - HANDLE_RADIUS;
							pVertex->m_r.right = ClientPos.x + HANDLE_RADIUS + 1;
							pVertex->m_r.bottom = ClientPos.y + HANDLE_RADIUS + 1;

							if (pVertex->m_bSelected)
							{
								color[0] = 220; color[1] = color[2] = 0; color[3] = 255;
							}
							else
							{
								color[0] = color[1] = color[2] = 255; color[3] = 255;
							}

							//
							// Render the vertex as a box.
							//
							pRender->SetHandleColor( color[0], color[1], color[2] );
							pRender->DrawHandle( vPoint );
						}
						else
						{
							pVertex->m_bVisible = FALSE;
						}
					}
				}
			}

			if ((nPass == 1) || (nPass == 2))
			{
				meshBuilder.End();
				pMesh->Draw();
			}
		}

		if ( bClientSpace )
		{
			pRender->EndClientSpace();
			bClientSpace = false;
		}

		pRender->PopRenderMode();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Renders a our selection bounds while we are drag-selecting.
// Input  : pRender - Rendering interface.
//-----------------------------------------------------------------------------
void Morph3D::RenderTool3D(CRender3D *pRender)
{
	if (m_bBoxSelecting)
	{
		Box3D::RenderTool3D(pRender);
	}

	for( int pos=0; pos < m_StrucSolids.Count(); pos++ )
	{
		RenderSolid3D(pRender, m_StrucSolids[pos] );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles key down events in the 2D view.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Morph3D::OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	bool bSnap = m_pDocument->IsSnapEnabled() && !(GetKeyState(VK_CONTROL) & 0x8000);
	if (nChar == VK_UP || nChar == VK_DOWN || nChar == VK_LEFT || nChar == VK_RIGHT)
	{
		if ( NudgeHandles( pView, nChar, bSnap ) )
			return true;
	}

	switch (nChar)
	{
		case VK_RETURN:
		{
			if ( IsBoxSelecting() )
			{
				SelectInBox();
			}
			break;
		}

		case VK_ESCAPE:
		{
			OnEscape();
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles character events in the 2D view.
// Input  : Per CWnd::OnChar.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool Morph3D::OnChar2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Morph3D::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseDown2D(pView, nFlags, vPoint);
	
	m_bLButtonDownControlState = (nFlags & MK_CONTROL) != 0;

	m_vLastMouseMovement = vPoint;
	
	m_DragHandle.ssh = 0;

	MORPHHANDLE mh;
	if ( IsBoxSelecting() )
	{
		if ( HitTest( pView, vPoint, true ) )
		{
			Box3D::StartTranslation( pView, vPoint, m_LastHitTestHandle );
		}
	}
	else if (MorphHitTest( pView, vPoint, &mh))
	{
		//
		// If they clicked on a valid handle, remember which one. We may need it in
		// left button up or mouse move messages.
		//
		m_DragHandle = mh;

		if (!m_bLButtonDownControlState)
		{
			//
			// If they are not holding down control and they clicked on an unselected
			// handle, select the handle they clicked on straightaway.
			//
			if (!IsSelected(m_DragHandle))
			{
				// Clear the selected handles and select this handle.
				UINT cmd = scClear | scSelect;
				SelectHandle2D( pView, &m_DragHandle, cmd);
			}
		}
	}
	else
	{
		// Try to put another solid into morph mode.
		SelectAt(pView, nFlags, vPoint);
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Morph3D::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseUp2D(pView, nFlags, vPoint);

	if (!IsTranslating())
	{
		if (m_DragHandle.ssh != 0)
		{
			//
			// They clicked on a handle and released the left button without moving the mouse.
			// Change the selection state of the handle that was clicked on.
			//
			UINT cmd = scClear | scSelect;
			if (m_bLButtonDownControlState)
			{
				// Control-click: toggle.
				cmd = scToggle;
			}
			SelectHandle2D( pView, &m_DragHandle, cmd);
		}
	}
	else
	{
		//
		// Dragging out a selection box or dragging the selected vertices.
		//
		FinishTranslation(true);

		if (IsBoxSelecting() && Options.view2d.bAutoSelect)
		{
			SelectInBox();
		}
	}

	m_pDocument->UpdateStatusbar();
	
	return true;
}

unsigned int Morph3D::GetConstraints(unsigned int nFlags)
{
	unsigned int uConstraints = Tool3D::GetConstraints(nFlags);

	if ( !IsBoxSelecting() )
	{
		if ( nFlags & MK_CONTROL )
			uConstraints |= constrainOnlyVert;

		if ( nFlags & MK_SHIFT )
			uConstraints |= constrainOnlyHorz;
	}

	return uConstraints;
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse move events in the 2D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Morph3D::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	vgui::HCursor hCursor = vgui::dc_none;

	unsigned int uConstraints = GetConstraints( nFlags ); 

	Tool3D::OnMouseMove2D(pView, nFlags, vPoint);
					    
	// Convert to world coords.
	
	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);
	
	//
	// Update status bar position display.
	//
	char szBuf[128];
	m_pDocument->Snap(vecWorld,uConstraints);
	sprintf(szBuf, " @%.0f, %.0f ", vecWorld[pView->axHorz], vecWorld[pView->axVert] );
	SetStatusText(SBI_COORDS, szBuf);

	if ( m_bMouseDown[MOUSE_LEFT] )
	{
		if ( IsTranslating() )
		{
			// If they are dragging a selection box or one or more handles, update
			// the drag based on the cursor position.

			Tool3D::UpdateTranslation( pView, vPoint, uConstraints );
		}
		else if ( m_bMouseDragged[MOUSE_LEFT] && m_DragHandle.ssh != 0 )
		{
			//
			// If they are not already dragging a handle and they clicked on a valid handle,
			// see if they have moved the mouse far enough to begin dragging the handle.
			//
			
			if (m_bLButtonDownControlState && !IsSelected(m_DragHandle))
			{
				//
				// If they control-clicked on an unselected handle and then dragged the mouse,
				// select the handle that they clicked on now.
				//
				SelectHandle2D( pView, &m_DragHandle, scSelect);
			}

			StartTranslation( pView, m_vMouseStart[MOUSE_LEFT], &m_DragHandle );
		}
		else if ( m_bMouseDragged[MOUSE_LEFT] && !IsBoxSelecting() )
		{
			//
			// Left dragging, didn't click on a handle, and we aren't yet dragging a
			// selection box. Start dragging the selection box.
			//
			if (!(nFlags & MK_CONTROL))
			{
				SelectHandle(NULL, scClear);
			}

			Vector ptOrg;
			pView->ClientToWorld(ptOrg, m_vMouseStart[MOUSE_LEFT] );

			// set best third axis value
			ptOrg[pView->axThird] = COORD_NOTINIT;
			m_pDocument->GetBestVisiblePoint(ptOrg);
			StartBoxSelection( pView, m_vMouseStart[MOUSE_LEFT], ptOrg);
		}
	}
	else if (!IsEmpty())
	{
		//
		// Left button is not down, just see what's under the cursor
		// position to update the cursor.
		//

		hCursor = vgui::dc_arrow;

		//
		// Check to see if the mouse is over a vertex handle.
		//

		if (!IsBoxSelecting() && MorphHitTest( pView, vPoint, NULL))
		{
			hCursor = vgui::dc_crosshair;
		}
		//
		// Check to see if the mouse is over a box handle.
		//
		else if ( HitTest(pView, vPoint, true) )
		{
			hCursor = UpdateCursor( pView, m_LastHitTestHandle, m_TranslateMode );
		}
	}
	else
	{
		hCursor = vgui::dc_arrow;
	}

	if ( hCursor != vgui::dc_none  )
		pView->SetCursor( hCursor );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button down events in the 3D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Morph3D::OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	m_bHit = false;

	Tool3D::OnLMouseDown3D(pView, nFlags, vPoint);

	//
	// Select morph handles?
	//
	MORPHHANDLE mh;
	if ( MorphHitTest(pView, vPoint, &mh) )
	{
		m_bHit = true;
		m_DragHandle = mh;
		m_bMorphing = true;
		m_vLastMouseMovement = vPoint;
		m_bMovingSelected = false;	// not moving them yet - might just select this
		StartTranslation( pView, vPoint, &m_DragHandle );
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_CROSS));
	}
	else 
	{ 
		SelectAt( pView, nFlags, vPoint );
	}

	return true;
}

bool Morph3D::SelectAt( CMapView *pView, UINT nFlags, const Vector2D &vPoint )
{
	CMapClass *pMorphObject = NULL;
	bool bUpdateView = false;
	m_pDocument->GetSelection()->ClearHitList();
	CMapObjectList SelectList;

	// Find out how many (and what) map objects are under the point clicked on.

	HitInfo_t Objects[MAX_PICK_HITS];
	int nHits = pView->ObjectsAt( vPoint, Objects, sizeof(Objects) / sizeof(Objects[0]));
	
	// We now have an array of pointers to CMapAtoms. Any that can be upcast to CMapClass
	// we add to a list of hits.
	
	for (int i = 0; i < nHits; i++)
	{
		CMapClass *pMapClass = dynamic_cast <CMapClass *>(Objects[i].pObject);
		if (pMapClass != NULL)
		{
			SelectList.AddToTail(pMapClass);
		}
	}

	//
	// Actual selection occurs here.
	//
	if (!SelectList.Count())
	{
		//
		// Clicked on nothing - clear selection.
		//
		pView->GetMapDoc()->SelectFace(NULL, 0, scClear);
		pView->GetMapDoc()->SelectObject(NULL, scClear );
		return false;
	}

	bool bFirst = true;
	SelectMode_t eSelectMode = m_pDocument->GetSelection()->GetMode();

	// Can we de-select objects?
	if ( !CanDeselectList() )
		return true;

	FOR_EACH_OBJ( SelectList, pos )
	{
		CMapClass *pObject = SelectList.Element(pos);

		// get hit object type and add it to the hit list
		CMapClass *pHitObject = pObject->PrepareSelection( eSelectMode );
		if (pHitObject)
		{
			m_pDocument->GetSelection()->AddHit( pHitObject );
					
			if (bFirst)
			{
				if (pObject->IsMapClass(MAPCLASS_TYPE(CMapSolid)))
				{
					CMapSolid *pSolid = (CMapSolid *)pObject;
					
					UINT cmd = scClear | scSelect;
					if (nFlags & MK_CONTROL)
					{
						cmd = scToggle;
					}
					SelectObject(pSolid, cmd);
					pMorphObject = pSolid;
					bUpdateView = true;
					break;
				}
			}
		
			bFirst = false;
		}
	}

	// do we want to deselect all morphs?
	if (!pMorphObject && !IsEmpty())
	{
		SetEmpty();
		bUpdateView = true;
	}

	if (bUpdateView)
	{
		GetMainWnd()->pObjectProperties->MarkDataDirty();
		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_SELECTION );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button up events in the 3D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Morph3D::OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseUp3D(pView, nFlags, vPoint);

	if (m_bHit)
	{
		m_bHit = false;
		UINT cmd = scClear | scSelect;
		if (nFlags & MK_CONTROL)
		{
			cmd = scToggle;
		}

		SelectHandle(&m_DragHandle, cmd);
	}

	if (m_bMorphing)
	{
		FinishTranslation( true );
		m_bMorphing = false;
	}

	ReleaseCapture();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Snap the selected handles to the grid
// Input  : 
//-----------------------------------------------------------------------------
void Morph3D::SnapSelectedToGrid( int nGridSpacing )
{
	CUtlVector<MORPHHANDLE> vecHandles;
	
	if ( GetSelectedHandleCount() != 0 )
	{
		// Remember selected verts
		vecHandles.AddVectorToTail( m_SelectedHandles );
	}
	else
	{
		// None selected.  Do nothing
		return;
	}

	FOR_EACH_VEC( vecHandles, i )
	{
		// Set as sole-selection
		SelectHandle( &vecHandles[i], scSelect | scClear );

		// Get current position
		Vector vCurPos;
		SSHANDLEINFO hi;
		vecHandles[i].pStrucSolid->GetHandleInfo(&hi, vecHandles[i].ssh);
		vCurPos = hi.pos;

		// Get snapped position
		Vector vSnappedPos( V_rint(vCurPos[0] / nGridSpacing) * nGridSpacing,
							V_rint(vCurPos[1] / nGridSpacing) * nGridSpacing,
							V_rint(vCurPos[2] / nGridSpacing) * nGridSpacing );

		// Get delta to move original position into snapped position
		Vector vDelta = vSnappedPos - vCurPos;

		// Move!
		MoveSelectedHandles( vDelta );
	}

	// Re-select all the handles
	SelectHandle( NULL, scClear );
	FOR_EACH_VEC( vecHandles, i )
	{
		SelectHandle( &vecHandles[i], scSelect );
	}

	FinishTranslation( true );
	m_pDocument->SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : axes - 
//			nChar - 
//			bSnap - 
//-----------------------------------------------------------------------------
bool Morph3D::NudgeHandles(CMapView *pView, UINT nChar, bool bSnap)
{
	if ( GetSelectedHandleCount() < 1 || !Options.view2d.bNudge )
		return false;

	Vector vecDelta, vHorz, vVert, vThrd;
	pView->GetBestTransformPlane( vHorz, vVert, vThrd );
	m_pDocument->GetNudgeVector( vHorz, vVert,  nChar, bSnap, vecDelta);

	if ( bSnap && (GetSelectedHandleCount() == 1) && (GetSelectedType() == shtVertex))
	{
		// we have one vertex selected, so make sure
		// it's going to snap to grid.
		Vector pos;	GetSelectedCenter(pos);

		SetTransformationPlane( pos, vHorz, vVert, vThrd );

		// calculate new delta
		ProjectOnTranslationPlane( pos + vecDelta, vecDelta, constrainSnap );
		vecDelta -= pos;
	}

	MoveSelectedHandles(vecDelta);
	FinishTranslation( true );	// force checking for merges

	m_pDocument->SetModifiedFlag();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the key down event in the 3D view.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Morph3D::OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	bool bSnap = m_pDocument->IsSnapEnabled() && !(GetAsyncKeyState(VK_CONTROL) & 0x8000);

	switch (nChar)
	{	
		case VK_ESCAPE:
		{
			OnEscape();
			return true;
		}

		case VK_UP :
		case VK_DOWN :
		case VK_LEFT :
		case VK_RIGHT :
			{
				if ( NudgeHandles( pView, nChar, bSnap ) )
					return true;
			}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the escape key in the 2D or 3D views.
//-----------------------------------------------------------------------------
void Morph3D::OnEscape(void)
{
	//
	// If we're box selecting with the morph tool, stop.
	//
	if ( IsBoxSelecting() )
	{
		EndBoxSelection();
	}
	//
	// If we have handle(s) selected, deselect them.
	//
	else if (!IsEmpty() && (GetSelectedHandleCount() != 0))
	{
		SelectHandle(NULL, scClear);
	}
	//
	// Stop using the morph tool.
	//
	else
	{
		ToolManager()->SetTool(TOOL_POINTER);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles the move move event in the 3D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Morph3D::OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnMouseMove3D(pView, nFlags, vPoint);

	if (m_bMorphing)
	{
		//
		// Check distance moved since left button down and don't start
		// moving unless it's greater than the threshold.
		//
		if (!m_bMovingSelected)
		{
			Vector2D sizeMoved = vPoint - m_vLastMouseMovement;
			if ((abs(sizeMoved.x) > 3) || (abs(sizeMoved.y) > 3))
			{
				m_bMovingSelected = true;

				if (m_bHit)
				{
					m_bHit = false;
					SSHANDLEINFO hi;
					m_DragHandle.pStrucSolid->GetHandleInfo(&hi, m_DragHandle.ssh);
					unsigned uSelFlags = scSelect;

					if (!(nFlags & MK_CONTROL) && !hi.p2DHandle->m_bSelected)
					{
						uSelFlags |= scClear;
					}

					SelectHandle(&m_DragHandle, uSelFlags);
				}
			}
			else
			{
				return true;
			}
		}

		unsigned int uConstraints = GetConstraints( nFlags ); 

		Tool3D::UpdateTranslation( pView, vPoint, uConstraints );

		m_vLastMouseMovement = vPoint;
	}
	else if ( MorphHitTest(pView, vPoint, NULL ))
	{
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_CROSS));
	}
	else
	{
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
	}

	return true;
}