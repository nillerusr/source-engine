//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A helper that repesents a player hull swept through space between a
//			start and end point. It writes out both points as keyvalues to the entity.
//
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "GlobalFunctions.h"
#include "fgdlib/HelperInfo.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "MainFrm.h"			// For refreshing the object properties dialog
#include "MapDoc.h"
#include "MapSweptPlayerHull.h"
#include "MapPlayerHullHandle.h"
#include "MapPointHandle.h"
#include "MapView2D.h"
#include "Material.h"
#include "Options.h"
#include "ObjectProperties.h"	// For refreshing the object properties dialog
#include "Render2D.h"
#include "Render3D.h"
#include "ToolManager.h"
#include "ToolSweptHull.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapSweptPlayerHull);


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapSweptPlayerHull from a set
//			of string parameters from the FGD file.
// Input  : pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapSweptPlayerHull::Create(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	CMapSweptPlayerHull *pBox = new CMapSweptPlayerHull;
	pBox->SetRenderColor(255,255,255);
	return(pBox);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapSweptPlayerHull::CMapSweptPlayerHull(void)
{
	Initialize();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::Initialize(void)
{
	r = 255;
	g = 255;
	b = 255;

	m_Point[0] = new CMapPlayerHullHandle;
	m_Point[0]->Attach(this);

	m_Point[1] = new CMapPlayerHullHandle;
	m_Point[1]->Attach(this);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapSweptPlayerHull::~CMapSweptPlayerHull(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);

	Vector vecMins;
	Vector vecMaxs;

	m_Render2DBox.ResetBounds();
	m_CullBox.ResetBounds();
	for (int i = 0; i < 2; i++)
	{
		m_Point[i]->CalcBounds(bFullUpdate);
		m_Point[i]->GetCullBox(vecMins, vecMaxs);

		m_CullBox.UpdateBounds(vecMins, vecMaxs);
	}

	m_BoundingBox = m_CullBox;
	m_Render2DBox = m_CullBox;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapSweptPlayerHull::Copy(bool bUpdateDependencies)
{
	CMapSweptPlayerHull *pCopy = new CMapSweptPlayerHull;

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapSweptPlayerHull::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	Assert(pObject->IsMapClass(MAPCLASS_TYPE(CMapSweptPlayerHull)));
	CMapSweptPlayerHull *pFrom = (CMapSweptPlayerHull *)pObject;

	CMapClass::CopyFrom(pObject, bUpdateDependencies);

	m_Point[0]->CopyFrom(pFrom->m_Point[0], bUpdateDependencies);
	m_Point[1]->CopyFrom(pFrom->m_Point[1], bUpdateDependencies);

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: Gets the tool object for a given context data from HitTest2D.
//-----------------------------------------------------------------------------
CBaseTool *CMapSweptPlayerHull::GetToolObject(int nHitData, bool bAttachObject)
{
	// FIXME: ideally, we could use CToolPointHandle here, because all it does is move
	// points around, but that would require some way for the CMapSweptPlayerHull to know
	// when the CMapPointHandle's position changes. This way the CToolAxisHandle can
	// handle the notification. In general, we need a better system for building complex
	// objects from simple ones and handling changes to the simple objects in the complex one.
	//
	// If we DID use a CToolPointHandle, we'd need to reconcile the status bar updates that
	// are done in OnMouseMove2D, because points and axes cause different status bar text
	// to be displayed as they are dragged around.
	CToolSweptPlayerHull *pTool = (CToolSweptPlayerHull *)ToolManager()->GetToolForID(TOOL_SWEPT_HULL);

	if ( bAttachObject )
		pTool->Attach(this, nHitData);

	return pTool;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			point - 
//			nData - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapSweptPlayerHull::HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData)
{
	if (IsVisible())
	{
		for (unsigned int i = 0; i < 2; i++)
		{
			if ( m_Point[i]->HitTest2D(pView, point, HitData) )
			{
				HitData.pObject = this;
				HitData.uData = i;
				HitData.nDepth = 0;
				return true;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::Render2D(CRender2D *pRender)
{
	SelectionState_t eState = GetSelectionState();

	CMapView2D *pView = (CMapView2D*)pRender->GetView();

	m_Point[0]->Render2D(pRender);
	m_Point[1]->Render2D(pRender);

	Vector vecOrigin1;
	Vector vecOrigin2;
	m_Point[0]->GetOrigin(vecOrigin1);
	m_Point[1]->GetOrigin(vecOrigin2);

	Vector mins1, maxs1;
	Vector mins2, maxs2;
	m_Point[0]->m_CullBox.GetBounds( mins1, maxs1 );
	m_Point[1]->m_CullBox.GetBounds( mins2, maxs2 );

	// Draw swept volume
	Vector dir = vecOrigin2 - vecOrigin1;

	int nHorz = pView->axHorz;
	int nVert = pView->axVert;
	int nThird = pView->axThird;

	dir[ nThird ] = 0;

	VectorNormalize( dir );

	float dx = dir[ nHorz ];
	float dy = dir[ nVert ];

	if ( dx == 0 && dy == 0 )
		return;

	if (eState == SELECT_MODIFY)
	{
		pRender->PushRenderMode( RENDER_MODE_DOTTED );
		pRender->SetDrawColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );

	}
	else
	{
		pRender->PushRenderMode( RENDER_MODE_FLAT );
		pRender->SetDrawColor( GetRValue(Options.colors.clrToolHandle), GetGValue(Options.colors.clrToolHandle), GetBValue(Options.colors.clrToolHandle) );
	}

	Vector line1[2];
	Vector line2[2];
	
	line1[0].Init();
	line1[1].Init();
	line2[0].Init();
	line2[1].Init();

	if ( dx > 0 )
	{
		if ( dy > 0 )
		{
			line1[0][nHorz] = mins1[ nHorz ];
			line1[0][nVert] = maxs1[ nVert ];
			line1[1][nHorz] = mins2[ nHorz ];
			line1[1][nVert] = maxs2[ nVert ];

			line2[0][nHorz] = maxs1[ nHorz ];
			line2[0][nVert] = mins1[ nVert ];
			line2[1][nHorz] = maxs2[ nHorz ];
			line2[1][nVert] = mins2[ nVert ];
		}
		else
		{
			line1[0][nHorz] = maxs1[ nHorz ];
			line1[0][nVert] = maxs1[ nVert ];
			line1[1][nHorz] = maxs2[ nHorz ];
			line1[1][nVert] = maxs2[ nVert ];

			line2[0][nHorz] = mins1[ nHorz ];
			line2[0][nVert] = mins1[ nVert ];
			line2[1][nHorz] = mins2[ nHorz ];
			line2[1][nVert] = mins2[ nVert ];
		}
	}
	else
	{
		if ( dy > 0 )
		{
			line1[0][nHorz] = maxs1[ nHorz ];
			line1[0][nVert] = maxs1[ nVert ];
			line1[1][nHorz] = maxs2[ nHorz ];
			line1[1][nVert] = maxs2[ nVert ];

			line2[0][nHorz] = mins1[ nHorz ];
			line2[0][nVert] = mins1[ nVert ];
			line2[1][nHorz] = mins2[ nHorz ];
			line2[1][nVert] = mins2[ nVert ];
		}
		else
		{
			line1[0][nHorz] = mins1[ nHorz ];
			line1[0][nVert] = maxs1[ nVert ];
			line1[1][nHorz] = mins2[ nHorz ];
			line1[1][nVert] = maxs2[ nVert ];

			line2[0][nHorz] = maxs1[ nHorz ];
			line2[0][nVert] = mins1[ nVert ];
			line2[1][nHorz] = maxs2[ nHorz ];
			line2[1][nVert] = mins2[ nVert ];
		}
	}

	pRender->DrawLine( line1[0], line1[1] );
	pRender->DrawLine( line2[0], line2[1] );

	pRender->PopRenderMode();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::Render3D(CRender3D *pRender)
{
	for (int i = 0; i < 2; i++)
	{	
		m_Point[i]->Render3D(pRender);
	}	

	if (GetSelectionState() == SELECT_NONE)
	{
		pRender->SetDrawColor( 200,180,0 );
	}
	else
	{
		pRender->SetDrawColor( 255,0,0 );
	}

	Vector vec1;
	Vector vec2;
	m_Point[0]->GetOrigin(vec1);
	m_Point[1]->GetOrigin(vec2);
	
	pRender->DrawLine(vec1, vec2);
	
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMapSweptPlayerHull::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMapSweptPlayerHull::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: Overridden to chain down to our endpoints, which are not children.
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::SetOrigin(Vector &vecOrigin)
{
	BaseClass::SetOrigin(vecOrigin);

	m_Point[0]->SetOrigin(vecOrigin);
	m_Point[1]->SetOrigin(vecOrigin);
}


//-----------------------------------------------------------------------------
// Purpose: Overridden to chain down to our endpoints, which are not children.
//-----------------------------------------------------------------------------
SelectionState_t CMapSweptPlayerHull::SetSelectionState(SelectionState_t eSelectionState)
{
	SelectionState_t eState = BaseClass::SetSelectionState(eSelectionState);

	m_Point[0]->SetSelectionState(eSelectionState);
	m_Point[1]->SetSelectionState(eSelectionState);

	return eState;
}


//-----------------------------------------------------------------------------
// Purpose: Special version of set SelectionState to set the state in only one
//			endpoint handle for dragging that handle.
//-----------------------------------------------------------------------------
SelectionState_t CMapSweptPlayerHull::SetSelectionState(SelectionState_t eSelectionState, int nHandle)
{
	SelectionState_t eState = BaseClass::SetSelectionState(eSelectionState);
	m_Point[nHandle]->SetSelectionState(eSelectionState);
	return eState;
}


//-----------------------------------------------------------------------------
// Purpose: Overridden because origin helpers don't take the color of their
//			parent entity.
// Input  : red, green, blue - 
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::SetRenderColor(unsigned char red, unsigned char green, unsigned char blue)
{
}


//-----------------------------------------------------------------------------
// Purpose: Overridden because origin helpers don't take the color of their
//			parent entity.
// Input  : red, green, blue - 
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::SetRenderColor(color32 rgbColor)
{
}

static Vector playerFixup( 0, 0, 36 );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : szKey - 
//			szValue - 
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::OnParentKeyChanged(const char *szKey, const char *szValue)
{
	if (!stricmp(szKey, "point0"))
	{
		Vector vecOrigin;
		sscanf(szValue, "%f %f %f", &vecOrigin.x, &vecOrigin.y, &vecOrigin.z );

		vecOrigin += playerFixup;

		m_Point[0]->SetOrigin(vecOrigin);
		PostUpdate(Notify_Changed);
	}
	else if (!stricmp(szKey, "point1"))
	{
		Vector vecOrigin;
		sscanf(szValue, "%f %f %f", &vecOrigin.x, &vecOrigin.y, &vecOrigin.z );

		vecOrigin += playerFixup;

		m_Point[1]->SetOrigin(vecOrigin);
		PostUpdate(Notify_Changed);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called by the axis tool to update the position of the endpoint.
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::UpdateEndPoint(Vector &vecPos, int nPointIndex)
{
	m_Point[nPointIndex]->SetOrigin( vecPos );
	PostUpdate(Notify_Changed);
	UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: Overridden to transform our endpoints.
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::DoTransform(const VMatrix &matrix)
{
	BaseClass::DoTransform(matrix);

	m_Point[0]->Transform(matrix);
	m_Point[1]->Transform(matrix);

	UpdateParentKey();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::UpdateParentKey(void)
{
	CMapEntity *pEntity = dynamic_cast <CMapEntity *> (m_pParent);
	if (pEntity != NULL)
	{
		Vector vecOrigin1;
		Vector vecOrigin2;
		m_Point[0]->GetOrigin(vecOrigin1);
		m_Point[1]->GetOrigin(vecOrigin2);

		vecOrigin1 -= playerFixup;
		vecOrigin2 -= playerFixup;

		PostUpdate(Notify_Changed);

		char szValue[KEYVALUE_MAX_VALUE_LENGTH];
		sprintf(szValue, "%g %g %g", (double)vecOrigin1.x, (double)vecOrigin1.y, (double)vecOrigin1.z );
		pEntity->NotifyChildKeyChanged(this, "point0", szValue);
		pEntity->NotifyChildKeyChanged(this, "origin", szValue);

		sprintf(szValue, "%g %g %g", (double)vecOrigin2.x, (double)vecOrigin2.y, (double)vecOrigin2.z);
		pEntity->NotifyChildKeyChanged(this, "point1", szValue);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the keyvalue in our parent when we are	added to the world.
// Input  : pWorld - 
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::OnAddToWorld(CMapWorld *pWorld)
{
	BaseClass::OnAddToWorld(pWorld);
	UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: Sets the keyvalue in our parent after the map is loaded.
// Input  : pWorld - 
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::PostloadWorld(CMapWorld *pWorld)
{
	BaseClass::PostloadWorld(pWorld);
	UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the position of the given endpoint.
// Input  : vecPos - Receives the position.
//			nPointIndex - Endpoint index [0,1].
//-----------------------------------------------------------------------------
void CMapSweptPlayerHull::GetEndPoint(Vector &vecPos, int nPointIndex)
{
	m_Point[nPointIndex]->GetOrigin(vecPos);
}
