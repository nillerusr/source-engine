//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPLINE_H
#define MAPLINE_H
#pragma once


#include "MapHelper.h"


class CRender3D;


#define MAX_KEYNAME_SIZE	32


class CMapLine : public CMapHelper
{
	public:

		DECLARE_MAPCLASS(CMapLine,CMapHelper)

		//
		// Factory for building from a list of string parameters.
		//
		static CMapClass *Create(CHelperInfo *pInfo, CMapEntity *pParent);

		//
		// Construction/destruction:
		//
		CMapLine(void);
		CMapLine(const char *pszStartValueKey, const char *pszStartKey, const char *pszEndValueKey, const char *pszEndKey);
		~CMapLine(void);

		void Initialize(void);

		void BuildLine(void);
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
		
		const char* GetDescription() { return("Line helper"); }

		void OnAddToWorld(CMapWorld *pWorld);
		void OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType);
		void OnParentKeyChanged( const char* key, const char* value );
		void OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren);

		virtual void UpdateDependencies(CMapWorld *pWorld, CMapClass *pObject);

	protected:
		
		//
		// Implements CMapAtom transformation functions.
		//
		void DoTransform(const VMatrix &matrix);
		
		char m_szStartValueKey[80];		// The key in our parent entity to look at for our start target.
		char m_szStartKey[80];			// The value in our parent entity to look at for our start target.

		char m_szEndValueKey[80];		// 
		char m_szEndKey[80];			// 

		CMapEntity *m_pStartEntity;		// Our start target.
		CMapEntity *m_pEndEntity;		// Our end target.
};


#endif // MAPLINE_H
