//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements an entity helper that extracts the bounds of a non-nodraw
//			face from a solid sibling, saving them as keyvalues in the entity.
//
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "fgdlib/HelperInfo.h"
#include "MapQuadBounds.h"
#include "mathlib/MathLib.h"
#include "Render3D.h"
#include "material.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "mapsolid.h"
#include "mapentity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapQuadBounds)


#define QUAD_ERR_NONE		0
#define QUAD_ERR_MULT_FACES 1
#define QUAD_ERR_NOT_QUAD	2


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapQuadBounds helper from a
//			set of string parameters from the FGD file.
// Input  : *pInfo - Pointer to helper info class which gives us information
//				about how to create the helper.
// Output : Returns a pointer to the helper, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapQuadBounds::CreateQuadBounds(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	CMapQuadBounds* pQuadBounds = new CMapQuadBounds;
	return(pQuadBounds);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CMapQuadBounds::CMapQuadBounds(void)
{
	m_vLowerLeft.Init();
	m_vUpperLeft.Init();
	m_vLowerRight.Init();
	m_vUpperRight.Init();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapQuadBounds::~CMapQuadBounds(void)
{
}


//------------------------------------------------------------------------------
// Purpose: Before saving, fill in my parent entity's keys with the bounds of
//			a non-nodraw face from a sibling solid.
//------------------------------------------------------------------------------
void CMapQuadBounds::PresaveWorld(void) 
{

	CMapEntity *pMapEntity = dynamic_cast<CMapEntity*>(GetParent());

	if (!pMapEntity)
	{
		return;
	}
	CMapSolid *pSolid = pMapEntity->GetChildOfType((CMapSolid*)NULL);

	if (pSolid)
	{
		int		nFaces = pSolid->GetFaceCount();
		bool	bFound = false;
		for (int i = 0; i < nFaces; i++)
		{
			//
			// Look for face with 4 points that isn't no draw
			//
			CMapFace *pFace = pSolid->GetFace(i);

			char szCurrentTexture[MAX_PATH];
			pFace->GetTextureName(szCurrentTexture);
			int nPoints = pFace->GetPointCount();

			// Ignore no draw surfaces
			if (stricmp(szCurrentTexture, "tools/toolsnodraw"))
			{
				if (bFound)
				{
					m_nError = QUAD_ERR_MULT_FACES;
				}
				else if (nPoints != 4)
				{
					m_nError = QUAD_ERR_NOT_QUAD;
				}
				else
				{
					Vector vLowerLeft,vUpperLeft,vLowerRight,vUpperRight;
					pFace->GetPoint(m_vLowerLeft, 0);
					pFace->GetPoint(m_vLowerRight,1);
					pFace->GetPoint(m_vUpperRight,2);
					pFace->GetPoint(m_vUpperLeft, 3);
					bFound	 = true;
					m_nError = QUAD_ERR_NONE;
				}
			}
		} 

		static char buf[64];
		sprintf( buf, "%g %g %g", (double)m_vLowerLeft[0], (double)m_vLowerLeft[1], (double)m_vLowerLeft[2] );
		pMapEntity->SetKeyValue( "lowerleft", buf );

		sprintf( buf, "%g %g %g", (double)m_vUpperLeft[0], (double)m_vUpperLeft[1], (double)m_vUpperLeft[2] );
		pMapEntity->SetKeyValue( "upperleft", buf );

		sprintf( buf, "%g %g %g", (double)m_vLowerRight[0], (double)m_vLowerRight[1], (double)m_vLowerRight[2] );
		pMapEntity->SetKeyValue( "lowerright", buf );

		sprintf( buf, "%g %g %g", (double)m_vUpperRight[0], (double)m_vUpperRight[1], (double)m_vUpperRight[2] );
		pMapEntity->SetKeyValue( "upperright", buf );

		sprintf( buf, "%i", m_nError);
		pMapEntity->SetKeyValue( "error", buf );

	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns an exact copy of this object.
//-----------------------------------------------------------------------------
CMapClass *CMapQuadBounds::Copy(bool bUpdateDependencies)
{
	CMapQuadBounds *pCopy = new CMapQuadBounds;

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}



//-----------------------------------------------------------------------------
// Purpose: Makes this an exact duplicate of pObject.
// Input  : pObject - Object to copy.
// Output : Returns this.
//-----------------------------------------------------------------------------
CMapClass *CMapQuadBounds::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	Assert(pObject->IsMapClass(MAPCLASS_TYPE(CMapQuadBounds)));
	CMapQuadBounds *pFrom = (CMapQuadBounds *)pObject;

	CMapClass::CopyFrom(pObject, bUpdateDependencies);

	m_vLowerLeft	= pFrom->m_vLowerLeft;
	m_vUpperLeft	= pFrom->m_vUpperLeft;
	m_vLowerRight	= pFrom->m_vLowerRight;
	m_vUpperRight	= pFrom->m_vUpperRight;
	m_nError		= pFrom->m_nError;

	return(this);
}



