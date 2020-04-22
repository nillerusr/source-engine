//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MAPSWEPTPLAYERHULL_H
#define MAPSWEPTPLAYERHULL_H
#ifdef _WIN32
#pragma once
#endif

#include "MapClass.h"
#include "MapPointHandle.h"
#include "ToolInterface.h"
#include "MapEntity.h"


class CHelperInfo;
class CRender2D;
class CRender3D;
class CToolSweptPlayerHull;
class CMapPlayerHullHandle;


class CMapSweptPlayerHull : public CMapHelper
{
	friend CToolSweptPlayerHull;

public:

	DECLARE_MAPCLASS(CMapSweptPlayerHull, CMapHelper)

	//
	// Factory for building from a list of string parameters.
	//
	static CMapClass *Create(CHelperInfo *pInfo, CMapEntity *pParent);

	//
	// Construction/destruction:
	//
	CMapSweptPlayerHull();
	~CMapSweptPlayerHull();

	void GetEndPoint(Vector &vecPos, int nPointIndex);
	void UpdateEndPoint(Vector &vecPos, int nPointIndex);

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
	virtual CBaseTool *GetToolObject(int nHitData, bool bAttachObject );

	virtual bool IsVisualElement(void) { return true; }
	virtual bool IsClutter(void) { return false; }
	
	virtual const char* GetDescription() { return("Swept player hull helper"); }

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

	CMapPlayerHullHandle *m_Point[2];				// The two endpoints of the axis.
};


inline bool IsSweptHullClass(CMapEntity *pEntity)
{
	return (pEntity->GetChildOfType((CMapSweptPlayerHull *)NULL) != NULL);
}


#endif // MAPSWEPTPLAYERHULL_H
