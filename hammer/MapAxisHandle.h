//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MAPAXISHANDLE_H
#define MAPAXISHANDLE_H
#ifdef _WIN32
#pragma once
#endif

#include "MapClass.h"
#include "MapPointHandle.h"
#include "ToolInterface.h"
#include "MapPointHandle.h"
#include "mapview.h"


class CHelperInfo;
class CRender2D;
class CRender3D;
class CToolAxisHandle;


#define MAX_KEYNAME_SIZE	32


class CMapAxisHandle : public CMapHelper
{
	friend CToolAxisHandle;

public:

	DECLARE_MAPCLASS(CMapAxisHandle, CMapHelper)

	//
	// Factory for building from a list of string parameters.
	//
	static CMapClass *Create(CHelperInfo *pInfo, CMapEntity *pParent);

	//
	// Construction/destruction:
	//
	CMapAxisHandle();
	CMapAxisHandle(const char *pszKey);
	~CMapAxisHandle();

	inline void GetEndPoint(Vector &vecPos, int nPointIndex);
	void UpdateEndPoint(Vector &vecPos, int nPointIndex);
	inline int GetEndPointRadius();

	//
	// CMapClass implementation.
	//
	void CalcBounds(BOOL bFullUpdate = FALSE);

	virtual CMapClass *Copy(bool bUpdateDependencies);
	virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

	virtual void Render2D(CRender2D *pRender);

	virtual int SerializeRMF(std::fstream &File, BOOL bRMF);
	virtual int SerializeMAP(std::fstream &File, BOOL bRMF);

	// Overridden to chain down to our endpoints, which are not children.
	void SetOrigin(Vector &vecOrigin);

	// Overridden to chain down to our endpoints, which are not children.
	virtual SelectionState_t SetSelectionState(SelectionState_t eSelectionState);

	// Overridden because axis helpers don't take the color of their parent entity.
	virtual void SetRenderColor(unsigned char red, unsigned char green, unsigned char blue);
	virtual void SetRenderColor(color32 rgbColor);

	virtual bool HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData);
	virtual CBaseTool *GetToolObject(int nHitData, bool bAttachObject);

	virtual bool IsVisualElement(void) { return(false); } // Only visible if our parent is selected.
	virtual bool IsClutter(void) { return true; }
	virtual bool IsCulledByCordon(const Vector &vecMins, const Vector &vecMaxs) { return false; } // We don't hide unless our parent hides.
	
	virtual const char* GetDescription() { return("Axis helper"); }

	virtual void OnAddToWorld(CMapWorld *pWorld);
	virtual void OnParentKeyChanged(const char *key, const char *value);

	virtual void PostloadWorld(CMapWorld *pWorld);

	virtual void Render3D(CRender3D *pRender);

protected:

	SelectionState_t SetSelectionState(SelectionState_t eSelectionState, int nHandle);

	void UpdateParentKey(void);

	// Overriden to transform our endpoints, which are not children.
	virtual void DoTransform(const VMatrix &matrix);
	
	void Initialize(void);

	CMapPointHandle m_Point[2];				// The two endpoints of the axis.

	char m_szKeyName[MAX_KEYNAME_SIZE];
};


//-----------------------------------------------------------------------------
// Purpose: Returns the position of the given endpoint.
// Input  : vecPos - Receives the position.
//			nPointIndex - Endpoint index [0,1].
//-----------------------------------------------------------------------------
inline void CMapAxisHandle::GetEndPoint(Vector &vecPos, int nPointIndex)
{
	m_Point[nPointIndex].GetOrigin(vecPos);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the radius to use in rendering the endpoints.
//-----------------------------------------------------------------------------
inline int CMapAxisHandle::GetEndPointRadius()
{
	return m_Point[0].GetRadius();
}


#endif // MAPAXISHANDLE_H
