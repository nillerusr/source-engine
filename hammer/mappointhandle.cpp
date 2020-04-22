//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "GlobalFunctions.h"
#include "fgdlib/HelperInfo.h"
#include "materialsystem/imaterialsystem.h"
#include "MainFrm.h"			// For refreshing the object properties dialog
#include "MapDoc.h"
#include "MapPointHandle.h"
#include "MapView2D.h"
#include "Material.h"
#include "Options.h"
#include "ObjectProperties.h"	// For refreshing the object properties dialog
#include "Render2D.h"
#include "Render3D.h"
#include "StatusBarIDs.h"		// For updating status bar text
#include "ToolManager.h"
#include "ToolPointHandle.h"
#include "vgui/Cursor.h"
#include "camera.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapPointHandle);


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapPointHandle from a set
//			of string parameters from the FGD file.
// Input  : pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapPointHandle::Create(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	static char *pszDefaultKeyName = "origin";

	bool bDrawLineToParent = !stricmp(pHelperInfo->GetName(), "vecline");

	const char *pszKey = pHelperInfo->GetParameter(0);
	if (pszKey == NULL)
	{
		pszKey = pszDefaultKeyName;
	}

	CMapPointHandle *pBox = new CMapPointHandle(pszKey, bDrawLineToParent);
	pBox->SetRenderColor(255, 255, 255);
	return(pBox);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pfMins - 
//			pfMaxs - 
//-----------------------------------------------------------------------------
CMapPointHandle::CMapPointHandle(void)
{
	Initialize();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszKey - 
//			bDrawLineToParent - 
//-----------------------------------------------------------------------------
CMapPointHandle::CMapPointHandle(const char *pszKey, bool bDrawLineToParent)
{
	Initialize();
	strcpy(m_szKeyName, pszKey);
	m_bDrawLineToParent = bDrawLineToParent;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapPointHandle::Initialize(void)
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
CMapPointHandle::~CMapPointHandle(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapPointHandle::CalcBounds(BOOL bFullUpdate)
{
	// We don't affect our parent's 2D render bounds.
	m_Render2DBox.ResetBounds();

	// Calculate 3D culling box.
	Vector Mins = m_Origin + Vector(2, 2, 2);
	Vector Maxs = m_Origin + Vector(2, 2, 2);
	m_CullBox.SetBounds(Mins, Maxs);
	m_BoundingBox = m_CullBox;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CMapClass *CMapPointHandle::Copy(bool bUpdateDependencies)
{
	CMapPointHandle *pCopy = new CMapPointHandle;

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
CMapClass *CMapPointHandle::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	Assert(pObject->IsMapClass(MAPCLASS_TYPE(CMapPointHandle)));
	CMapPointHandle *pFrom = (CMapPointHandle *)pObject;

	CMapClass::CopyFrom(pObject, bUpdateDependencies);

	strcpy(m_szKeyName, pFrom->m_szKeyName);

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nHitData - 
//-----------------------------------------------------------------------------
CBaseTool *CMapPointHandle::GetToolObject(int nHitData, bool bAttachObject)
{
	CToolPointHandle *pTool = (CToolPointHandle *)ToolManager()->GetToolForID(TOOL_POINT_HANDLE);

	if ( bAttachObject )
		pTool->Attach(this);

	return pTool;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			point - 
//			nData - 
// Output : 
//-----------------------------------------------------------------------------
bool CMapPointHandle::HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData)
{
	if ( IsVisible() && IsSelected() )
	{
		Vector2D vecClient;
		pView->WorldToClient(vecClient, m_Origin);
		if (pView->CheckDistance(point, vecClient, HANDLE_RADIUS))
		{
			HitData.pObject = this;
			HitData.uData = 0;
			HitData.nDepth = 0; // handles have no depth
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapPointHandle::Render2D(CRender2D *pRender)
{
	SelectionState_t eState = GetSelectionState();

	if (eState == SELECT_NONE )
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

	pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_CIRCLE );
	pRender->DrawHandle( m_Origin );
	
	// Draw a line from origin helpers to their parent while they are being dragged.
	if ((m_pParent != NULL) && (m_bDrawLineToParent || (eState == SELECT_MODIFY)))
	{
		if (eState == SELECT_MODIFY)
		{
			pRender->SetDrawColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );
		}
		else
		{
			pRender->SetDrawColor( GetRValue(Options.colors.clrToolHandle), GetGValue(Options.colors.clrToolHandle), GetBValue(Options.colors.clrToolHandle) );
		}

		Vector vecOrigin;
		GetParent()->GetOrigin(vecOrigin);
		pRender->DrawLine(m_Origin, vecOrigin);
	}

	pRender->PopRenderMode();

	if (eState == SELECT_MODIFY)
	{
		Vector2D ptText;
		pRender->TransformPoint(ptText, m_Origin);

		ptText.y += HANDLE_RADIUS + 4;

		pRender->SetTextColor(GetRValue(Options.colors.clrToolHandle), GetGValue(Options.colors.clrToolHandle), GetBValue(Options.colors.clrToolHandle) );
		
		char szText[100];
		sprintf(szText, "(%0.f, %0.f, %0.f)", m_Origin.x, m_Origin.y, m_Origin.z);
		pRender->DrawText(szText, ptText.x, ptText.y, CRender2D::TEXT_JUSTIFY_LEFT);
	}

}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapPointHandle::Render3D(CRender3D *pRender)
{
	if (GetSelectionState() != SELECT_NONE)
	{
		Vector vecViewPoint;
		pRender->GetCamera()->GetViewPoint(vecViewPoint);
		float flDist = (m_Origin - vecViewPoint).Length();

		pRender->RenderSphere(m_Origin, 0.04 * flDist, 12, 12, 128, 128, 255);

		if ((m_pParent != NULL) && (m_bDrawLineToParent))
		{
			Vector vecOrigin;
			GetParent()->GetOrigin(vecOrigin);
			pRender->SetDrawColor( 255, 255, 255 );
			pRender->DrawLine( m_Origin, vecOrigin );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMapPointHandle::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMapPointHandle::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: Overridden because origin helpers don't take the color of their
//			parent entity.
// Input  : red, green, blue - 
//-----------------------------------------------------------------------------
void CMapPointHandle::SetRenderColor(unsigned char red, unsigned char green, unsigned char blue)
{
}


//-----------------------------------------------------------------------------
// Purpose: Overridden because origin helpers don't take the color of their
//			parent entity.
// Input  : red, green, blue - 
//-----------------------------------------------------------------------------
void CMapPointHandle::SetRenderColor(color32 rgbColor)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : szKey - 
//			szValue - 
//-----------------------------------------------------------------------------
void CMapPointHandle::OnParentKeyChanged(const char *szKey, const char *szValue)
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
void CMapPointHandle::UpdateOrigin(const Vector &vecOrigin)
{
	m_Origin = vecOrigin;
	CalcBounds();
	UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapPointHandle::UpdateParentKey(void)
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
void CMapPointHandle::DoTransform(const VMatrix &matrix)
{
	BaseClass::DoTransform(matrix);
	UpdateParentKey();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the keyvalue in our parent when we are	added to the world.
// Input  : pWorld - 
//-----------------------------------------------------------------------------
void CMapPointHandle::OnAddToWorld(CMapWorld *pWorld)
{
	BaseClass::OnAddToWorld(pWorld);
	UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: Called when we change because of an Undo or Redo.
//-----------------------------------------------------------------------------
void CMapPointHandle::OnUndoRedo(void)
{
	// We've changed but our parent entity may not have. Update our parent.
	UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: Sets the keyvalue in our parent after the map is loaded.
// Input  : pWorld - 
//-----------------------------------------------------------------------------
void CMapPointHandle::PostloadWorld(CMapWorld *pWorld)
{
	BaseClass::PostloadWorld(pWorld);
	UpdateParentKey();
}
