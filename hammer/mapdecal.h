//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a decal helper. The decal attaches itself to nearby
//			solids, dynamically creating decal faces as necessary.
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPDECAL_H
#define MAPDECAL_H
#pragma once

#include "MapHelper.h"



class CHelperInfo;
class CMapFace;
class CRender3D;
class IEditorTexture;


//
// Structure containing a decal face and the solid to which it is attached.
// We keep one of these for every decal face that is created.
//
struct DecalFace_t
{
	CMapFace *pFace;		// Textured face representing the decal.
	CMapSolid *pSolid;		// The solid to which the face is attached.
};


class CMapDecal : public CMapHelper
{
	public:

		DECLARE_MAPCLASS(CMapDecal,CMapHelper)

		//
		// Factory for building from a list of string parameters.
		//
		static CMapClass *CreateMapDecal(CHelperInfo *pInfo, CMapEntity *pParent);

		//
		// Construction/destruction:
		//
		CMapDecal(void);
		CMapDecal(float *pfMins, float *pfMaxs);
		~CMapDecal(void);

		void CalcBounds(BOOL bFullUpdate = FALSE);

		virtual CMapClass *Copy(bool bUpdateDependencies);
		virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

		virtual void OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType);
		virtual void OnParentKeyChanged(const char* szKey, const char* szValue);
		virtual void OnPaste(CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList);
		virtual void OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren);

		virtual void Render3D(CRender3D *pRender);

		int SerializeRMF(std::fstream &File, BOOL bRMF);
		int SerializeMAP(std::fstream &File, BOOL bRMF);

		bool IsVisualElement(void) { return(true); }
		
		const char* GetDescription() { return("Decal helper"); }

		virtual void PostloadWorld(CMapWorld *pWorld);

		void DecalAllSolids(CMapWorld *pWorld);

		bool ShouldRenderLast(void) { return true; }

	protected:

		//
		// Implements CMapAtom transformation functions.
		//
		void DoTransform(const VMatrix &matrix);
		
		void AddSolid(CMapSolid *pSolid);

		int CanDecalSolid(CMapSolid *pSolid, CMapFace **ppFaces);
		int DecalSolid(CMapSolid *pSolid);

		void RebuildDecalFaces(void);

		IEditorTexture *m_pTexture;		// Pointer to the texture this decal uses.
		CMapObjectList m_Solids;	// List of solids to which we are attached.
		CUtlVector<DecalFace_t*> m_Faces;		// List of decal faces and the solids that they are attached to.
};


#endif // MAPDECAL_H
