//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This helper is used for entities that represent a line between two
//			entities. Examples of these are: beams and special node connections.
//
//			The helper factory parameters are:
//
//			<red> <green> <blue> <start key> <start key value> <end key> <end key value>
//
//			The line helper looks in the given keys in its parent entity and
//			attaches itself to the entities with those key values. If only one
//			endpoint entity is specified, the other end is assumed to be the parent
//			entity.
//
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "MapEntity.h"
#include "MapCylinder.h"
#include "MapWorld.h"
#include "Render2D.h"
#include "Render3D.h"
#include "TextureSystem.h"
#include "materialsystem/imesh.h"
#include "Material.h"
#include "mapdoc.h"
#include "options.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapCylinder);


#define CYLINDER_VERTEX_COUNT 16
#define CYLINDER_VERTEX_COUNT_2D 8

//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapCylinder from a set
//			of string parameters from the FGD file.
// Input  : *pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapCylinder::Create(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	CMapCylinder *pCylinder = NULL;

	//
	// Extract the line color from the parameter list.
	//
	unsigned char chRed = 255;
	unsigned char chGreen = 255;
	unsigned char chBlue = 255;

	const char *pszParam = pHelperInfo->GetParameter(0);
	if (pszParam != NULL)
	{
		chRed = atoi(pszParam);
	}

	pszParam = pHelperInfo->GetParameter(1);
	if (pszParam != NULL)
	{
		chGreen = atoi(pszParam);
	}

	pszParam = pHelperInfo->GetParameter(2);
	if (pszParam != NULL)
	{
		chBlue = atoi(pszParam);
	}

	const char *pszStartKey = pHelperInfo->GetParameter(3);
	const char *pszStartValueKey = pHelperInfo->GetParameter(4);
	const char *pszStartRadiusKey = pHelperInfo->GetParameter(5);
	const char *pszEndKey = pHelperInfo->GetParameter(6);
	const char *pszEndValueKey = pHelperInfo->GetParameter(7);
	const char *pszEndRadiusKey = pHelperInfo->GetParameter(8);

	//
	// Make sure we'll have at least one endpoint to work with.
	//
	if ((pszStartKey == NULL) || (pszStartValueKey == NULL))
	{
		return NULL;
	}

	pCylinder = new CMapCylinder(pszStartKey, pszStartValueKey, pszStartRadiusKey, pszEndKey, pszEndValueKey, pszEndRadiusKey);
	pCylinder->SetRenderColor(chRed, chGreen, chBlue);

	//
	// If they only specified a start entity, use our parent as the end entity.
	//
	if ((pszEndKey == NULL) || (pszEndValueKey == NULL))
	{
		pCylinder->m_pEndEntity = pParent;
	}

	return(pCylinder);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapCylinder::CMapCylinder(void)
{
	Initialize();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
// Input  : pszStartKey - The key to search in other entities for a match against the value of pszStartValueKey, ex 'targetname'.
//			pszStartValueKey - The key in our parent entity from which to get a search term for the start entity ex 'beamstart01'.
//			pszEndKey - The key to search in other entities for a match against the value of pszEndValueKey ex 'targetname'.
//			pszEndValueKey - The key in our parent entity from which to get a search term for the end entity ex 'beamend01'.
//-----------------------------------------------------------------------------
CMapCylinder::CMapCylinder(const char *pszStartKey, const char *pszStartValueKey, const char *pszStartRadiusKey, 
						   const char *pszEndKey, const char *pszEndValueKey, const char *pszEndRadiusKey )
{	
	Initialize();

	strcpy(m_szStartKey, pszStartKey);
	strcpy(m_szStartValueKey, pszStartValueKey);

	if ( pszStartRadiusKey != NULL )
	{
		strcpy(m_szStartRadiusKey, pszStartRadiusKey);
	}

	if ((pszEndKey != NULL) && (pszEndValueKey != NULL))
	{
		strcpy(m_szEndKey, pszEndKey);
		strcpy(m_szEndValueKey, pszEndValueKey);

		if ( pszEndRadiusKey != NULL )
		{
			strcpy(m_szEndRadiusKey, pszEndRadiusKey);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets data members to initial values.
//-----------------------------------------------------------------------------
void CMapCylinder::Initialize(void)
{
	m_szStartKey[0] = '\0';
	m_szStartValueKey[0] = '\0';
	m_szStartRadiusKey[0] = '\0';

	m_szEndKey[0] = '\0';
	m_szEndValueKey[0] = '\0';
	m_szEndRadiusKey[0] = '\0';

	m_pStartEntity = NULL;
	m_pEndEntity = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapCylinder::~CMapCylinder(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the midpoint of the line and sets our origin there.
//-----------------------------------------------------------------------------
void CMapCylinder::BuildCylinder(void)
{
	if ((m_pStartEntity != NULL) && (m_pEndEntity != NULL))
	{
		//
		// Set our origin to our midpoint. This moves our selection handle box to the
		// midpoint.
		//
		Vector Start;
		Vector End;

		m_pStartEntity->GetOrigin(Start);
		m_pEndEntity->GetOrigin(End);

		SetOrigin((Start + End) / 2);
	}

	CalcBounds();
}


//-----------------------------------------------------------------------------
// Purpose: Recalculates our bounding box.
// Input  : bFullUpdate - Whether to force our children to recalculate or not.
//-----------------------------------------------------------------------------
void CMapCylinder::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);
	
	//
	// Don't calculate 2D bounds - we don't occupy any space in 2D. This keeps our
	// parent entity's bounds from expanding to encompass our endpoints.
	//

	//
	// Update our 3D culling box and possibly our origin.
	//
	// If our start and end entities are resolved, calcuate our bounds
	// based on the positions of the start and end entities.
	//
	if (m_pStartEntity && m_pEndEntity)
	{
		//
		// Update the 3D bounds.
		//
		Vector Start;
		Vector End;

		Vector pStartVerts[CYLINDER_VERTEX_COUNT];
		Vector pEndVerts[CYLINDER_VERTEX_COUNT];
		ComputeCylinderPoints( CYLINDER_VERTEX_COUNT, pStartVerts, pEndVerts );
		for ( int i = 0; i < CYLINDER_VERTEX_COUNT; ++i )
		{
			m_CullBox.UpdateBounds(pStartVerts[i]);
			m_CullBox.UpdateBounds(pEndVerts[i]);
		}
		m_BoundingBox = m_CullBox;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bUpdateDependencies - 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapCylinder::Copy(bool bUpdateDependencies)
{
	CMapCylinder *pCopy = new CMapCylinder;

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: Turns 'this' into an exact replica of 'pObject'.
// Input  : pObject - Object to replicate.
//			bUpdateDependencies - 
// Output : 
//-----------------------------------------------------------------------------
CMapClass *CMapCylinder::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	CMapCylinder *pFrom = dynamic_cast <CMapCylinder *>(pObject);

	if (pFrom != NULL)
	{
		CMapClass::CopyFrom(pObject, bUpdateDependencies);

		if (bUpdateDependencies)
		{
			m_pStartEntity = (CMapEntity *)UpdateDependency(m_pStartEntity, pFrom->m_pStartEntity);
			m_pEndEntity = (CMapEntity *)UpdateDependency(m_pEndEntity, pFrom->m_pEndEntity);
		}
		else
		{
			m_pStartEntity = pFrom->m_pStartEntity;
			m_pEndEntity = pFrom->m_pEndEntity;
		}

		m_flStartRadius = pFrom->m_flStartRadius;
		m_flEndRadius = pFrom->m_flEndRadius;

		strcpy(m_szStartValueKey, pFrom->m_szStartValueKey);
		strcpy(m_szStartKey, pFrom->m_szStartKey);
		strcpy(m_szStartRadiusKey, pFrom->m_szStartRadiusKey);

		strcpy(m_szEndValueKey, pFrom->m_szEndValueKey);
		strcpy(m_szEndKey, pFrom->m_szEndKey);
		strcpy(m_szEndRadiusKey, pFrom->m_szEndRadiusKey);
	}

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: Called after this object is added to the world.
//
//			NOTE: This function is NOT called during serialization. Use PostloadWorld
//				  to do similar bookkeeping after map load.
//
// Input  : pWorld - The world that we have been added to.
//-----------------------------------------------------------------------------
void CMapCylinder::OnAddToWorld(CMapWorld *pWorld)
{
	CMapClass::OnAddToWorld(pWorld);

	//
	// Updates our start and end entity pointers since we are being added
	// into the world.
	//
	UpdateDependencies(pWorld, NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Called just after this object has been removed from the world so
//			that it can unlink itself from other objects in the world.
// Input  : pWorld - The world that we were just removed from.
//			bNotifyChildren - Whether we should forward notification to our children.
//-----------------------------------------------------------------------------
void CMapCylinder::OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren)
{
	CMapClass::OnRemoveFromWorld(pWorld, bNotifyChildren);

	//
	// Detach ourselves from the endpoint entities.
	//
	m_pStartEntity = (CMapEntity *)UpdateDependency(m_pStartEntity, NULL);
	m_pEndEntity = (CMapEntity *)UpdateDependency(m_pEndEntity, NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Our start or end entity has changed; recalculate our bounds and midpoint.
// Input  : pObject - Entity that changed.
//-----------------------------------------------------------------------------
void CMapCylinder::OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType)
{
	CMapClass::OnNotifyDependent(pObject, eNotifyType);

	CMapWorld *pWorld = (CMapWorld *)GetWorldObject(this);
	UpdateDependencies(pWorld, pObject);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : key - 
//			value - 
//-----------------------------------------------------------------------------
void CMapCylinder::OnParentKeyChanged( const char* key, const char* value )
{
	CMapWorld *pWorld = (CMapWorld *)GetWorldObject(this);
	if (pWorld != NULL)
	{
		if (stricmp(key, m_szStartValueKey) == 0)
		{
			m_pStartEntity = (CMapEntity *)UpdateDependency(m_pStartEntity, pWorld->FindChildByKeyValue(m_szStartKey, value));
			BuildCylinder();
		}
		else if (stricmp(key, m_szEndValueKey) == 0)
		{
			m_pEndEntity = (CMapEntity *)UpdateDependency(m_pEndEntity, pWorld->FindChildByKeyValue(m_szEndKey, value));
			BuildCylinder();
		}
		
		if (m_pStartEntity && stricmp(key, m_szStartRadiusKey) == 0)
		{
			const char *pRadiusKey = m_pStartEntity->GetKeyValue( m_szStartRadiusKey );
			m_flStartRadius = pRadiusKey ? atof( pRadiusKey ) : 0.0f;
			BuildCylinder();
		}

		if (m_pEndEntity && stricmp(key, m_szEndRadiusKey) == 0)
		{
			const char *pRadiusKey = m_pEndEntity->GetKeyValue( m_szEndRadiusKey );
			m_flEndRadius = pRadiusKey ? atof( pRadiusKey ) : 0.0f;
			BuildCylinder();
		}
	}
}


//-----------------------------------------------------------------------------
// Computes the vertices of the cylinder
//-----------------------------------------------------------------------------
void CMapCylinder::ComputeCylinderPoints( int nCount, Vector *pStartVerts, Vector *pEndVerts )
{
	Assert ((m_pStartEntity != NULL) && (m_pEndEntity != NULL));

	Vector vecStart;
	Vector vecEnd;
	m_pStartEntity->GetOrigin(vecStart);
	m_pEndEntity->GetOrigin(vecEnd);

	// Compute a basis perpendicular to the entities
	Vector xvec, yvec, zvec;
	VectorSubtract( vecEnd, vecStart, zvec );
	float flLength = VectorNormalize( zvec );
	if ( flLength < 1e-3 )
	{
		zvec.Init( 0, 0, 1 );
	}
	VectorVectors( zvec, xvec, yvec );

	int i;
	float flDAngle = 2.0f * M_PI / nCount;
	for ( i = 0; i < nCount; ++i )
	{
		float flCosAngle = cos( flDAngle * i );
		float flSinAngle = sin( flDAngle * i );

		VectorMA( vecStart, flCosAngle * m_flStartRadius, xvec, pStartVerts[i] );
		VectorMA( pStartVerts[i], flSinAngle * m_flStartRadius, yvec, pStartVerts[i] );

		VectorMA( vecEnd, flCosAngle * m_flEndRadius, xvec, pEndVerts[i] );
		VectorMA( pEndVerts[i], flSinAngle * m_flEndRadius, yvec, pEndVerts[i] );
	}
}


//-----------------------------------------------------------------------------
// Should we draw the cylinder as a line?
//-----------------------------------------------------------------------------
bool CMapCylinder::ShouldDrawAsLine()
{
	return !IsSelected() || ((m_flStartRadius == 0.0f) && (m_flEndRadius == 0.0f)) || !Options.GetShowHelpers();
}


//-----------------------------------------------------------------------------
// Purpose: Renders the line helper in the 2D view.
// Input  : pRender - 2D rendering interface.
//-----------------------------------------------------------------------------
void CMapCylinder::Render2D(CRender2D *pRender)
{
	if ((m_pStartEntity != NULL) && (m_pEndEntity != NULL))
	{
		if (!ShouldDrawAsLine())
		{
			pRender->SetDrawColor( SELECT_FACE_RED, SELECT_FACE_GREEN, SELECT_FACE_BLUE );

			Vector pStartVerts[CYLINDER_VERTEX_COUNT_2D];
			Vector pEndVerts[CYLINDER_VERTEX_COUNT_2D];
			ComputeCylinderPoints( CYLINDER_VERTEX_COUNT_2D, pStartVerts, pEndVerts );
			int j = CYLINDER_VERTEX_COUNT_2D - 1;
			for (int i = 0; i < CYLINDER_VERTEX_COUNT_2D; j = i++ )
			{
				pRender->DrawLine(pStartVerts[i], pStartVerts[j]);
				pRender->DrawLine(pEndVerts[i], pEndVerts[j]);
				pRender->DrawLine(pStartVerts[i], pEndVerts[i]);
			}

		}
		else
		{
			pRender->SetDrawColor( r, g, b );

			Vector Start;
			Vector End;

			m_pStartEntity->GetOrigin(Start);
			m_pEndEntity->GetOrigin(End);
			pRender->DrawLine(Start, End);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapCylinder::Render3D(CRender3D *pRender)
{
	if ( (m_pStartEntity == NULL) || (m_pEndEntity == NULL))
		return;

	pRender->BeginRenderHitTarget(this);
	pRender->PushRenderMode(RENDER_MODE_WIREFRAME);
	
	Vector Start,End;
	
	m_pStartEntity->GetOrigin(Start);
	m_pEndEntity->GetOrigin(End);

	unsigned char color[3];
	if (IsSelected())
	{
		color[0] = SELECT_EDGE_RED; 
		color[1] = SELECT_EDGE_GREEN;
		color[2] = SELECT_EDGE_BLUE;
	}
	else
	{
		color[0] = r;
		color[1] = g; 
		color[2] = b;
	}

	CMeshBuilder meshBuilder;
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	if ( !ShouldDrawAsLine() )
	{
		Vector pStartVerts[CYLINDER_VERTEX_COUNT];
		Vector pEndVerts[CYLINDER_VERTEX_COUNT];
		ComputeCylinderPoints( CYLINDER_VERTEX_COUNT, pStartVerts, pEndVerts );

		meshBuilder.Begin( pMesh, MATERIAL_LINES, 3 * CYLINDER_VERTEX_COUNT );

		int j = CYLINDER_VERTEX_COUNT - 1;
		for ( int i = 0; i < CYLINDER_VERTEX_COUNT; j = i++ )
		{
			meshBuilder.Color3ubv( color );
			meshBuilder.Position3f(pStartVerts[i].x, pStartVerts[i].y, pStartVerts[i].z);
			meshBuilder.AdvanceVertex();

			meshBuilder.Color3ubv( color );
			meshBuilder.Position3f(pStartVerts[j].x, pStartVerts[j].y, pStartVerts[j].z);
			meshBuilder.AdvanceVertex();

			meshBuilder.Color3ubv( color );
			meshBuilder.Position3f(pEndVerts[i].x, pEndVerts[i].y, pEndVerts[i].z);
			meshBuilder.AdvanceVertex();

			meshBuilder.Color3ubv( color );
			meshBuilder.Position3f(pEndVerts[j].x, pEndVerts[j].y, pEndVerts[j].z);
			meshBuilder.AdvanceVertex();

			meshBuilder.Color3ubv( color );
			meshBuilder.Position3f(pStartVerts[i].x, pStartVerts[i].y, pStartVerts[i].z);
			meshBuilder.AdvanceVertex();

			meshBuilder.Color3ubv( color );
			meshBuilder.Position3f(pEndVerts[i].x, pEndVerts[i].y, pEndVerts[i].z);
			meshBuilder.AdvanceVertex();
		}

		meshBuilder.End();
	}
	else
	{
		meshBuilder.Begin( pMesh, MATERIAL_LINES, 1 );

		meshBuilder.Color3ubv( color );
		meshBuilder.Position3f(Start.x, Start.y, Start.z);
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3ubv( color );
		meshBuilder.Position3f(End.x, End.y, End.z);
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
	}

	pMesh->Draw();

	pRender->PopRenderMode();
	pRender->EndRenderHitTarget();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapCylinder::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapCylinder::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTransBox - 
//-----------------------------------------------------------------------------
void CMapCylinder::DoTransform(const VMatrix &matrix)
{
	CMapClass::DoTransform(matrix);
	BuildCylinder();
}

//-----------------------------------------------------------------------------
// Purpose: Updates the cached pointers to our start and end entities by looking
//			for them in the given world.
// Input  : pWorld - World to search.
//-----------------------------------------------------------------------------
void CMapCylinder::UpdateDependencies(CMapWorld *pWorld, CMapClass *pObject)
{
	CMapClass::UpdateDependencies(pWorld, pObject);

	if (pWorld == NULL)
	{
		return;
	}

	CMapEntity *pEntity = dynamic_cast <CMapEntity *> (m_pParent);
	Assert(pEntity != NULL);

	if (pEntity != NULL)
	{
		const char *pszValue = pEntity->GetKeyValue(m_szStartValueKey);
		m_pStartEntity = (CMapEntity *)UpdateDependency(m_pStartEntity, pWorld->FindChildByKeyValue(m_szStartKey, pszValue));

		if (m_szEndValueKey[0] != '\0')
		{
			pszValue = pEntity->GetKeyValue(m_szEndValueKey);
			m_pEndEntity = (CMapEntity *)UpdateDependency(m_pEndEntity, pWorld->FindChildByKeyValue(m_szEndKey, pszValue));
		}
		else
		{
			// We don't have an end entity specified, use our parent as the end point.
			m_pEndEntity = (CMapEntity *)UpdateDependency(m_pEndEntity, GetParent());
		}

		if (pObject == m_pStartEntity)
		{
			m_flStartRadius = 0.0f;
			if ( m_pStartEntity && m_szStartRadiusKey[0] != '\0' )
			{
				const char *pRadiusKey = m_pStartEntity->GetKeyValue( m_szStartRadiusKey );
				m_flStartRadius = pRadiusKey ? atof( pRadiusKey ) : 0.0f;
			}
		}

		if (pObject == m_pEndEntity)
		{
			m_flEndRadius = 0.0f;
			if ( m_pEndEntity && m_szEndRadiusKey[0] != '\0' )
			{
				const char *pRadiusKey = m_pEndEntity->GetKeyValue( m_szEndRadiusKey );
				m_flEndRadius = pRadiusKey ? atof( pRadiusKey ) : 0.0f;
			}
		}

		BuildCylinder();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Never select anything because of this helper.
//-----------------------------------------------------------------------------
CMapClass *CMapCylinder::PrepareSelection(SelectMode_t eSelectMode)
{
	return NULL;
}


