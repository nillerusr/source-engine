//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a connection (output-to-input) between two entities.
//
//			The behavior in-game is as follows:
//
//			When the given output in the source entity is triggered, the given
//			input in the target entity is called after a specified delay, and
//			the parameter override (if any) is passed to the input handler. If
//			there is no parameter override, the default parameter is passed.
//
//			This behavior will occur a specified number of times before the
//			connection between the two entities is removed.
//
//=============================================================================//

#include "stdafx.h"
#include "EntityConnection.h"
#include "MapEntity.h"
#include "MapDoc.h"
#include "MapWorld.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CEntityConnection::CEntityConnection(void)
{
	memset(m_szSourceEntity, 0, sizeof(m_szSourceEntity));
	memset(m_szTargetEntity, 0, sizeof(m_szTargetEntity));
	memset(m_szOutput, 0, sizeof(m_szOutput));
	memset(m_szInput, 0, sizeof(m_szInput));
	memset(m_szParam, 0, sizeof(m_szParam));

	m_pSourceEntityList = new CMapEntityList;
	m_pTargetEntityList = new CMapEntityList;

	m_fDelay = 0;
	m_nTimesToFire = EVENT_FIRE_ALWAYS;
}

//-----------------------------------------------------------------------------
// Purpose: Copy Constructor.
//-----------------------------------------------------------------------------

CEntityConnection::CEntityConnection( const CEntityConnection &Other )
{
	m_pSourceEntityList = new CMapEntityList;
	m_pTargetEntityList = new CMapEntityList;

	*this = Other;	// Invoke the Operator= to complete the construction job
}

//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CEntityConnection::~CEntityConnection()
{
	if ( m_pSourceEntityList )
	{
        m_pSourceEntityList->RemoveAll();
		delete m_pSourceEntityList;
		m_pSourceEntityList = NULL;
	}
	if ( m_pTargetEntityList )
	{
		m_pTargetEntityList->RemoveAll();
		delete m_pTargetEntityList;
		m_pTargetEntityList = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Operator= overload. Makes 'this' identical to 'Other'.
//-----------------------------------------------------------------------------
CEntityConnection &CEntityConnection::operator =(const CEntityConnection &Other)
{
	strcpy(m_szSourceEntity, Other.m_szSourceEntity);
	strcpy(m_szTargetEntity, Other.m_szTargetEntity);
	strcpy(m_szOutput, Other.m_szOutput);
	strcpy(m_szInput, Other.m_szInput);
	strcpy(m_szParam, Other.m_szParam);
	m_fDelay = Other.m_fDelay;
	m_nTimesToFire = Other.m_nTimesToFire;

	// Invoke EntityList operator= to make copies.
	*m_pSourceEntityList = *Other.m_pSourceEntityList;
	*m_pTargetEntityList = *Other.m_pTargetEntityList;

	return(*this);
}

//-----------------------------------------------------------------------------
// Purpose: Sets a new Input Name and sets links to any matching entities
//-----------------------------------------------------------------------------

void CEntityConnection::SetSourceName(const char *pszName) 
{
	// Save the name of the entity(ies)
	lstrcpyn(m_szSourceEntity, pszName ? pszName : "<<null>>", sizeof(m_szSourceEntity));
	
	// Update the source entity list
	// LinkSourceEntities(); // Changing the entity connection source name shouldnt change the source entity linkage, right?
}

//-----------------------------------------------------------------------------
// Purpose: Sets a new Output Name and sets links to any matching entities
//-----------------------------------------------------------------------------

void CEntityConnection::SetTargetName(const char *pszName) 
{
	// Save the name of the entity(ies)
	lstrcpyn(m_szTargetEntity, pszName ? pszName : "<<null>>", sizeof(m_szTargetEntity));

	// Update the target entity list
	LinkTargetEntities();
}

//-----------------------------------------------------------------------------
// Purpose: Links to any matching Source entities
//-----------------------------------------------------------------------------
void CEntityConnection::LinkSourceEntities()
{
	// Empty out the existing entity list
	m_pSourceEntityList->RemoveAll();

	// Get a list of all the entities in the world
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	if (pDoc)
	{
		CMapWorld *pWorld = pDoc->GetMapWorld();

		if (pWorld)
		{
			CMapEntityList matches;
			pWorld->FindEntitiesByName( matches, m_szSourceEntity, false );
		
			for ( int i = 0; i < matches.Count(); i++ )
			{
				CMapEntity *pEntity = matches.Element( i );

				m_pSourceEntityList->AddToTail( pEntity );
				//pEntity->Connection_Add( this ); // This should already be true on creation, investigate need for this func
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Links to any matching Target entities
//-----------------------------------------------------------------------------
void CEntityConnection::LinkTargetEntities()
{
	// Unlink us from the downstream entities.
	FOR_EACH_OBJ( *m_pTargetEntityList, pos )
	{
		CMapEntity *pEntity = m_pTargetEntityList->Element( pos );
		pEntity->Upstream_Remove( this );
	}

	// Empty out the existing entity list
	m_pTargetEntityList->RemoveAll();

	// Get a list of all the entities in the world
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	if (pDoc)
	{
		CMapWorld *pWorld = pDoc->GetMapWorld();

		if (pWorld)
		{
			CMapEntityList matches;
			pWorld->FindEntitiesByName( matches, m_szTargetEntity, false );
		
			for ( int i = 0; i < matches.Count(); i++ )
			{
				CMapEntity *pEntity = matches.Element( i );
		
				m_pTargetEntityList->AddToTail( pEntity );

				// Special -- Add this connection to the target entity connection list
				pEntity->Upstream_Add( this );
			}
		}
	}
}


//------------------------------------------------------------------------------
// Purpose: Tells if any of the target entities are visible.
//------------------------------------------------------------------------------
bool CEntityConnection::AreAnyTargetEntitiesVisible()
{
	CMapEntityList *pList = GetTargetEntityList();
	for ( int iTarget=0; iTarget < pList->Count(); iTarget++ )
	{
		if ( pList->Element( iTarget )->IsVisible() )
			return true;
	}

	return false;
}


//------------------------------------------------------------------------------
// Purpose: Returns true if output string is valid for all this entity
//------------------------------------------------------------------------------
bool CEntityConnection::ValidateOutput(CMapEntity *pEntity, const char* pszOutput)
{
	if (!pEntity)
	{
		return false;
	}

	GDclass*	pClass	= pEntity->GetClass();
	if (pClass != NULL)
	{
		if (pClass->FindOutput(pszOutput) == NULL)
		{
			return false;
		}
	}
	return true;
}

//------------------------------------------------------------------------------
// Purpose : Returns true if output string is valid for all the entities in 
//			 the entity list
// Input   :
// Output  :
//------------------------------------------------------------------------------
bool CEntityConnection::ValidateOutput(const CMapEntityList *pEntityList, const char* pszOutput)
{
	if (!pEntityList)
	{
		return false;
	}

	FOR_EACH_OBJ( *pEntityList, pos )
	{
		CMapEntity*	pEntity = pEntityList->Element(pos);
		if (!ValidateOutput(pEntity,pszOutput))
		{
			return false;
		}
	}
	return true;
}


//------------------------------------------------------------------------------
// Purpose: Returns true if the given entity list contains an entity of the
//			given target name
//------------------------------------------------------------------------------
bool CEntityConnection::ValidateTarget( const CMapEntityList *pEntityList, bool bVisibilityCheck, const char *pszTarget)
{
	if (!pEntityList || !pszTarget)
		return false;

	// These procedural names are always assumed to exist.
	if (!stricmp(pszTarget, "!activator") || !stricmp(pszTarget, "!caller") || !stricmp(pszTarget, "!player") || !stricmp(pszTarget, "!self"))
		return true;

	FOR_EACH_OBJ( *pEntityList, pos )
	{
		CMapEntity *pEntity = pEntityList->Element(pos);
		if ( bVisibilityCheck && !pEntity->IsVisible() )
			continue;

		if (pEntity->NameMatches(pszTarget))
			return true;
	}

	return false;
}


//------------------------------------------------------------------------------
// Purpose: Returns true if all entities with the given target name
//			have an input of the given input name
//------------------------------------------------------------------------------
bool CEntityConnection::ValidateInput(const char* pszTarget, const char *pszInput, bool bVisiblesOnly)
{
	// Allow any input into !activator and !player.
	// dvs: TODO: pass in the entity to resolve !self and check input list
	if (!stricmp(pszTarget, "!activator") || !stricmp(pszTarget, "!caller") || !stricmp(pszTarget, "!player") || !stricmp(pszTarget, "!self"))
	{
		return true;
	}

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	CMapEntityList EntityList;
	pDoc->FindEntitiesByName(EntityList, pszTarget, bVisiblesOnly);

	if (EntityList.Count() == 0)
	{
		return false;
	}

	if (!MapEntityList_HasInput( &EntityList, pszInput))
	{
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Finds any output connections from this entity that are bad.
//			"Bad" is defined as:
//
//			1) An output that this entity doesn't actually have.
//			2) Connecting to a nonexistent entity.
//			3) Connecting to a nonexistent input in an entity that exists.
//
// Input  : pEntity - The entity to check for bad connections.
//-----------------------------------------------------------------------------
void CEntityConnection::FindBadConnections(CMapEntity *pEntity, bool bVisibilityCheck, CUtlVector<CEntityConnection *> &BadConnectionList, bool bIgnoreHiddenTargets)
{
	BadConnectionList.RemoveAll();

	if ((!pEntity) || (pEntity->Connections_GetCount() == 0))
	{
		return;
	}

	// Get a list of all the entities in the world
	const CMapEntityList *pAllWorldEntities = NULL;
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc)
	{
		CMapWorld *pWorld = pDoc->GetMapWorld();
		if (pWorld)
		{
			pAllWorldEntities = pWorld->EntityList_GetList();
		}
	}

	// For each connection
	int nConnCount = pEntity->Connections_GetCount();
	for (int i = 0; i < nConnCount; i++)
	{
		CEntityConnection *pConnection = pEntity->Connections_Get(i);
		if (pConnection != NULL)
		{
			if ( bIgnoreHiddenTargets )
			{
				if ( pConnection->GetTargetEntityList()->Count() > 0 && !pConnection->AreAnyTargetEntitiesVisible() )
					continue;
			}
			
			// Check validity of output for this entity
			if (!CEntityConnection::ValidateOutput(pEntity, pConnection->GetOutputName()))
			{
				BadConnectionList.AddToTail(pConnection);
			}
			// Check validity of target entity (is it in the map?)
			else if (!CEntityConnection::ValidateTarget(pAllWorldEntities, bVisibilityCheck, pConnection->GetTargetName()))
			{
				BadConnectionList.AddToTail(pConnection);
			}
			// Check validity of input
			else if (!CEntityConnection::ValidateInput(pConnection->GetTargetName(), pConnection->GetInputName(), true))
			{
				BadConnectionList.AddToTail(pConnection);
			}
		}
	}
}


//------------------------------------------------------------------------------
// Purpose: Check if all the output connections in the given entity are valid.
// Output :		OUTPUTS_NONE	if entity has no outputs
//				OUTPUTS_GOOD	if all entity outputs are good
//				OUTPUTS_BAD		if any entity output is bad
//------------------------------------------------------------------------------
int CEntityConnection::ValidateOutputConnections(CMapEntity *pEntity, bool bVisibilityCheck, bool bIgnoreHiddenTargets)
{
	if (!pEntity)
	{
		return CONNECTION_NONE;
	}

	if (pEntity->Connections_GetCount() == 0)
	{
		return CONNECTION_NONE;
	}

	CUtlVector<CEntityConnection *> BadConnectionList;
	FindBadConnections(pEntity, bVisibilityCheck, BadConnectionList, bIgnoreHiddenTargets);

	if (BadConnectionList.Count() > 0)
	{
		return CONNECTION_BAD;
	}

	return CONNECTION_GOOD;	
}


//-----------------------------------------------------------------------------
// Purpose: Fixes any output connections from this entity that are bad.
// Input  : pEntity - The entity to fix.
//-----------------------------------------------------------------------------
void CEntityConnection::FixBadConnections(CMapEntity *pEntity, bool bVisibilityCheck )
{
	CUtlVector<CEntityConnection *> BadConnectionList;
	FindBadConnections(pEntity, bVisibilityCheck, BadConnectionList);

	// Remove the bad connections.
	int nBadConnCount = BadConnectionList.Count();
	for (int i = 0; i < nBadConnCount; i++)
	{
		CEntityConnection *pConnection = BadConnectionList.Element(i);
		pEntity->Connections_Remove( pConnection );
		
		//								
		// Remove the connection from the upstream list of all entities it targets.
		//
		CMapEntityList *pTargetList = pConnection->GetTargetEntityList();
		if ( pTargetList )
		{
			FOR_EACH_OBJ( *pTargetList, pos )
			{
				pEntity = pTargetList->Element( pos );
				pEntity->Upstream_Remove( pConnection );
			}
		}
				
		delete pConnection;
	}
}


//------------------------------------------------------------------------------
// Purpose: Check if all the output connections in the given entity are valid.
// Output :		INPUTS_NONE,	// if entity list has no inputs
//				INPUTS_GOOD,	// if all entity inputs are good
//				INPUTS_BAD,		// if any entity input is bad
//------------------------------------------------------------------------------
int CEntityConnection::ValidateInputConnections(CMapEntity *pEntity, bool bVisibilityCheck)
{
	if (!pEntity)
	{
		return CONNECTION_NONE;
	}

	// No inputs if entity doesn't have a target name
	const char *pszTargetName = pEntity->GetKeyValue("targetname");
	if (!pszTargetName)
	{
		return CONNECTION_NONE;
	}

	GDclass *pClass = pEntity->GetClass();
	if (!pClass)
	{
		return CONNECTION_NONE;
	}

	// Get a list of all the entities in the world
	const CMapEntityList *pAllWorldEntities = NULL;
	CMapDoc	*pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc)
	{
		CMapWorld *pWorld = pDoc->GetMapWorld();
		if (pWorld)
		{
			pAllWorldEntities = pWorld->EntityList_GetList();
		}
	}

	// Look at outputs from each entity in the world
	
	bool bHaveConnection = false;
	FOR_EACH_OBJ( *pAllWorldEntities, pos )
	{
		CMapEntity *pTestEntity = pAllWorldEntities->Element(pos);
		if (pTestEntity == NULL)
			continue;

		if ( bVisibilityCheck && !pTestEntity->IsVisible() )
			continue;

		int nConnCount = pTestEntity->Connections_GetCount();
		for (int i = 0; i < nConnCount; i++)
		{
			// If the connection targets me
			CEntityConnection *pConnection = pTestEntity->Connections_Get(i);
			if ( pConnection && pEntity->NameMatches( pConnection->GetTargetName() ) )
			{
				// Validate output
				if (!ValidateOutput(pTestEntity, pConnection->GetOutputName()))
				{
					return CONNECTION_BAD;
				}

				// Validate input
				if (pClass->FindInput(pConnection->GetInputName()) == NULL)
				{
					return CONNECTION_BAD;
				}

	// FIXME -- Validate the upstream connections the target entities.
				bHaveConnection = true;
			}
		}
	}


	if (bHaveConnection)
	{
		return CONNECTION_GOOD;
	}
	return CONNECTION_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Compares by delays. Used as a secondary sort by all other columns.
//-----------------------------------------------------------------------------
int CALLBACK CEntityConnection::CompareDelaysSecondary(CEntityConnection *pConn1, CEntityConnection *pConn2, SortDirection_t eDirection)
{
	float fDelay1;
	float fDelay2;

	if (eDirection == Sort_Ascending)
	{
		fDelay1 = pConn1->GetDelay();
		fDelay2 = pConn2->GetDelay();
	}
	else
	{
		fDelay1 = pConn2->GetDelay();
		fDelay2 = pConn1->GetDelay();
	}

	if (fDelay1 < fDelay2)
	{
		return(-1);
	}
	else if (fDelay1 > fDelay2)
	{
		return(1);
	}

	return(0);
}

//-----------------------------------------------------------------------------
// Purpose: Compares by delays, does a secondary compare by output name.
//-----------------------------------------------------------------------------
int CALLBACK CEntityConnection::CompareDelays(CEntityConnection *pConn1, CEntityConnection *pConn2, SortDirection_t eDirection)
{
	int nReturn = CompareDelaysSecondary(pConn1, pConn2, eDirection);
	if (nReturn != 0)
	{
		return(nReturn);
	}

	//
	// Always do a secondary sort by output name.
	//
	return(CompareOutputNames(pConn1, pConn2, Sort_Ascending));
}


//-----------------------------------------------------------------------------
// Purpose: Compares by output name, does a secondary compare by delay.
//-----------------------------------------------------------------------------
int CALLBACK CEntityConnection::CompareOutputNames(CEntityConnection *pConn1, CEntityConnection *pConn2, SortDirection_t eDirection)
{
	int nReturn = 0;

	if (eDirection == Sort_Ascending)
	{
		nReturn = stricmp(pConn1->GetOutputName(), pConn2->GetOutputName());
	}
	else
	{
		nReturn = stricmp(pConn2->GetOutputName(), pConn1->GetOutputName());
	}

	//
	// Always do a secondary sort by delay.
	//
	if (nReturn == 0)
	{
		nReturn = CompareDelaysSecondary(pConn1, pConn2, Sort_Ascending);
	}

	return(nReturn);
}


//-----------------------------------------------------------------------------
// Purpose: Compares by input name, does a secondary compare by delay.
//-----------------------------------------------------------------------------
int CALLBACK CEntityConnection::CompareInputNames(CEntityConnection *pConn1, CEntityConnection *pConn2, SortDirection_t eDirection)
{
	int nReturn = 0;

	if (eDirection == Sort_Ascending)
	{
		nReturn = stricmp(pConn1->GetInputName(), pConn2->GetInputName());
	}
	else
	{
		nReturn = stricmp(pConn2->GetInputName(), pConn1->GetInputName());
	}

	//
	// Always do a secondary sort by delay.
	//
	if (nReturn == 0)
	{
		nReturn = CompareDelaysSecondary(pConn1, pConn2, Sort_Ascending);
	}

	return(nReturn);
}

//-----------------------------------------------------------------------------
// Purpose: Compares by source name, does a secondary compare by delay.
//-----------------------------------------------------------------------------
int CALLBACK CEntityConnection::CompareSourceNames(CEntityConnection *pConn1, CEntityConnection *pConn2, SortDirection_t eDirection)
{
	int nReturn = 0;

	if (eDirection == Sort_Ascending)
	{
		nReturn = CompareEntityNames(pConn1->GetSourceName(), pConn2->GetSourceName());
	}
	else
	{
		nReturn = CompareEntityNames(pConn2->GetSourceName(), pConn1->GetSourceName());
	}

	//
	// Always do a secondary sort by delay.
	//
	if (nReturn == 0)
	{
		nReturn = CompareDelaysSecondary(pConn1, pConn2, Sort_Ascending);
	}

	return(nReturn);
}

//-----------------------------------------------------------------------------
// Purpose: Compares by target name, does a secondary compare by delay.
//-----------------------------------------------------------------------------
int CALLBACK CEntityConnection::CompareTargetNames(CEntityConnection *pConn1, CEntityConnection *pConn2, SortDirection_t eDirection)
{
	int nReturn = 0;

	if (eDirection == Sort_Ascending)
	{
		nReturn = CompareEntityNames(pConn1->GetTargetName(), pConn2->GetTargetName());
	}
	else
	{
		nReturn = CompareEntityNames(pConn2->GetTargetName(), pConn1->GetTargetName());
	}

	//
	// Always do a secondary sort by delay.
	//
	if (nReturn == 0)
	{
		nReturn = CompareDelaysSecondary(pConn1, pConn2, Sort_Ascending);
	}

	return(nReturn);
}







