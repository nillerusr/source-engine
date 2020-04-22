//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "tf_xp_source.h"
#include "gcsdk/enumutils.h"
#include "schemainitutils.h"
// memdbgon must be the last include file in a .cpp file!!!
#ifdef GC
IMPLEMENT_CLASS_MEMPOOL( CXPSource, 1000, UTLMEMORYPOOL_GROW_SLOW );
#endif

#include "tier0/memdbgon.h"

using namespace GCSDK;

CXPSource::CXPSource()
{
	Obj().set_account_id( 0 );
	Obj().set_amount( 0 );
	Obj().set_match_id( 0 );
	Obj().set_type( CMsgTFXPSource_XPSourceType_NUM_SOURCE_TYPES ); // Invalid
	Obj().set_match_group( EMatchGroup::k_nMatchGroup_Invalid );
}


#ifdef GC
CXPSource::CXPSource( CMsgTFXPSource msg )
{
	Obj() = msg;
}

bool CXPSource::BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchPlayerXPSources schXPSource;
	WriteToRecord( &schXPSource );
	return CSchemaSharedObjectHelper::BYieldingAddInsertToTransaction( sqlAccess, &schXPSource );
}

bool CXPSource::BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields )
{
	CSchPlayerXPSources schXPSource;
	WriteToRecord( &schXPSource );
	CColumnSet csDatabaseDirty( schXPSource.GetPSchema()->GetRecordInfo() );
	csDatabaseDirty.MakeEmpty();
	FOR_EACH_VEC( fields, nField )
	{
		switch ( fields[nField] )
		{
			case CMsgTFXPSource::kAccountIdFieldNumber		: csDatabaseDirty.BAddColumn( CSchPlayerXPSources::k_iField_unAccountID );	break;
			case CMsgTFXPSource::kAmountFieldNumber			: csDatabaseDirty.BAddColumn( CSchPlayerXPSources::k_iField_unAmount );		break;
			case CMsgTFXPSource::kTypeFieldNumber			: csDatabaseDirty.BAddColumn( CSchPlayerXPSources::k_iField_eSource );		break;
			case CMsgTFXPSource::kMatchGroupFieldNumber		: csDatabaseDirty.BAddColumn( CSchPlayerXPSources::k_iField_eMatchGroup );	break;
			case CMsgTFXPSource::kMatchIdFieldNumber		: csDatabaseDirty.BAddColumn( CSchPlayerXPSources::k_iField_unMatchID );	break;
			default:
				Assert( false );
		}
	}
	return CSchemaSharedObjectHelper::BYieldingAddWriteToTransaction( sqlAccess, &schXPSource, csDatabaseDirty );
}

bool CXPSource::BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchPlayerXPSources schXPSource;
	WriteToRecord( &schXPSource );
	return CSchemaSharedObjectHelper::BYieldingAddRemoveToTransaction( sqlAccess, &schXPSource );
}

void CXPSource::WriteToRecord( CSchPlayerXPSources *pXPSource ) const
{
	pXPSource->m_unAccountID	= Obj().account_id();
	pXPSource->m_eSource		= Obj().type();
	pXPSource->m_eMatchGroup	= Obj().match_group();
	pXPSource->m_unAmount		= Obj().amount();
	pXPSource->m_unMatchID		= Obj().match_id();
}

void CXPSource::ReadFromRecord( const CSchPlayerXPSources & xpSource )
{
	Obj().set_account_id( xpSource.m_unAccountID );
	Obj().set_type( ( CMsgTFXPSource::XPSourceType )xpSource.m_eSource );
	Obj().set_match_group( xpSource.m_eMatchGroup );
	Obj().set_amount( xpSource.m_unAmount );
	Obj().set_match_id( xpSource.m_unMatchID );
}

#endif