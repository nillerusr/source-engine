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
#include "MapLine.h"
#include "MapWorld.h"
#include "Render2D.h"
#include "Render3D.h"
#include "TextureSystem.h"
#include "materialsystem/imesh.h"
#include "Material.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

IMPLEMENT_MAPCLASS(CMapLine);


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapLine from a set
//			of string parameters from the FGD file.
// Input  : *pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapLine::Create(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	CMapLine *pLine = NULL;

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

	const char *pszEndKey = pHelperInfo->GetParameter(5);
	const char *pszEndValueKey = pHelperInfo->GetParameter(6);

	//
	// Make sure we'll have at least one endpoint to work with.
	//
	if ((pszStartKey == NULL) || (pszStartValueKey == NULL))
	{
		return NULL;
	}

	pLine = new CMapLine(pszStartKey, pszStartValueKey, pszEndKey, pszEndValueKey);
	pLine->SetRenderColor(chRed, chGreen, chBlue);

	//
	// If they only specified a start entity, use our parent as the end entity.
	//
	if ((pszEndKey == NULL) || (pszEndValueKey == NULL))
	{
		pLine->m_pEndEntity = pParent;
	}

	return(pLine);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapLine::CMapLine(void)
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
CMapLine::CMapLine(const char *pszStartKey, const char *pszStartValueKey, const char *pszEndKey, const char *pszEndValueKey)
{	
	Initialize();

	strcpy(m_szStartKey, pszStartKey);
	strcpy(m_szStartValueKey, pszStartValueKey);

	if ((pszEndKey != NULL) && (pszEndValueKey != NULL))
	{
		strcpy(m_szEndKey, pszEndKey);
		strcpy(m_szEndValueKey, pszEndValueKey);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets data members to initial values.
//-----------------------------------------------------------------------------
void CMapLine::Initialize(void)
{
	m_szStartKey[0] = '\0';
	m_szStartValueKey[0] = '\0';

	m_szEndKey[0] = '\0';
	m_szEndValueKey[0] = '\0';

	m_pStartEntity = NULL;
	m_pEndEntity = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapLine::~CMapLine(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the midpoint of the line and sets our origin there.
//-----------------------------------------------------------------------------
void CMapLine::BuildLine(void)
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
void CMapLine::CalcBounds(BOOL bFullUpdate)
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

		m_pStartEntity->GetOrigin(Start);
		m_CullBox.UpdateBounds(Start);

		m_pEndEntity->GetOrigin(End);
		m_CullBox.UpdateBounds(End);
	}

	m_BoundingBox = m_CullBox;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bUpdateDependencies - 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapLine::Copy(bool bUpdateDependencies)
{
	CMapLine *pCopy = new CMapLine;

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
CMapClass *CMapLine::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	CMapLine *pFrom = dynamic_cast <CMapLine *>(pObject);

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

		strcpy(m_szStartValueKey, pFrom->m_szStartValueKey);
		strcpy(m_szStartKey, pFrom->m_szStartKey);

		strcpy(m_szEndValueKey, pFrom->m_szEndValueKey);
		strcpy(m_szEndKey, pFrom->m_szEndKey);
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
void CMapLine::OnAddToWorld(CMapWorld *pWorld)
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
void CMapLine::OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren)
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
void CMapLine::OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType)
{
	CMapClass::OnNotifyDependent(pObject, eNotifyType);

	CMapWorld *pWorld = (CMapWorld *)GetWorldObject(this);
	UpdateDependencies(pWorld, NULL);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : key - 
//			value - 
//-----------------------------------------------------------------------------
void CMapLine::OnParentKeyChanged( const char* key, const char* value )
{
	CMapWorld *pWorld = (CMapWorld *)GetWorldObject(this);
	if (pWorld != NULL)
	{
		if (stricmp(key, m_szStartValueKey) == 0)
		{
			m_pStartEntity = (CMapEntity *)UpdateDependency(m_pStartEntity, pWorld->FindChildByKeyValue(m_szStartKey, value));
			BuildLine();
		}
		else if (stricmp(key, m_szEndValueKey) == 0)
		{
			m_pEndEntity = (CMapEntity *)UpdateDependency(m_pEndEntity, pWorld->FindChildByKeyValue(m_szEndKey, value));
			BuildLine();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Renders the line helper in the 2D view.
// Input  : pRender - 2D rendering interface.
//-----------------------------------------------------------------------------
void CMapLine::Render2D(CRender2D *pRender)
{
	if ((m_pStartEntity != NULL) && (m_pEndEntity != NULL))
	{
		Vector Start;
		Vector End;

		m_pStartEntity->GetOrigin(Start);
		m_pEndEntity->GetOrigin(End);

		if (IsSelected())
		{
			pRender->SetDrawColor( SELECT_FACE_RED, SELECT_FACE_GREEN, SELECT_FACE_BLUE );
		}
		else
		{
			pRender->SetDrawColor( r, g, b );
		}

		pRender->DrawLine(Start, End);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapLine::Render3D(CRender3D *pRender)
{
	if ( (m_pStartEntity == NULL) || (m_pEndEntity == NULL) )
		return;

	pRender->BeginRenderHitTarget(this);
	pRender->PushRenderMode(RENDER_MODE_WIREFRAME);
	
	Vector Start, End;
	
	m_pStartEntity->GetOrigin(Start);
	m_pEndEntity->GetOrigin(End);



	CMeshBuilder meshBuilder;
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	// FIXME: Can't do this...! glLineWidth(2);

	meshBuilder.Begin( pMesh, MATERIAL_LINES, 1 );

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

	meshBuilder.Color3ubv( color );
	meshBuilder.Position3f(Start.x, Start.y, Start.z);
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3ubv( color );
	meshBuilder.Position3f(End.x, End.y, End.z);
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRender->EndRenderHitTarget();
	pRender->PopRenderMode();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapLine::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapLine::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTransBox - 
//-----------------------------------------------------------------------------
void CMapLine::DoTransform(const VMatrix &matrix)
{
	BaseClass::DoTransform(matrix);
	BuildLine();
}

//-----------------------------------------------------------------------------
// Purpose: Updates the cached pointers to our start and end entities by looking
//			for them in the given world.
// Input  : pWorld - World to search.
//-----------------------------------------------------------------------------
void CMapLine::UpdateDependencies(CMapWorld *pWorld, CMapClass *pObject)
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

		BuildLine();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Never select anything because of this helper.
//-----------------------------------------------------------------------------
CMapClass *CMapLine::PrepareSelection(SelectMode_t eSelectMode)
{
	return NULL;
}


