//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MAPSPHERE_H
#define MAPSPHERE_H
#ifdef _WIN32
#pragma once
#endif

#include "fgdlib/WCKeyValues.h"
#include "MapHelper.h"
#include "ToolInterface.h"


class CToolSphere;
class CHelperInfo;
class CRender2D;
class CRender3D;
class IMesh;


class CMapSphere : public CMapHelper
{
	friend class CToolSphere;

	public:

		DECLARE_MAPCLASS(CMapSphere,CMapHelper)

		//
		// Factory for building from a list of string parameters.
		//
		static CMapClass *Create(CHelperInfo *pInfo, CMapEntity *pParent);

		//
		// Construction/destruction:
		//
		CMapSphere(void);
		~CMapSphere(void);

		virtual void CalcBounds(BOOL bFullUpdate = FALSE);

		virtual CMapClass *Copy(bool bUpdateDependencies);
		virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

		virtual void OnParentKeyChanged(const char *szKey, const char *szValue);

		virtual void Render2D(CRender2D *pRender);
		virtual void Render3D(CRender3D *pRender);

		virtual int SerializeRMF(std::fstream &File, BOOL bRMF) { return(0); }
		virtual int SerializeMAP(std::fstream &File, BOOL bRMF) { return(0); }

		virtual bool IsVisualElement(void) { return false; } // Only visible when the parent entity is selected.
		virtual bool IsScaleable(void) { return false; } // TODO: allow for scaling the sphere by itself
		virtual bool IsClutter(void) { return true; }
		virtual bool IsCulledByCordon(const Vector &vecMins, const Vector &vecMaxs) { return false; } // We don't hide unless our parent hides.

		virtual CBaseTool *GetToolObject(int nHitData, bool bAttachObject );
		
		virtual bool HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData);

		virtual const char* GetDescription() { return "Sphere helper"; }

	protected:

		void SetRadius(float flRadius);

		char m_szKeyName[KEYVALUE_MAX_KEY_LENGTH];
		float m_flRadius;
};

#endif // MAPSPHERE_H
