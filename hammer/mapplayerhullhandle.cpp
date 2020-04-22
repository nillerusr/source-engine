//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "GlobalFunctions.h"
#include "fgdlib/HelperInfo.h"
#include "materialsystem/imaterialsystem.h"
#include "MainFrm.h"			// For refreshing the object properties dialog
#include "MapDoc.h"
#include "MapPlayerHullHandle.h"
#include "MapSweptPlayerHull.h"
#include "MapView2D.h"
#include "Material.h"
#include "Options.h"
#include "ObjectProperties.h"	// For refreshing the object properties dialog
#include "Render2D.h"
#include "Render3D.h"
#include "StatusBarIDs.h"		// For updating status bar text
#include "ToolManager.h"
#include "vgui/Cursor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapPlayerHullHandle);


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapPlayerHullHandle from a set
//			of string parameters from the FGD file.
// Input  : pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapPlayerHullHandle::Create(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	static char *pszDefaultKeyName = "origin";

	bool bDrawLineToParent = !stricmp(pHelperInfo->GetName(), "vecline");

	const char *pszKey = pHelperInfo->GetParameter(0);
	if (pszKey == NULL)
	{
		pszKey = pszDefaultKeyName;
	}

	CMapPlayerHullHandle *pBox = new CMapPlayerHullHandle(pszKey, bDrawLineToParent);
	pBox->SetRenderColor(255, 255, 255);
	return(pBox);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pfMins - 
//			pfMaxs - 
//-----------------------------------------------------------------------------
CMapPlayerHullHandle::CMapPlayerHullHandle(void)
{
	Initialize();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszKey - 
//			bDrawLineToParent - 
//-----------------------------------------------------------------------------
CMapPlayerHullHandle::CMapPlayerHullHandle(const char *pszKey, bool bDrawLineToParent)
{
	Initialize();
	strcpy(m_szKeyName, pszKey);
	m_bDrawLineToParent = bDrawLineToParent;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::Initialize(void)
{
	m_szKeyName[0] = '\0';

	m_bDrawLineToParent = 0;

	r = 255;
	g = 255;
	b = 255;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapPlayerHullHandle::~CMapPlayerHullHandle(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Sets a backlink to our owner so we can call them in PrepareSelection.
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::Attach( CMapSweptPlayerHull *pOwner )
{
	m_pOwner = pOwner;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::CalcBounds(BOOL bFullUpdate)
{
	// We don't affect our parent's 2D render bounds.
	m_Render2DBox.ResetBounds();

	// Calculate 3D culling box.
	Vector Mins = m_Origin + Vector(-16, -16, -36);
	Vector Maxs = m_Origin + Vector(16, 16, 36);

	m_Render2DBox.UpdateBounds(Mins, Maxs);
	m_CullBox = m_Render2DBox;
	m_BoundingBox = m_CullBox;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CMapClass *CMapPlayerHullHandle::Copy(bool bUpdateDependencies)
{
	CMapPlayerHullHandle *pCopy = new CMapPlayerHullHandle;

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
// Output : 
//-----------------------------------------------------------------------------
CMapClass *CMapPlayerHullHandle::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	Assert(pObject->IsMapClass(MAPCLASS_TYPE(CMapPlayerHullHandle)));
	CMapPlayerHullHandle *pFrom = (CMapPlayerHullHandle *)pObject;

	CMapClass::CopyFrom(pObject, bUpdateDependencies);

	strcpy(m_szKeyName, pFrom->m_szKeyName);

	return(this);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			point - 
//			nData - 
// Output : 
//-----------------------------------------------------------------------------
bool CMapPlayerHullHandle::HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData)
{
	if ( IsVisible() )
	{
		Vector2D vecClient;
		pView->WorldToClient(vecClient, m_Origin);
		if (pView->CheckDistance(point, vecClient, HANDLE_RADIUS))
		{
			HitData.pObject = this;
			HitData.uData = 0;
			HitData.nDepth = 0; // // handles have no depth
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::Render2D(CRender2D *pRender)
{
	SelectionState_t eState = GetSelectionState();
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

	Vector vecMins, vecMaxs;
	GetRender2DBox(vecMins, vecMaxs);


	pRender->DrawBox( vecMins, vecMaxs );

	pRender->PopRenderMode();

	// Draw center handle.
	
	color32 rgbColor = GetRenderColor();
	
	pRender->SetDrawColor( rgbColor.r, rgbColor.g, rgbColor.b );

	if (eState == SELECT_NONE)
	{
		pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_CROSS );
		
	}
	else
	{
		pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_CIRCLE );
	}

	pRender->DrawHandle( (vecMins+vecMaxs)/2 );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::Render3D(CRender3D *pRender)
{
	if (GetSelectionState() == SELECT_NONE)
	{
		pRender->SetDrawColor( 200,180,0 );
	}
	else
	{
		pRender->SetDrawColor( 255,0,0 );
	}

	Vector Mins, Maxs;
	m_Render2DBox.GetBounds( Mins, Maxs );

	pRender->BeginRenderHitTarget(this);
	pRender->RenderBox( Mins, Maxs, 200, 180, 0, SELECT_NONE );
	pRender->EndRenderHitTarget();

	if ((m_pParent != NULL) && (m_bDrawLineToParent))
	{
		Vector vecOrigin;
		GetParent()->GetOrigin(vecOrigin);
		pRender->DrawLine( m_Origin, vecOrigin );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMapPlayerHullHandle::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMapPlayerHullHandle::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: Overridden because origin helpers don't take the color of their
//			parent entity.
// Input  : red, green, blue - 
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::SetRenderColor(unsigned char red, unsigned char green, unsigned char blue)
{
}


//-----------------------------------------------------------------------------
// Purpose: Overridden because origin helpers don't take the color of their
//			parent entity.
// Input  : red, green, blue - 
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::SetRenderColor(color32 rgbColor)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : szKey - 
//			szValue - 
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::OnParentKeyChanged(const char *szKey, const char *szValue)
{
	if (stricmp(szKey, m_szKeyName) == 0)
	{
		sscanf(szValue, "%f %f %f", &m_Origin.x, &m_Origin.y, &m_Origin.z);
		CalcBounds();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : vecOrigin - 
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::UpdateOrigin(const Vector &vecOrigin)
{
	m_Origin = vecOrigin;
	CalcBounds();
	UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::UpdateParentKey(void)
{
	// Snap to prevent error creep.
	for (int i = 0; i < 3; i++)
	{
		m_Origin[i] = V_rint(m_Origin[i] / 0.01f) * 0.01f;
	}

	if (m_szKeyName[0])
	{
		CMapEntity *pEntity = dynamic_cast <CMapEntity *> (m_pParent);
		if (pEntity != NULL)
		{
			char szValue[KEYVALUE_MAX_VALUE_LENGTH];
			sprintf(szValue, "%g %g %g", (double)m_Origin.x, (double)m_Origin.y, (double)m_Origin.z);
			pEntity->NotifyChildKeyChanged(this, m_szKeyName, szValue);

		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTransBox - 
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::DoTransform(const VMatrix &matrix)
{
	BaseClass::DoTransform(matrix);
	UpdateParentKey();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the keyvalue in our parent when we are	added to the world.
// Input  : pWorld - 
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::OnAddToWorld(CMapWorld *pWorld)
{
	BaseClass::OnAddToWorld(pWorld);
	UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: Called when we change because of an Undo or Redo.
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::OnUndoRedo(void)
{
	// We've changed but our parent entity may not have. Update our parent.
	UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: Sets the keyvalue in our parent after the map is loaded.
// Input  : pWorld - 
//-----------------------------------------------------------------------------
void CMapPlayerHullHandle::PostloadWorld(CMapWorld *pWorld)
{
	BaseClass::PostloadWorld(pWorld);
	UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the appropriate object to the selection code.
// Input  : dwFlags - selectPicky or selectNormal
// Output : Returns a pointer to the object that should be selected, based on
//			the selection mode.
//-----------------------------------------------------------------------------
CMapClass *CMapPlayerHullHandle::PrepareSelection(SelectMode_t eSelectMode)
{
	Assert(m_pOwner);
	return m_pOwner->PrepareSelection(eSelectMode);
}
