//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a decal helper. The decal attaches itself to nearby
//			solids, dynamically creating decal faces as necessary.
//
//=============================================================================//

#include "stdafx.h"
#include "ClipCode.h"
#include "MapDoc.h"
#include "MapDecal.h"
#include "MapFace.h"
#include "MapSolid.h"
#include "MapWorld.h"
#include "Render3D.h"
#include "TextureSystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapDecal)


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapDecal from a set
//			of string parameters from the FGD file.
// Input  : pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapDecal::CreateMapDecal(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	CMapDecal *pDecal = new CMapDecal;
	return(pDecal);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CMapDecal::CMapDecal(void)
{
	m_pTexture = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees allocated memory.
//-----------------------------------------------------------------------------
CMapDecal::~CMapDecal(void)
{
	// Delete our list of faces and each face in the list.
	FOR_EACH_OBJ( m_Faces, pos )
	{
		DecalFace_t *pDecalFace = m_Faces.Element(pos);
		delete pDecalFace->pFace;
		delete pDecalFace;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pSolid - 
//-----------------------------------------------------------------------------
void CMapDecal::AddSolid(CMapSolid *pSolid)
{
	if ( m_Solids.Find(pSolid) == -1 )
	{
		UpdateDependency(NULL, pSolid);
		m_Solids.AddToTail(pSolid);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapDecal::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);

	//
	// Calculate the 2D render box.
	//
	Vector Mins = m_Origin - Vector(2, 2, 2);
	Vector Maxs = m_Origin + Vector(2, 2, 2);

	m_Render2DBox.UpdateBounds(Mins, Maxs);

	//
	// Calculate the 3D culling bounds.
	//
	m_CullBox.ResetBounds();

	if (m_Faces.Count() > 0)
	{
		Vector MinsFace;
		Vector MaxsFace;

		FOR_EACH_OBJ( m_Faces, pos )
		{
			DecalFace_t *pDecalFace = m_Faces.Element(pos);

			if ((pDecalFace != NULL) && (pDecalFace->pFace != NULL))
			{
				pDecalFace->pFace->GetFaceBounds( MinsFace, MaxsFace );

				m_CullBox.UpdateBounds( MinsFace, MaxsFace );
			}
		}

		//
		// Insure that the 3D bounds are at least 1 unit in all dimensions.
		//
		for (int nDim = 0; nDim < 3; nDim++)
		{
			if ((m_CullBox.bmaxs[nDim] - m_CullBox.bmins[nDim]) == 0)
			{
				m_CullBox.bmins[nDim] -= 0.5;
				m_CullBox.bmaxs[nDim] += 0.5;
			}
		}
	}
	else
	{
		m_CullBox.UpdateBounds(Mins, Maxs);
	}

	m_BoundingBox = m_CullBox;
}


//-----------------------------------------------------------------------------
// Purpose: Determines whether we can attach to this solid by looking at the
//			normal distance to the solid face. We still may not lie within the
//			face; that is determined by the clipping code.
// Input  : pSolid - Solid to check.
//			ppFaces - Returns with pointers to the faces that are eligible.
// Output : Returns the number of faces that were eligible.
//-----------------------------------------------------------------------------
int CMapDecal::CanDecalSolid(CMapSolid *pSolid, CMapFace **ppFaces)
{
	//
	// Check distance from our origin to each face along the face's normal.
	// If the distance is very very small, add the face.
	//
	int nDecalFaces = 0;

	int nFaces = pSolid->GetFaceCount();
	Assert(nFaces <= MAPSOLID_MAX_FACES);
	for (int i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = pSolid->GetFace(i);
		float fDistance = pFace->GetNormalDistance(m_Origin);

		if ((fDistance <= 16.0f) && (fDistance >= -0.0001))
		{
			if (ppFaces != NULL)
			{
				ppFaces[nDecalFaces] = pFace;
			}

			nDecalFaces++;
		}
	}

	return(nDecalFaces);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapDecal::Copy(bool bUpdateDependencies)
{
	CMapDecal *pCopy = new CMapDecal;

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: Makes this object identical to the given object.
// Input  : pObject - Object to copy.
//			bUpdateDependencies - 
//-----------------------------------------------------------------------------
CMapClass *CMapDecal::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	Assert(pObject->IsMapClass(MAPCLASS_TYPE(CMapDecal)));
	CMapDecal *pFrom = (CMapDecal *)pObject;

	CMapClass::CopyFrom(pObject, bUpdateDependencies);

	m_pTexture = pFrom->m_pTexture;

	//
	// Copy our list of solids to which we are attached.
	//
	m_Solids.RemoveAll();
	m_Solids.AddVectorToTail(pFrom->m_Solids);

	//
	// Copy our decal faces. We don't copy the pointers because we don't do
	// reference counting yet.
	//
	m_Faces.RemoveAll();
	
	FOR_EACH_OBJ( pFrom->m_Faces, pos )
	{
		DecalFace_t *pDecalFace = new DecalFace_t;
		if (pDecalFace != NULL)
		{
			pDecalFace->pFace = new CMapFace;
			if (pDecalFace->pFace != NULL)
			{
				DecalFace_t *pFromDecalFace = pFrom->m_Faces.Element(pos);

				pDecalFace->pFace->CopyFrom(pFromDecalFace->pFace);
				pDecalFace->pFace->SetParent(this);

				pDecalFace->pSolid = pFromDecalFace->pSolid;

				m_Faces.AddToTail(pDecalFace);
			}
		}
	}

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pSolid - 
//			org - 
//			piFacesRvl - 
// Output : int
//-----------------------------------------------------------------------------
int CMapDecal::DecalSolid(CMapSolid *pSolid)
{
	if (m_pTexture == NULL)
	{
		return(0);
	}

	//
	// Determine how many, if any, faces will accept the decal.
	//
	CMapFace *ppFaces[MAPSOLID_MAX_FACES];
	int nDecalFaces = 0;
	int nTestFaces = CanDecalSolid(pSolid, ppFaces);
	if (nTestFaces != 0)
	{
		//
		// Apply the decal to each face that will accept it.
		//
		for (int nFace = 0; nFace < nTestFaces; nFace++)
		{
			CMapFace *pFace = ppFaces[nFace];

			//
			// Create the polygon, clipping it to this face.
			//
			vec5_t ClipPoints[MAX_CLIPVERT];
			int nPointCount = CreateClippedPoly(pFace, m_pTexture, m_Origin, ClipPoints, sizeof(ClipPoints) / sizeof(ClipPoints[0]));

			if (nPointCount != 0)
			{
				nDecalFaces++;

				Vector CreatePoints[64];
				for (int nPoint = 0; nPoint < nPointCount; nPoint++)
				{
					CreatePoints[nPoint][0] = ClipPoints[nPoint][0];
					CreatePoints[nPoint][1] = ClipPoints[nPoint][1];
					CreatePoints[nPoint][2] = ClipPoints[nPoint][2];
				}

				//
				// Create the decal face from the polygon.
				//
				DecalFace_t *pDecalFace = new DecalFace_t;
				pDecalFace->pFace = new CMapFace;

				pDecalFace->pFace->CreateFace(CreatePoints, nPointCount);

				pDecalFace->pFace->SetRenderColor(255, 255, 255);
				pDecalFace->pFace->SetParent(this);

				//
				// Associate this decal face with the solid.
				//
				pDecalFace->pSolid = pSolid;

				//
				// Set the texture in the decal face.
				//
				pDecalFace->pFace->SetTexture(m_pTexture);
				pDecalFace->pFace->CalcTextureCoords();

				for (int nPoint = 0; nPoint < nPointCount; nPoint++)
				{
					pDecalFace->pFace->SetTextureCoords(nPoint, ClipPoints[nPoint][3], ClipPoints[nPoint][4]);
				}

				m_Faces.AddToTail(pDecalFace);
				break;
			}
		}
	}

	return(nDecalFaces);
}


//-----------------------------------------------------------------------------
// Purpose: Notifies that this object's parent entity has had a key value change.
// Input  : szKey - The key that changed.
//			szValue - The new value of the key.
//-----------------------------------------------------------------------------
void CMapDecal::OnParentKeyChanged(const char* szKey, const char* szValue)
{
	//
	// The decal texture has changed.
	//
	if (!stricmp(szKey, "texture"))
	{
		IEditorTexture *pTexNew = g_Textures.FindActiveTexture(szValue);

		if (pTexNew != NULL)
		{
			m_pTexture = pTexNew;

			//
			// Rebuild all the decal faces with the new texture by pretending
			// that all the solids we are attached to have changed.
			//
			FOR_EACH_OBJ( m_Solids, pos )
			{
				CMapSolid *pSolid = (CMapSolid *)m_Solids.Element(pos);
				if (pSolid != NULL)
				{
					OnNotifyDependent(pSolid, Notify_Changed);
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: This function enumerates all children of the world and tries to
//			apply the decal to them.
//-----------------------------------------------------------------------------
void CMapDecal::DecalAllSolids(CMapWorld *pWorld)
{
	Assert(pWorld != NULL);

	if (pWorld != NULL)
	{
		//
		// Try to apply the decal to every solid in the world.
		//
		EnumChildrenPos_t pos;
		CMapClass *pChild = pWorld->GetFirstDescendent(pos);
		while (pChild != NULL)
		{
			CMapSolid *pSolid = dynamic_cast <CMapSolid *> (pChild);
			if ((pSolid != NULL) && (DecalSolid(pSolid) != 0))
			{
				AddSolid(pSolid);
			}

			pChild = pWorld->GetNextDescendent(pos);
		}

		PostUpdate(Notify_Changed);
	}
}		


//-----------------------------------------------------------------------------
// Purpose: Notifys this decal of a change to a solid that it is attached to.
// Input  : pSolid - The solid that is changing.
//			bSolidDeleted - whether the solid is being deleted.
// Output : Returns true if the decal is still attached to the solid, false if not.
//-----------------------------------------------------------------------------
void CMapDecal::OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType)
{
	CMapSolid *pSolid = dynamic_cast <CMapSolid *> (pObject);

	if (pSolid != NULL)
	{
		//
		// Delete any decal faces that are attached to this solid. They will be
		// rebuilt if we can still decal the solid.
		//
		
		for( int pos = m_Faces.Count()-1; pos>=0; pos-- )
		{
			DecalFace_t *pDecalFace = m_Faces.Element(pos);
			if ((pDecalFace != NULL) && (pDecalFace->pSolid == pSolid))
			{
				delete pDecalFace->pFace;
				delete pDecalFace;
				m_Faces.Remove(pos);
			}
		}

		//
		// Attempt to re-attach to the solid.
		//
		if (eNotifyType != Notify_Removed)
		{
			if (DecalSolid(pSolid) != 0)
			{
				CalcBounds();
				return;
			}
		}

		//
		// We could not re-attach to the solid because it was moved out of range or deleted. If we are
		// no longer attached to any solids, remove our entity from the world.
		//
		int index = m_Solids.Find(pSolid);
		if (index != -1)
		{
			m_Solids.Remove(index);
			UpdateDependency(pSolid, NULL);
		}
	}

	CalcBounds();
}


//-----------------------------------------------------------------------------
// Purpose: Called after the entire map has been loaded. This allows the object
//			to perform any linking with other map objects or to do other operations
//			that require all world objects to be present.
// Input  : pWorld - The world that we are in.
//-----------------------------------------------------------------------------
void CMapDecal::PostloadWorld(CMapWorld *pWorld)
{
	CMapClass::PostloadWorld(pWorld);

	// Apply ourselves to all solids now that the map is loaded.
	DecalAllSolids(pWorld);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDecal::RebuildDecalFaces(void)
{
	//
	// Delete all current decal faces. They will be rebuilt below.
	//
	FOR_EACH_OBJ( m_Faces, pos )
	{
		DecalFace_t *pDecalFace = m_Faces.Element(pos);
		if (pDecalFace != NULL)
		{
			delete pDecalFace->pFace;
			delete pDecalFace;
		}
	}

	m_Faces.RemoveAll();

	//
	// Attach to all eligible solids in the world.
	//
	CMapWorld *pWorld = (CMapWorld *)GetWorldObject(this);
	DecalAllSolids(pWorld);	
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapDecal::Render3D(CRender3D *pRender)
{
	//
	// Determine whether we need to render in one or two passes. If we are selected,
	// and rendering in flat or textured mode, we need to render using two passes.
	//
	int nPasses = 1;
	int nStart = 1;
	SelectionState_t eSelectionState = GetSelectionState();
	EditorRenderMode_t eDefaultRenderMode = pRender->GetDefaultRenderMode();
	if ((eSelectionState != SELECT_NONE) && (eDefaultRenderMode != RENDER_MODE_WIREFRAME))
	{
		nPasses = 2;
	}
	

	if ( eSelectionState == SELECT_MODIFY )
	{
		 nStart = 2;
	}

	pRender->RenderEnable( RENDER_POLYGON_OFFSET_FILL, true );

	for (int nPass = nStart; nPass <= nPasses; nPass++)
	{
		//
		// Render the second pass in wireframe.
		//
		
		if ( nPass == 1 )
		{
			// use the texture instead of the lightmap coord for decals
			if (eDefaultRenderMode == RENDER_MODE_LIGHTMAP_GRID)
				pRender->PushRenderMode( RENDER_MODE_TEXTURED );
			else
				pRender->PushRenderMode( RENDER_MODE_CURRENT );
		}
		else
		{
			pRender->PushRenderMode(RENDER_MODE_WIREFRAME);
		}

		FOR_EACH_OBJ( m_Faces, pos )
		{
			DecalFace_t *pDecalFace = m_Faces.Element(pos);

			if ((pDecalFace != NULL) && (pDecalFace->pFace != NULL))
			{
				pDecalFace->pFace->Render3D(pRender);
			}
		}

		pRender->PopRenderMode();
	}

	pRender->RenderEnable( RENDER_POLYGON_OFFSET_FILL, false );

	
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapDecal::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapDecal::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: Notifies us that a copy of ourselves was pasted.
//-----------------------------------------------------------------------------
void CMapDecal::OnPaste(CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	CMapClass::OnPaste(pCopy, pSourceWorld, pDestWorld, OriginalList, NewList);

	//
	// Apply the copy to all solids in the destination world.
	//
	((CMapDecal *)pCopy)->DecalAllSolids(pDestWorld);
}


//-----------------------------------------------------------------------------
// Purpose: Called just after this object has been removed from the world so
//			that it can unlink itself from other objects in the world.
// Input  : pWorld - The world that we were just removed from.
//			bNotifyChildren - Whether we should forward notification to our children.
//-----------------------------------------------------------------------------
void CMapDecal::OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren)
{
	CMapClass::OnRemoveFromWorld(pWorld, bNotifyChildren);

	//
	// We're going away. Unlink ourselves from any solids that we are attached to.
	//
	FOR_EACH_OBJ( m_Solids, pos )
	{
		CMapSolid *pSolid = (CMapSolid *)m_Solids.Element(pos);
		UpdateDependency(pSolid, NULL);
	}

	m_Solids.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTransBox - 
//-----------------------------------------------------------------------------
void CMapDecal::DoTransform(const VMatrix &matrix)
{
	BaseClass::DoTransform(matrix);
	RebuildDecalFaces();
}
