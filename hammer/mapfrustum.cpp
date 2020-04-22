//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Renders a cone for spotlight entities. Only renders when the parent
//			entity is selected.
//
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "fgdlib/HelperInfo.h"
#include "MapDefs.h"		// dvs: For COORD_NOTINIT
#include "MapEntity.h"
#include "MapFrustum.h"
#include "Render3D.h"
#include "Material.h"
#include "materialsystem/imaterialsystem.h"
#include "TextureSystem.h"
#include "hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapFrustum)


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapFrustum helper from a
//			set of string parameters from the FGD file.
// Input  : *pInfo - Pointer to helper info class which gives us information
//				about how to create the helper.
// Output : Returns a pointer to the helper, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapFrustum::Create(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	CMapFrustum *new1 = new CMapFrustum;
	if( new1 != NULL )
	{
		const char *pszKeyName;
		
		// The first parameter should be the fov key name.
		pszKeyName = pHelperInfo->GetParameter(0);
		if ( pszKeyName )
			V_strncpy( new1->m_szFOVKeyName, pszKeyName, sizeof( new1->m_szFOVKeyName ) );
		
		// Second parameter should be the near plane name.
		pszKeyName = pHelperInfo->GetParameter(1);
		if ( pszKeyName )
			V_strncpy( new1->m_szNearPlaneKeyName, pszKeyName, sizeof( new1->m_szNearPlaneKeyName ) );

		// Third parameter should be the far plane name.
		pszKeyName = pHelperInfo->GetParameter(2);
		if ( pszKeyName )
			V_strncpy( new1->m_szFarPlaneKeyName, pszKeyName, sizeof( new1->m_szFarPlaneKeyName ) );

		pszKeyName = pHelperInfo->GetParameter(3);
		if (pszKeyName != NULL)
		{
			V_strncpy( new1->m_szColorKeyName, pszKeyName, sizeof( new1->m_szColorKeyName ) );
		}

		pszKeyName = pHelperInfo->GetParameter(4);
		if (pszKeyName != NULL)
		{
			new1->m_flPitchScale = Q_atof( pszKeyName );
		}
		else
		{
			new1->m_flPitchScale = 1.0f;
		}
	}
	
	return new1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapFrustum::CMapFrustum(void)
{
	// Set default parameter names.
	V_strncpy( m_szFOVKeyName, "_fov", sizeof( m_szFOVKeyName ) );
	V_strncpy( m_szNearPlaneKeyName, "_NearPlane", sizeof( m_szNearPlaneKeyName ) );
	V_strncpy( m_szFarPlaneKeyName, "_FarPlane", sizeof( m_szFarPlaneKeyName ) );
	V_strncpy( m_szColorKeyName, "_light", sizeof( m_szColorKeyName ) );

	m_flFOV = 90;
	m_flNearPlane = 10;
	m_flFarPlane = 200;
	m_flPitchScale = -1;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Deletes faces allocated by BuildCone.
//-----------------------------------------------------------------------------
CMapFrustum::~CMapFrustum(void)
{
	for (int i = 0; i < m_Faces.Count(); i++)
	{
		CMapFace *pFace = m_Faces.Element(i);
		delete pFace;
	}
}


CMapFace* CMapFrustum::CreateMapFace( const Vector &v1, const Vector &v2, const Vector &v3, const Vector &v4, float flAlpha )
{
	Assert( IsFinite(v1.x) && IsFinite(v1.y) && IsFinite(v1.z) );

	Vector points[4] = {v1,v2,v3,v4};
	
	CMapFace *pFace = new CMapFace;
	pFace->SetRenderColor( r, g, b );
	pFace->SetRenderAlpha( flAlpha );
	pFace->CreateFace( points, 4 );
	pFace->RenderUnlit(true);
	
	return pFace;
}

//-----------------------------------------------------------------------------
// Purpose: Builds the light cone faces in local space. Does NOT call CalcBounds,
//			because that CalcBounds updates the parent, which causes problems
//			in the undo system.
//-----------------------------------------------------------------------------
void CMapFrustum::BuildFrustumFaces(void)
{
	//
	// Delete the current face list.
	//
	for (int i = 0; i < m_Faces.Count(); i++)
	{
		CMapFace *pFace = m_Faces.Element(i);
		delete pFace;
	}
	m_Faces.RemoveAll();

	
	// 6 total faces. 4 on the sides and 2 caps.
	Vector vNearFace[4], vFarFace[4];

	float flHalfFOV = m_flFOV / 2.0f;
	flHalfFOV = clamp( flHalfFOV, 0.01f, 89.0f );
	
	float flScaleFactor = tan( DEG2RAD( flHalfFOV ) );
	float flBaseAlpha = 180;
	
	vNearFace[0].Init( m_flNearPlane, -flScaleFactor*m_flNearPlane, -flScaleFactor*m_flNearPlane );
	vNearFace[1].Init( m_flNearPlane, +flScaleFactor*m_flNearPlane, -flScaleFactor*m_flNearPlane );
	vNearFace[2].Init( m_flNearPlane, +flScaleFactor*m_flNearPlane, +flScaleFactor*m_flNearPlane );
	vNearFace[3].Init( m_flNearPlane, -flScaleFactor*m_flNearPlane, +flScaleFactor*m_flNearPlane );
	
	vFarFace[0].Init( m_flFarPlane, -flScaleFactor*m_flFarPlane, -flScaleFactor*m_flFarPlane );
	vFarFace[1].Init( m_flFarPlane, +flScaleFactor*m_flFarPlane, -flScaleFactor*m_flFarPlane );
	vFarFace[2].Init( m_flFarPlane, +flScaleFactor*m_flFarPlane, +flScaleFactor*m_flFarPlane );
	vFarFace[3].Init( m_flFarPlane, -flScaleFactor*m_flFarPlane, +flScaleFactor*m_flFarPlane );

	// Build the near and far faces.
	m_Faces.AddToTail( CreateMapFace( vNearFace[0], vNearFace[1], vNearFace[2], vNearFace[3], flBaseAlpha ) );
	m_Faces.AddToTail( CreateMapFace( vFarFace[3], vFarFace[2], vFarFace[1], vFarFace[0], flBaseAlpha ) );

	// Build the 4 cap faces.
	for ( int i=0; i < 4; i++ )
	{
		m_Faces.AddToTail( CreateMapFace( vNearFace[i], vFarFace[i], vFarFace[(i+1)%4], vNearFace[(i+1)%4], flBaseAlpha ) );
	}
	
	// Also build some really translucent faces from the origin to the near plane.
	float flOriginFacesAlpha = 40.0f;
	for ( int i=0; i < 4; i++ )
	{
		m_Faces.AddToTail( CreateMapFace( Vector(0,0,0), vNearFace[i], vNearFace[(i+1)%4], Vector(0,0,0), flOriginFacesAlpha ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapFrustum::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);

	//
	// HACK: Update our origin to stick to our parent.
	//
	if (m_pParent != NULL)
	{
		GetParent()->GetOrigin(m_Origin);
	}

	//
	// Pretend to be very small for the 2D view. Won't be necessary when 2D
	// rendering is done in the map classes.
	//
	m_Render2DBox.ResetBounds();
	m_Render2DBox.UpdateBounds(m_Origin);

	SetCullBoxFromFaceList( &m_Faces );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapFrustum::Copy(bool bUpdateDependencies)
{
	CMapFrustum *pCopy = new CMapFrustum;

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapFrustum::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	Assert(pObject->IsMapClass(MAPCLASS_TYPE(CMapFrustum)));
	CMapFrustum *pFrom = (CMapFrustum *)pObject;

	CMapClass::CopyFrom(pObject, bUpdateDependencies);

	m_flFOV = pFrom->m_flFOV;
	m_flNearPlane = pFrom->m_flNearPlane;
	m_flFarPlane = pFrom->m_flFarPlane;
	m_Angles = pFrom->m_Angles;
	m_flPitchScale = pFrom->m_flPitchScale;
	
	V_strncpy( m_szFOVKeyName, pFrom->m_szFOVKeyName, sizeof( m_szFOVKeyName ) );
	V_strncpy( m_szNearPlaneKeyName, pFrom->m_szNearPlaneKeyName, sizeof( m_szNearPlaneKeyName ) );
	V_strncpy( m_szFarPlaneKeyName, pFrom->m_szFarPlaneKeyName, sizeof( m_szFarPlaneKeyName ) );
	V_strncpy( m_szColorKeyName, pFrom->m_szColorKeyName, sizeof( m_szColorKeyName ) );

	BuildFrustumFaces();

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: Notifies that this object's parent entity has had a key value change.
// Input  : szKey - The key that changed.
//			szValue - The new value of the key.
//-----------------------------------------------------------------------------
void CMapFrustum::OnParentKeyChanged(const char *szKey, const char *szValue)
{
	bool bRebuild = true;

	if (!stricmp(szKey, "angles"))
	{
		sscanf(szValue, "%f %f %f", &m_Angles[PITCH], &m_Angles[YAW], &m_Angles[ROLL]);
	}
	else if (!stricmp(szKey, m_szColorKeyName))
	{
		int nRed;
		int nGreen;
		int nBlue;
		sscanf(szValue, "%d %d %d", &nRed, &nGreen, &nBlue);

		r = nRed;
		g = nGreen;
		b = nBlue;
	}
	else if (!stricmp(szKey, m_szFOVKeyName))
	{
		m_flFOV = atof(szValue);
	}
	else if (!stricmp(szKey, m_szNearPlaneKeyName))
	{
		m_flNearPlane = atof(szValue);
	}
	else if (!stricmp(szKey, m_szFarPlaneKeyName))
	{
		m_flFarPlane = atof(szValue);
	}
	else
	{
		bRebuild = false;
	}

	if (bRebuild)
	{
		BuildFrustumFaces();
		PostUpdate(Notify_Changed);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called after the entire map has been loaded. This allows the object
//			to perform any linking with other map objects or to do other operations
//			that require all world objects to be present.
// Input  : pWorld - The world that we are in.
//-----------------------------------------------------------------------------
void CMapFrustum::PostloadWorld(CMapWorld *pWorld)
{
	CMapClass::PostloadWorld(pWorld);

	BuildFrustumFaces();
	CalcBounds();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapFrustum::Render3D(CRender3D *pRender)
{
	if (m_pParent->IsSelected())
	{
		CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
		pRenderContext->MatrixMode(MATERIAL_MODEL);
		pRenderContext->PushMatrix();

		pRenderContext->Translate(m_Origin[0],  m_Origin[1],  m_Origin[2]);

		QAngle Angles;
		GetAngles(Angles);

		pRenderContext->Rotate(Angles[YAW], 0, 0, 1);
		pRenderContext->Rotate(m_flPitchScale * Angles[PITCH], 0, -1, 0);
		pRenderContext->Rotate(Angles[ROLL], 1, 0, 0);

		if (
			(pRender->GetCurrentRenderMode() != RENDER_MODE_LIGHT_PREVIEW2) &&
			(pRender->GetCurrentRenderMode() != RENDER_MODE_LIGHT_PREVIEW_RAYTRACED) &&
			(GetSelectionState() != SELECT_MODIFY )
			)
		{
			// Render the cone faces flatshaded.
			pRender->PushRenderMode( RENDER_MODE_TRANSLUCENT_FLAT );
			
			for (int i = 0; i < m_Faces.Count(); i++)
			{
				CMapFace *pFace = m_Faces.Element(i);
				pFace->Render3D(pRender);
			}

			pRender->PopRenderMode();
		}

		//
		// Render the cone faces in yellow wireframe (on top)
		//
		pRender->PushRenderMode( RENDER_MODE_WIREFRAME );

		for (int i = 0; i < m_Faces.Count(); i++)
		{
			CMapFace *pFace = m_Faces.Element(i);
			pFace->Render3D(pRender);
		}

		//
		// Restore the default rendering mode.
		//
		pRender->PopRenderMode();

		pRenderContext->PopMatrix();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapFrustum::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapFrustum::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


void CMapFrustum::GetAngles(QAngle& Angles)
{
	Angles = m_Angles;
}

