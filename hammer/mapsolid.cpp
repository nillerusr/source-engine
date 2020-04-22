//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "BrushOps.h"
#include "GlobalFunctions.h"
#include "MapDefs.h"		// dvs: For COORD_NOTINIT
#include "MapView2D.h" // dvs FIXME: For HitTest2D implementation
#include "MapWorld.h"
#include "MapSolid.h"
#include "Options.h"
#include "Render2D.h"
#include "Render3D.h"
#include "SaveInfo.h"
#include "MapDoc.h"
#include "MapDisp.h"
#include "camera.h"
#include "ssolid.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapSolid)


#define CENTER_HANDLE_RADIUS 3


int CMapSolid::g_nBadSolidCount = 0;


//-----------------------------------------------------------------------------
// Purpose: Constructor. Sets this solid's color to a random blue-green color.
// Input  : Parent0 - The parent of this solid. Typically this is the world.
//-----------------------------------------------------------------------------
CMapSolid::CMapSolid(CMapClass *Parent0)
	: m_bValid(FALSE)	// well, no faces
{
	m_pParent = Parent0;
	m_eSolidType = btSolid;
	m_bIsCordonBrush = false;

	PickRandomColor();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapSolid::~CMapSolid(void)
{
	Faces.SetCount(0);
}


//-----------------------------------------------------------------------------
// Purpose: Adds the plane to the given solid.
// Input  : pSolid - Solid to which the plane is being added.
//			p - Plane to add to the solid.
// Output : Returns true if the solid is still valid after the addition of the
//			plane, false if it was entirely behind the plane.
//-----------------------------------------------------------------------------
bool CMapSolid::AddPlane(const CMapFace *p)
{
	CMapFace NewFace;

	//
	// Copy the info from the carving face, including the plane, but not the points.
	//
	NewFace.CopyFrom(p, COPY_FACE_PLANE);

	//
	// Use texture from our first face - this function adds a plane
	// from the subtraction brush itself.
	//
	const CMapFace *pSolidFace = GetFace(0);

	NewFace.SetTexture(pSolidFace->texture.texture);
	NewFace.texture.q2contents = pSolidFace->texture.q2contents;
	NewFace.texture.q2surface = pSolidFace->texture.q2surface;
	NewFace.texture.nLightmapScale = pSolidFace->texture.nLightmapScale;

	//
	// Set up the texture axes for the new face.
	//
	NewFace.InitializeTextureAxes(Options.GetTextureAlignment(), INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE);

	//
	// Add the face to the solid and rebuild the solid.
	//
	AddFace(&NewFace);
	CreateFromPlanes();
	SetValid(TRUE);

	PostUpdate(Notify_Changed);

	RemoveEmptyFaces();
	
	return(GetFaceCount() >= 4);
}


//-----------------------------------------------------------------------------
// Purpose: Carves this solid using another.
// Input  : Inside - Receives the pieces of this solid that were inside the carver.
//			Outside - Receives the pieces of this solid that were outside the carver.
//			pCarver - The solid that is being subtracted from us.
//-----------------------------------------------------------------------------
bool CMapSolid::Carve(CMapObjectList *pInside, CMapObjectList *pOutside, CMapSolid *pCarver)
{
	int	i;
	CMapSolid *front = NULL;
	CMapSolid *back = NULL;
	Vector bmins, bmaxs;
	Vector carvemins, carvemaxs;

	GetRender2DBox(bmins, bmaxs);
	pCarver->GetRender2DBox(carvemins, carvemaxs);

	//
	// If this solid doesn't intersect the carving solid at all, add a copy
	// to the outside list and exit.
	//
	for (i=0 ; i<3 ; i++)
	{
		if ((bmins[i] >= carvemaxs[i]) || (bmaxs[i] <= carvemins[i]))
		{
			if (pOutside != NULL)
			{
				CMapSolid *pCopy = (CMapSolid *)Copy(false);
				pOutside->AddToTail(pCopy);
			}
			return(false);
		}
	}

	//
	// Build a duplicate of this solid to carve from.
	//
	CMapSolid CarveFrom;
	CarveFrom.CopyFrom(this, false);

	//
	// Carve the solid by the faces in the carving solid.
	//
	for (i = 0; i < pCarver->GetFaceCount(); i++)
	{
		const CMapFace *pFace = pCarver->GetFace(i);

		//
		// Split the solid by this face, into a front and a back piece.
		//
		CarveFrom.ClipByFace(pFace, pOutside != NULL ? &front : NULL, &back);

		//
		// If there was a front piece, add it to the outside list.
		//
		if ((front != NULL) && (pOutside != NULL))
		{
			pOutside->AddToTail(front);
		}
		else
		{
			delete front;
		}

		//
		// If there was no back piece, we have found a face the solid is completely in front of.
		// Per the separating axis theorem, the two solids cannot intersect, so we are done.
		//
		if (back == NULL)
		{
			return(false);
		}

		//
		// The next clip uses the back piece from this clip to prevent the carve results
		// from overlapping.
		//
		CarveFrom.CopyFrom(back, false);

		//
		// Add the back piece of the carved solid to the inside list.
		//
		if (pInside != NULL)
		{
			pInside->AddToTail(back);
		}
		else
		{
			delete back;
		}
	}

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Clips the given solid by the given face, returning the results.
// Input  : pSolid - Solid to clip.
//			fa - Face to use for the clipping operation.
//			f - Returns the part of the solid that was in front of the clipping
//				face (in the direction of the face normal).
//			b - Returns the part of the solid that was in back of the clipping
//				face (in the opposite direction of the face normal).
//-----------------------------------------------------------------------------
void CMapSolid::ClipByFace(const CMapFace *fa, CMapSolid **fsolid, CMapSolid **bSolid)
{
	CMapSolid *front = new CMapSolid;
	CMapSolid *back = new CMapSolid;
	CMapFace fb;

	//
	// Build a back facing version of the clip face by reversing the plane points
	// and recalculate the plane normal and distance.
	//
	fb.CopyFrom(fa);
	Vector temp = fb.plane.planepts[0];
	fb.plane.planepts[0] = fb.plane.planepts[2];
	fb.plane.planepts[2] = temp;
	fb.CalcPlane();
	
	front->CopyFrom(this, false);
	front->SetParent(NULL);
	
	back->CopyFrom(this, false);
	back->SetParent(NULL);
	
	if (!back->AddPlane(fa))
	{
		delete back;
		back = NULL;
	}
	
	if (!front->AddPlane(&fb))
	{
		delete front;
		front = NULL;
	}

	if (fsolid != NULL)
	{
		*fsolid = front;
	}
	else
	{
		delete front;
	}

	if (bSolid != NULL)
	{
		*bSolid = back;
	}
	else
	{
		delete back;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if this solid contains a face with the given ID, false if not.
// Input  : nFaceID - unique face ID to look for.
//-----------------------------------------------------------------------------
CMapFace *CMapSolid::FindFaceID(int nFaceID)
{
	int nFaceCount = GetFaceCount();
	for (int nFace = 0; nFace < nFaceCount; nFace++)
	{	
		CMapFace *pFace = GetFace(nFace);
		if (pFace->GetFaceID() == nFaceID)
		{
			return(pFace);
		}
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pWorld - 
//-----------------------------------------------------------------------------
void CMapSolid::GenerateNewFaceIDs(CMapWorld *pWorld)
{
	int nFaceCount = GetFaceCount();
	for (int nFace = 0; nFace < nFaceCount; nFace++)
	{
		CMapFace *pFace = GetFace(nFace);
		pFace->SetFaceID(pWorld->FaceID_GetNext());
	}
}


//-----------------------------------------------------------------------------
// Purpose: Allows the solid to generate new face IDs before being added to the
//			world because of a clone.
// Input  : pClone - The clone of this object that is being added to the world.
//			pWorld - The world that the clone is being added to.
//			OriginalList - The list of objects that were cloned.
//			NewList - The list of clones.
//-----------------------------------------------------------------------------
void CMapSolid::OnPreClone(CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	((CMapSolid *)pClone)->GenerateNewFaceIDs(pWorld);
}


//-----------------------------------------------------------------------------
// Purpose: Notifies the object that a copy of it is being pasted from the
//			clipboard before the copy is added to the world.
// Input  : pCopy - The copy of this object that is being added to the world.
//			pSourceWorld - The world that the originals were in.
//			pDestWorld - The world that the copies are being added to.
//			OriginalList - The list of original objects that were copied.
//			NewList - The list of copied.
//-----------------------------------------------------------------------------
void CMapSolid::OnPrePaste(CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	((CMapSolid *)pCopy)->GenerateNewFaceIDs(pDestWorld);
}


//-----------------------------------------------------------------------------
// Purpose: to split the solid by the given plane into frontside and backside 
//          solids; memory is allocated in the function for the solids;
//          solids are only generated for (pointers to) CMapSolid pointers that 
//          exist -- if a pointer is NULL that sidedness is ignored; the 
//          function returns whether or not an actual split happened (TRUE/FALSE)
//   Input: pPlane - the plane to split the solid with
//          pFront - the front sided solid (if any)
//          pBack - the back sided solid (if any)
//  Output: 0 - on front side
//          1 - on back side
//          2 - split
//-----------------------------------------------------------------------------
int CMapSolid::Split( PLANE *pPlane, CMapSolid **pFront, CMapSolid **pBack )
{
    const float SPLIT_DIST_EPSILON = 0.001f;
    CMapSolid *pFrontSolid = NULL;
    CMapSolid *pBackSolid = NULL;
    CMapFace  face;

	//
	// The newly added face should get its texture from face zero of the solid.
	//
	CMapFace *pFirstFace = GetFace(0);
	if (pFirstFace != NULL)
	{
		face.SetTexture(pFirstFace->GetTexture());
	}

    //
    // check for plane intersection with solid
    //
    int   frontCount = 0;
    int   backCount = 0;
    
    int faceCount = GetFaceCount();
    for( int i = 0; i < faceCount; i++ )
    {
        CMapFace *pFace = GetFace( i );

        for( int j = 0; j < pFace->nPoints; j++ )
        {
            float dist = DotProduct( pFace->Points[j], pPlane->normal ) - pPlane->dist;
            if( dist > SPLIT_DIST_EPSILON )
            {
                frontCount++;
            }
            else if( dist < -SPLIT_DIST_EPSILON )
            {
                backCount++;
            }
        }
    }

    //
    // If we're all on one side of the splitting plane, copy ourselves into the appropriate
	// destination solid.
    //
	if ((frontCount == 0) || (backCount == 0))
	{
		CMapSolid **pReturn;

		if (frontCount == 0)
		{
			pReturn = pBack;
		}
		else
		{
			pReturn = pFront;
		}

		if (pReturn == NULL)
		{
			return -1;
		}

		//
		// The returned solid is just a copy of ourselves.
		//
		CMapSolid *pReturnSolid = new CMapSolid;
		pReturnSolid->CopyFrom(this, false);
		pReturnSolid->SetParent(NULL);
		pReturnSolid->SetTemporary(TRUE);

		//
		// Rebuild the solid because mappers are accustomed to using the clipper tool as a way of
		// verifying that geometry is valid.
		//
		if (pReturnSolid->CreateFromPlanes( CREATE_FROM_PLANES_CLIPPING ))
		{
            // Initialize the texture axes only of the newly created faces. Leave the others alone.
			pReturnSolid->InitializeTextureAxes(Options.GetTextureAlignment(), INIT_TEXTURE_ALL);
			pReturnSolid->PostUpdate(Notify_Changed);
		}
				
		*pReturn = pReturnSolid;
		return 1;
	}

    //
    // split the solid and create the "front" solid
    //
    if( pFront )
    {
        //
        // copy the original solid info
        //
        pFrontSolid = new CMapSolid;
        pFrontSolid->CopyFrom(this, false);
		pFrontSolid->SetParent(NULL);
        pFrontSolid->SetTemporary( TRUE );

        face.plane.normal = pPlane->normal;
        VectorNegate( face.plane.normal );
        face.plane.dist = -pPlane->dist;

        pFrontSolid->AddFace( &face );

		//
		// The new face doesn't have plane points, only a normal and a distance, so we tell CreateFromPlanes
		// to generate new plane points for us after creation.
		//
        if (pFrontSolid->CreateFromPlanes(CREATE_BUILD_PLANE_POINTS | CREATE_FROM_PLANES_CLIPPING))
        {
            // Initialize the texture axes only of the newly created faces. Leave the others alone.
			pFrontSolid->InitializeTextureAxes( Options.GetTextureAlignment(), INIT_TEXTURE_ALL );
			pFrontSolid->PostUpdate(Notify_Clipped_Intermediate);

            *pFront = pFrontSolid;
        }
    }

    //
    // split the solid and create the "back" solid
    //
    if( pBack )
    {
        //
        // copy the original solid info
        //
        pBackSolid = new CMapSolid;
        pBackSolid->CopyFrom(this, false);
		pBackSolid->SetParent(NULL);
        pBackSolid->SetTemporary( TRUE );

        face.plane.normal = pPlane->normal;
        face.plane.dist = pPlane->dist;

        pBackSolid->AddFace( &face );

		//
		// The new face doesn't have plane points, only a normal and a distance, so we tell CreateFromPlanes
		// to generate new plane points for us after creation.
		//
        if (pBackSolid->CreateFromPlanes(CREATE_BUILD_PLANE_POINTS | CREATE_FROM_PLANES_CLIPPING))
        {
            // Initialize the texture axes only of the newly created faces. Leave the others alone.
			pBackSolid->InitializeTextureAxes( Options.GetTextureAlignment(), INIT_TEXTURE_ALL );
			pBackSolid->PostUpdate(Notify_Clipped_Intermediate);

            *pBack = pBackSolid;
        }
    }

    return 2;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the index (you could use it with GetFace) or -1 if the face doesn't exist in this solid.
//-----------------------------------------------------------------------------
int CMapSolid::GetFaceIndex( CMapFace *pFace )
{
	for ( int i=0; i < Faces.GetCount(); i++ )
	{
		if ( pFace == &Faces[i] )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: Adds a face to this solid.
// Input  : pFace - The face to add. The face is copied into this solid's face
//				array, so it can be safely freed when AddFace returns.
//-----------------------------------------------------------------------------
void CMapSolid::AddFace(CMapFace *pFace)
{
	int nFaces = Faces.GetCount();
	Faces.SetCount(nFaces + 1);
	CMapFace *pNewFace = &Faces[nFaces];

	pNewFace->CopyFrom(pFace, COPY_FACE_POINTS);
	pNewFace->SetRenderColor(r, g, b);
	pNewFace->SetCordonFace( m_bIsCordonBrush );
	pNewFace->SetParent(this);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapSolid::Copy(bool bUpdateDependencies)
{
	CMapSolid *pNew = new CMapSolid;
	pNew->CopyFrom(this, bUpdateDependencies);
	return pNew;
}


//-----------------------------------------------------------------------------
// Purpose: Turns this solid into a duplicate of the given solid.
// Input  : pobj - Object to copy, must be a CMapSolid. 
// Output : Returns a pointer to this object.
//-----------------------------------------------------------------------------
CMapClass *CMapSolid::CopyFrom(CMapClass *pobj, bool bUpdateDependencies)
{
	Assert(pobj->IsMapClass(MAPCLASS_TYPE(CMapSolid)));
	CMapSolid *pFrom = (CMapSolid *)pobj;

	CMapClass::CopyFrom(pobj, bUpdateDependencies);
	m_eSolidType = pFrom->GetHL1SolidType();

	m_bIsCordonBrush = pFrom->m_bIsCordonBrush;
	
	int nFaces = pFrom->Faces.GetCount();
	Faces.SetCount(nFaces);
	
	// copy faces
	CMapFace *pFromFace;
	CMapFace *pToFace;

	for (int i = nFaces - 1; i >= 0; i--)
	{
		
		pToFace = &Faces[i];
		pFromFace = &pFrom->Faces[i];

		if (!pToFace)
		{
			continue;
		}

		pToFace->SetParent(this);
		pToFace->CopyFrom(pFromFace, COPY_FACE_POINTS, bUpdateDependencies);
		Assert(pToFace->GetPointCount() != 0);
	}

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: Walks the faces of a solid for debugging.
//-----------------------------------------------------------------------------
#ifdef _DEBUG
#pragma warning (disable:4189)
void CMapSolid::DebugSolid(void)
{
	int nFaceCount = Faces.GetCount();
	for (int nFace = 0; nFace < nFaceCount; nFace++)
	{
		CMapFace *pFace = GetFace(nFace);
	}
}
#pragma warning (default:4189)
#endif // _DEBUG


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iIndex - 
//-----------------------------------------------------------------------------
void CMapSolid::DeleteFace(int iIndex)
{
	// shifts 'em down.
	int nFaces = Faces.GetCount();
	for(int j = iIndex; j < nFaces-1; j++)
	{
		Faces[j].CopyFrom(&Faces[j+1]);
	}

	Faces.SetCount(nFaces-1);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char* CMapSolid::GetDescription(void)
{
	static char szBuf[128];
	sprintf(szBuf, "solid with %d faces", Faces.GetCount());
	return szBuf;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates this solid's axis aligned bounding box.
// Input  : bFullUpdate - Whether to evaluate all children when calculating.
//-----------------------------------------------------------------------------
void CMapSolid::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);

	//
	// Update mins/maxes based on our faces.
	//
	int nFaces = Faces.GetCount();
	for( int i = 0; i < nFaces; i++ )
	{
		// check for valid face
		if (!Faces[i].Points)
			continue;

		//
		// Get the 2d render bounds of this face and update the solid. 2D render bounds
		// can be different from 3D culling bounds because the 2D bounds do not consider
		// displacement faces.
		//
		Vector mins, maxs;
		bool result = Faces[i].GetRender2DBox( mins, maxs );
		if( result )
		{
			m_Render2DBox.UpdateBounds( mins, maxs );
		}
	
		//
		// Get the culling bounds and update the solid
		//
		result = Faces[i].GetCullBox( mins, maxs );
		if( result )
		{
			m_CullBox.UpdateBounds( mins, maxs );
		}
	}

	m_Render2DBox.GetBoundsCenter(m_Origin);
	m_BoundingBox = m_CullBox;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : t - 
//-----------------------------------------------------------------------------
void CMapSolid::DoTransform(const VMatrix &matrix)
{
	// get all points, transform them
	int nFaces = Faces.GetCount();
	for (int i = 0; i < nFaces; i++)
	{
		Faces[i].DoTransform( matrix );
	}

	BaseClass::DoTransform(matrix);
}

//-----------------------------------------------------------------------------
// Purpose: Sets the render color of all of our faces when our render color is set.
//-----------------------------------------------------------------------------
void CMapSolid::SetRenderColor(color32 rgbColor)
{
	CMapClass::SetRenderColor(rgbColor);

	int nFaces = Faces.GetCount();
	for (int i = 0; i < nFaces; i++)
	{
		Faces[i].SetRenderColor(rgbColor);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the render color of all of our faces when our render color is set.
//-----------------------------------------------------------------------------
void CMapSolid::SetRenderColor(unsigned char uchRed, unsigned char uchGreen, unsigned char uchBlue)
{
	CMapClass::SetRenderColor(uchRed, uchGreen, uchBlue);

	int nFaces = Faces.GetCount();
	for (int i = 0; i < nFaces; i++)
	{
		Faces[i].SetRenderColor(uchRed, uchGreen, uchBlue);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : size_t
//-----------------------------------------------------------------------------
size_t CMapSolid::GetSize(void)
{
	size_t size = CMapClass::GetSize();
	size += sizeof *this;

	int nFaces = Faces.GetCount();
	for( int i = 0; i < nFaces; i++ )
	{
		size += Faces[i].GetDataSize();
	}

	return size;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the texture for a given face.
// Input  : pszTex - Texture name.
//			iFace - Index of face for which to set texture.
//-----------------------------------------------------------------------------
void CMapSolid::SetTexture(LPCTSTR pszTex, int iFace)
{
	if(iFace == -1)
	{
		int nFaces = Faces.GetCount();
		for(int i = 0 ; i < nFaces; i++)
		{
			Faces[i].SetTexture(pszTex);
		}
	}
	else
	{
		Faces[iFace].SetTexture(pszTex);
	}

	CMapDoc		*pMapDoc = CMapDoc::GetActiveMapDoc();

	pMapDoc->RemoveFromAutoVisGroups( this );
	pMapDoc->AddToAutoVisGroup( this );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the texture name of a given face.
// Input  : iFace - Index of face. If -1, returns the texture of face 0.
// Output : Returns the texture name.
//-----------------------------------------------------------------------------
LPCTSTR CMapSolid::GetTexture(int iFace)
{
	return Faces[iFace == -1 ? 0 : iFace].texture.texture;
}


//-----------------------------------------------------------------------------
// Purpose: Creates the solid using the plane information from the solid's faces.
//
//			ASSUMPTIONS: This solid's faces are assumed to have valid plane points.
//
// Input  : dwFlags - Can be any or none of the following flags:
//
//				CREATE_BUILD_PLANE_POINTS - if this flag is set, the 3-point
//					definition of each face plane will be regenerated based
//					on the face points after the solid is generated.
//
// Output : Returns TRUE if the solid is valid, FALSE if not.
//
// dvs: this should really use the public API of CMapSolid to add faces so that
//      parentage and render color are set automatically.
//-----------------------------------------------------------------------------
int CMapSolid::CreateFromPlanes( DWORD dwFlags )
{
	int i, j, k;
    BOOL useplane[MAPSOLID_MAX_FACES];

	m_Render2DBox.SetBounds(Vector(COORD_NOTINIT, COORD_NOTINIT, COORD_NOTINIT), 
							Vector(-COORD_NOTINIT, -COORD_NOTINIT, -COORD_NOTINIT));

	m_bValid = TRUE;

	//
	// Free all points from all faces and assign parentage.
	//
	int nFaces = GetFaceCount();

	for (i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = GetFace(i);

		pFace->AllocatePoints(0);
		pFace->SetParent(this);
		pFace->SetRenderColor(r, g, b);

		useplane[i] = false;
	}

	//
	// For every face that is not set to be ignored, check the plane and make sure
	// it is unique. We mark each plane that we intend to keep with a TRUE in the
	// 'useplane' array.
	//
	for (i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = GetFace(i);
		PLANE *f = &pFace->plane;

        //
        // Don't use this plane if it has a zero-length normal.
        //
        if (VectorCompare(f->normal, vec3_origin))
        {
            useplane[i] = FALSE;
			continue;
        }
        
		//
		// If the plane duplicates another plane, don't use it (assume it is a brush
		// being edited that will be fixed).
		//
		useplane[i] = TRUE;
		for (j = 0; j < i; j++)
		{
			CMapFace *pFaceCheck = GetFace(j);

			Vector& f1 = f->normal;
			Vector& f2 = pFaceCheck->plane.normal;

			//
			// Check for duplicate plane within some tolerance.
			//
			if ((DotProduct(f1, f2) > 0.999) && (fabs(f->dist - pFaceCheck->plane.dist) < 0.01))
			{
				useplane[j] = FALSE;
				break;
			}
		}
	}

	//
	// Now we have a set of planes, indicated by TRUE values in the 'useplanes' array,
	// from which we will build a solid.
	//
	BOOL bGotFaces = FALSE;

	for (i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = GetFace(i);

		if (!useplane[i])
			continue;

		//
		// Create a huge winding from this face's plane, then clip it by all other
		// face planes.
		//
		winding_t *w = CreateWindingFromPlane(&pFace->plane);
		for (j = 0; j < nFaces && w; j++)
		{
			CMapFace *pFaceClip = GetFace(j);

			//
			// Flip the plane, because we want to keep the back side
			//
			if (j != i)
			{
				PLANE plane;

				VectorSubtract(vec3_origin, pFaceClip->plane.normal, plane.normal);
				plane.dist = -pFaceClip->plane.dist;

				w = ClipWinding(w, &plane);
			}
		}

		//
		// If we still have a winding after all that clipping, build a face from
		// the winding.
		//
		if (w != NULL)
		{
			//
			// Round all points in the winding that are within ROUND_VERTEX_EPSILON of
			// integer values.
			//
			for (j = 0; j < w->numpoints; j++)
			{
				for (k = 0; k < 3; k++)
				{
					float v = w->p[j][k];
					float v1 = V_rint(v);
					if ((v != v1) && (fabs(v - v1) < ROUND_VERTEX_EPSILON))
					{
					   w->p[j][k] = v1;
					}
				}
			}

			//
			// The above rounding process may have created duplicate points. Eliminate them.
			//
			RemoveDuplicateWindingPoints(w, MIN_EDGE_LENGTH_EPSILON);

			bGotFaces = TRUE;

			//
			// Create a face from this winding. Leave the face plane
			// alone because we are still in the process of building our solid.
			//
			if ( dwFlags & CREATE_FROM_PLANES_CLIPPING )
			{
				pFace->CreateFace( w, CREATE_FACE_PRESERVE_PLANE | CREATE_FACE_CLIPPING );
			}
			else
			{
				pFace->CreateFace(w, CREATE_FACE_PRESERVE_PLANE);
			}
			
			//
			// Done with the winding, we can free it now.
			//
			FreeWinding(w);
		}
	}

	if (!bGotFaces)
	{
		m_bValid = FALSE;
		m_Render2DBox.SetBounds(vec3_origin, vec3_origin);
	}
	else
	{
		//
		// Remove faces that don't contribute to this solid.
		//
		int nFace = GetFaceCount();
		while (nFace > 0)
		{
			nFace--;
			CMapFace *pFace = GetFace(nFace);

			if ((!useplane[nFace]) || (pFace->GetPointCount() == 0))
			{
				DeleteFace(nFace);

				memcpy(useplane + nFace, useplane + nFace + 1, MAPSOLID_MAX_FACES - (nFace + 1));
			}
		}
	}

	//
	// Now that we have built the faces from the planes that we were given,
	// calculate the plane normals, distances, and texture coordinates.
	//
	nFaces = GetFaceCount();
	for (i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = GetFace(i);

		if (dwFlags & CREATE_BUILD_PLANE_POINTS)
		{
			pFace->CalcPlaneFromFacePoints();
		}
		else
		{
			pFace->CalcPlane();
		}

		pFace->CalcTextureCoords();

		//
		// Make sure the face is valid.
		//
		if (!pFace->CheckFace())
		{
			m_bValid = FALSE;
		}
	}

    // 
    // remove faces that do not contribute -- not just "unused or ignored" faces
    //
	int faceCount = Faces.GetCount();
    for( i = 0; i < faceCount; i++ )
    {
        if( Faces[i].nPoints == 0 )
        {
            DeleteFace( i );
            i--;
            faceCount--;
        }
    }

	return(m_bValid ? TRUE : FALSE);
}


//-----------------------------------------------------------------------------
// Purpose: Initializes the texture axes for all faces in the solid.
// Input  : eAlignment - See CMapFace::InitializeTextureAxes
//			dwFlags - See CMapFace::InitializeTextureAxes
//-----------------------------------------------------------------------------
void CMapSolid::InitializeTextureAxes(TextureAlignment_t eAlignment, DWORD dwFlags)
{
	int nFaces = Faces.GetCount();
	for (int i = 0; i < nFaces; i++)
	{
		Faces[i].InitializeTextureAxes(eAlignment, dwFlags);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pLoadInfo - 
//			*pSolid - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapSolid::LoadSideCallback(CChunkFile *pFile, CMapSolid *pSolid)
{
	ChunkFileResult_t eResult = ChunkFile_Ok;

	//
	// this is hear in place of the AddFace -- may want to handle this better later!!!
	//
	int faceCount = pSolid->Faces.GetCount();
	pSolid->Faces.SetCount( faceCount + 1 );
	CMapFace *pFace = &pSolid->Faces[faceCount];

	eResult = pFace->LoadVMF(pFile);
	if (eResult == ChunkFile_Ok)
	{
		pFace->SetRenderColor( pSolid->r, pSolid->g, pSolid->b );
		pFace->SetParent( pSolid );
	}
	else
	{
		// UNDONE: need a better solution for user errors.
		AfxMessageBox("Out of memory loading solid.");
		eResult = ChunkFile_OutOfMemory;
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			pData - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapSolid::LoadVMF(CChunkFile *pFile, bool &bValid)
{
	//
	// Set up handlers for the subchunks that we are interested in.
	//
	CChunkHandlerMap Handlers;
	Handlers.AddHandler("side", (ChunkHandler_t)LoadSideCallback, this);
	Handlers.AddHandler("editor", (ChunkHandler_t)LoadEditorCallback, this);

	pFile->PushHandlers(&Handlers);
	ChunkFileResult_t eResult = pFile->ReadChunk((KeyHandler_t)LoadEditorKeyCallback, this);
	pFile->PopHandlers();

	bValid = false;

	if (eResult == ChunkFile_Ok)
	{
		//
		// Create the solid using the planes that were read from the MAP file.
		//
		if (CreateFromPlanes())
		{
			bValid = true;
			CalcBounds();

			//
			// Set solid type based on texture name.
			//
			m_eSolidType = HL1SolidTypeFromTextureName(Faces[0].texture.texture);

			//
			// create all of the displacement surfaces for faces with the displacement property
			//
			int faceCount = GetFaceCount();
			for( int i = 0; i < faceCount; i++ )
			{
				CMapFace *pFace = GetFace( i );
				if( !pFace->HasDisp() )
					continue;

				EditDispHandle_t handle = pFace->GetDisp();
				CMapDisp *pMapDisp = EditDispMgr()->GetDisp( handle );
				pMapDisp->InitDispSurfaceData( pFace, false );
				pMapDisp->Create();
				pMapDisp->PostLoad();
			}

			// There once was a bug that caused black solids. Fix it here.
			if ((r == 0) && (g == 0) || (b == 0))
			{
				PickRandomColor();
			}
		}
		else
		{
			g_nBadSolidCount++;
		}
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: Picks a random shade of blue/green for this solid.
//-----------------------------------------------------------------------------
void CMapSolid::PickRandomColor()
{
	SetRenderColor(0, 100 + (random() % 156), 100 + (random() % 156));
}


//-----------------------------------------------------------------------------
// Purpose: Called before loading a map file.
//-----------------------------------------------------------------------------
void CMapSolid::PreloadWorld(void)
{
	g_nBadSolidCount = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of solids that could not be loaded due to errors
//			in the VMF file. This should only occur after the first load of an
//			old RMF file.
//-----------------------------------------------------------------------------
int CMapSolid::GetBadSolidCount(void)
{
	return(g_nBadSolidCount);
}


//-----------------------------------------------------------------------------
// Purpose: Called after this object is added to the world.
//
//			NOTE: This function is NOT called during serialization. Use PostloadWorld
//				  to do similar bookkeeping after map load.
//
// Input  : pWorld - The world that we have been added to.
//-----------------------------------------------------------------------------
void CMapSolid::OnAddToWorld(CMapWorld *pWorld)
{
	CMapClass::OnAddToWorld(pWorld);

	//
	// First, the common case: all our face IDs are zero. Assign new IDs to all faces
	// with zero IDs. Add unhandled faces to a list. Those we will need to check against
	// the world for uniqueness.
	//
	CMapFaceList CheckList;
	int nFaceCount = GetFaceCount();
	for (int i = 0; i < nFaceCount; i++)
	{
		CMapFace *pFace = GetFace(i);
		if (pFace->GetFaceID() == 0)
		{
			pFace->SetFaceID(pWorld->FaceID_GetNext());
		}
		else
		{
			CheckList.AddToTail(pFace);
		}
	}

	if (CheckList.Count() > 0)
	{
		//
		// The less common case: make sure all our face IDs are unique in this world.
		// We do it here instead of in CMapFace in order to save world tree traversals.
		//
		EnumChildrenPos_t pos;
		CMapClass *pChild = pWorld->GetFirstDescendent(pos);
		while (pChild != NULL)
		{
			CMapSolid *pSolid = dynamic_cast<CMapSolid *>(pChild);
			
			if ( pSolid && pSolid != this )
			{
				CUtlRBTree<int,int> faceIDs;
				SetDefLessFunc( faceIDs );

				nFaceCount = GetFaceCount();
				for (int nFace = 0; nFace < nFaceCount; nFace++)
				{	
					CMapFace *pFace = GetFace(nFace);
					faceIDs.Insert( pFace->GetFaceID() );
				}
				
				for (int i = CheckList.Count() - 1; i >= 0; i--)
				{
					CMapFace *pFace = CheckList.Element(i);

					// If this face ID is not unique, assign it a new unique face ID
					// and remove it from our list.

					if ( faceIDs.Find( pFace->GetFaceID() ) != faceIDs.InvalidIndex() )
					{
						pFace->SetFaceID(pWorld->FaceID_GetNext());
						CheckList.FastRemove(i);
					}
				}
								
				if (CheckList.Count() <= 0)
				{
					// We've handled all the faces in our list, early out.
					break;
				}
			}
			
			pChild = pWorld->GetNextDescendent(pos);
		}
	}

	//
	// Notify all faces that we are being added to the world.
	//
	for (int i = 0; i < nFaceCount; i++)
	{
		CMapFace *pFace = GetFace(i);
		pFace->OnAddToWorld(pWorld);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called after the entire map has been loaded. This allows the object
//			to perform any linking with other map objects or to do other operations
//			that require all world objects to be present.
// Input  : pWorld - The world that we are in.
//-----------------------------------------------------------------------------
void CMapSolid::PostloadWorld(CMapWorld *pWorld)
{
	CMapClass::PostloadWorld(pWorld);

	//
	// Make sure all our faces have nonzero IDs. They might if the map was created
	// before unique IDs were added.
	//
	int nFaces = GetFaceCount();
	for (int i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = GetFace(i);
		if (pFace->GetFaceID() == 0)
		{
			pFace->SetFaceID(pWorld->FaceID_GetNext());
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eSelectMode - 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapSolid::PrepareSelection(SelectMode_t eSelectMode)
{
	//
	// If we have a parent who is not the world object, consider whether we should
	// select it instead.
	//
	if ((eSelectMode != selectSolids) && (m_pParent != NULL) && !IsWorldObject(m_pParent) )
	{
		//
		// If we are in group selection mode or our parent is an entity, select our
		// parent.
		//

		if ( (eSelectMode == selectGroups) || (dynamic_cast <CMapEntity *>(m_pParent) != NULL))
		{
			return GetParent()->PrepareSelection(eSelectMode);
		}
	}

	return this;	
}


//-----------------------------------------------------------------------------
// Purpose: Called just after this object has been removed from the world so
//			that it can unlink itself from other objects in the world.
// Input  : pWorld - The world that we were just removed from.
//			bNotifyChildren - Whether we should forward notification to our children.
//-----------------------------------------------------------------------------
void CMapSolid::OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren)
{
	CMapClass::OnRemoveFromWorld(pWorld, bNotifyChildren);

	//
	// Notify all faces that we are being removed from the world.
	//
	int nFaces = GetFaceCount();
	for (int i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = GetFace(i);
		pFace->OnRemoveFromWorld();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapSolid::RemoveEmptyFaces(void)
{
	int nFaces = GetFaceCount();

	for (int i = 0; i < nFaces; i++)
	{
		//
		// If this face has no points, delete it.
		//
		const CMapFace *pFace = GetFace(i);
		if (pFace->Points == NULL)
		{
			DeleteFace(i);
			i--;
			nFaces--;
		}
	}

	if (nFaces >= 4)
	{
		// dvs: test to verify that the SetFaceCount below is unnecessary
		int nTest = GetFaceCount();
		Assert(nTest == nFaces);
		SetFaceCount(nFaces);
	}
}


//-----------------------------------------------------------------------------
// for sorting
//-----------------------------------------------------------------------------

bool CMapSolid::ShouldRenderLast()
{
	for (int nFace = 0; nFace < GetFaceCount(); nFace++)
	{
		CMapFace *pFace = GetFace(nFace);
		if (pFace->ShouldRenderLast())
			return true;
	}
	return false;
}

void CMapSolid::AddShadowingTriangles( CUtlVector<Vector> &tri_list )
{
	for (int nFace = 0; nFace < GetFaceCount(); nFace++)
	{
		CMapFace *pFace = GetFace(nFace);
		pFace->AddShadowingTriangles( tri_list );
		if( pFace->HasDisp() )
		{
			EditDispHandle_t handle = pFace->GetDisp();
			CMapDisp *pMapDisp = EditDispMgr()->GetDisp( handle );
			pMapDisp->AddShadowingTriangles( tri_list );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Renders the solid using the default render mode. If the solid is
//			currently selected, it will be rendered with a yellow wireframe
//			in a second pass.
// Input  : pRender - Rendering interface.
//-----------------------------------------------------------------------------
void CMapSolid::Render3D(CRender3D *pRender)
{
	//
	// determine whether or not this is a displacement solid - i.e. one of the faces
	// on this solid is displaced
	//
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if( !pDoc )
		return;

	bool bMaskFaces = pDoc->IsDispSolidDrawMask() && HasDisp();

	//
	// Determine whether we need to render in one or two passes. If we are selected,
	// and rendering in flat or textured mode, we need to render using two passes.
	//
	int nPasses = 1;
	int iStartPass = 1;

	SelectionState_t eSolidSelectionState = GetSelectionState();
	EditorRenderMode_t eDefaultRenderMode = pRender->GetDefaultRenderMode();

	if ((eSolidSelectionState != SELECT_NONE) && (eDefaultRenderMode != RENDER_MODE_WIREFRAME))
	{
		nPasses = 2;
	}
	
	if ( ( eSolidSelectionState == SELECT_MODIFY ) )
	{
		nPasses = 2;
		iStartPass = 2;
	}
	
 	for (int nPass = iStartPass; nPass <= nPasses; nPass++)
	{
		//
		// Render the second pass in wireframe.
		//
		if (nPass == 1)
		{
			pRender->PushRenderMode(RENDER_MODE_CURRENT);
		}
		else
		{
			pRender->PushRenderMode(RENDER_MODE_WIREFRAME);
		}

		for (int nFace = 0; nFace < GetFaceCount(); nFace++)
		{
			CMapFace *pFace = GetFace(nFace);

			// only render displaced faces on a displaced solid when the displacement
			// solid render mask is set
			if( bMaskFaces && !pFace->HasDisp() )
				continue;

			if( pRender->IsInLightingPreview() )
			{
				if( nPass == 1 )
				{
					if( pFace->GetSelectionState() != SELECT_NONE )
					{
						pRender->BeginRenderHitTarget(this, nFace);
						pFace->Render3D( pRender );
						pRender->EndRenderHitTarget();
					}
				}
				else
				{
					pFace->Render3D( pRender );
				}
			}
			else
			{
				pRender->BeginRenderHitTarget(this, nFace);
				pFace->Render3D( pRender );
				pRender->EndRenderHitTarget();
			}
		}

		pRender->PopRenderMode();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMapSolid::HasDisp( void )
{
	for( int ndxFace = 0; ndxFace < GetFaceCount(); ndxFace++ )
	{
		CMapFace *pFace = GetFace( ndxFace );
		if( pFace->HasDisp() )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a solid type for the given texture name.
// Input  : pszTexture - 
//-----------------------------------------------------------------------------
HL1_SolidType_t CMapSolid::HL1SolidTypeFromTextureName(const char *pszTexture)
{
	HL1_SolidType_t eSolidType;

	if (pszTexture[0] == '*')
	{
		if (!strncmp(pszTexture + 1, "slime", 5))
		{
			eSolidType = btSlime;
		}
		else if (!strncmp(pszTexture + 1, "lava", 4))
		{
			eSolidType = btLava;
		}
		else
		{
			eSolidType = btWater;
		}
	} 
	else
	{
		eSolidType = btSolid;
	}

	return(eSolidType);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapSolid::SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	//
	// Check rules before saving this object.
	//
	if (!pSaveInfo->ShouldSaveObject(this))
	{
		return(ChunkFile_Ok);
	}

	ChunkFileResult_t eResult = ChunkFile_Ok;

	//
	// If we are hidden, place this object inside of a hidden chunk.
	//
	if (!IsVisible())
	{
		eResult = pFile->BeginChunk("hidden");
	}

	//
	// Begin the solid chunk.
	//
	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->BeginChunk("solid");
	}

	if (eResult == ChunkFile_Ok)
	{
		//
		// Save the solid's ID.
		//
		if (eResult == ChunkFile_Ok)
		{
			eResult = pFile->WriteKeyValueInt("id", GetID());
		}

		//
		// Save all the brush faces.
		//
		int nFaceCount = GetFaceCount();
		for (int nFace = 0; nFace < nFaceCount; nFace++)
		{
			CMapFace *pFace = GetFace(nFace);
			eResult = pFace->SaveVMF(pFile, pSaveInfo);

			if (eResult != ChunkFile_Ok)
			{
				break;
			}
		}

		//
		// Save our base class' information within our chunk.
		//
		if (eResult == ChunkFile_Ok)
		{
			eResult = CMapClass::SaveVMF(pFile, pSaveInfo);
		}

		if (eResult == ChunkFile_Ok)
		{
			eResult = pFile->EndChunk();
		}
	}

	//
	// End the hidden chunk if we began it.
	//
	if (!IsVisible())
	{
		eResult = pFile->EndChunk();
	}

	return(eResult);	
}


bool CMapSolid::ShouldAppearInLightingPreview(void)
{
	return true;
}

bool CMapSolid::ShouldAppearInRaytracedLightingPreview(void)
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapSolid::SaveEditorData(CChunkFile *pFile)
{
	if (m_bIsCordonBrush)
	{
		return(pFile->WriteKeyValueBool("cordonsolid", true));
	}

	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: Sets whether this brush was created by the cordon tool. Brushes that
//			were created by the cordon tool are not loaded.
// Input  : bSet - true to set, false to clear.
//-----------------------------------------------------------------------------
void CMapSolid::SetCordonBrush(bool bSet)
{
	m_bIsCordonBrush = bSet;

	for ( int i = 0; i < GetFaceCount(); i++ )
	{
		CMapFace *pFace = GetFace( i );
		pFace->SetCordonFace( bSet );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Subtracts one solid from another.
// Input  : pSubtraction - Solid (or group of solids) to subtract with.
//			pOther - Solid (or group of solids) to subtract from.
//			pSubParent - Receives the results of the subtraction as children.
// Output : Returns true if the objects intersected (subtraction was performed),
//			false if the objects did not intersect (no subtraction was performed).
//-----------------------------------------------------------------------------
bool CMapSolid::Subtract(CMapObjectList *pInside, CMapObjectList *pOutside, CMapClass *pSubtractWith)
{
	//
	// Build a list of solids to subtract with.
	//
	CMapObjectList SubList;
	if (pSubtractWith->IsMapClass(MAPCLASS_TYPE(CMapSolid)))
	{
		SubList.AddToTail(pSubtractWith);
	}

	EnumChildrenPos_t pos;
	CMapClass *pChild = pSubtractWith->GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		CMapSolid *pSolid = dynamic_cast <CMapSolid *> (pChild);
		if (pSolid != NULL)
		{
			SubList.AddToTail(pSolid);
		}

		pChild = pSubtractWith->GetNextDescendent(pos);
	}

	//
	// For every solid that we are subtracting with...
	//
	bool bIntersected = false;

	FOR_EACH_OBJ( SubList, p )
	{
		CMapSolid *pCarver = (CMapSolid *)SubList.Element(p);

		//
		// Subtract the 'with' solid from the 'from' solid, and place the
		// results in the carve_in and carve_out lists.
		//
		CMapObjectList carve_in;
		CMapObjectList carve_out;

		CMapObjectList *pCarveIn = NULL;
		CMapObjectList *pCarveOut = NULL;

		if (pInside != NULL)
		{
			pCarveIn = &carve_in;
		}

		if (pOutside != NULL)
		{
			pCarveOut = &carve_out;
		}

		bIntersected |= Carve(pCarveIn, pCarveOut, pCarver);

		if (pInside != NULL)
		{
			pInside->AddVectorToTail(carve_in);
			carve_in.RemoveAll();
		}

		if (pOutside != NULL)
		{
			pOutside->AddVectorToTail(carve_out);
			carve_out.RemoveAll();
		}
	}

	return(bIntersected);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
color32 CMapSolid::GetLineColor( CRender2D *pRender )
{
	//
	// If the solid is not selected, determine the appropriate pen color.
	//
	if ( !IsSelected() )
	{
		//
		// If this is a solid entity, use the entity pen color.
		//
		CMapEntity *pEntity = dynamic_cast<CMapEntity *>(GetParent());
		if (pEntity != NULL)
		{
			GDclass *pClass = pEntity->GetClass();
			if (pClass)
			{
				return pClass->GetColor();
			}
			else
			{
				color32 clr;
				clr.r = GetRValue(Options.colors.clrEntity);
				clr.g = GetGValue(Options.colors.clrEntity);
				clr.b = GetBValue(Options.colors.clrEntity);
				clr.a = 255;
				return clr;
			}
		}
		//
		// Otherwise, use the solid color.
		//
		else
		{
			if (Options.view2d.bUsegroupcolors)
			{
				return GetRenderColor();
			}
			else
			{
				color32 clr;
				clr.r = GetRValue(Options.colors.clrBrush);
				clr.g = GetGValue(Options.colors.clrBrush);
				clr.b = GetBValue(Options.colors.clrBrush);
				clr.a = 255;
				return clr;
			}
		}
	}
	//
	// The solid is selected, use the selected pen color.
	//
	else
	{
		color32 clr;
		clr.r = GetRValue(Options.colors.clrSelection);
		clr.g = GetGValue(Options.colors.clrSelection);
		clr.b = GetBValue(Options.colors.clrSelection);
		clr.a = 255;
		return clr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------

void CMapSolid::Render2D(CRender2D *pRender)
{
	Vector vecMins, vecMaxs, vViewNormal;

	GetRender2DBox(vecMins, vecMaxs);


	pRender->GetCamera()->GetViewForward( vViewNormal );

	Vector2D pt, pt2;

	pRender->TransformPoint(pt, vecMins);
	pRender->TransformPoint(pt2, vecMaxs);

	int sizex = abs(pt2.x-pt.x)+1;
	int sizey = abs(pt2.y-pt.y)+1;

	color32 rgbLineColor = GetLineColor( pRender );

	// check if we should draw handles & vertices
	
	bool bIsSmall  = sizex < (HANDLE_RADIUS*2) || sizey < (HANDLE_RADIUS*2);
	bool bIsTiny = sizex < 2 || sizey < 2;
	bool bDrawHandles  = pRender->IsActiveView() && !bIsSmall && IsEditable();
	bool bDrawVertices = Options.view2d.bDrawVertices && !bIsTiny;

	pRender->SetDrawColor( rgbLineColor.r, rgbLineColor.g, rgbLineColor.b );
	
	//
	// Draw center handle if the solid is larger than the handle along either axis.
	//
	if ( bDrawHandles )
	{		
		// draw center handle as cross
		pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_CROSS );
		pRender->SetHandleColor( rgbLineColor.r, rgbLineColor.g, rgbLineColor.b );
		pRender->DrawHandle( (vecMins+vecMaxs)/2 );
	}
	
	if ( bDrawVertices )
	{
		// set handle style for upcoming vertex drawing
		pRender->SetHandleStyle( 2, CRender::HANDLE_SQUARE );
		pRender->SetHandleColor( GetRValue(Options.colors.clrVertex), GetGValue(Options.colors.clrVertex), GetBValue(Options.colors.clrVertex) );
	}

	// is solid projection is too small, draw simple line
	if ( bIsTiny )
	{
		pRender->DrawLine( vecMins, vecMaxs );
	}
	else
	{
		int nFaces = GetFaceCount();

		for ( int i = 0; i < nFaces; i++)
		{
			CMapFace *pFace = GetFace(i);
			pFace->Render2D( pRender );
		}

		if ( bDrawVertices )
		{
			bool bPop = pRender->BeginClientSpace();

			for ( int i = 0; i < nFaces; i++)
			{
				CMapFace *pFace = GetFace(i);
				pFace->RenderVertices( pRender );
			}

			if ( bPop )
				pRender->EndClientSpace();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			vecPoint - 
//			nHitData - 
// Output : 
//-----------------------------------------------------------------------------
bool CMapSolid::HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData)
{
	if (!IsVisible())
		return false;
	
	//
	// First check center X.
	//
	Vector vecCenter, vecViewPoint;
	GetBoundsCenter(vecCenter);

	Vector2D vecClientCenter;
	pView->WorldToClient(vecClientCenter, vecCenter);
	pView->GetCamera()->GetViewPoint( vecViewPoint );

	HitData.pObject = this;
	HitData.nDepth = vecViewPoint[pView->axThird]-vecCenter[pView->axThird];
	HitData.uData = 0;

	if (pView->CheckDistance(point, vecClientCenter, HANDLE_RADIUS))
	{
		return true;
	}
	else if (!Options.view2d.bSelectbyhandles || !IsEditable() )
	{
		//
		// See if any edges are within certain distance from the the point.
		//
		int iSelUnits = 2;
		int x1 = point.x - iSelUnits;
		int x2 = point.x + iSelUnits;
		int y1 = point.y - iSelUnits;
		int y2 = point.y + iSelUnits;

		int nFaces = GetFaceCount();
		for (int i = 0; i < nFaces; i++)
		{
			CMapFace *pFace = GetFace(i);
			int nPoints = pFace->nPoints;
			if (nPoints > 0)
			{
				Vector *pPoints = pFace->Points;

				Vector2D vec1;
				pView->WorldToClient(vec1, pPoints[0]);

				for (int j = 1; j < nPoints; j++)
				{
					Vector2D vec2;
					pView->WorldToClient(vec2, pPoints[j]);

					if (IsLineInside(vec1, vec2, x1, y1, x2, y2))
					{
						return true;
					}
					else
					{
						vec1 = vec2;
					}
				}
			}
		}
	}

	HitData.pObject = NULL;

	return false;
}

bool CMapSolid::SaveDXF(ExportDXFInfo_s *pInfo)
{
	if (pInfo->bVisOnly)
	{
		if (!IsVisible())
		{
			return true;
		}
	}

	CSSolid *pStrucSolid = new CSSolid;
	pStrucSolid->Attach(this);
	pStrucSolid->Convert( true, true );
	pStrucSolid->SerializeDXF(pInfo->fp, pInfo->nObject++);
	delete pStrucSolid;

	// Serialize displacements
	for (int i = 0; i < GetFaceCount(); ++i)
	{
		CMapFace *pMapFace = GetFace( i );
		if (pMapFace->HasDisp())
		{
			EditDispHandle_t hDisp = pMapFace->GetDisp();
			CMapDisp *pDisp = EditDispMgr()->GetDisp( hDisp );
			if (!pDisp->SaveDXF( pInfo ))
				return FALSE;
		}
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// Called any time this object is modified by Undo or Redo.
//-----------------------------------------------------------------------------
void CMapSolid::OnUndoRedo()
{
	int nFaces = GetFaceCount();
	for (int i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = GetFace(i);
		pFace->OnUndoRedo();
	}
}

