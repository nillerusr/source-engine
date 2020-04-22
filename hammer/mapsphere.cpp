//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a sphere helper for entities that have a radius of effect.
//			Renders only when the parent entity is selected.
//
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "fgdlib/HelperInfo.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "MapDoc.h"
#include "MapSphere.h"
#include "MapView2D.h"
#include "Material.h"
#include "mathlib/MathLib.h"
#include "Render2D.h"
#include "Render3D.h"
#include "ToolManager.h"
#include "ToolSphere.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapSphere)


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapSphere helper from a
//			set of string parameters from the FGD file.
// Input  : pInfo - Pointer to helper info class which gives us information
//				about how to create the helper.
// Output : Returns a pointer to the helper, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapSphere::Create(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	CMapSphere *pSphere = new CMapSphere;
	if (pSphere != NULL)
	{
		//
		// The first parameter should be the key name to represent. If it isn't
		// there we assume "radius".
		//
		const char *pszKeyName = pHelperInfo->GetParameter(0);
		if (pszKeyName != NULL)
		{
			strcpy(pSphere->m_szKeyName, pszKeyName);
		}
		else
		{
			strcpy(pSphere->m_szKeyName, "radius");
		}

		//
		// Extract the line color from the parameter list.
		//
		unsigned char chRed = 255;
		unsigned char chGreen = 255;
		unsigned char chBlue = 255;

		const char *pszParam = pHelperInfo->GetParameter(1);
		if (pszParam != NULL)
		{
			chRed = atoi(pszParam);
		}

		pszParam = pHelperInfo->GetParameter(2);
		if (pszParam != NULL)
		{
			chGreen = atoi(pszParam);
		}

		pszParam = pHelperInfo->GetParameter(3);
		if (pszParam != NULL)
		{
			chBlue = atoi(pszParam);
		}

		pSphere->SetRenderColor(chRed, chGreen, chBlue);
	}

	return pSphere;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CMapSphere::CMapSphere(void)
{
	m_szKeyName[0] = '\0';

	m_flRadius = 0;

	r = 255;
	g = 255;
	b = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapSphere::~CMapSphere(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapSphere::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);

	//
	// Pretend we're a point so that we don't change our parent entity bounds
	// in the 2D view.
	//
	m_Render2DBox.ResetBounds();
	m_Render2DBox.UpdateBounds(m_Origin);

	//
	// Build our bounds for frustum culling in the 3D views.
	//
	m_CullBox.ResetBounds();
	Vector mins = m_Origin - Vector(m_flRadius, m_flRadius, m_flRadius);
	Vector maxs = m_Origin + Vector(m_flRadius, m_flRadius, m_flRadius);
	m_CullBox.UpdateBounds(mins, maxs);

	m_BoundingBox.ResetBounds(); // we don't want to use the bounds of the sphere for our bounding box
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapSphere::Copy(bool bUpdateDependencies)
{
	CMapSphere *pCopy = new CMapSphere;

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: Makes this an exact duplicate of pObject.
// Input  : pObject - Object to copy.
// Output : Returns this.
//-----------------------------------------------------------------------------
CMapClass *CMapSphere::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	Assert(pObject->IsMapClass(MAPCLASS_TYPE(CMapSphere)));
	CMapSphere *pFrom = (CMapSphere *)pObject;

	CMapClass::CopyFrom(pObject, bUpdateDependencies);

	m_flRadius = pFrom->m_flRadius;
	strcpy(m_szKeyName, pFrom->m_szKeyName);

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: Sets the radius of the sphere helper and updates our parent
//			entity's keyvalue.
//-----------------------------------------------------------------------------
void CMapSphere::SetRadius(float flRadius)
{
	m_flRadius = V_rint(flRadius);

	CMapEntity *pEntity = dynamic_cast <CMapEntity *>(m_pParent);
	if (pEntity != NULL)
	{
		char szValue[80];
		sprintf(szValue, "%g", m_flRadius);
		pEntity->NotifyChildKeyChanged(this, m_szKeyName, szValue);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets the tool object for a given context data from HitTest2D.
//-----------------------------------------------------------------------------
CBaseTool *CMapSphere::GetToolObject(int nHitData, bool bAttachObject)
{
	CToolSphere *pTool = (CToolSphere *)ToolManager()->GetToolForID(TOOL_SPHERE);

	if ( bAttachObject )
		pTool->Attach(this);

	return pTool;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			point - point in client coordinates
// Output : 
//-----------------------------------------------------------------------------
bool CMapSphere::HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData)
{
	if (!IsVisible() || m_flRadius <= 0 || !IsSelected() )
	{
		return NULL;
	}

	Vector2D vecClientOrigin;
	pView->WorldToClient(vecClientOrigin, m_Origin);

	Vector vecRadius = m_Origin;
	vecRadius[pView->axHorz] += m_flRadius;

	Vector2D vecClientRadius;
	pView->WorldToClient(vecClientRadius, vecRadius);

	int nRadius = abs(vecClientRadius.x - vecClientOrigin.x);

	vecClientRadius.x = nRadius;
	vecClientRadius.y = nRadius;

	HitData.pObject = this;
	HitData.nDepth = 0; // handles have no depth
	HitData.uData = nRadius;

	Vector2D vecClientMin = vecClientOrigin - vecClientRadius;
	Vector2D vecClientMax = vecClientOrigin + vecClientRadius;

	//
	// Check the four resize handles.
	//
	Vector2D vecTemp(vecClientOrigin.x, vecClientMin.y - HANDLE_OFFSET);

	if (pView->CheckDistance(point, vecTemp, 6))
	{
		// Top handle
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENS));
		return true;
	}

	vecTemp.x = vecClientOrigin.x;
	vecTemp.y = vecClientMax.y + HANDLE_OFFSET;
	if (pView->CheckDistance(point, vecTemp, HANDLE_RADIUS))
	{
		// Bottom handle
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENS));
		return true;
	}

	vecTemp.x = vecClientMin.x - HANDLE_OFFSET;
	vecTemp.y = vecClientOrigin.y;
	if (pView->CheckDistance(point, vecTemp, HANDLE_RADIUS))
	{
		// Left handle
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
		return true;
	}

	vecTemp.x = vecClientMax.x + HANDLE_OFFSET;
	vecTemp.y = vecClientOrigin.y;
	if (pView->CheckDistance(point, vecTemp, HANDLE_RADIUS))
	{
		// Right handle
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
		return true;
	}

	HitData.pObject = NULL;
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Notifies that this object's parent entity has had a key value change.
// Input  : szKey - The key that changed.
//			szValue - The new value of the key.
//-----------------------------------------------------------------------------
void CMapSphere::OnParentKeyChanged(const char *szKey, const char *szValue)
{
	if (!stricmp(szKey, m_szKeyName))
	{
		m_flRadius = atof(szValue);
		PostUpdate(Notify_Changed);
	}
}




//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapSphere::Render2D(CRender2D *pRender)
{
	if (m_pParent->IsSelected() && (m_flRadius > 0) )
	{
		pRender->SetDrawColor( 255, 255, 0 );

		Vector2D ptClientRadius;
		pRender->TransformNormal(ptClientRadius, Vector(m_flRadius,m_flRadius,m_flRadius) );

		int radius = ptClientRadius.x;
        
		pRender->DrawCircle( m_Origin, m_flRadius );

		bool bPopMode = pRender->BeginClientSpace();
		
		//
		// Draw the four resize handles.
		//
		pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_SQUARE );
		pRender->SetHandleColor( 255,255,255 );

		Vector2D offset;
		offset.x = 0;
		offset.y = -(radius + HANDLE_OFFSET);
		pRender->DrawHandle( m_Origin, &offset );

		offset.x = 0;
		offset.y = radius + HANDLE_OFFSET;
		pRender->DrawHandle( m_Origin, &offset );

		offset.x = -(radius + HANDLE_OFFSET);
		offset.y = 0;
		pRender->DrawHandle( m_Origin, &offset );

		offset.x = radius + HANDLE_OFFSET;
		offset.y = 0;
		pRender->DrawHandle( m_Origin, &offset );

		if ( bPopMode )
			pRender->EndClientSpace();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Renders the wireframe sphere.
// Input  : pRender - Interface to renderer.
//-----------------------------------------------------------------------------
void CMapSphere::Render3D(CRender3D *pRender)
{
	if (m_pParent->IsSelected() && (m_flRadius > 0))
	{
		pRender->RenderWireframeSphere(m_Origin, m_flRadius, 12, 12, 255, 255, 0);
	}
}


