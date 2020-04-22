//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a hitbox
//
//===========================================================================//

#include "mdlobjects/dmelodlist.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmelod.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeLODList, CDmeLODList );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeLODList::OnConstruction()
{
	m_LODs.Init( this, "lods" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeLODList::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Returns the number of LODs in this body part, can be 0
//-----------------------------------------------------------------------------
int CDmeLODList::LODCount() const
{
	return m_LODs.Count();
}


//-----------------------------------------------------------------------------
// Returns the root LOD. This is the one with the switch metric 0
//-----------------------------------------------------------------------------
CDmeLOD* CDmeLODList::GetRootLOD()
{
	int nCount = m_LODs.Count();
	int nMinIndex = -1;
	float flMinMetric = FLT_MAX;
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_LODs[i]->m_flSwitchMetric < flMinMetric )
		{
			nMinIndex = i;
			flMinMetric = m_LODs[i]->m_flSwitchMetric;
			if ( flMinMetric == 0.0f )
				break;
		}
	}
	return ( nMinIndex >= 0 ) ? m_LODs[nMinIndex] : NULL;
}


//-----------------------------------------------------------------------------
// Returns the shadow LOD
//-----------------------------------------------------------------------------
CDmeLOD* CDmeLODList::GetShadowLOD()
{
	int nCount = m_LODs.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_LODs[i]->m_bIsShadowLOD )
			return m_LODs[i];
	}
	return NULL;
}
