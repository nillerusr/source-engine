//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Functions related to dynamic recipes
//
//=============================================================================


#include "cbase.h"
#include "econ_quests.h"
#ifndef GC_DLL
#include "quest_objective_manager.h"
#endif

bool IsQuestItemUnidentified( const CEconItem* pQuestItem )
{
	return pQuestItem && IsUnacknowledged( pQuestItem->GetInventoryToken() );
}

bool IsQuestItemReadyToTurnIn( const IEconItemInterface* pQuestItem )
{
	uint32 nRequiredPoints = pQuestItem->GetItemDefinition()->GetQuestDef()->GetMaxStandardPoints();
	uint32 nEarnedStandardPoints = GetEarnedStandardPoints( pQuestItem );
	uint32 nEarnedBonusPoints = GetEarnedBonusPoints( pQuestItem );

	return ( nEarnedStandardPoints + nEarnedBonusPoints ) >= nRequiredPoints;
}

bool IsQuestItemFullyCompleted( const IEconItemInterface* pQuestItem )
{
	uint32 nRequiredStandardPoints = pQuestItem->GetItemDefinition()->GetQuestDef()->GetMaxStandardPoints();
	uint32 nRequiredBonusPoints = pQuestItem->GetItemDefinition()->GetQuestDef()->GetMaxBonusPoints();
	uint32 nEarnedStandardPoints = GetEarnedStandardPoints( pQuestItem );
	uint32 nEarnedBonusPoints = GetEarnedBonusPoints( pQuestItem );

	return ( nEarnedStandardPoints + nEarnedBonusPoints ) == ( nRequiredStandardPoints + nRequiredBonusPoints );
}

uint32 GetEarnedStandardPoints( const IEconItemInterface* pQuestItem )
{
#ifndef GC_DLL
	const CQuestItemTracker* pItemTracker = assert_cast< const CQuestItemTracker* >( QuestObjectiveManager()->GetTypedTracker< CQuestItemTracker* >( pQuestItem->GetID() ) );
	if ( pItemTracker )
	{
		return pItemTracker->GetEarnedStandardPoints();
	}
#endif

	uint32 nEarnedStandardPoints = 0;
	static CSchemaAttributeDefHandle pAttribDef_EarnedStandardPoints( "quest earned standard points" );
	pQuestItem->FindAttribute( pAttribDef_EarnedStandardPoints, &nEarnedStandardPoints );


	return nEarnedStandardPoints;
}

uint32 GetEarnedBonusPoints( const IEconItemInterface* pQuestItem )
{
	
#ifndef GC_DLL
	const CQuestItemTracker* pItemTracker = assert_cast< const CQuestItemTracker* >( QuestObjectiveManager()->GetTypedTracker< CQuestItemTracker* >( pQuestItem->GetID() ) );
	if ( pItemTracker )
	{
		return pItemTracker->GetEarnedBonusPoints();
	}
#endif
	uint32 nEarnedBonusPoints = 0;
	static CSchemaAttributeDefHandle pAttribDef_EarnedBonusPoints( "quest earned bonus points" );
	pQuestItem->FindAttribute( pAttribDef_EarnedBonusPoints, &nEarnedBonusPoints );

	return nEarnedBonusPoints;
}