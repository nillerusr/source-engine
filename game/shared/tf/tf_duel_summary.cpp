//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "tf_duel_summary.h"
#include "gcsdk/enumutils.h"

using namespace GCSDK;

#ifdef GC
IMPLEMENT_CLASS_MEMPOOL( CTFDuelSummary, 1000, UTLMEMORYPOOL_GROW_SLOW );

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


bool CTFDuelSummary::BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchDuelSummary schDuelSummary;
	WriteToRecord( &schDuelSummary );
	return CSchemaSharedObjectHelper::BYieldingAddInsertToTransaction( sqlAccess, &schDuelSummary );
}

bool CTFDuelSummary::BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields )
{
	CSchDuelSummary schDuelSummary;
	WriteToRecord( &schDuelSummary );
	CColumnSet csDatabaseDirty( schDuelSummary.GetPSchema()->GetRecordInfo() );
	csDatabaseDirty.MakeEmpty();
	FOR_EACH_VEC( fields, nField )
	{
		switch ( fields[nField] )
		{
			case CSOTFDuelSummary::kLastDuelAccountIdFieldNumber:	csDatabaseDirty.BAddColumn( CSchDuelSummary::k_iField_unLastDuelAccountID ); break;
			case CSOTFDuelSummary::kLastDuelTimestampFieldNumber:	csDatabaseDirty.BAddColumn( CSchDuelSummary::k_iField_rtLastDuelTimestamp ); break;
			case CSOTFDuelSummary::kLastDuelStatusFieldNumber:		csDatabaseDirty.BAddColumn( CSchDuelSummary::k_iField_eLastDuelStatus ); break;
			case CSOTFDuelSummary::kDuelLossesFieldNumber:			csDatabaseDirty.BAddColumn( CSchDuelSummary::k_iField_unDuelLosses ); break;
			case CSOTFDuelSummary::kDuelWinsFieldNumber:			csDatabaseDirty.BAddColumn( CSchDuelSummary::k_iField_unDuelWins ); break;
		}
	}
	return CSchemaSharedObjectHelper::BYieldingAddWriteToTransaction( sqlAccess, &schDuelSummary, csDatabaseDirty );
}

bool CTFDuelSummary::BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchDuelSummary schDuelSummary;
	WriteToRecord( &schDuelSummary );
	return CSchemaSharedObjectHelper::BYieldingAddRemoveToTransaction( sqlAccess, &schDuelSummary );
}

void CTFDuelSummary::WriteToRecord( CSchDuelSummary *pDuelSummary ) const
{
	pDuelSummary->m_unAccountID = Obj().account_id();
	pDuelSummary->m_unDuelWins = Obj().duel_wins();
	pDuelSummary->m_unDuelLosses = Obj().duel_losses();
	pDuelSummary->m_unLastDuelAccountID = Obj().last_duel_account_id();
	pDuelSummary->m_rtLastDuelTimestamp = Obj().last_duel_timestamp();
	pDuelSummary->m_eLastDuelStatus = Obj().last_duel_status();
}


void CTFDuelSummary::ReadFromRecord( const CSchDuelSummary & duelSummary )
{
	Obj().set_account_id( duelSummary.m_unAccountID );
	Obj().set_duel_wins( duelSummary.m_unDuelWins );
	Obj().set_duel_losses( duelSummary.m_unDuelLosses );
	Obj().set_last_duel_account_id( duelSummary.m_unLastDuelAccountID );
	Obj().set_last_duel_timestamp( duelSummary.m_rtLastDuelTimestamp );
	Obj().set_last_duel_status( duelSummary.m_eLastDuelStatus );

}

ENUMSTRINGS_START( eDuelEndReason )
{ 	kDuelEndReason_DuelOver, "Complete" },
{ 	kDuelEndReason_PlayerDisconnected, "Player Disconnected" },
{ 	kDuelEndReason_PlayerSwappedTeams, "Player Swapped Teams" },
{ 	kDuelEndReason_LevelShutdown, "Level Shutdown" },
{ 	kDuelEndReason_ScoreTiedAtZero, "Tied" },
{ 	kDuelEndReason_PlayerKicked, "Player Kicked" },
{ 	kDuelEndReason_PlayerForceSwappedTeams, "Forced to Swap Teams" },
{	kDuelEndReason_ScoreTied, "Tied" },
ENUMSTRINGS_END( eDuelEndReason )


#endif
