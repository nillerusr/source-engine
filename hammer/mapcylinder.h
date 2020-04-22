//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPCYLINDER_H
#define MAPCYLINDER_H

#ifdef _WIN32
#pragma once
#endif


#include "MapHelper.h"


class CRender3D;


#define MAX_KEYNAME_SIZE	32


class CMapCylinder : public CMapHelper
{
	public:

		DECLARE_MAPCLASS(CMapCylinder, CMapHelper)

		//
		// Factory for building from a list of string parameters.
		//
		static CMapClass *Create(CHelperInfo *pInfo, CMapEntity *pParent);

		//
		// Construction/destruction:
		//
		CMapCylinder(void);
		CMapCylinder(const char *pszStartValueKey, const char *pszStartKey, const char *pszStartRadiusKey,
			const char *pszEndValueKey, const char *pszEndKey, const char *pszEndRadiusKey );
		~CMapCylinder(void);

		void Initialize(void);

		void CalcBounds(BOOL bFullUpdate = FALSE);

		virtual CMapClass *Copy(bool bUpdateDependencies);
		virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

		void Render3D(CRender3D *pRender);
		void Render2D(CRender2D *pRender);

		int SerializeRMF(std::fstream &File, BOOL bRMF);
		int SerializeMAP(std::fstream &File, BOOL bRMF);

		bool IsVisualElement(void) { return(true); }
		bool IsCulledByCordon(const Vector &vecMins, const Vector &vecMaxs) { return false; } // We don't hide unless our parent hides.

		virtual CMapClass *PrepareSelection(SelectMode_t eSelectMode);
		
		const char* GetDescription() { return("Cylinder helper"); }

		void OnAddToWorld(CMapWorld *pWorld);
		void OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType);
		void OnParentKeyChanged( const char* key, const char* value );
		void OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren);

		virtual void UpdateDependencies(CMapWorld *pWorld, CMapClass *pObject);

	protected:
		void BuildCylinder(void);

		// Computes the vertices of the cylinder
		void ComputeCylinderPoints( int nCount, Vector *pStartVerts, Vector *pEndVerts );

		// How do we draw it?
		bool ShouldDrawAsLine();

		// Implements CMapAtom transformation functions.
		void DoTransform(const VMatrix &matrix);
		
		char m_szStartValueKey[80];		// The key in our parent entity to look at for our start target.
		char m_szStartKey[80];			// The value in our parent entity to look at for our start target.
		char m_szStartRadiusKey[80];		// The key in our parent entity to look at for the cylinder Radius

		char m_szEndValueKey[80];		// 
		char m_szEndKey[80];			// 
		char m_szEndRadiusKey[80];		// 

		CMapEntity *m_pStartEntity;		// Our start target.
		CMapEntity *m_pEndEntity;		// Our end target.
		float	m_flStartRadius;
		float	m_flEndRadius;
};


#endif // MAPCYLINDER_H
