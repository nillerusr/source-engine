//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A helper that represents the axis of rotation for a rotating entity.
//			When selected, it exposes handles for the endpoints of the axis.
//
//			It writes the axis as a keyvalue of the form:
//
//			"x0 y0 z0, x1 y1 z1"
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
#include "MapAxisHandle.h"
#include "MapPointHandle.h"
#include "MapView2D.h"
#include "Material.h"
#include "Options.h"
#include "Render2D.h"
#include "Render3D.h"
#include "ToolManager.h"
#include "ToolAxisHandle.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapAxisHandle);


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapAxisHandle from a set
//			of string parameters from the FGD file.
// Input  : pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapAxisHandle::Create(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	static char *pszDefaultKeyName = "axis";

	const char *pszKey = pHelperInfo->GetParameter(0);
	if (pszKey == NULL)
	{
		pszKey = pszDefaultKeyName;
	}

	CMapAxisHandle *pBox = new CMapAxisHandle(pszKey);
	pBox->SetRenderColor(255, 255, 255);
	return(pBox);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pfMins - 
//			pfMaxs - 
//-----------------------------------------------------------------------------
CMapAxisHandle::CMapAxisHandle(void)
{
	Initialize();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pfMins - 
//			pfMaxs - 
//-----------------------------------------------------------------------------
CMapAxisHandle::CMapAxisHandle(const char *pszKey)
{
	Initialize();
	strcpy(m_szKeyName, pszKey);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapAxisHandle::Initialize(void)
{
	m_szKeyName[0] = '\0';

	r = 255;
	g = 255;
	b = 255;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapAxisHandle::~CMapAxisHandle(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapAxisHandle::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);

	//
	// We don't affect our parent's 2D render bounds.
	//
	m_Render2DBox.ResetBounds();

	//
	// Calculate 3D culling box.
	//
	m_CullBox.ResetBounds();
	for (int i = 0; i < 2; i++)
	{
		m_Point[i].CalcBounds(bFullUpdate);

		Vector vecMins;
		Vector vecMaxs;
		m_Point[i].GetCullBox(vecMins, vecMaxs);
		m_CullBox.UpdateBounds(vecMins, vecMaxs);
	}
	m_BoundingBox = m_CullBox;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapAxisHandle::Copy(bool bUpdateDependencies)
{
	CMapAxisHandle *pCopy = new CMapAxisHandle;

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
CMapClass *CMapAxisHandle::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	Assert(pObject->IsMapClass(MAPCLASS_TYPE(CMapAxisHandle)));
	CMapAxisHandle *pFrom = (CMapAxisHandle *)pObject;

	CMapClass::CopyFrom(pObject, bUpdateDependencies);

	m_Point[0].CopyFrom(&pFrom->m_Point[0], bUpdateDependencies);
	m_Point[1].CopyFrom(&pFrom->m_Point[1], bUpdateDependencies);

	strcpy(m_szKeyName, pFrom->m_szKeyName);

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: Gets the tool object for a given context data from HitTest2D.
//-----------------------------------------------------------------------------
CBaseTool *CMapAxisHandle::GetToolObject(int nHitData, bool bAttachObject)
{
	// FIXME: ideally, we could use CToolPointHandle here, because all it does is move
	// points around, but that would require some way for the CMapAxisHandle to know
	// when the CMapPointHandle's position changes. This way the CToolAxisHandle can
	// handle the notification. In general, we need a better system for building complex
	// objects from simple ones and handling changes to the simple objects in the complex one.
	//
	// If we DID use a CToolPointHandle, we'd need to reconcile the status bar updates that
	// are done in OnMouseMove2D, because points and axes cause different status bar text
	// to be displayed as they are dragged around.
	CToolAxisHandle *pTool = (CToolAxisHandle *)ToolManager()->GetToolForID(TOOL_AXIS_HANDLE);

	if ( bAttachObject )
	{
		pTool->Attach(this, nHitData);
	}

	return pTool;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			point - 
//			nData - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapAxisHandle::HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData)
{
	if ( IsVisible() )
	{
		for (int i = 0; i < 2; i++)
		{
			if ( m_Point[i].HitTest2D(pView, point, HitData) )
			{
				HitData.pObject = this;
				HitData.uData = i;
				HitData.nDepth = 0; // map helpers have no real depth

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
void CMapAxisHandle::Render2D(CRender2D *pRender)
{
	SelectionState_t eState = GetSelectionState();
	if (eState == SELECT_NONE)
		return;
	
	m_Point[0].Render2D(pRender);
	m_Point[1].Render2D(pRender);

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

	Vector vecOrigin1;
	Vector vecOrigin2;
	m_Point[0].GetOrigin(vecOrigin1);
	m_Point[1].GetOrigin(vecOrigin2);

	pRender->DrawLine(vecOrigin1, vecOrigin2);
	
	pRender->PopRenderMode();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapAxisHandle::Render3D(CRender3D *pRender)
{
	if (GetSelectionState() != SELECT_NONE)
	{
		for (int i = 0; i < 2; i++)
		{	
			m_Point[i].Render3D(pRender);
		}	

		Vector vec1;
		Vector vec2;
		m_Point[0].GetOrigin(vec1);
		m_Point[1].GetOrigin(vec2);
		
		pRender->SetDrawColor( 255, 255, 255 );
		pRender->DrawLine(vec1, vec2);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMapAxisHandle::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMapAxisHandle::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: Overridden to chain down to our endpoints, which are not children.
//-----------------------------------------------------------------------------
void CMapAxisHandle::SetOrigin(Vector &vecOrigin)
{
	BaseClass::SetOrigin(vecOrigin);

	m_Point[0].SetOrigin(vecOrigin);
	m_Point[1].SetOrigin(vecOrigin);
}


//-----------------------------------------------------------------------------
// Purpose: Overridden to chain down to our endpoints, which are not children.
//-----------------------------------------------------------------------------
SelectionState_t CMapAxisHandle::SetSelectionState(SelectionState_t eSelectionState)
{
	SelectionState_t eState = BaseClass::SetSelectionState(eSelectionState);

	m_Point[0].SetSelectionState(eSelectionState);
	m_Point[1].SetSelectionState(eSelectionState);

	return eState;
}


//-----------------------------------------------------------------------------
// Purpose: Special version of set SelectionState to set the state in only one
//			endpoint handle for dragging that handle.
//-----------------------------------------------------------------------------
SelectionState_t CMapAxisHandle::SetSelectionState(SelectionState_t eSelectionState, int nHandle)
{
	SelectionState_t eState = BaseClass::SetSelectionState(eSelectionState);
	m_Point[nHandle].SetSelectionState(eSelectionState);
	return eState;
}


//-----------------------------------------------------------------------------
// Purpose: Overridden because origin helpers don't take the color of their
//			parent entity.
// Input  : red, green, blue - 
//-----------------------------------------------------------------------------
void CMapAxisHandle::SetRenderColor(unsigned char red, unsigned char green, unsigned char blue)
{
}


//-----------------------------------------------------------------------------
// Purpose: Overridden because origin helpers don't take the color of their
//			parent entity.
// Input  : red, green, blue - 
//-----------------------------------------------------------------------------
void CMapAxisHandle::SetRenderColor(color32 rgbColor)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : szKey - 
//			szValue - 
//-----------------------------------------------------------------------------
void CMapAxisHandle::OnParentKeyChanged(const char *szKey, const char *szValue)
{
	if (!stricmp(szKey, m_szKeyName))
	{
		Vector vecOrigin1;
		Vector vecOrigin2;

		sscanf(szValue, "%f %f %f, %f %f %f", &vecOrigin1.x, &vecOrigin1.y, &vecOrigin1.z,  &vecOrigin2.x, &vecOrigin2.y, &vecOrigin2.z);

		m_Point[0].SetOrigin(vecOrigin1);
		m_Point[1].SetOrigin(vecOrigin2);

		CalcBounds();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called by the axis tool to update the position of the endpoint.
//-----------------------------------------------------------------------------
void CMapAxisHandle::UpdateEndPoint(Vector &vecPos, int nPointIndex)
{
	m_Point[nPointIndex].m_Origin = vecPos;
	CalcBounds();
	UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: Overridden to transform our endpoints.
//-----------------------------------------------------------------------------
void CMapAxisHandle::DoTransform(const VMatrix &matrix)
{
	BaseClass::DoTransform(matrix);

	m_Point[0].Transform(matrix);
	m_Point[1].Transform(matrix);

	UpdateParentKey();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapAxisHandle::UpdateParentKey(void)
{
	if (m_szKeyName[0])
	{
		CMapEntity *pEntity = dynamic_cast <CMapEntity *> (m_pParent);
		if (pEntity != NULL)
		{
			Vector vecOrigin1;
			Vector vecOrigin2;
			m_Point[0].GetOrigin(vecOrigin1);
			m_Point[1].GetOrigin(vecOrigin2);

			CalcBounds();

			char szValue[KEYVALUE_MAX_VALUE_LENGTH];
			sprintf(szValue, "%g %g %g, %g %g %g", (double)vecOrigin1.x, (double)vecOrigin1.y, (double)vecOrigin1.z, (double)vecOrigin2.x, (double)vecOrigin2.y, (double)vecOrigin2.z);
			pEntity->NotifyChildKeyChanged(this, m_szKeyName, szValue);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the keyvalue in our parent when we are	added to the world.
// Input  : pWorld - 
//-----------------------------------------------------------------------------
void CMapAxisHandle::OnAddToWorld(CMapWorld *pWorld)
{
	BaseClass::OnAddToWorld(pWorld);
	UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: Sets the keyvalue in our parent after the map is loaded.
// Input  : pWorld - 
//-----------------------------------------------------------------------------
void CMapAxisHandle::PostloadWorld(CMapWorld *pWorld)
{
	BaseClass::PostloadWorld(pWorld);
	UpdateParentKey();
}

