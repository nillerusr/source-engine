//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPPOINTHANDLE_H
#define MAPPOINTHANDLE_H
#ifdef _WIN32
#pragma once
#endif

#include "MapHelper.h"
#include "ToolInterface.h"


class CHelperInfo;
class CRender2D;
class CRender3D;
class CMapPointHandle;


#define MAX_KEYNAME_SIZE	32

class CMapPointHandle : public CMapHelper
{

friend class CToolPointHandle;
friend class CMapAxisHandle;
friend class CMapSweptPlayerHull;

public:

	DECLARE_MAPCLASS(CMapPointHandle,CMapHelper)

	//
	// Factory for building from a list of string parameters.
	//
	static CMapClass *Create(CHelperInfo *pInfo, CMapEntity *pParent);

	inline int GetRadius(void);

	//
	// Construction/destruction:
	//
	CMapPointHandle(void);
	CMapPointHandle(const char *pszKey, bool bDrawLineToParent);
	~CMapPointHandle(void);

	void CalcBounds(BOOL bFullUpdate = FALSE);

	virtual CMapClass *Copy(bool bUpdateDependencies);
	virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

	virtual void Render2D(CRender2D *pRender);
	virtual void Render3D(CRender3D *pRender);

	virtual int SerializeRMF(std::fstream &File, BOOL bRMF);
	virtual int SerializeMAP(std::fstream &File, BOOL bRMF);

	// Overridden because origin helpers don't take the color of their parent entity.
	virtual void SetRenderColor(unsigned char red, unsigned char green, unsigned char blue);
	virtual void SetRenderColor(color32 rgbColor);

	virtual bool HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData);

	virtual bool IsVisualElement(void) { return(false); } // Only visible if our parent is selected.
	virtual bool IsClutter(void) { return true; }
	virtual bool IsCulledByCordon(const Vector &vecMins, const Vector &vecMaxs) { return false; } // We don't hide unless our parent hides.
	
	virtual const char* GetDescription() { return("Point helper"); }

	virtual void OnAddToWorld(CMapWorld *pWorld);
	virtual void OnParentKeyChanged(const char *key, const char *value);
	virtual void OnUndoRedo(void);

	virtual void PostloadWorld(CMapWorld *pWorld);

	virtual CBaseTool *GetToolObject(int nHitData, bool bAttachObject);

protected:

	// Called by the point handle tool while dragging.
	void UpdateOrigin(const Vector &vecOrigin);

	// Overridden to update our parent's keyvalue when we move.
	virtual void DoTransform(const VMatrix &matrix);
	
private:

	void Initialize(void);
	void UpdateParentKey(void);

	char m_szKeyName[MAX_KEYNAME_SIZE];
	bool m_bDrawLineToParent;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMapPointHandle::GetRadius(void)
{
	return HANDLE_RADIUS;
}


#endif // MAPPOINTHANDLE_H
