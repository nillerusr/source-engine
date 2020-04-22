//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "collisionutils.h"
#include "mainfrm.h"
#include "MapDefs.h"
#include "MapFace.h"
#include "MapDisp.h"
#include "MapWorld.h"
#include "fgdlib/WCKeyValues.h"
#include "GlobalFunctions.h"
#include "Render3D.h"
#include "Render2D.h"
#include "SaveInfo.h"
#include "TextureSystem.h"
#include "MapDoc.h"
#include "materialsystem/imesh.h"
#include "Material.h"
#include "utlrbtree.h"
#include "mathlib/vector.h"
#include "camera.h"
#include "options.h"
#include "hammer.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//#define DEBUGPTS


#define TEXTURE_AXIS_LENGTH				10			// Rendered texture axis length in world units.

//
// Components of the texture axes are rounded to integers within this tolerance. This tolerance corresponds
// to an angle of about 0.06 degrees.
//
#define TEXTURE_AXIS_ROUND_EPSILON		0.001		


//
// For passing into LoadKeyCallback. Collects key value data while loading.
//
struct LoadFace_t
{
	CMapFace *pFace;
	char szTexName[MAX_PATH];
};


BOOL CheckFace(Vector *Points, int nPoints, Vector *normal, float dist, CCheckFaceInfo *pInfo);
LPCTSTR GetDefaultTextureName();


#pragma warning(disable:4244)


//
// Static member data initialization.
//
bool CMapFace::m_bShowFaceSelection = true;
IEditorTexture *CMapFace::m_pLightmapGrid = NULL;


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members and sets the texture to the
//			default texture.
//-----------------------------------------------------------------------------
CMapFace::CMapFace(void)
{
	memset(&texture, 0, sizeof(texture));
	memset(&plane, 0, sizeof(plane));

	m_pTexture = NULL;
	m_pTangentAxes = NULL;
    m_DispHandle = EDITDISPHANDLE_INVALID;

	Points = NULL;
	nPoints = 0;
	m_nFaceID = 0;
	m_pTextureCoords = NULL;
	m_pLightmapCoords = NULL;
	m_uchAlpha = 255;

	m_pDetailObjects = NULL;

	texture.nLightmapScale = g_pGameConfig->GetDefaultLightmapScale();

	texture.scale[0] = g_pGameConfig->GetDefaultTextureScale();
	texture.scale[1] = g_pGameConfig->GetDefaultTextureScale();

	SetTexture(GetNullTextureName());
	
	if (m_pLightmapGrid == NULL)
	{
		m_pLightmapGrid = g_Textures.FindActiveTexture("Debug/debugluxelsnoalpha");
	}

	m_bIgnoreLighting = false;
	m_fSmoothingGroups = SMOOTHING_GROUP_DEFAULT;
	UpdateFaceFlags();
	SignalUpdate( EVTYPE_FACE_CHANGED );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees points and texture coordinates.
//-----------------------------------------------------------------------------
CMapFace::~CMapFace(void)
{
	SignalUpdate( EVTYPE_FACE_CHANGED );
	delete [] Points;
	Points = NULL;

	delete [] m_pTextureCoords;
	m_pTextureCoords = NULL;

	delete [] m_pLightmapCoords;
	m_pLightmapCoords = NULL;

	delete m_pDetailObjects;
	m_pDetailObjects = NULL;

	FreeTangentSpaceAxes();

	if( HasDisp() )
	{
		IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();

		if( pDispMgr )
		{
			pDispMgr->RemoveFromWorld( GetDisp() );
		}

		// destroy handle
		SetDisp( EDITDISPHANDLE_INVALID );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Attempts to fix this face. This is called by the check for problems
//			code when a face is reported as invalid.
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapFace::Fix(void)
{
	CalcPlane();
	CalcTextureCoords();

	// Create any detail objects if appropriate
	DetailObjects::BuildAnyDetailObjects(this);

	return(CheckFace());
}


//-----------------------------------------------------------------------------
// Purpose: Returns the short texture name in 'pszName'. Places an empty string
//			in 'pszName' if the face has no texture.
//-----------------------------------------------------------------------------
void CMapFace::GetTextureName(char *pszName) const
{
	Assert(pszName != NULL);

	if (pszName != NULL)
	{
		if (m_pTexture != NULL)
		{
			m_pTexture->GetShortName(pszName);
		}
		else
		{
			pszName[0] = '\0';
		}
	}
}

static char *InvisToolTextures[]={
	"occluder",
	"areaportal",
	"invisible",
	"skip",
	"trigger",
	"hint",
	"fog",
	"origin",
	"toolsnodraw",
};

void CMapFace::UpdateFaceFlags( void )
{
	char tname[2048];
	GetTextureName( tname );
	m_nFaceFlags = 0;
	if (strstr(tname,"tools"))
	{
		if ( strstr( tname, "blocklight" ) )
		{
			m_nFaceFlags |= FACE_FLAGS_NODRAW_IN_LPREVIEW;
		}					 
		if ( strstr( tname, "skybox") )
		{
			m_nFaceFlags |= FACE_FLAGS_NOSHADOW;
		}					 
		for(int i=0;i<NELEMS(InvisToolTextures); i++)
			if (strstr( tname, InvisToolTextures[i] ) )
			{
				m_nFaceFlags |= FACE_FLAGS_NODRAW_IN_LPREVIEW | FACE_FLAGS_NOSHADOW;
				break;
			}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Populates this face with another face's information.
// Input  : pFrom - The face to copy.
// Output : CMapFace
//-----------------------------------------------------------------------------
CMapFace *CMapFace::CopyFrom(const CMapFace *pObject, DWORD dwFlags, bool bUpdateDependencies)
{
	SignalUpdate( EVTYPE_FACE_CHANGED );
	const CMapFace *pFrom = dynamic_cast<const CMapFace *>(pObject);
	Assert(pFrom != NULL);

	if (pFrom != NULL)
	{
		//
		// Free our points first.
		//
		if (Points != NULL)
		{
			delete [] Points;
			Points = NULL;
		}

		if (m_pTextureCoords != NULL)
		{
			delete [] m_pTextureCoords;
			m_pTextureCoords = NULL;
		}

		if (m_pLightmapCoords != NULL)
		{
			delete [] m_pLightmapCoords;
			m_pLightmapCoords = NULL;
		}

		FreeTangentSpaceAxes();

		nPoints = 0;

		//
		// Copy the member data.
		//
		m_nFaceID = pFrom->m_nFaceID;
		m_eSelectionState = pFrom->GetSelectionState();
		texture = pFrom->texture;
		m_pTexture = pFrom->m_pTexture;
		m_bIsCordonFace = pFrom->m_bIsCordonFace;

		//
		// Allocate points memory.
		//
		if (dwFlags & COPY_FACE_POINTS)
		{
			Points = NULL;
			nPoints = pFrom->nPoints;

			if (pFrom->Points && nPoints)
			{
				AllocatePoints(nPoints);
				AllocTangentSpaceAxes( nPoints );
				memcpy(Points, pFrom->Points, sizeof(Vector) * nPoints);
				memcpy(m_pTextureCoords, pFrom->m_pTextureCoords, sizeof(Vector2D) * nPoints);
				memcpy(m_pLightmapCoords, pFrom->m_pLightmapCoords, sizeof(Vector2D) * nPoints);
				memcpy(m_pTangentAxes, pFrom->m_pTangentAxes, sizeof(TangentSpaceAxes_t) * nPoints);
			}
		}
		else
		{
			Points = NULL;
			m_pTextureCoords = 0;
			m_pLightmapCoords = 0;
			m_pTangentAxes = 0;
			nPoints = 0;
		}

		//
		// Copy the plane. You shouldn't copy the points without copying the plane,
		// so we do it if either bit is set.
		//
		if ((dwFlags & COPY_FACE_POINTS) || (dwFlags & COPY_FACE_PLANE))
		{
			plane = pFrom->plane;
		}
		else
		{
			memset(&plane, 0, sizeof(plane));
		}

        //
        // copy the displacement info.
		//
		// If we do have displacement, then we'll never be asked to become a copy of
		// a face that does not have a displacement, because you cannot undo a Generate
		// Displacement operation.
        //
        if( pFrom->HasDisp() )
        {
			//
			// Allocate a new displacement info if we don't already have one.
			//
			if( !HasDisp() )
			{
				SetDisp( EditDispMgr()->Create() );
			}

			CMapDisp *pDisp = EditDispMgr()->GetDisp( m_DispHandle );
			pDisp->SetParent( this );

			CMapDisp *pFromDisp = EditDispMgr()->GetDisp( pFrom->m_DispHandle );
			pDisp->CopyFrom( pFromDisp, bUpdateDependencies );
        }
		else
		{
			SetDisp( EDITDISPHANDLE_INVALID );
		}
	
		// Copy CMapAtom fields. dvs: this should be done in CMapAtom::CopyFrom!
		r = pFrom->r;
		g = pFrom->g;
		b = pFrom->b;

		m_uchAlpha = pFrom->m_uchAlpha;
		m_bIgnoreLighting = pFrom->m_bIgnoreLighting;

		// Copy the smoothing group data.
		m_fSmoothingGroups = pFrom->m_fSmoothingGroups;

		// Delete any existing and build any new detail objects
		delete m_pDetailObjects;
		m_pDetailObjects = NULL;
		DetailObjects::BuildAnyDetailObjects(this);
	}
	UpdateFaceFlags();
	return(this);
}


//-----------------------------------------------------------------------------
// Called any time this object is modified due to an Undo or Redo.
//-----------------------------------------------------------------------------
void CMapFace::OnUndoRedo()
{
	// It's not valid to have selected faces outside of face edit mode.
	// If the user modified this face, then closed the texture application
	// dialog, then did an Undo, clear our selection state.
	if ( !GetMainWnd()->IsInFaceEditMode() )
	{
		m_eSelectionState = SELECT_NONE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Creates a face from a list of points.
// Input  : pPoints - An array of points.
//			_nPoints - Number of points. If nPoints < 0, reverse points.
//-----------------------------------------------------------------------------
void CMapFace::CreateFace(Vector *pPoints, int _nPoints, bool bIsCordonFace)
{
	SignalUpdate( EVTYPE_FACE_CHANGED );
	if (_nPoints > 0)
	{
		AllocatePoints(_nPoints);
		Assert(nPoints > 0);
		if (nPoints > 0)
		{
			memcpy(Points, pPoints, nPoints * sizeof(Vector));
		}
	}
	else
	{
		AllocatePoints(-_nPoints);
		Assert(nPoints > 0);
		if (nPoints > 0)
		{
			int j = 0;
			for (int i = nPoints - 1; i >= 0; i--)
			{
				Points[j++] = pPoints[i];
			}
		}
	}

	SetCordonFace( bIsCordonFace );

#ifdef DEBUGPTS
	DebugPoints();
#endif

	CalcPlaneFromFacePoints();
	CalcTextureCoords();

	// Create any detail objects if appropriate
	DetailObjects::BuildAnyDetailObjects(this);

#if 0
    //
    // create the displacement map -- if need be
    //
    if( m_pMapDisp )
    {
		m_pMapDisp->InitSurfData( this, false );
        m_pMapDisp->Create();
    }
#endif
}


Vector FaceNormals[6] =
{
	Vector(0, 0, 1),		// floor
	Vector(0, 0, -1),		// ceiling
	Vector(0, -1, 0),		// north wall
	Vector(0, 1, 0),		// south wall
	Vector(-1, 0, 0),		// east wall
	Vector(1, 0, 0),		// west wall
};


Vector DownVectors[6] =
{
	Vector(0, -1, 0),		// floor
	Vector(0, -1, 0),		// ceiling
	Vector(0, 0, -1),		// north wall
	Vector(0, 0, -1),		// south wall
	Vector(0, 0, -1),		// east wall
	Vector(0, 0, -1),		// west wall
};


Vector RightVectors[6] =
{
	Vector(1, 0, 0),		// floor
	Vector(1, 0, 0),		// ceiling
	Vector(1, 0, 0),		// north wall
	Vector(1, 0, 0),		// south wall
	Vector(0, 1, 0),		// east wall
	Vector(0, 1, 0),		// west wall
};


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//			downVect - 
//-----------------------------------------------------------------------------
void CMapFace::GetDownVector( int index, Vector& downVect )
{
    downVect = DownVectors[index];
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Center - 
//-----------------------------------------------------------------------------
void CMapFace::GetCenter(Vector& Center)
{
	Assert(nPoints > 0);

	Center.Init();

	if (nPoints != 0)
	{
		for (int i = 0; i < nPoints; i++)
		{
			Center[0] += Points[i][0];
			Center[1] += Points[i][1];
			Center[2] += Points[i][2];
		}

		Center[0] /= nPoints;
		Center[1] /= nPoints;
		Center[2] /= nPoints;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determines the general orientation of a face based on its normal vector.
// Output : FaceOrientation_t
//-----------------------------------------------------------------------------
FaceOrientation_t CMapFace::GetOrientation(void) const
{
	// The normal must have a nonzero length!
	if ((plane.normal[0] == 0) && (plane.normal[1] == 0) && (plane.normal[2] == 0))
	{
		return(FACE_ORIENTATION_INVALID);
	}

	//
	// Find the axis that the surface normal has the greatest projection onto.
	//
	float fDot;
	float fMaxDot;
	Vector Normal;

	FaceOrientation_t eOrientation = FACE_ORIENTATION_INVALID;

	Normal = plane.normal;
	VectorNormalize(Normal);

	fMaxDot = 0;
	for (int i = 0; i < 6; i++)
	{
		fDot = DotProduct(Normal, FaceNormals[i]);

		if (fDot >= fMaxDot)
		{
			fMaxDot = fDot;
			eOrientation = (FaceOrientation_t)i;
		}
	}

	return(eOrientation);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eAlignment - 
//			dwFlags - 
//-----------------------------------------------------------------------------
void CMapFace::InitializeTextureAxes(TextureAlignment_t eAlignment, DWORD dwFlags)
{
	FaceOrientation_t eOrientation;

	//
	// If the texture axis information has been initialized, don't reinitialize unless
	// the FORCE flag is set.
	//
	if ((!(dwFlags & INIT_TEXTURE_FORCE)) && 
		((texture.UAxis[0] != 0) || (texture.UAxis[1] != 0) || (texture.UAxis[2] != 0) ||
		 (texture.VAxis[0] != 0) || (texture.VAxis[1] != 0) || (texture.VAxis[2] != 0)))
	{
		return;
	}

	if (dwFlags & INIT_TEXTURE_ROTATION)
	{
		texture.rotate = 0;
	}

	if (dwFlags & INIT_TEXTURE_SHIFT)
	{
		texture.UAxis[3] = 0;
		texture.VAxis[3] = 0;
	}

	if (dwFlags & INIT_TEXTURE_SCALE)
	{
		texture.scale[0] = g_pGameConfig->GetDefaultTextureScale();
		texture.scale[1] = g_pGameConfig->GetDefaultTextureScale();
	}

	if (dwFlags & INIT_TEXTURE_AXES)
	{
		// don't reset the shift component [3]
		texture.UAxis.AsVector3D().Init();
		texture.VAxis.AsVector3D().Init();

		// Determine the general orientation of this face (floor, ceiling, n wall, etc.)
		eOrientation = GetOrientation();
		if (eOrientation == FACE_ORIENTATION_INVALID)
		{
			CalcTextureCoords();
			return;
		}

		// Pick a world axis aligned V axis based on the face orientation.
		texture.VAxis.AsVector3D() = DownVectors[eOrientation];

		//
		// If we are using face aligned textures, calculate the texture axes.
		//
		if (eAlignment == TEXTURE_ALIGN_FACE)
		{
			// Using that axis-aligned V axis, calculate the true U axis
			CrossProduct(plane.normal, texture.VAxis.AsVector3D(), texture.UAxis.AsVector3D());
			VectorNormalize(texture.UAxis.AsVector3D());

			// Now use the true U axis to calculate the true V axis.
			CrossProduct(texture.UAxis.AsVector3D(), plane.normal, texture.VAxis.AsVector3D());
			VectorNormalize(texture.VAxis.AsVector3D());
		}
		//
		// If we are using world (or "natural") aligned textures, use the V axis as is
		// and pick the corresponding U axis from the table.
		//
		else if (eAlignment == TEXTURE_ALIGN_WORLD)
		{
			texture.UAxis.AsVector3D() = RightVectors[eOrientation];
		}
		//
		// Quake-style texture alignment used a different axis convention.
		//
		else
		{
			InitializeQuakeStyleTextureAxes(texture.UAxis, texture.VAxis);
		}
	
		if (texture.rotate != 0)
		{
			RotateTextureAxes(texture.rotate);
		}
	}

	CalcTextureCoords();

	// Create any detail objects if appropriate
	DetailObjects::BuildAnyDetailObjects(this);

}


//-----------------------------------------------------------------------------
// Purpose: Checks for a texture axis perpendicular to the face.
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapFace::IsTextureAxisValid(void) const
{
	//
	// Generate the texture normal axis, which may be different from the
	// face normal, depending on texture alignment.
	//
	Vector TexNormalAxis;
	CrossProduct(texture.VAxis.AsVector3D(), texture.UAxis.AsVector3D(), TexNormalAxis);
	return(DotProduct(plane.normal, TexNormalAxis) != 0);
}


//-----------------------------------------------------------------------------
// Purpose: Normalize the U/V shift values to be less than the texture width/height.
//-----------------------------------------------------------------------------
void CMapFace::NormalizeTextureShifts(void)
{
	//
	// HACK: this should really be elsewhere, but it can live here for now.
	// Round all components of our texture axes within an epsilon.
	//
	for (int nDim = 0; nDim < 4; nDim++)
	{
		int nValue = V_rint(texture.UAxis[nDim]);
		if (fabs(texture.UAxis[nDim] - nValue) < TEXTURE_AXIS_ROUND_EPSILON)
		{
			texture.UAxis[nDim] = nValue;
		}

		nValue = V_rint(texture.VAxis[nDim]);
		if (fabs(texture.VAxis[nDim] - nValue) < TEXTURE_AXIS_ROUND_EPSILON)
		{
			texture.VAxis[nDim] = nValue;
		}
	}

	if (m_pTexture == NULL)
	{
		return;
	}

	if (m_pTexture->GetWidth() != 0)
	{
		texture.UAxis[3] = fmod(texture.UAxis[3], m_pTexture->GetWidth());
	}

	if (m_pTexture->GetHeight() != 0)
	{
		texture.VAxis[3] = fmod(texture.VAxis[3], m_pTexture->GetHeight());
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determines the bounding box of a face in world space.
// Input  : pfMins - Receives the face X, Y, Z minima.
//			pfMaxs - Receives the face X, Y, Z maxima.
//-----------------------------------------------------------------------------
void CMapFace::GetFaceBounds(Vector& pfMins, Vector& pfMaxs) const
{
	for (int nPoint = 0; nPoint < nPoints; nPoint++)
	{
		if ((Points[nPoint][0] < pfMins[0]) || (nPoint == 0))
		{
			pfMins[0] = Points[nPoint][0];
		}

		if ((Points[nPoint][1] < pfMins[1]) || (nPoint == 0))
		{
			pfMins[1] = Points[nPoint][1];
		}

		if ((Points[nPoint][2] < pfMins[2]) || (nPoint == 0))
		{
			pfMins[2] = Points[nPoint][2];
		}

		if ((Points[nPoint][0] > pfMaxs[0]) || (nPoint == 0))
		{
			pfMaxs[0] = Points[nPoint][0];
		}

		if ((Points[nPoint][1] > pfMaxs[1]) || (nPoint == 0))
		{
			pfMaxs[1] = Points[nPoint][1];
		}

		if ((Points[nPoint][2] > pfMaxs[2]) || (nPoint == 0))
		{
			pfMaxs[2] = Points[nPoint][2];
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Finds the top left and bottom right points on the face in texture space.
//			These points are returned in texture space, not world space.
// Input  : TopLeft - 
//			BottomRight - 
//-----------------------------------------------------------------------------
void CMapFace::GetFaceTextureExtents(Vector2D & TopLeft, Vector2D & BottomRight) const
{
	BOOL bFirst = TRUE;

	for (int nPoint = 0; nPoint < nPoints; nPoint++)
	{
		Vector2D Test;

		Test[0] = DotProduct(Points[nPoint], texture.UAxis.AsVector3D()) / texture.scale[0];
		Test[1] = DotProduct(Points[nPoint], texture.VAxis.AsVector3D()) / texture.scale[1];

		if ((Test[0] < TopLeft[0]) || (bFirst))
		{
			TopLeft[0] = Test[0];
		}

		if ((Test[1] < TopLeft[1]) || (bFirst))
		{
			TopLeft[1] = Test[1];
		}

		if ((Test[0] > BottomRight[0]) || (bFirst))
		{
			BottomRight[0] = Test[0];
		}

		if ((Test[1] > BottomRight[1]) || (bFirst))
		{
			BottomRight[1] = Test[1];
		}

		bFirst = FALSE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the distance along the face normal of a given point. The
//			distance will be negative if the point is behind the face, positive
//			if the point is in front of the face.
// Input  : fPoint - Point to calculate normal distance.
//-----------------------------------------------------------------------------
float CMapFace::GetNormalDistance(Vector& fPoint)
{
	float fDot = DotProduct(fPoint, plane.normal);
	return(fDot - plane.dist);
}


//-----------------------------------------------------------------------------
// Purpose: Determines the texture alignment(s) of this face. The alignments are
//			are returned as TextureAlignment_t values OR'ed together.
//
// Output : Returns an integer with any of the following flags set:
//
//			TEXTURE_ALIGN_FACE - the texture axes are face aligned.
//			TEXTURE_ALIGN_WORLD - the texture axes are world aligned.
//
//			If the returned value is zero (TEXTURE_ALIGN_NONE), the texture axes
//			are neither face aligned nor world aligned.
//-----------------------------------------------------------------------------
int CMapFace::GetTextureAlignment(void) const
{
	Vector TexNormalAxis;
	int nAlignment = TEXTURE_ALIGN_NONE;

	//
	// Generate the texture normal axis, which may be different from the
	// face normal, depending on texture alignment.
	//
	CrossProduct(texture.VAxis.AsVector3D(), texture.UAxis.AsVector3D(), TexNormalAxis);
	VectorNormalize(TexNormalAxis);

	//
	// Check for face alignment.
	//
	if (DotProduct(TexNormalAxis, plane.normal) > 0.9999)
	{
		nAlignment |= TEXTURE_ALIGN_FACE;
	}

	//
	// Check for world alignment.
	//
	FaceOrientation_t eOrientation = GetOrientation();
	if (eOrientation != FACE_ORIENTATION_INVALID)
	{
		Vector WorldTexNormal;

		CrossProduct(DownVectors[eOrientation], RightVectors[eOrientation], WorldTexNormal);
		if (DotProduct(TexNormalAxis, WorldTexNormal) > 0.9999)
		{
			nAlignment |= TEXTURE_ALIGN_WORLD;
		}
	}

	return(nAlignment);
}


//-----------------------------------------------------------------------------
// Purpose: Finds the top left and bottom right points of the given world extents
//			in texture space. These points are returned in texture space, not world space,
//			so a simple rectangle will suffice.
// Input  : Extents - 
//			TopLeft - 
//			BottomRight - 
//-----------------------------------------------------------------------------
void CMapFace::GetTextureExtents(Extents_t Extents, Vector2D & TopLeft, Vector2D & BottomRight) const
{
	BOOL bFirst = TRUE;

	for (int nPoint = 0; nPoint < NUM_EXTENTS_DIMS; nPoint++)
	{
		Vector2D Test;

		Test[0] = DotProduct(Extents[nPoint], texture.UAxis.AsVector3D()) / texture.scale[0];
		Test[1] = DotProduct(Extents[nPoint], texture.VAxis.AsVector3D()) / texture.scale[1];

		if ((Test[0] < TopLeft[0]) || (bFirst))
		{
			TopLeft[0] = Test[0];
		}

		if ((Test[1] < TopLeft[1]) || (bFirst))
		{
			TopLeft[1] = Test[1];
		}

		if ((Test[0] > BottomRight[0]) || (bFirst))
		{
			BottomRight[0] = Test[0];
		}

		if ((Test[1] > BottomRight[1]) || (bFirst))
		{
			BottomRight[1] = Test[1];
		}

		bFirst = FALSE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determines the world extents of the face. Different from a bounding
//			box in that each point in the returned extents is actually on the face.
// Input  : Extents - 
//-----------------------------------------------------------------------------
void CMapFace::GetFaceExtents(Extents_t Extents) const
{
	BOOL bFirst = TRUE;

	for (int nPoint = 0; nPoint < nPoints; nPoint++)
	{
		if ((Points[nPoint][0] < Extents[EXTENTS_XMIN][0]) || (bFirst))
		{
			Extents[EXTENTS_XMIN] = Points[nPoint];
		}

		if ((Points[nPoint][0] > Extents[EXTENTS_XMAX][0]) || (bFirst))
		{
			Extents[EXTENTS_XMAX] = Points[nPoint];
		}

		if ((Points[nPoint][1] < Extents[EXTENTS_YMIN][1]) || (bFirst))
		{
			Extents[EXTENTS_YMIN] = Points[nPoint];
		}

		if ((Points[nPoint][1] > Extents[EXTENTS_YMAX][1]) || (bFirst))
		{
			Extents[EXTENTS_YMAX] = Points[nPoint];
		}

		if ((Points[nPoint][2] < Extents[EXTENTS_ZMIN][2]) || (bFirst))
		{
			Extents[EXTENTS_ZMIN] = Points[nPoint];
		}

		if ((Points[nPoint][2] > Extents[EXTENTS_ZMAX][2]) || (bFirst))
		{
			Extents[EXTENTS_ZMAX] = Points[nPoint];
		}

		bFirst = FALSE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eJustification - 
//			Extents - 
//-----------------------------------------------------------------------------
void CMapFace::JustifyTextureUsingExtents(TextureJustification_t eJustification, Extents_t Extents)
{
	Vector2D Center;

	if (!texture.scale[0])
	{
		texture.scale[0] = g_pGameConfig->GetDefaultTextureScale();
	}

	if (!texture.scale[1])
	{
		texture.scale[1] = g_pGameConfig->GetDefaultTextureScale();
	}

	// Skip all the mucking about for a justification of NONE.
	if (eJustification == TEXTURE_JUSTIFY_NONE)
	{
		texture.UAxis[3] = 0;
		texture.VAxis[3] = 0;
		CalcTextureCoords();
		return;
	}

	// For fit justification, use a scale of 1 for initial calculations.
	if (eJustification == TEXTURE_JUSTIFY_FIT)
	{
		texture.scale[0] = 1.0;
		texture.scale[1] = 1.0;
	}

	Vector2D TopLeft;
	Vector2D BottomRight;

	GetTextureExtents(Extents, TopLeft, BottomRight);

	// Find the face center in U/V space.
	Center[0] = (TopLeft[0] + BottomRight[0]) / 2;
	Center[1] = (TopLeft[1] + BottomRight[1]) / 2;

	//
	// Perform the justification.
	//
	switch (eJustification)
	{
		// Align the top left corner of the texture with the top left corner of the face.
		case TEXTURE_JUSTIFY_TOP:
		{
			texture.VAxis[3] = -TopLeft[1];
			break;
		}

		// Align the top left corner of the texture with the top left corner of the face.
		case TEXTURE_JUSTIFY_BOTTOM:
		{
			texture.VAxis[3] = -BottomRight[1] + m_pTexture->GetHeight();
			break;
		}

		// Align the left side of the texture with the left side of the face.
		case TEXTURE_JUSTIFY_LEFT:
		{
			texture.UAxis[3] = -TopLeft[0];
			break;
		}

		// Align the right side of the texture with the right side of the face.
		case TEXTURE_JUSTIFY_RIGHT:
		{
			texture.UAxis[3] = -BottomRight[0] + m_pTexture->GetWidth();
			break;
		}

		// Center the texture on the face.
		case TEXTURE_JUSTIFY_CENTER:
		{
			texture.UAxis[3] = -Center[0] + (m_pTexture->GetWidth() / 2);
			texture.VAxis[3] = -Center[1] + (m_pTexture->GetHeight() / 2);
			break;
		}

		// Scale the texture to exactly fit the face.
		case TEXTURE_JUSTIFY_FIT:
		{
			// Calculate the appropriate scale.
			if (m_pTexture && m_pTexture->GetWidth() && m_pTexture->GetHeight())
			{
				texture.scale[0] = (BottomRight[0] - TopLeft[0]) / m_pTexture->GetWidth();
				texture.scale[1] = (BottomRight[1] - TopLeft[1]) / m_pTexture->GetHeight();
			}
			else
			{
				texture.scale[0] = g_pGameConfig->GetDefaultTextureScale();
				texture.scale[1] = g_pGameConfig->GetDefaultTextureScale();
			}

			// Justify top left.
			JustifyTextureUsingExtents(TEXTURE_JUSTIFY_TOP, Extents);
			JustifyTextureUsingExtents(TEXTURE_JUSTIFY_LEFT, Extents);

			break;
		}
	}

	NormalizeTextureShifts();
	CalcTextureCoords();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eJustification - 
//-----------------------------------------------------------------------------
void CMapFace::JustifyTexture(TextureJustification_t eJustification)
{
	Extents_t Extents;
	GetFaceExtents(Extents);
	JustifyTextureUsingExtents(eJustification, Extents);
}


//-----------------------------------------------------------------------------
// Purpose: Offsets a texture due to texture locking when moving a face.
// Input  : Delta - The x, y, z translation that was applied to the face points.
//-----------------------------------------------------------------------------
void CMapFace::OffsetTexture(const Vector &Delta)
{
	//
	// Find the projection in U/V space of this movement
	// and shift the textures by that.
	//
	texture.UAxis[3] -= DotProduct(Delta, texture.UAxis.AsVector3D()) / texture.scale[0];
	texture.VAxis[3] -= DotProduct(Delta, texture.VAxis.AsVector3D()) / texture.scale[1];

	NormalizeTextureShifts();
}


//-----------------------------------------------------------------------------
// Purpose: Rotates the texture axes fDegrees counterclockwise around the
//			texture normal axis.
// Input  : fDegrees - Degrees to rotate the texture axes.
//-----------------------------------------------------------------------------
void CMapFace::RotateTextureAxes(float fDegrees)
{
	VMatrix Matrix;
	Vector TexNormalAxis;
	Vector4D UAxis;
	Vector4D VAxis;
	
	// Generate the texture normal axis, which may be different from the
	// face normal, depending on texture alignment.
	CrossProduct(texture.VAxis.AsVector3D(), texture.UAxis.AsVector3D(), TexNormalAxis);

	// Rotate the texture axes around the texture normal.
	AxisAngleMatrix(Matrix, TexNormalAxis, fDegrees);

	Matrix.V4Mul( texture.UAxis, UAxis );
	Matrix.V4Mul( texture.VAxis, VAxis );

	texture.UAxis = UAxis;
	texture.VAxis = VAxis;
}


//-----------------------------------------------------------------------------
// Purpose: Rebuilds the plane normal and distance from the plane points.
//-----------------------------------------------------------------------------
void CMapFace::CalcPlane(void)
{
	//
	// Build the plane normal and distance from the three plane points.
	//
	plane.normal = GetNormalFromPoints( plane.planepts[0], plane.planepts[1], plane.planepts[2] );
	plane.dist = DotProduct(plane.planepts[0], plane.normal);
}


//-----------------------------------------------------------------------------
// Purpose: Rebuilds the plane points from our face points.
//-----------------------------------------------------------------------------
void CMapFace::CalcPlaneFromFacePoints(void)
{
	if ((nPoints >= 3) && (Points != NULL))
	{
		//
		// Use the face points as a preliminary set of plane points.
		//
		memcpy(plane.planepts, Points, sizeof(Vector) * 3);

		//
		// Generate the plane normal and distance from the plane points.
		//
		CalcPlane();

		//
		// Now project large coordinates onto the plane to generate new
		// plane points that will be less prone to error creep.
		//
		// UNDONE: push out the points along the plane for better precision
	}
}


void CMapFace::AddShadowingTriangles( CUtlVector<Vector> &tri_list )
{
	// create a fan
	if (! (m_nFaceFlags & FACE_FLAGS_NOSHADOW ))
		for(int i=2;i<nPoints;i++)
		{
			tri_list.AddToTail( Points[0] );
			tri_list.AddToTail( Points[i-1] );
			tri_list.AddToTail( Points[i] );
		}
}

#ifdef DEBUGPTS
void CMapFace::DebugPoints(void)
{
	// check for dup points
	for(i = 0; i < nPoints; i++)
	{
		for(int j = 0; j < nPoints; j++)
		{
			if(j == i)
				continue;
			if(Points[j][0] == Points[i][0] &&
				Points[j][1] == Points[i][1] &&
				Points[j][2] == Points[i][2])
			{
				AfxMessageBox("Dup Points in CMapFace::Create(winding_t*)");
				break;
			}
		}
	}
}
#endif



//-----------------------------------------------------------------------------
// Purpose: Create the face from a winding type.
//	w - Winding from which to create the face.
//	nFlags - 
//		CREATE_FACE_PRESERVE_PLANE:
//		CREATE_FACE_CLIPPING: the new face is a clipped version of this face
//-----------------------------------------------------------------------------
void CMapFace::CreateFace(winding_t *w, int nFlags)
{
	SignalUpdate( EVTYPE_FACE_CHANGED );
	AllocatePoints(w->numpoints);
	for (int i = 0; i < nPoints; i++)
	{
		Points[i][0] = w->p[i][0];
		Points[i][1] = w->p[i][1];
		Points[i][2] = w->p[i][2];
	}

	if (!(nFlags & CREATE_FACE_PRESERVE_PLANE))
	{
		CalcPlaneFromFacePoints();
	}

    //
    // Create a new displacement surface if the clipped surfaces is a quad.
    //
	// This assumes it is being called by the clipper!!! (Bad assumption).
	//
    if( HasDisp() && ( nFlags & CREATE_FACE_CLIPPING ) )
	{
		if ( nPoints == 4 )
		{
			// Setup new displacement surface.
			EditDispHandle_t hClipDisp = EditDispMgr()->Create();
			CMapDisp *pClipDisp = EditDispMgr()->GetDisp( hClipDisp );
			
			// Get older displacement surface.
			EditDispHandle_t hDisp = GetDisp();
			CMapDisp *pDisp = EditDispMgr()->GetDisp( hDisp );
			
			// Init new displacement surface.
			pClipDisp->SetParent( this );

			// Apply the new displacement to this face, but keep the old one
			// around -- we need it for the split operation.
			SetDisp( hClipDisp, false );
			pClipDisp->InitData( pDisp->GetPower() );
			
			// Calculate texture coordinates before splitting because we
			// need the texture coords during the split.
			CalcTextureCoords();

			// Split the old displacement and put the results into hClipDisp.
			pDisp->Split( hClipDisp );

			// Delete the old displacement that was on this face.
			EditDispMgr()->Destroy( hDisp );
		}
		else
		{
			SetDisp( EDITDISPHANDLE_INVALID );
		}
    }
	else
	{
		CalcTextureCoords();
	}
#ifdef ENSUREDETAILS
	// Create any detail objects if appropriate
	DetailObjects::BuildAnyDetailObjects(this);
#endif

#ifdef DEBUGPTS
	DebugPoints();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Allocates space in Points array for nPoints worth of Vectors and
//			the corresponding texture and lightmap coordinates (Vector2D's). Frees
//			current points if there are any.
// Input  : _nPoints - number of points needed.
// Output : Total size of memory used by the points, texture, and lightmap coordinates.
//-----------------------------------------------------------------------------
size_t CMapFace::AllocatePoints(int _nPoints)
{
	//
	// If we have already allocated this many points, do nothing.
	//
	if ((Points != NULL) && (_nPoints == nPoints))
	{
		return(nPoints * (sizeof(Vector) + sizeof(Vector2D) + sizeof(Vector2D)));
	}

	//
	// If we have the wrong number of points allocated, free the memory.
	//
	if (Points != NULL)
	{
		delete [] Points;
		Points = NULL;

		delete [] m_pTextureCoords;
		m_pTextureCoords = NULL;

		delete [] m_pLightmapCoords;
		m_pLightmapCoords = NULL;
	}

	Assert( nPoints == 0 || nPoints > 2 );

	nPoints = _nPoints;

	if (!_nPoints)
	{
		return(0);
	}
	
	//
	// Allocate the correct number of points, texture coords, and lightmap coords.
	//
	Points = new Vector[nPoints];
	m_pTextureCoords = new Vector2D[nPoints];
	m_pLightmapCoords = new Vector2D[nPoints];

	// dvs: check for failure here and report an out of memory error
	Assert(Points != NULL);
	Assert(m_pTextureCoords != NULL);
	Assert(m_pLightmapCoords != NULL);

	return(nPoints * (sizeof(Vector) + sizeof(Vector2D) + sizeof(Vector2D)));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTexture - 
//-----------------------------------------------------------------------------
void CMapFace::SetTexture(IEditorTexture *pTexture, bool bRescaleTextureCoordinates)
{
	SignalUpdate( EVTYPE_FACE_CHANGED );
	if ( m_pTexture && pTexture && bRescaleTextureCoordinates )
	{
		float flXFactor = (float)m_pTexture->GetWidth() / pTexture->GetWidth();
		float flYFactor = (float)m_pTexture->GetHeight() / pTexture->GetHeight();

		texture.scale[0] *= flXFactor;
		texture.scale[1] *= flYFactor;

		texture.UAxis[3] /= flXFactor;
		texture.VAxis[3] /= flYFactor;
	}

	m_pTexture = pTexture;

	// Copy other things from m_pTexture.
	m_pTexture->GetShortName(texture.texture);
	texture.q2surface = m_pTexture->GetSurfaceAttributes();
	texture.q2contents = m_pTexture->GetSurfaceContents();

	BOOL bTexValid = FALSE;
	if (m_pTexture != NULL)
	{
		// Insure that the texture is loaded.
		m_pTexture->Load();

		bTexValid = !(
			m_pTexture->GetWidth() == 0 || 
			m_pTexture->GetHeight() == 0 ||
			m_pTexture->GetImageWidth() == 0 ||
			m_pTexture->GetImageHeight() == 0 || 
			!m_pTexture->HasData()
		);
	}
	
	if (bTexValid)
	{
		CalcTextureCoords();
	}

	UpdateFaceFlags();
}


//-----------------------------------------------------------------------------
// Purpose: Sets this face's texture by name.
// Input  : pszNewTex - Short name of texture to apply to this face.
//-----------------------------------------------------------------------------
void CMapFace::SetTexture(const char *pszNewTex, bool bRescaleTextureCoordinates)
{
	SignalUpdate( EVTYPE_FACE_CHANGED );
	IEditorTexture *pTexture = g_Textures.FindActiveTexture(pszNewTex);
	SetTexture(pTexture, bRescaleTextureCoordinates);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapFace::CalcTextureCoordAtPoint( const Vector& pt, Vector2D &texCoord )
{
	// sanity check
	if( m_pTexture == NULL )
		return;

	//
	// projected s, t (u, v) texture coordinates
	//
	float s = DotProduct( texture.UAxis.AsVector3D(), pt ) / texture.scale[0] + texture.UAxis[3];
	float t = DotProduct( texture.VAxis.AsVector3D(), pt ) / texture.scale[1] + texture.VAxis[3];

	//
	// "normalize" the texture coordinates
	//
	if (m_pTexture->GetWidth())
		texCoord[0] = s / ( float )m_pTexture->GetWidth();
	else
		texCoord[0] = 0.0;
	
	if (m_pTexture->GetHeight())
		texCoord[1] = t / ( float )m_pTexture->GetHeight();
	else
		texCoord[1] = 0.0;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapFace::CalcLightmapCoordAtPoint( const Vector& pt, Vector2D &lightCoord )
{
	lightCoord[0] = DotProduct( texture.UAxis.AsVector3D(), pt ) / texture.nLightmapScale + 0.5f;
	lightCoord[1] = DotProduct( texture.VAxis.AsVector3D(), pt ) / texture.nLightmapScale + 0.5f;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the U,V texture coordinates of all points on this face.
//-----------------------------------------------------------------------------
void CMapFace::CalcTextureCoords(void)
{
	float s, t;
	int i;

	if (m_pTexture == NULL)
	{
		return;
	}

	//
	// Make sure that scales are nonzero.
	//
	if (texture.scale[0] == 0)
	{
		texture.scale[0] = g_pGameConfig->GetDefaultTextureScale();
	}

	if (texture.scale[1] == 0)
	{
		texture.scale[1] = g_pGameConfig->GetDefaultTextureScale();
	}

	//
	// Recalculate U,V coordinates for all points.
	//
	for (i = 0; i < nPoints; i++)
	{
		//
		// Generate texture coordinates.
		//
		s = DotProduct(texture.UAxis.AsVector3D(), Points[i]) / texture.scale[0] + texture.UAxis[3];
		t = DotProduct(texture.VAxis.AsVector3D(), Points[i]) / texture.scale[1] + texture.VAxis[3];

		if (m_pTexture->GetWidth())
			m_pTextureCoords[i][0] = s / (float)m_pTexture->GetWidth();
		else
			m_pTextureCoords[i][0] = 0.0f;

		if (m_pTexture->GetHeight())
			m_pTextureCoords[i][1] = t / (float)m_pTexture->GetHeight();
 		else
			m_pTextureCoords[i][1] = 0.0f;

		//
		// Generate lightmap coordinates.  Lightmap coordinates for displacements happens below.
		//
		if ( m_DispHandle == EDITDISPHANDLE_INVALID )
		{
			float shiftScaleU = texture.scale[0] / (float)texture.nLightmapScale;
			float shiftScaleV = texture.scale[1] / (float)texture.nLightmapScale;

			m_pLightmapCoords[i][0] = DotProduct(texture.UAxis.AsVector3D(), Points[i]) / texture.nLightmapScale + texture.UAxis[3] * shiftScaleU + 0.5;
			m_pLightmapCoords[i][1] = DotProduct(texture.VAxis.AsVector3D(), Points[i]) / texture.nLightmapScale + texture.VAxis[3] * shiftScaleV + 0.5;
		}
 	}

	//
    // update the displacement map with new texture coordinates and calculate lightmap coordinates
	//
	if( ( m_DispHandle != EDITDISPHANDLE_INVALID ) && nPoints == 4 )
    {
		CMapDisp *pDisp = EditDispMgr()->GetDisp( m_DispHandle );
		pDisp->InitDispSurfaceData( this, false );
		pDisp->Create();
    }

	// re-calculate the tangent space
	CalcTangentSpaceAxes();
}


//-----------------------------------------------------------------------------
// Returns the max lightmap size for this face
//-----------------------------------------------------------------------------
int CMapFace::MaxLightmapSize() const
{
	return HasDisp() ? MAX_DISP_LIGHTMAP_DIM_WITHOUT_BORDER : MAX_BRUSH_LIGHTMAP_DIM_WITHOUT_BORDER;
}

//-----------------------------------------------------------------------------
// Purpose: Checks the validity of this face.
// Input  : pInfo - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapFace::CheckFace(CCheckFaceInfo *pInfo)
{
	if (!::CheckFace(Points, nPoints, &plane.normal, plane.dist, pInfo))
	{
		return(FALSE);
	}

	//
	// Check for duplicate plane points. All three plane points must be unique
	// or it isn't a valid plane.
	//
	for (int nPlane = 0; nPlane < 3; nPlane++)
	{
		for (int nPlaneCheck = 0; nPlaneCheck < 3; nPlaneCheck++)
		{
			if (nPlane != nPlaneCheck)
			{
				if (VectorCompare(plane.planepts[nPlane], plane.planepts[nPlaneCheck]))
				{
					if (pInfo != NULL)
					{
						strcpy(pInfo->szDescription, "face has duplicate plane points");
					}
					return(FALSE);		
				}
			}
		}
	}

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Included for loading old (quake-style) maps. This sets up the texture axes
//			the same way QCSG and pre-2.2 Hammer did.
// Input  : UAxis - 
//			VAxis - 
//-----------------------------------------------------------------------------
void CMapFace::InitializeQuakeStyleTextureAxes(Vector4D& UAxis, Vector4D& VAxis)
{
	static Vector baseaxis[18] =
	{
		Vector(0,0,1), Vector(1,0,0), Vector(0,-1,0),			// floor
		Vector(0,0,-1), Vector(1,0,0), Vector(0,-1,0),		// ceiling
		Vector(1,0,0), Vector(0,1,0), Vector(0,0,-1),			// west wall
		Vector(-1,0,0), Vector(0,1,0), Vector(0,0,-1),		// east wall
		Vector(0,1,0), Vector(1,0,0), Vector(0,0,-1),			// south wall
		Vector(0,-1,0), Vector(1,0,0), Vector(0,0,-1)			// north wall
	};

	int		bestaxis;
	vec_t	dot,best;
	int		i;
	
	best = 0;
	bestaxis = 0;
	
	for (i=0 ; i<6 ; i++)
	{
		dot = DotProduct(plane.normal, baseaxis[i*3]);
		if (dot > best)
		{
			best = dot;
			bestaxis = i;
		}
	}
	
	UAxis.AsVector3D() = baseaxis[bestaxis * 3 + 1];
	VAxis.AsVector3D() = baseaxis[bestaxis * 3 + 2];
}


//-----------------------------------------------------------------------------
// Should we render this lit or not
//-----------------------------------------------------------------------------
void CMapFace::RenderUnlit( bool enable )
{
	m_bIgnoreLighting = enable;
}



inline void Modulate( Color &pColor, float f )
{
	pColor[0] *= f;
	pColor[1] *= f;
	pColor[2] *= f;
}


//-----------------------------------------------------------------------------
// Computes the color and texture to use
//-----------------------------------------------------------------------------

void CMapFace::ComputeColor( CRender3D* pRender, bool bRenderAsSelected,
							 SelectionState_t faceSelectionState,
							 bool ignoreLighting, Color &pColor )
{
	EditorRenderMode_t eCurrentRenderMode = pRender->GetCurrentRenderMode();
	
	// White w/alpha by default
	pColor[0] = pColor[1] = pColor[2] = 255;
	pColor[3] = m_uchAlpha;

	float fShade;
	if (!ignoreLighting)
		fShade = pRender->LightPlane(plane.normal);
	else
		fShade = 1.0;

	switch (eCurrentRenderMode)
	{
	case RENDER_MODE_TEXTURED:
	case RENDER_MODE_TEXTURED_SHADED:
	case RENDER_MODE_LIGHT_PREVIEW2:
	case RENDER_MODE_LIGHT_PREVIEW_RAYTRACED:
		Modulate( pColor, fShade );
		break;

	case RENDER_MODE_SELECTION_OVERLAY:
		if( faceSelectionState == SELECT_MULTI_PARTIAL )
		{
			pColor[2] = 100;
			pColor[3] = 64;
		}
		else if( ( faceSelectionState == SELECT_NORMAL ) || bRenderAsSelected )
		{
			SelectFaceColor( pColor );
			pColor[3] = 64;
		}
		break;

	case RENDER_MODE_LIGHTMAP_GRID:
		if (bRenderAsSelected)
		{
			SelectFaceColor( pColor );
		}
		else if (texture.nLightmapScale > DEFAULT_LIGHTMAP_SCALE)
		{
			pColor[0] = 150;
		}
		else if (texture.nLightmapScale < DEFAULT_LIGHTMAP_SCALE)
		{
			pColor[2] = 100;
		}

		Modulate( pColor, fShade );
		break;

	case RENDER_MODE_TRANSLUCENT_FLAT:
	case RENDER_MODE_FLAT:
		if (bRenderAsSelected)
			SelectFaceColor( pColor );
		else
			pColor.SetColor( r,g,b,m_uchAlpha );
								
		Modulate( pColor, fShade );
		break;

	case RENDER_MODE_WIREFRAME:
		if (bRenderAsSelected)
			SelectEdgeColor( pColor );
		else
			pColor.SetColor( r,g,b,m_uchAlpha );
		
		break;

	case RENDER_MODE_SMOOTHING_GROUP:
		{
			// Render the non-smoothing group faces in white, yellow for the others.
			CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
			if ( pDoc )
			{
				int iGroup = pDoc->GetSmoothingGroupVisual();
				if ( InSmoothingGroup( iGroup ) )
				{
					pColor[2] = 0;
				}
			}

			Modulate( pColor, fShade );
			break;
		}

	default:
		assert(0);
		break;
	}
}


static bool ModeUsesTextureCoords(EditorRenderMode_t mode)
{
	return (
		(mode == RENDER_MODE_TEXTURED) ||
		(mode == RENDER_MODE_LIGHTMAP_GRID) || 
		(mode == RENDER_MODE_TEXTURED_SHADED) ||
		(mode == RENDER_MODE_LIGHT_PREVIEW2) ||
		(mode == RENDER_MODE_LIGHT_PREVIEW_RAYTRACED)
		);
}

//-----------------------------------------------------------------------------
// Draws the face using the material system material
//-----------------------------------------------------------------------------
void CMapFace::DrawFace( Color &pColor, EditorRenderMode_t mode )
{
	// retrieve the coordinate frame to render into 
	// (most likely just the identity, unless we're animating)
	VMatrix frame;
	bool hasParent = GetTransformMatrix( frame );

	// don't do this -- if you use the material system to rotate and/or translate
	// this will cull the locally spaced object!! -- need to pass around a flag!
#if 0 
	// A little culling....
	float fEyeDot = DotProduct(plane.normal, ViewPoint);
	if ((fEyeDot < plane.dist) && (mode != RENDER_MODE_WIREFRAME) && !hasParent && 
		(m_uchAlpha == 255))
	{
		return;
	}
#endif


	// don't draw no draws in ray tracced mode
	if ( mode == RENDER_MODE_LIGHT_PREVIEW_RAYTRACED )
	{
		if ( m_nFaceFlags & FACE_FLAGS_NODRAW_IN_LPREVIEW )
			return;
	}


	MaterialPrimitiveType_t type = (mode == RENDER_MODE_WIREFRAME) ? 
		MATERIAL_LINE_LOOP : MATERIAL_POLYGON; 

	CMeshBuilder meshBuilder;
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	meshBuilder.Begin( pMesh, type, nPoints );
    
    for (int nPoint = 0; nPoint < nPoints; nPoint++)
    {
		if (ModeUsesTextureCoords(mode))
		{
			meshBuilder.TexCoord2f( 0, m_pTextureCoords[nPoint][0], m_pTextureCoords[nPoint][1] );
			meshBuilder.TexCoord2f( 1, m_pLightmapCoords[nPoint][0], m_pLightmapCoords[nPoint][1]);
		}

		meshBuilder.Color4ubv( (byte*)&pColor );

        // transform into absolute space
        if ( hasParent )
        {
            Vector point;
            VectorTransform( Points[nPoint], frame.As3x4(), point );
            meshBuilder.Position3f(point[0], point[1], point[2]);
        }
        else
        {
            meshBuilder.Position3f(Points[nPoint][0], Points[nPoint][1], Points[nPoint][2]);
        }

		// FIXME: no smoothing group information
		meshBuilder.Normal3fv(plane.normal.Base());
		meshBuilder.TangentS3fv( m_pTangentAxes[nPoint].tangent.Base() );
		meshBuilder.TangentT3fv( m_pTangentAxes[nPoint].binormal.Base() );

		meshBuilder.AdvanceVertex();
    }
    
    meshBuilder.End();
	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Renders the grid on the face
//-----------------------------------------------------------------------------
void CMapFace::RenderGridIfCloseEnough( CRender3D* pRender )
{
	CMapFace *pThis = this;
	RenderGridsIfCloseEnough( pRender, 1, &pThis );
}


//-----------------------------------------------------------------------------
// renders the texture axes
//-----------------------------------------------------------------------------
void CMapFace::RenderTextureAxes( CRender3D* pRender )
{
	CMapFace *pThis = this;
	RenderTextureAxes( pRender, 1, &pThis );
}


//-----------------------------------------------------------------------------
// for sorting
//-----------------------------------------------------------------------------
bool CMapFace::ShouldRenderLast()
{
	if (!m_pTexture || !m_pTexture->GetMaterial())
		return false;

	return m_pTexture->GetMaterial()->IsTranslucent() || (m_uchAlpha != 255) ;
}

//-----------------------------------------------------------------------------
// render texture axes
//-----------------------------------------------------------------------------
void CMapFace::RenderTextureAxes( CRender3D* pRender, int nCount, CMapFace **ppFaces )
{
	// Render the world axes.
	pRender->PushRenderMode( RENDER_MODE_WIREFRAME );

	CMeshBuilder meshBuilder;
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	meshBuilder.Begin( pMesh, MATERIAL_LINES, 2 * nCount );

	Vector Center;
	for ( int i = 0; i < nCount; ++i )
	{
		ppFaces[i]->GetCenter(Center);

		meshBuilder.Color3ub(255, 255, 0);
		meshBuilder.Position3f(Center[0], Center[1], Center[2]);
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3ub(255, 255, 0);
		meshBuilder.Position3f(Center[0] + ppFaces[i]->texture.UAxis[0] * TEXTURE_AXIS_LENGTH, 
			Center[1] + ppFaces[i]->texture.UAxis[1] * TEXTURE_AXIS_LENGTH, 
			Center[2] + ppFaces[i]->texture.UAxis[2] * TEXTURE_AXIS_LENGTH);
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3ub(0, 255, 0);
		meshBuilder.Position3f(Center[0], Center[1], Center[2]);
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3ub(0, 255, 0);
		meshBuilder.Position3f(Center[0] + ppFaces[i]->texture.VAxis[0] * TEXTURE_AXIS_LENGTH, 
			Center[1] + ppFaces[i]->texture.VAxis[1] * TEXTURE_AXIS_LENGTH, 
			Center[2] + ppFaces[i]->texture.VAxis[2] * TEXTURE_AXIS_LENGTH);
		meshBuilder.AdvanceVertex();
	}
		
	meshBuilder.End();
	pMesh->Draw();

	pRender->PopRenderMode();
}


//-----------------------------------------------------------------------------
// Render grids
//-----------------------------------------------------------------------------
void CMapFace::Render3DGrids( CRender3D *pRender, int nCount, CMapFace **ppFaces )
{
	// FIXME: Optimize this to render all of them in a single call
	for ( int i = 0; i < nCount; ++i )
	{
		ppFaces[i]->Render3DGrid( pRender );
	}
}


//-----------------------------------------------------------------------------
// Render grids
//-----------------------------------------------------------------------------
void CMapFace::RenderGridsIfCloseEnough( CRender3D* pRender, int nCount, CMapFace **ppFaces )
{
	// If the 3D grid is enabled and we aren't picking, 
	// render the grid on this face.
	if ( (!pRender->IsEnabled(RENDER_GRID)) || pRender->IsPicking() )
		return;

	Vector Maxs;
	Vector Mins;
	float fGridSize = pRender->GetGridDistance();

	Vector viewPoint; pRender->GetCamera()->GetViewPoint( viewPoint );

	CMapFace **ppFinalList = (CMapFace**)_alloca( nCount * sizeof(CMapFace*) );
	int nFinalCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		ppFaces[i]->GetFaceBounds(Mins, Maxs);

		for ( int j = 0; j < 3; j++)
		{
			Mins[j] -= fGridSize;
			Maxs[j] += fGridSize;
		}

		// Only render the grid if the face is close enough to the camera.
		if ( IsPointInBox(viewPoint, Mins, Maxs) )
		{
			ppFinalList[nFinalCount++] = ppFaces[i];
		}
	}

	Render3DGrids( pRender, nFinalCount, ppFinalList );
}


//-----------------------------------------------------------------------------
// Adds a face's vertices to the meshbuilder
//-----------------------------------------------------------------------------
void CMapFace::AddFaceVertices( CMeshBuilder &meshBuilder, CRender3D* pRender, bool bRenderSelected, SelectionState_t faceSelectionState)
{
	Vector point;
	VMatrix frame;
	Color color;

	bool bHasParent = GetTransformMatrix( frame );
	ComputeColor( pRender, bRenderSelected, faceSelectionState, m_bIgnoreLighting, color );

	for ( int nPoint = 0; nPoint < nPoints; nPoint++ )
	{
		if ( bHasParent )
		{
			// transform into absolute space
			VectorTransform( Points[nPoint], frame.As3x4(), point );
			meshBuilder.Position3fv( point.Base() );
		}
		else
		{
			meshBuilder.Position3fv( Points[nPoint].Base() );
		}

		meshBuilder.Normal3fv( plane.normal.Base() );
		meshBuilder.Color4ubv( (byte*)&color );

		meshBuilder.TexCoord2fv( 0, m_pTextureCoords[nPoint].Base() );
		meshBuilder.TexCoord2fv( 1, m_pLightmapCoords[nPoint].Base() );
		meshBuilder.TangentS3fv( m_pTangentAxes[nPoint].tangent.Base() );
		meshBuilder.TangentT3fv( m_pTangentAxes[nPoint].binormal.Base() );

		meshBuilder.AdvanceVertex();
	}
}


struct MapFaceRender_t
{
	bool m_RenderSelected;
	EditorRenderMode_t m_RenderMode;
	IEditorTexture* m_pTexture;
	CMapFace* m_pMapFace;
	SelectionState_t m_FaceSelectionState;
};

typedef CUtlRBTree<MapFaceRender_t, int>	FaceQueue_t;


//-----------------------------------------------------------------------------
// draws a list of faces in wireframe
//-----------------------------------------------------------------------------
void CMapFace::RenderWireframeFaces( CRender3D* pRender, int nCount, MapFaceRender_t **ppFaces )
{

	// Draw the texture axes
	int nAxesCount = 0;
	CMapFace **ppAxesFaces = (CMapFace**)_alloca( nCount * sizeof(CMapFace*) );
	for ( int i = 0; i < nCount; ++i )
	{
		if ( ppFaces[i]->m_FaceSelectionState != SELECT_NONE )
		{
			ppAxesFaces[ nAxesCount++ ] = ppFaces[i]->m_pMapFace;
		}
	}

	if ( nAxesCount != 0 )
	{
		RenderTextureAxes( pRender, nAxesCount, ppAxesFaces );
	}

	if ( pRender->IsEnabled(RENDER_GRID) )
	{
		// Draw the grid
		CMapFace **ppGridFaces = (CMapFace**)_alloca( nCount * sizeof(CMapFace*) );
		for ( int i = 0; i < nCount; ++i )
		{
			ppGridFaces[i] = ppFaces[i]->m_pMapFace;
		}

		RenderGridsIfCloseEnough( pRender, nCount, ppGridFaces );
	}

}

//-----------------------------------------------------------------------------
// Draws a batch of faces.
//-----------------------------------------------------------------------------
void CMapFace::RenderFacesBatch( CMeshBuilder &meshBuilder, IMesh* pMesh, CRender3D* pRender, MapFaceRender_t **ppFaces, int nFaceCount, int nVertexCount, int nIndexCount, bool bWireframe )
{
	if ( bWireframe )
	{
		meshBuilder.Begin( pMesh, MATERIAL_LINES, nVertexCount, nIndexCount );
	}
	else
	{
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nVertexCount, nIndexCount );
	}
	
	int nFirstVertex = 0;
	
	for ( int i = 0; i < nFaceCount; ++i )
	{
		CMapFace *pMapFace = ppFaces[i]->m_pMapFace;

		pMapFace->AddFaceVertices( meshBuilder,	pRender, ppFaces[i]->m_RenderSelected, ppFaces[i]->m_FaceSelectionState );

		int nPoints = pMapFace->GetPointCount();

		if ( bWireframe )
		{
			meshBuilder.FastIndex( nFirstVertex );
			for ( int j = 1; j < nPoints; ++j )
			{
				meshBuilder.FastIndex( nFirstVertex + j );
				meshBuilder.FastIndex( nFirstVertex + j );
			}
			meshBuilder.FastIndex( nFirstVertex );
		}
		else
		{
			for ( int j = 2; j < nPoints; ++j )
			{
				meshBuilder.FastIndex( nFirstVertex );
				meshBuilder.FastIndex( nFirstVertex + j - 1 );
				meshBuilder.FastIndex( nFirstVertex + j );
			}
		}

		nFirstVertex += nPoints;
	}
	
	meshBuilder.End();
	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Draws a list of faces, breaking them up into batches if necessary.
//-----------------------------------------------------------------------------
void CMapFace::RenderFaces( CRender3D* pRender, int nCount, MapFaceRender_t **ppFaces )
{
	if ( nCount == 0 )
		return;

	bool bWireframe = ppFaces[0]->m_RenderMode == RENDER_MODE_WIREFRAME;

	if ( RenderingModeIsTextured(ppFaces[0]->m_RenderMode))
	{
		pRender->BindTexture( ppFaces[0]->m_pTexture );
	}

	pRender->PushRenderMode( ppFaces[0]->m_RenderMode );

	int nBatchStart = 0;
	int nIndexCount = 0;
	int nVertexCount = 0;

	int nMaxVerts, nMaxIndices;

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();

	pRenderContext->GetMaxToRender( pMesh, true, &nMaxVerts, &nMaxIndices );

	// Make sure we have enough for at least one triangle...
	int nMinVerts = ppFaces[0]->m_pMapFace->GetPointCount();
	int nMinIndices = max( nMinVerts*2, (nMinVerts-2)*3 );
	if ( nMaxVerts < nMinVerts || nMaxIndices < nMinIndices )
	{
		pRenderContext->GetMaxToRender( pMesh, false, &nMaxVerts, &nMaxIndices );
	}

	CMeshBuilder meshBuilder;	
	for ( int nFace = 0; nFace < nCount; nFace++ )
	{
		Assert( ppFaces[nFace]->m_RenderMode == ppFaces[0]->m_RenderMode );
		Assert( ppFaces[nFace]->m_pTexture == ppFaces[0]->m_pTexture );

		int newIndices, newVertices = ppFaces[nFace]->m_pMapFace->GetPointCount();
	
		if( bWireframe )
		{
			newIndices = newVertices*2;
		}
		else
		{
			newIndices = (newVertices-2) * 3;
		}
		
		if ( ( ( nVertexCount + newVertices ) > nMaxVerts ) || ( ( nIndexCount + newIndices )  > nMaxIndices ) )
		{
			// If we hit this assert, there's a single face that's too big for the meshbuilder to handle!
			Assert( ( nFace - nBatchStart ) > 0 );
		
			// We have a full batch, render it.
			
			RenderFacesBatch( meshBuilder, pMesh, pRender, &ppFaces[nBatchStart], nFace - nBatchStart, nVertexCount, nIndexCount, bWireframe );

			pRenderContext->GetMaxToRender( pMesh, false, &nMaxVerts, &nMaxIndices );

			nBatchStart = nFace;
			nVertexCount = 0;
			nIndexCount = 0;
		}

		nVertexCount += newVertices;
		nIndexCount += newIndices;
	}

	// Render whatever is left over.
	RenderFacesBatch( meshBuilder, pMesh, pRender, &ppFaces[nBatchStart], nCount - nBatchStart, nVertexCount, nIndexCount, bWireframe );
	
	//render additional wireframe stuff
	if ( bWireframe )
	{
		RenderWireframeFaces( pRender, nCount, ppFaces );
	}

	pRender->PopRenderMode();
}


//-----------------------------------------------------------------------------
// draws a single face (displacement or normal)
//-----------------------------------------------------------------------------
void CMapFace::RenderFace3D( CRender3D* pRender, EditorRenderMode_t renderMode, bool renderSelected, SelectionState_t faceSelectionState )
{
	pRender->PushRenderMode( renderMode );

	if ( HasDisp() && CMapDoc::GetActiveMapDoc() && CMapDoc::GetActiveMapDoc()->IsDispDraw3D() )
	{
		CMapDisp *pDisp = EditDispMgr()->GetDisp( m_DispHandle );
		pDisp->Render3D( pRender, renderSelected, faceSelectionState );
	}
	else
	{
		Color color;
		ComputeColor( pRender, renderSelected, faceSelectionState, m_bIgnoreLighting, color );
  		DrawFace( color, renderMode );
	}

	// Draw the texture axes
	if( renderMode == RENDER_MODE_WIREFRAME )
	{
		if (faceSelectionState != SELECT_NONE)
			RenderTextureAxes(pRender);

		// Draw the grid
		RenderGridIfCloseEnough( pRender );
	} 
	else if ( m_pDetailObjects && Options.general.bShowDetailObjects )
	{

		// Only draw the detailed objects if the displacement/face is not currently selected.
		pRender->AddTranslucentDeferredRendering( m_pDetailObjects );
	}

	pRender->PopRenderMode();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mode - 
//-----------------------------------------------------------------------------
static int SortVal(EditorRenderMode_t mode)
{
	if (mode == RENDER_MODE_WIREFRAME)
		return 2;
	if ( mode == RENDER_MODE_SELECTION_OVERLAY )
		return 1;
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : s1 - 
//			s2 - 
// Output : 
//-----------------------------------------------------------------------------
static bool OpaqueFacesLessFunc( const MapFaceRender_t &s1, const MapFaceRender_t &s2 )
{
	// Render texture first, overlay second, wireframe 3rd
	int nSort1 = SortVal(s1.m_RenderMode);
	int nSort2 = SortVal(s2.m_RenderMode);
	if (nSort1 < nSort2)
		return true;
	if (nSort1 > nSort2)
		return false;

	return s1.m_pTexture < s2.m_pTexture;
}


static FaceQueue_t g_OpaqueFaces(0, 0, OpaqueFacesLessFunc);
static CUtlVector< FaceQueue_t * >	g_OpaqueInstanceFaces;
static FaceQueue_t *g_CurrentOpaqueFaces = &g_OpaqueFaces;


//-----------------------------------------------------------------------------
// Purpose: this function will add the face to the sorted current queue
// Input  : pMapFace - the face to be added
//			pTexture - the texture of the face
//			renderMode - what type of rendering mode
//			selected - if it is selected or not ( selected appears on top )
//			faceSelectionState - if the face is individual selected
//-----------------------------------------------------------------------------
void CMapFace::AddFaceToQueue( CMapFace* pMapFace, IEditorTexture* pTexture, EditorRenderMode_t renderMode, bool selected, SelectionState_t faceSelectionState )
{
	MapFaceRender_t newEntry;
	newEntry.m_RenderMode = renderMode;
	newEntry.m_pTexture = pTexture;
	newEntry.m_RenderSelected = selected;
	newEntry.m_pMapFace = pMapFace;
	newEntry.m_FaceSelectionState = faceSelectionState;
	g_CurrentOpaqueFaces->Insert( newEntry );
}


//-----------------------------------------------------------------------------
// Purpose: this function will add a new face queue to the top of the list and 
//			make it active
//-----------------------------------------------------------------------------
void CMapFace::PushFaceQueue( void )
{
	g_OpaqueInstanceFaces.AddToHead( new FaceQueue_t( 0, 0, OpaqueFacesLessFunc ) );

	g_CurrentOpaqueFaces = g_OpaqueInstanceFaces.Head();
}


//-----------------------------------------------------------------------------
// Purpose: this function will pop the top face queue off the list
//-----------------------------------------------------------------------------
void CMapFace::PopFaceQueue( void )
{
	Assert( g_OpaqueInstanceFaces.Count() > 0 );

	FaceQueue_t *pHead = g_OpaqueInstanceFaces.Head();

	g_OpaqueInstanceFaces.Remove( 0 );
	delete pHead;

	if ( g_OpaqueInstanceFaces.Count() )
	{
		g_CurrentOpaqueFaces = g_OpaqueInstanceFaces.Head();
	}
	else
	{
		g_CurrentOpaqueFaces = &g_OpaqueFaces;
	}
}


//-----------------------------------------------------------------------------
// renders queued up opaque faces, sorted by material
//-----------------------------------------------------------------------------
void CMapFace::RenderOpaqueFaces( CRender3D* pRender )
{
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );

	MapFaceRender_t **ppMapFaces = (MapFaceRender_t**)_alloca( g_CurrentOpaqueFaces->Count() * sizeof( MapFaceRender_t* ) );
	int nFaceCount = 0;

	int nLastRenderMode = RENDER_MODE_NONE;
	IEditorTexture *pLastTexture = NULL;

	for ( int i = g_CurrentOpaqueFaces->FirstInorder(); i != g_CurrentOpaqueFaces->InvalidIndex(); i = g_CurrentOpaqueFaces->NextInorder(i) )
	{
		MapFaceRender_t& mapFace = ( *g_CurrentOpaqueFaces)[i];

		if ( ( mapFace.m_RenderMode != nLastRenderMode ) || ( mapFace.m_pTexture != pLastTexture ) )
		{
			RenderFaces( pRender, nFaceCount, ppMapFaces );
			nFaceCount = 0;
		}

		if ( mapFace.m_pMapFace->HasDisp() )
		{
			if ( RenderingModeIsTextured( mapFace.m_RenderMode ))
			{
				pRender->BindTexture( mapFace.m_pTexture );
			}

			mapFace.m_pMapFace->RenderFace3D( pRender, mapFace.m_RenderMode, mapFace.m_RenderSelected, mapFace.m_FaceSelectionState );
		}
		else
		{
			ppMapFaces[ nFaceCount++ ] = &mapFace;
			nLastRenderMode = mapFace.m_RenderMode;
		    pLastTexture = mapFace.m_pTexture;
		}
	}

	RenderFaces( pRender, nFaceCount, ppMapFaces ); 

	g_CurrentOpaqueFaces->RemoveAll();
}


void CMapFace::Render2D(CRender2D *pRender)
{	
	SelectionState_t eFaceSelectionState = GetSelectionState();
	SelectionState_t eSolidSelectionState;
	if (m_pParent != NULL)
	{
		eSolidSelectionState = m_pParent->GetSelectionState();
	}
	else
	{
		eSolidSelectionState = eFaceSelectionState;
	}

	bool bRenderSelected = ( eSolidSelectionState != SELECT_NONE );
	bRenderSelected = bRenderSelected || ( ( eFaceSelectionState != SELECT_NONE ) && (CMapFace::m_bShowFaceSelection) );

	Vector vViewNormal; pRender->GetCamera()->GetViewForward( vViewNormal );
	Vector vNormal; GetFaceNormal( vNormal );

	// if face is parallel to view axis, skip it
	bool bIsParallel = ( fabs( vViewNormal.Dot( vNormal) ) < 0.0001f );
		
	if ( HasDisp() && ( bIsParallel || bRenderSelected ) )
	{
		Vector mins,maxs;

		GetRender2DBox( mins,maxs );

		Vector2D pt,pt2;
		pRender->TransformPoint(pt, mins );
		pRender->TransformPoint(pt2, maxs );

		int sizeX = abs(pt2.x-pt.x);
		int sizeY = abs(pt2.y-pt.y);

		bool bDrawDispMap = Options.view2d.bDrawModels && ( (sizeX+sizeY) > 50 || bRenderSelected );

		if ( bDrawDispMap )
		{
			CMapDisp *pDisp = EditDispMgr()->GetDisp( m_DispHandle );
			pDisp->Render2D( pRender, bRenderSelected, eFaceSelectionState );
			return;
		}
	}
	
	if ( !bIsParallel )
	{
		pRender->DrawPolyLine( nPoints, Points );
	}
}

void CMapFace::RenderVertices(CRender *pRender)
{
	for ( int i=0; i< nPoints;i++ )
		pRender->DrawHandle( Points[i] );
}


//-----------------------------------------------------------------------------
// Purpose: Renders this face using the given 3D renderer.
// Input  : pRender - Renderer to draw with. 
//-----------------------------------------------------------------------------
void CMapFace::Render3D( CRender3D *pRender )
{
	if (nPoints == 0)
	{
		return;
	}

	//
	// Skip back faces unless rendering in wireframe.
	//
	EditorRenderMode_t eCurrentRenderMode = pRender->GetCurrentRenderMode();

	if ( eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW_RAYTRACED )
	{
		if ( m_nFaceFlags & FACE_FLAGS_NODRAW_IN_LPREVIEW )
			return;
	}

	SelectionState_t eFaceSelectionState = GetSelectionState();
	SelectionState_t eSolidSelectionState;
	if (m_pParent != NULL)
	{
		eSolidSelectionState = m_pParent->GetSelectionState();
	}
	else
	{
		eSolidSelectionState = eFaceSelectionState;
	}

	if ( !Options.general.bShowNoDrawBrushes && eSolidSelectionState == SELECT_NONE && m_pTexture == g_Textures.GetNoDrawTexture() )
		return;

	//
	// Draw the face.
	//
	bool renderSelected = ( ( eSolidSelectionState != SELECT_NONE ) );
	renderSelected = renderSelected || ( ( eFaceSelectionState != SELECT_NONE ) && (CMapFace::m_bShowFaceSelection) );

	if (pRender->DeferRendering())
	{
		AddFaceToQueue( this, m_pTexture, eCurrentRenderMode, renderSelected, eFaceSelectionState );
		if ( ( renderSelected && pRender->NeedsOverlay() ) )
		{
			AddFaceToQueue( this, m_pTexture, RENDER_MODE_SELECTION_OVERLAY, renderSelected, eFaceSelectionState );
		}

	}
	else
	{
		// Set up the texture to use
		pRender->BindTexture( m_pTexture );

		RenderFace3D( pRender, eCurrentRenderMode, renderSelected, eFaceSelectionState );
		if ( ( renderSelected && pRender->NeedsOverlay() ) )
		{
			RenderFace3D( pRender, RENDER_MODE_SELECTION_OVERLAY, renderSelected, eFaceSelectionState );
		}
    }
}


//-----------------------------------------------------------------------------
// Purpose: Renders the world grid projected onto the given face.
// Input  : pFace - The face onto which the grid will be projected.
//-----------------------------------------------------------------------------
void CMapFace::Render3DGrid(CRender3D *pRender)
{
	//
	// Determine the extents of this face.
	//
	Extents_t Extents;
	float fDelta[3];
	float fGridSpacing = pRender->GetGridSize();

	GetFaceExtents(Extents);

	fDelta[0] = Extents[EXTENTS_XMAX][0] - Extents[EXTENTS_XMIN][0];
	fDelta[1] = Extents[EXTENTS_YMAX][1] - Extents[EXTENTS_YMIN][1];
	fDelta[2] = Extents[EXTENTS_ZMAX][2] - Extents[EXTENTS_ZMIN][2];

	//
	// Render the grid lines with wireframe material.
	//
	pRender->PushRenderMode( RENDER_MODE_WIREFRAME );

	CMeshBuilder meshBuilder;
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();

	//
	// For every dimension in which this face has a nonzero projection.
	//
	for (int nDim = 0; nDim < 3; nDim++)
	{
		if (fDelta[nDim] != 0)
		{
			Vector Normal;

			Normal[0] = (float)((nDim  % 3) == 0);
			Normal[1] = (float)((nDim  % 3) == 1);
			Normal[2] = (float)((nDim  % 3) == 2);

			float fMin = Extents[nDim * 2][nDim];
			float fMax = Extents[(nDim * 2) + 1][nDim];

			float fStart = (float)(floor(fMin / fGridSpacing) * fGridSpacing);
			float fEnd = (float)(ceil(fMax / fGridSpacing) * fGridSpacing);

			float fGridPoint = fStart;

			while (fGridPoint < fEnd)
			{
				int nPointsFound = 0;

				//
				// For every edge.
				//
				for (int nPoint = 0; nPoint < nPoints; nPoint++)
				{
					Vector PointFound[2];

					//
					// Get the start and end points of the edge.
					//
					Vector Point1 = Points[nPoint];

					Vector Point2;
					if (nPoint < nPoints - 1)
					{
						Point2 = Points[nPoint + 1];
					}
					else
					{
						Point2 = Points[0];
					}

					// 
					// If there is a projection of the normal vector along this edge.
					//
					if (Point2[nDim] != Point1[nDim])
					{
						//
						// Solve for the point along this edge that intersects the grid line
						// as a parameter from zero to one.
						//
						float fScale = (fGridPoint - Point1[nDim]) / (Point2[nDim] - Point1[nDim]);
						if ((fScale >= 0) && (fScale <= 1))
						{
							PointFound[nPointsFound][0] = Point1[0] + (Point2[0] - Point1[0]) * fScale;
							PointFound[nPointsFound][1] = Point1[1] + (Point2[1] - Point1[1]) * fScale;
							PointFound[nPointsFound][2] = Point1[2] + (Point2[2] - Point1[2]) * fScale;

							nPointsFound++;

							if (nPointsFound == 2)
							{
								Vector RenderPoint;

								meshBuilder.Begin( pMesh, MATERIAL_LINES, 1 );

								VectorMA(PointFound[0], 0.2, plane.normal, RenderPoint);
								meshBuilder.Position3f(RenderPoint[0], RenderPoint[1], RenderPoint[2]);
								meshBuilder.Color3ub(Normal[0] * 255, Normal[1] * 255, Normal[2] * 255);
								meshBuilder.AdvanceVertex();

								VectorMA(PointFound[1], 0.2, plane.normal, RenderPoint);
								meshBuilder.Position3f(RenderPoint[0], RenderPoint[1], RenderPoint[2]);
								meshBuilder.Color3ub(Normal[0] * 255, Normal[1] * 255, Normal[2] * 255);
								meshBuilder.AdvanceVertex();

								meshBuilder.End();
								pMesh->Draw();

								nPointsFound = 0;
							}
						}
					}
				}
			
				fGridPoint += fGridSpacing;
			}
		}
	}

	pRender->PopRenderMode();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pLoadInfo - 
//			pFace - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapFace::LoadDispInfoCallback(CChunkFile *pFile, CMapFace *pFace)
{
	SignalUpdate( EVTYPE_FACE_CHANGED );
	// allocate a displacement (for the face)
	EditDispHandle_t dispHandle = EditDispMgr()->Create();
	CMapDisp *pDisp = EditDispMgr()->GetDisp( dispHandle );	

	//
	// load the displacement info and set relationships
	//
	ChunkFileResult_t eResult = pDisp->LoadVMF( pFile );
	if( eResult == ChunkFile_Ok )
	{
		pDisp->SetParent( pFace );
		pFace->SetDisp( dispHandle );

		CMapWorld *pWorld = GetActiveWorld();
		if( pWorld )
		{
			IWorldEditDispMgr *pDispMgr = pWorld->GetWorldEditDispManager();
			if( pDispMgr )
			{
				pDispMgr->AddToWorld( dispHandle );
			}
		}
	}

	return( eResult );
}


//-----------------------------------------------------------------------------
// Purpose: Handles key values when loading a VMF file.
// Input  : szKey - Key being handled.
//			szValue - Value of the key in the VMF file.
// Output : Returns ChunkFile_Ok or an error if there was a parsing error.
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapFace::LoadKeyCallback(const char *szKey, const char *szValue, LoadFace_t *pLoadFace)
{
	SignalUpdate( EVTYPE_FACE_CHANGED );
	CMapFace *pFace = pLoadFace->pFace;

	if (!stricmp(szKey, "id"))
	{
		CChunkFile::ReadKeyValueInt(szValue, pFace->m_nFaceID);
	}
	else if (!stricmp(szKey, "rotation"))
	{
		pFace->texture.rotate = atof(szValue);
	}
	else if (!stricmp(szKey, "plane"))
	{
		int nRead = sscanf(szValue, "(%f %f %f) (%f %f %f) (%f %f %f)", 
			&pFace->plane.planepts[0][0], &pFace->plane.planepts[0][1], &pFace->plane.planepts[0][2],
			&pFace->plane.planepts[1][0], &pFace->plane.planepts[1][1], &pFace->plane.planepts[1][2],
			&pFace->plane.planepts[2][0], &pFace->plane.planepts[2][1], &pFace->plane.planepts[2][2]);

		if (nRead != 9)
		{
			// TODO: need specific error message
			return(ChunkFile_Fail);
		}
	}
	else if (!stricmp(szKey, "material"))
	{
		strcpy(pLoadFace->szTexName, szValue);
	}
	else if (!stricmp(szKey, "uaxis"))
	{
		int nRead = sscanf(szValue, "[%f %f %f %f] %f",
			&pFace->texture.UAxis[0], &pFace->texture.UAxis[1], &pFace->texture.UAxis[2], &pFace->texture.UAxis[3], &pFace->texture.scale[0]);

		if (nRead != 5)
		{
			// TODO: need specific error message
			return(ChunkFile_Fail);
		}
	}
	else if (!stricmp(szKey, "vaxis"))
	{
		int nRead = sscanf(szValue, "[%f %f %f %f] %f",
			&pFace->texture.VAxis[0], &pFace->texture.VAxis[1], &pFace->texture.VAxis[2], &pFace->texture.VAxis[3], &pFace->texture.scale[1]);

		if (nRead != 5)
		{
			// TODO: need specific error message
			return(ChunkFile_Fail);
		}
	}
	else if (!stricmp(szKey, "lightmapscale"))
	{
		pFace->texture.nLightmapScale = atoi(szValue);
	}
	else if (!stricmp(szKey, "smoothing_groups"))
	{
		pFace->m_fSmoothingGroups = atoi(szValue);
	}

	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: Loads a face chunk from the VMF file.
// Input  : pFile - Chunk file being loaded.
// Output : Returns ChunkFile_Ok or an error if there was a parsing error.
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapFace::LoadVMF(CChunkFile *pFile)
{
	SignalUpdate( EVTYPE_FACE_CHANGED );
	//
	// Set up handlers for the subchunks that we are interested in.
	//
	CChunkHandlerMap Handlers;
	Handlers.AddHandler("dispinfo", (ChunkHandler_t)LoadDispInfoCallback, this);

	//
	// Read the keys and sub-chunks.
	//
	LoadFace_t LoadFace;
	memset(&LoadFace, 0, sizeof(LoadFace));
	LoadFace.pFace = this;

	pFile->PushHandlers(&Handlers);
	ChunkFileResult_t eResult = pFile->ReadChunk((KeyHandler_t)LoadKeyCallback, &LoadFace);
	pFile->PopHandlers();

	if (eResult == ChunkFile_Ok)
	{
		CalcPlane();
		SetTexture(LoadFace.szTexName);
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: Called after this object is added to the world.
//
//			NOTE: This function is NOT called during serialization. Use PostloadWorld
//				  to do similar bookkeeping after map load.
//
// Input  : pWorld - The world that we have been added to.
//-----------------------------------------------------------------------------
void CMapFace::OnAddToWorld(CMapWorld *pWorld)
{
	SignalUpdate( EVTYPE_FACE_CHANGED );
	if (HasDisp())
	{
		//
		// Add it to the world displacement list.
		//
		IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
		if (pDispMgr != NULL)
		{
			pDispMgr->AddToWorld( m_DispHandle );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called just after this object has been removed from the world so
//			that it can unlink itself from other objects in the world.
// Input  : pWorld - The world that we were just removed from.
//			bNotifyChildren - Whether we should forward notification to our children.
//-----------------------------------------------------------------------------
void CMapFace::OnRemoveFromWorld(void)
{
	SignalUpdate( EVTYPE_FACE_CHANGED );
	if (HasDisp())
	{
		//
		// Add it to the world displacement list.
		//
		IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
		if (pDispMgr != NULL)
		{
			pDispMgr->RemoveFromWorld( m_DispHandle );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapFace::SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	NormalizeTextureShifts();

	//
	// Check for duplicate plane points. All three plane points must be unique
	// or it isn't a valid plane. Try to fix it if it isn't valid.
	//
	if (!CheckFace())
	{
		Fix();
	}

	ChunkFileResult_t eResult = pFile->BeginChunk("side");

	char szBuf[512];

	//
	// Write our unique face ID.
	//
	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueInt("id", m_nFaceID);
	}

	//
	// Write the plane information.
	//
	if (eResult == ChunkFile_Ok)
	{
		sprintf(szBuf, "(%g %g %g) (%g %g %g) (%g %g %g)",
				(double)plane.planepts[0][0], (double)plane.planepts[0][1], (double)plane.planepts[0][2],
				(double)plane.planepts[1][0], (double)plane.planepts[1][1], (double)plane.planepts[1][2],
				(double)plane.planepts[2][0], (double)plane.planepts[2][1], (double)plane.planepts[2][2]);

		eResult = pFile->WriteKeyValue("plane", szBuf);
	}

	if (eResult == ChunkFile_Ok)
	{
		char szTexture[MAX_PATH];
		strcpy(szTexture, texture.texture);
		strupr(szTexture);

		eResult = pFile->WriteKeyValue("material", szTexture);
	}

	if (eResult == ChunkFile_Ok)
	{
		sprintf(szBuf, "[%g %g %g %g] %g", (double)texture.UAxis[0], (double)texture.UAxis[1], (double)texture.UAxis[2], (double)texture.UAxis[3], (double)texture.scale[0]);
		eResult = pFile->WriteKeyValue("uaxis", szBuf);
	}

	if (eResult == ChunkFile_Ok)
	{
		sprintf(szBuf, "[%g %g %g %g] %g", (double)texture.VAxis[0], (double)texture.VAxis[1], (double)texture.VAxis[2], (double)texture.VAxis[3], (double)texture.scale[1]);
		eResult = pFile->WriteKeyValue("vaxis", szBuf);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueFloat("rotation", texture.rotate);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueFloat("lightmapscale", texture.nLightmapScale);
	}

	// Save smoothing group data.
	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueInt("smoothing_groups", m_fSmoothingGroups );
	}

	//
	// Write the displacement chunk.
	//
	if ((eResult == ChunkFile_Ok) && (HasDisp()))
	{
		CMapDisp *pDisp = EditDispMgr()->GetDisp( m_DispHandle );
		eResult = pDisp->SaveVMF(pFile, pSaveInfo);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->EndChunk();
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables the special rendering of selected faces.
// Input  : bShowSelection - true to enable, false to disable.
//-----------------------------------------------------------------------------
void CMapFace::SetShowSelection(bool bShowSelection)
{
	CMapFace::m_bShowFaceSelection = bShowSelection;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nPoint - 
//			u - 
//			v - 
//-----------------------------------------------------------------------------
void CMapFace::SetTextureCoords(int nPoint, float u, float v)
{
	if (nPoint < nPoints)
	{
		m_pTextureCoords[nPoint][0] = u;
		m_pTextureCoords[nPoint][1] = v;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
size_t CMapFace::GetDataSize( void )
{
	// get base map class size
	size_t size = sizeof( CMapFace );
	
	//
	// better approximate by added in verts, texture coordinates, 
	// and lightmap coordinates
	//
	size += ( sizeof( Vector ) * nPoints );
	size += ( sizeof( Vector2D ) * ( nPoints * 2 ) );

	// add displacement size if necessary
	if( HasDisp() )
	{
		CMapDisp *pDisp = EditDispMgr()->GetDisp( m_DispHandle );
		size += pDisp->GetSize();
	}

	return size;
}


//-----------------------------------------------------------------------------
// Purpose: Returns our bounds for 2D rendering. These bounds do not consider
//			any displacement information.
// Input  : boundMin - 
//			boundMax - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapFace::GetRender2DBox( Vector& boundMin, Vector& boundMax )
{
	// valid face?
	if( nPoints == 0 )
		return false;

	//
	// find min and maximum points on face
	//
	VectorFill( boundMin, COORD_NOTINIT );
	VectorFill( boundMax, -COORD_NOTINIT );
	for( int i = 0; i < nPoints; i++ )
	{
		if( Points[i][0] < boundMin[0] ) { boundMin[0] = Points[i][0]; }
		if( Points[i][1] < boundMin[1] ) { boundMin[1] = Points[i][1]; }
		if( Points[i][2] < boundMin[2] ) { boundMin[2] = Points[i][2]; }

		if( Points[i][0] > boundMax[0] ) { boundMax[0] = Points[i][0]; }
		if( Points[i][1] > boundMax[1] ) { boundMax[1] = Points[i][1]; }
		if( Points[i][2] > boundMax[2] ) { boundMax[2] = Points[i][2]; }
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns our bounds for frustum culling, including the bounds of
//			any displacement information.
// Input  : boundMin - 
//			boundMax - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapFace::GetCullBox( Vector& boundMin, Vector& boundMax )
{
	// get the face bounds
	if( !GetRender2DBox( boundMin, boundMax ) )
		return false;

	//
	// add displacement to bounds
	//
	if( HasDisp() )
	{
		CMapDisp *pDisp = EditDispMgr()->GetDisp( m_DispHandle );

		Vector bbox[2];
		pDisp->GetBoundingBox( bbox[0], bbox[1] );

		for( int i = 0; i < 2; i++ )
		{
			if( bbox[i][0] < boundMin[0] ) { boundMin[0] = bbox[i][0]; }
			if( bbox[i][1] < boundMin[1] ) { boundMin[1] = bbox[i][1]; }
			if( bbox[i][2] < boundMin[2] ) { boundMin[2] = bbox[i][2]; }
			
			if( bbox[i][0] > boundMax[0] ) { boundMax[0] = bbox[i][0]; }
			if( bbox[i][1] > boundMax[1] ) { boundMax[1] = bbox[i][1]; }
			if( bbox[i][2] > boundMax[2] ) { boundMax[2] = bbox[i][2]; }
		}
	}

	return true;
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : HitPos - 
//			Start - 
//			End - 
// Output : Returns true if the ray intersected the face, false if not.
//-----------------------------------------------------------------------------
bool CMapFace::TraceLine(Vector &HitPos, Vector &HitNormal, Vector const &Start, Vector const &End )
{
	if (HasDisp())
	{
		CMapDisp *pDisp = EditDispMgr()->GetDisp( m_DispHandle );
		return( pDisp->TraceLine( HitPos, HitNormal, Start, End ) );
	}

	//
	// Find the point of intersection of the ray with the given plane.
	//
	float t = Start.Dot(plane.normal) - plane.dist;
	t = t / -(End - Start).Dot(plane.normal);
	
	HitPos = Start + (t * (End - Start));
	HitNormal = plane.normal;
	return(true);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMapFace::TraceLineInside( Vector &HitPos, Vector &HitNormal, 
							    Vector const &Start, Vector const &End, bool bNoDisp )
{
	// if the face is displaced -- collide with that
	if( HasDisp() && !bNoDisp )
	{
		CMapDisp *pDisp = EditDispMgr()->GetDisp( m_DispHandle );
		return( pDisp->TraceLine( HitPos, HitNormal, Start, End ) );
	}

	//
	// Find the point of intersection of the ray with the given plane.
	//
	float t = Start.Dot( plane.normal ) - plane.dist;
	if ( -( End - Start ).Dot( plane.normal ) != 0.0f )
	{
		t = t / -( End - Start ).Dot( plane.normal );	
	}
	HitPos = Start + ( t * ( End - Start ) );

	//
	// determine if the plane point lies behind all of the polygon planes (edges)
	//
	for( int ndxEdge = 0; ndxEdge < nPoints; ndxEdge++ )
	{
		// create the edge and cross it with the face plane normal
		Vector edge;
		edge = Points[(ndxEdge+1)%nPoints] - Points[ndxEdge];

		PLANE edgePlane;
		edgePlane.normal = edge.Cross( plane.normal );
		VectorNormalize( edgePlane.normal );
		edgePlane.dist = edgePlane.normal.Dot( Points[ndxEdge] );

		// determine if the facing is correct
		float dist = edgePlane.normal.Dot( Points[(ndxEdge+2)%nPoints] ) - edgePlane.dist;
		if( dist > 0.0f )
		{
			// flip
			edgePlane.normal.Negate();
			edgePlane.dist = -edgePlane.dist;
		}
		
		// check to see if plane point lives behind the plane
		dist = edgePlane.normal.Dot( HitPos ) - edgePlane.dist;
		if( dist > 0.0f )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// NOTE: actually this could be calculated once for the face since only the
//       face normal is being used (no smoothing groups, etc.), but that may 
//       change????
//-----------------------------------------------------------------------------
void CMapFace::CalcTangentSpaceAxes( void )
{
	// destroy old axes if need be
	FreeTangentSpaceAxes();

	// allocate memory for tangent space axes
	if( !AllocTangentSpaceAxes( nPoints ) )
		return;

	//
	// get the texture space axes
	//
	Vector4D& uVect = texture.UAxis;
	Vector4D& vVect = texture.VAxis;

	//
	// calculate the tangent space per polygon point
	//

	for( int ptIndex = 0; ptIndex < nPoints; ptIndex++ )
	{
		// get the current tangent space axes
		TangentSpaceAxes_t *pAxis = &m_pTangentAxes[ptIndex];

		//
		// create the axes
		//
		pAxis->binormal = vVect.AsVector3D();
		VectorNormalize( pAxis->binormal );
		CrossProduct( plane.normal, pAxis->binormal, pAxis->tangent );
		VectorNormalize( pAxis->tangent );
		CrossProduct( pAxis->tangent, plane.normal, pAxis->binormal );
		VectorNormalize( pAxis->binormal );

		//
		// adjust tangent for "backwards" mapping if need be
		//
		Vector tmpVect;
		CrossProduct( uVect.AsVector3D(), vVect.AsVector3D(), tmpVect );
		if( DotProduct( plane.normal, tmpVect ) > 0.0f )
		{
			VectorScale( pAxis->tangent, -1.0f, pAxis->tangent );
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMapFace::AllocTangentSpaceAxes( int count )
{
	if( count < 1 )
		return false;

	m_pTangentAxes = new TangentSpaceAxes_t[count];
	if( !m_pTangentAxes )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapFace::FreeTangentSpaceAxes( void )
{
	if( m_pTangentAxes )
	{
		delete [] m_pTangentAxes;
		m_pTangentAxes = NULL;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CMapFace::SmoothingGroupCount( void )
{
	int nCount = 0;
	for ( int iGroup = 0; iGroup < SMOOTHING_GROUP_MAX_COUNT; ++iGroup )
	{
		if ( ( m_fSmoothingGroups & ( 1 << iGroup ) ) != 0 )
		{
			nCount++;
		}
	}

	return nCount;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapFace::AddSmoothingGroup( int iGroup )
{
	Assert( iGroup >= 0 );
	Assert( iGroup < SMOOTHING_GROUP_MAX_COUNT );

	m_fSmoothingGroups |= ( 1 << ( iGroup - 1 ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapFace::RemoveSmoothingGroup( int iGroup )
{
	Assert( iGroup >= 0 );
	Assert( iGroup < SMOOTHING_GROUP_MAX_COUNT );

	m_fSmoothingGroups &= ~( 1 << ( iGroup - 1 ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMapFace::InSmoothingGroup( int iGroup )
{
	if ( ( m_fSmoothingGroups & ( 1 << ( iGroup - 1 ) ) ) != 0 )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Performs an intersection of this list with another.
// Input  : IntersectWith - the list to intersect with.
//			In - the list of items that were in both lists
//			Out - the list of items that were in one list but not the other.
//-----------------------------------------------------------------------------
void CMapFaceList::Intersect(CMapFaceList &IntersectWith, CMapFaceList &In, CMapFaceList &Out)
{
	//
	// See what we items have that are in the other list.
	//
	for (int i = 0; i < Count(); i++)
	{
		CMapFace *pFace = Element(i);

		if (IntersectWith.Find(pFace) != -1)
		{
			if (In.Find(pFace) == -1)
			{
				In.AddToTail(pFace);
			}
		}
		else
		{
			if (Out.Find(pFace) == -1)
			{
				Out.AddToTail(pFace);
			}
		}
	}

	//
	// Now go the other way.
	//
	for (int i = 0; i < IntersectWith.Count(); i++)
	{
		CMapFace *pFace = IntersectWith.Element(i);

		if (Find(pFace) != -1)
		{
			if (In.Find(pFace) == -1)
			{
				In.AddToTail(pFace);
			}
		}
		else
		{
			if (Out.Find(pFace) == -1)
			{
				Out.AddToTail(pFace);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Performs an intersection of this list with another.
// Input  : IntersectWith - the list to intersect with.
//			In - the list of items that were in both lists
//			Out - the list of items that were in one list but not the other.
//-----------------------------------------------------------------------------
void CMapFaceIDList::Intersect(CMapFaceIDList &IntersectWith, CMapFaceIDList &In, CMapFaceIDList &Out)
{
	//
	// See what we items have that are in the other list.
	//
	for (int i = 0; i < Count(); i++)
	{
		int nFaceID = Element(i);
		if (IntersectWith.Find(nFaceID) != -1)
		{
			if (In.Find(nFaceID) == -1)
			{
				In.AddToTail(nFaceID);
			}
		}
		else
		{
			if (Out.Find(nFaceID) == -1)
			{
				Out.AddToTail(nFaceID);
			}
		}
	}

	//
	// Now go the other way.
	//
	for (int i = 0; i < IntersectWith.Count(); i++)
	{
		int nFaceID = IntersectWith.Element(i);

		if (Find(nFaceID) != -1)
		{
			if (In.Find(nFaceID) == -1)
			{
				In.AddToTail(nFaceID);
			}
		}
		else
		{
			if (Out.Find(nFaceID) == -1)
			{
				Out.AddToTail(nFaceID);
			}
		}
	}
}

void CMapFace::DoTransform(const VMatrix &matrix)
{
	SignalUpdate( EVTYPE_FACE_CHANGED );
	if( nPoints < 3 )
	{
		Assert( nPoints > 2 );
		return;
	}

	CMapDisp *pDisp = NULL;
	Vector bbDispOld[2]; // Old bbox for the disp.
	if( HasDisp() )
	{
		EditDispHandle_t handle = GetDisp();
		pDisp = EditDispMgr()->GetDisp( handle );
		if ( pDisp )
			pDisp->GetBoundingBox( bbDispOld[0], bbDispOld[1] );
	}

	Vector oldPoint = Points[0];
	
	// Transform the face points.
	for (int i = 0; i < nPoints; i++)
	{
		TransformPoint( matrix, Points[i] );
	}

	bool bFlip = (matrix[0][0]*matrix[1][1]*matrix[2][2]) < 0;

	if ( bFlip )
	{
		// mirror transformation would break CCW culling, so revert point order
		PointsRevertOrder( Points, nPoints );
	}

	CalcPlaneFromFacePoints();

	// ok, now apply all changes to texture & displacment too
	VMatrix mTrans = matrix;

	QAngle rotateAngles;
	Vector moveDelta;
	MatrixAngles( matrix.As3x4(), rotateAngles, moveDelta );

	bool bIsLocking = Options.IsLockingTextures()!=0;
	bool bIsMoving = moveDelta.LengthSqr() > 0.00001;

	// erase move component from matrix
	mTrans.SetTranslation( vec3_origin );

	// check if new matrix is simple identity matrix
	if ( mTrans.IsIdentity() )
	{
		if ( GetTexture() )
		{
			if ( bIsMoving && bIsLocking )
			{
				// Offset texture coordinates if we're moving and texture locking.
				OffsetTexture(moveDelta);
			}

			// Recalculate the texture coordinates for this face.
			CalcTextureCoords();

		}

		if ( pDisp )
		{
			pDisp->UpdateSurfData( this );
			
			// Update the neighbors of displacements that intersect the old as well as the new bbox.
			// Without this, there can be errors if you drag > 2 edges to interset each other, then
			// move one of the intersectors (cloning can easily cause this case to happen).
			Vector bbDispNew[2];
			pDisp->GetBoundingBox( bbDispNew[0], bbDispNew[1] );
			
			CMapDisp::UpdateNeighborsOfDispsIntersectingBox( bbDispOld[0], bbDispOld[1], 1.0 );
			CMapDisp::UpdateNeighborsOfDispsIntersectingBox( bbDispNew[0], bbDispNew[1], 1.0 );
		}

		// Create any detail objects if appropriate
		DetailObjects::BuildAnyDetailObjects(this);

		return;
	}

	// ok, transformation is more complex then a simple move

	if ( GetTexture() )
	{
		Vector vU = texture.UAxis.AsVector3D();
		Vector vV = texture.VAxis.AsVector3D();

		// store original length
		float fScaleU = vU.Length();
		float fScaleV = vV.Length();

		if ( fScaleU <= 0 )
			fScaleU = 1;

		if ( fScaleV <=0 )
			fScaleV = 1;

		// transform UV axis
		TransformPoint( mTrans, vU );
		TransformPoint( mTrans, vV );

		// get scaling factor for both axes
		fScaleU = vU.Length()/fScaleU;
		fScaleV = vV.Length()/fScaleV;

		if ( fScaleU <= 0 )
			fScaleU = 1;

		if ( fScaleV <=0 )
			fScaleV = 1;

		// check is the transformation would destory the UV axis (both normals & perpendicular)
		bool bUVAxisSameScale = fequal(fScaleV,1,0.0001) && fequal(fScaleU,1,0.0001);
		bool bUVAxisPerpendicular = fequal(DotProduct( vU, vV ),0,0.0025);

		// Rotate the U/V axes to keep them oriented the same relative
		// to this face.

		if ( bIsLocking && bUVAxisPerpendicular )
		{
			// make sure UV axes are normals & perpendicalur
			texture.UAxis.AsVector3D() = vU/fScaleU;
			texture.VAxis.AsVector3D() = vV/fScaleV;
		}

		if ( bUVAxisSameScale )
		{
			// scale is fine
			if ( !bIsLocking )
			{
				// If we are not texture locking, recalculate the texture axes based on current
				// texture alignment setting. This prevents the axes from becoming normal to the face.
				InitializeTextureAxes(Options.GetTextureAlignment(), INIT_TEXTURE_AXES | INIT_TEXTURE_FORCE);
			}
		}
		else // we stretch/scale axes 
		{
			// operation changes scale of textures, check if we really want that:
			bIsLocking = Options.IsScaleLockingTextures()!=0;

			if ( bIsLocking )
			{
				texture.scale[0] *= fScaleU;
				texture.scale[1] *= fScaleV;
			}
		}
		
		if ( bIsMoving && bIsLocking )
		{
			// Offset texture coordinates if we're moving and texture locking.
			OffsetTexture(moveDelta);
		}

		// Recalculate the texture coordinates for this face.
		CalcTextureCoords();
	}

	// rotate the displacement field data - if any!
	if( pDisp )
	{
 		pDisp->DoTransform( mTrans );

		// Update the neighbors of displacements that intersect the old as well as the new bbox.
		// Without this, there can be errors if you drag > 2 edges to interset each other, then
		// move one of the intersectors (cloning can easily cause this case to happen).
		Vector bbDispNew[2];
		pDisp->GetBoundingBox( bbDispNew[0], bbDispNew[1] );
		
		CMapDisp::UpdateNeighborsOfDispsIntersectingBox( bbDispOld[0], bbDispOld[1], 1.0 );
		CMapDisp::UpdateNeighborsOfDispsIntersectingBox( bbDispNew[0], bbDispNew[1], 1.0 );
	}
	// Create any detail objects if appropriate
	DetailObjects::BuildAnyDetailObjects(this);
}

