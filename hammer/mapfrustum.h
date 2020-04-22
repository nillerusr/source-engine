//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MAPFRUSTUM_H
#define MAPFRUSTUM_H
#ifdef _WIN32
#pragma once
#endif


#include "MapHelper.h"
#include "MapFace.h"
#include "fgdlib/WCKeyValues.h"


class CHelperInfo;
class CRender3D;


class CMapFrustum : public CMapHelper
{
public:

	DECLARE_MAPCLASS(CMapFrustum,CMapHelper);
	
	//
	// Factory for building from a list of string parameters.
	//
	static CMapClass *Create(CHelperInfo *pInfo, CMapEntity *pParent);
	
	//
	// Construction/destruction:
	//
	CMapFrustum(void);
	~CMapFrustum(void);

	void BuildFrustumFaces();
	void CalcBounds(BOOL bFullUpdate = FALSE);

	virtual CMapClass *Copy(bool bUpdateDependencies);
	virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

	void Render3D(CRender3D *pRender);

	int SerializeRMF(std::fstream &File, BOOL bRMF);
	int SerializeMAP(std::fstream &File, BOOL bRMF);

	virtual void PostloadWorld(CMapWorld *pWorld);

	virtual bool IsVisualElement(void) { return(false); } // Only visible when parent entity is selected.
	virtual bool IsClutter(void) { return true; }
	virtual bool IsCulledByCordon(const Vector &vecMins, const Vector &vecMaxs) { return false; } // We don't hide unless our parent hides.

	const char* GetDescription() { return("Frustum helper"); }

	void OnParentKeyChanged( const char* key, const char* value );
	bool ShouldRenderLast(void) { return(true); }
	void GetAngles(QAngle& fAngles);


private:

	CMapFace* CreateMapFace( const Vector &v1, const Vector &v2, const Vector &v3, const Vector &v4, float flAlpha );


protected:

	CMapFaceList m_Faces;

	float m_flFOV;
	float m_flNearPlane;
	float m_flFarPlane;
	float m_flPitchScale;
	float m_fBrightness;

	QAngle m_Angles;	
	char m_szFOVKeyName[KEYVALUE_MAX_KEY_LENGTH];
	char m_szColorKeyName[KEYVALUE_MAX_KEY_LENGTH];
	char m_szNearPlaneKeyName[KEYVALUE_MAX_KEY_LENGTH];
	char m_szFarPlaneKeyName[KEYVALUE_MAX_KEY_LENGTH];
};

#endif // MAPFRUSTUM_H
