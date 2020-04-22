//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a body group
//
//===========================================================================//

#include "mdlobjects/dmebodygroup.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmelodlist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBodyGroup, CDmeBodyGroup );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeBodyGroup::OnConstruction()
{
	m_BodyParts.Init( this, "bodyParts" );
}

void CDmeBodyGroup::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Finds a body part by name 
//-----------------------------------------------------------------------------
CDmeLODList *CDmeBodyGroup::FindBodyPart( const char *pName )
{
	int nCount = m_BodyParts.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeLODList *pLODList = CastElement< CDmeLODList >( m_BodyParts[ i ] );
		if ( !pLODList )
			continue;

		if ( !Q_stricmp( pName, pLODList->GetName() )	)
			return pLODList;
	}

	return NULL;
}
