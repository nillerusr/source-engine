//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPALIGNEDBOX_H
#define MAPALIGNEDBOX_H
#pragma once


#include "MapHelper.h"


class CHelperInfo;
class CRender2D;
class CRender3D;


#define MAX_KEYNAME_SIZE	32


class CMapAlignedBox : public CMapHelper
{
	public:

		DECLARE_MAPCLASS(CMapAlignedBox,CMapHelper)

		//
		// Factory for building from a list of string parameters.
		//
		static CMapClass *Create(CHelperInfo *pInfo, CMapEntity *pParent);

		//
		// Construction/destruction:
		//
		CMapAlignedBox(void);
		CMapAlignedBox(Vector& pfMins, Vector& pfMaxs);
		CMapAlignedBox(const char *pMinsKeyName, const char *pMaxsKeyName);
		~CMapAlignedBox(void);

		virtual void CalcBounds(BOOL bFullUpdate = FALSE);

		virtual CMapClass *Copy(bool bUpdateDependencies);
		virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

		virtual void Render2D(CRender2D *pRender);
		virtual void Render3D(CRender3D *pRender);

		int SerializeRMF(std::fstream &File, BOOL bRMF);
		int SerializeMAP(std::fstream &File, BOOL bRMF);

		bool IsVisualElement(void) { return !m_bWireframe; }
		
		const char* GetDescription() { return("Aligned box"); }

		virtual void OnParentKeyChanged( const char* key, const char* value );

	protected:

		//void UpdateBox(const Vector &vecMins, const Vector &vecMaxs);

		bool m_bWireframe;	// Whether we render as a wireframe box when the entity is selected.

		// Determines whether we use the key names to get the box size
		// or m_fMins/m_fMaxs.
		bool	m_bUseKeyName;

		char	m_MinsKeyName[MAX_KEYNAME_SIZE];
		char	m_MaxsKeyName[MAX_KEYNAME_SIZE];

		Vector m_Mins;
		Vector m_Maxs;
};


#endif // MAPALIGNEDBOX_H
