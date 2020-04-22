//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a body group list
//
//===========================================================================//

#include "mdlobjects/dmebodygrouplist.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmebodygroup.h"
#include "mdlobjects/dmelodlist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBodyGroupList, CDmeBodyGroupList );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeBodyGroupList::OnConstruction()
{
	m_BodyGroups.Init( this, "bodyGroups" );
}

void CDmeBodyGroupList::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Finds a body group by name 
//-----------------------------------------------------------------------------
CDmeBodyGroup *CDmeBodyGroupList::FindBodyGroup( const char *pName )
{
	int nCount = m_BodyGroups.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pName, m_BodyGroups[i]->GetName() ) )
			return m_BodyGroups[i];
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Gets the 'main' body part (used for compilation)
//-----------------------------------------------------------------------------
CDmeLODList *CDmeBodyGroupList::GetMainBodyPart()
{
	if ( m_BodyGroups.Count() == 0 )
		return NULL;

	CDmeBodyGroup *pMainBodyGroup = FindBodyGroup( "default" ); 
	if ( !pMainBodyGroup )
	{
		pMainBodyGroup = m_BodyGroups[0];
	}

	CDmeLODList *pLODList = CastElement< CDmeLODList >( pMainBodyGroup->FindBodyPart( "default" ) );
	if ( pLODList )
		return pLODList;

	const int nBodypartCount = pMainBodyGroup->m_BodyParts.Count();
	for ( int i = 0; i < nBodypartCount; ++i )
	{
		pLODList = CastElement< CDmeLODList >( pMainBodyGroup->m_BodyParts[ i ] );
		if ( pLODList && pLODList->m_LODs.Count() > 0 )
			return pLODList;
	}

	return NULL;
}
