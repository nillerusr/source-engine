//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Code for the CTFMapContribution object
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "econ_contribution.h"

using namespace GCSDK;

#ifdef GC
IMPLEMENT_CLASS_MEMPOOL( CTFMapContribution, 10 * 1000, UTLMEMORYPOOL_GROW_SLOW );

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool CTFMapContribution::BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchMapContribution schMapContribution;
	WriteToRecord( &schMapContribution );
	return CSchemaSharedObjectHelper::BYieldingAddInsertToTransaction( sqlAccess, &schMapContribution );
}

bool CTFMapContribution::BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields )
{
	CSchMapContribution schMapContribution;
	WriteToRecord( &schMapContribution );
	CColumnSet csDatabaseDirty( schMapContribution.GetPSchema()->GetRecordInfo() );
	csDatabaseDirty.MakeEmpty();
	if ( fields.HasElement( CSOTFMapContribution::kContributionLevelFieldNumber ) )
	{
		csDatabaseDirty.BAddColumn( CSchMapContribution::k_iField_unContributionLevel );
	}
	return CSchemaSharedObjectHelper::BYieldingAddWriteToTransaction( sqlAccess, &schMapContribution, csDatabaseDirty );
}

bool CTFMapContribution::BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchMapContribution schMapContribution;
	WriteToRecord( &schMapContribution );
	return CSchemaSharedObjectHelper::BYieldingAddRemoveToTransaction( sqlAccess, &schMapContribution );
}

void CTFMapContribution::WriteToRecord( CSchMapContribution *pMapContribution ) const
{
	pMapContribution->m_unAccountID = Obj().account_id();
	pMapContribution->m_unDefIndex = Obj().def_index();
	pMapContribution->m_unContributionLevel = Obj().contribution_level();
}


void CTFMapContribution::ReadFromRecord( const CSchMapContribution & mapContribution )
{
	Obj().set_account_id( mapContribution.m_unAccountID );
	Obj().set_def_index( mapContribution.m_unDefIndex );
	Obj().set_contribution_level( mapContribution.m_unContributionLevel );
}



#endif
