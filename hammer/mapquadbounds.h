//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A helper that measures the extents of the only non-NODRAW face of
//			its only solid sibling. Writes the extents as four vector keyvalues:
//
//				lowerleft
//				upperleft
//				lowerright
//				upperright
//			
//			ASSUMPTIONS:
//
//			1. Only one solid sibling.
//			2. That solid only has one face with a texture other than "toolsnodraw".
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPQUADBOUNDS_H
#define MAPQUADBOUNDS_H
#pragma once

#include "MapHelper.h"


class CHelperInfo;
class CRender3D;


class CMapQuadBounds : public CMapHelper
{
	public:

		DECLARE_MAPCLASS(CMapQuadBounds,CMapHelper)

		//
		// Factory for building from a list of string parameters.
		//
		static CMapClass *CreateQuadBounds(CHelperInfo *pInfo, CMapEntity *pParent);

		//
		// Construction/destruction:
		//
		CMapQuadBounds(void);
		~CMapQuadBounds(void);

		void PresaveWorld(void);

		virtual CMapClass *Copy(bool bUpdateDependencies);
		virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

		int SerializeRMF(std::fstream &File, BOOL bRMF) { return(0); }
		int SerializeMAP(std::fstream &File, BOOL bRMF) { return(0); }

		bool IsVisualElement(void) { return(false); } // Only visible when the parent entity is selected.
		
		const char* GetDescription() { return("Quad bounds helper"); }

	protected:
		Vector			m_vLowerLeft;
		Vector			m_vUpperLeft;
		Vector			m_vLowerRight;
		Vector			m_vUpperRight;
		int				m_nError;
};

#endif // MAPQUADBOUNDS_H
