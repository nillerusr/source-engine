//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "quest_objective_manager.h"
#include "gcsdk/gcclient.h"
#include "gc_clientsystem.h"
#include "econ_quests.h"
#include "tf_gamerules.h"
#include "schemainitutils.h"
#include "econ_item_system.h"
#ifdef CLIENT_DLL
	#include "quest_log_panel.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( DEBUG ) || defined( STAGING_ONLY )
ConVar tf_quests_commit_every_point( "tf_quests_commit_every_point", "0", FCVAR_REPLICATED );
ConVar tf_quests_progress_enabled( "tf_quests_progress_enabled", "1", FCVAR_REPLICATED );
#endif


CQuestObjectiveManager *QuestObjectiveManager( void )
{
	static CQuestObjectiveManager g_QuestObjectiveManager;
	return &g_QuestObjectiveManager;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseQuestObjectiveTracker::CBaseQuestObjectiveTracker( const CTFQuestObjectiveDefinition* pObjective, CQuestItemTracker* pParent )
	: m_nObjectiveDefIndex( pObjective->GetDefinitionIndex() )
	, m_pParent( pParent )
	, m_pEvaluator( NULL )
{
	KeyValues *pKVConditions = pObjective->GetConditionsKeyValues();
	
	AssertMsg( !m_pEvaluator, "%s", CFmtStr( "Too many input for operator '%s'.", GetConditionName() ).Get() );

	const char *pszType = pKVConditions->GetString( "type" );
	m_pEvaluator = CreateEvaluatorByName( pszType, this );
	AssertMsg( m_pEvaluator != NULL, "%s", CFmtStr( "Failed to create quest condition name '%s' for '%s'", pszType, GetConditionName() ).Get() );

	SO_TRACKER_SPEW( CFmtStr( "Creating objective tracker def %d for quest def %d on item %llu for user %s\n",
							  pObjective->GetDefinitionIndex(),
							  pParent->GetItem()->GetItemDefinition()->GetDefinitionIndex(),
							  pParent->GetItem()->GetID(),
							  pParent->GetOwnerSteamID().Render() ),
					 SO_TRACKER_SPEW_OBJECTIVE_TRACKER_MANAGEMENT );

	if ( !m_pEvaluator->BInitFromKV( pKVConditions, NULL ) )
	{
		AssertMsg( false, "Failed to init from KeyValues" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseQuestObjectiveTracker::~CBaseQuestObjectiveTracker()
{
	if ( m_pEvaluator )
	{
		delete m_pEvaluator;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseQuestObjectiveTracker::IsValidForPlayer( const CTFPlayer *pOwner, InvalidReasonsContainer_t& invalidReasons ) const
{
	return m_pEvaluator->IsValidForPlayer( pOwner, invalidReasons );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const CTFPlayer *CBaseQuestObjectiveTracker::GetQuestOwner() const
{
	return GetTrackedPlayer();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseQuestObjectiveTracker::EvaluateCondition( CTFQuestEvaluator *pSender, int nScore )
{
#ifdef GAME_DLL
	// tracker should be the root
	Assert( !GetParent() );
	IncrementCount( nScore );
	ResetCondition();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseQuestObjectiveTracker::ResetCondition()
{
	m_pEvaluator->ResetCondition();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseQuestObjectiveTracker::UpdateConditions()
{
	const CTFQuestObjectiveDefinition *pObjective = (CTFQuestObjectiveDefinition*)ItemSystem()->GetItemSchema()->GetQuestObjectiveByDefIndex( m_nObjectiveDefIndex );
	if ( !pObjective )
		return false;

	// clean up previous evaluator
	if ( m_pEvaluator )
	{
		delete m_pEvaluator;
		m_pEvaluator = NULL;
	}

	CUtlVector< CUtlString > vecErrors;
	return BInitFromKV( pObjective->GetConditionsKeyValues(), &vecErrors );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const CTFPlayer* CBaseQuestObjectiveTracker::GetTrackedPlayer() const
{
#ifdef CLIENT_DLL
	return ToTFPlayer( C_BasePlayer::GetLocalPlayer() );
#else
	return m_pParent->GetTrackedPlayer();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseQuestObjectiveTracker::IncrementCount( int nIncrementValue )
{
	const CTFQuestObjectiveDefinition *pObjective = (CTFQuestObjectiveDefinition*)ItemSystem()->GetItemSchema()->GetQuestObjectiveByDefIndex( m_nObjectiveDefIndex );
	Assert( pObjective );
	if ( !pObjective )
		return;

	uint32 nPointsToAdd = nIncrementValue * pObjective->GetPoints();
	m_pParent->IncrementCount( nPointsToAdd, pObjective );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CQuestItemTracker::CQuestItemTracker( const CSharedObject* pItem, CSteamID SteamIDOwner, CSOTrackerManager* pManager )
	: CBaseSOTracker( pItem, SteamIDOwner, pManager )
	, m_pItem( NULL )
	, m_nStandardPoints( 0 )
	, m_nBonusPoints( 0 )
#ifdef GAME_DLL
	, m_nStartingStandardPoints( 0 )
	, m_nStartingBonusPoints( 0 )
#endif
{
	m_pItem = assert_cast< const CEconItem* >( pItem );
	// Retrieve starting numbers
	UpdatePointsFromSOItem();

	SO_TRACKER_SPEW( CFmtStr( "Creating tracker for quest %d on item %llu for user %s with %dsp and %dbp\n",
							  GetItem()->GetItemDefinition()->GetDefinitionIndex(),
							  GetItem()->GetID(),
							  GetOwnerSteamID().Render(),
							  GetEarnedStandardPoints(),
							  GetEarnedBonusPoints() ),
					 SO_TRACKER_SPEW_ITEM_TRACKER_MANAGEMENT );

	// Create trackers for each objective
	QuestObjectiveDefVec_t vecChosenObjectives;
	m_pItem->GetItemDefinition()->GetQuestDef()->GetRolledObjectivesForItem( vecChosenObjectives, m_pItem );
	FOR_EACH_VEC( vecChosenObjectives, i )
	{
		if ( !DoesObjectiveNeedToBeTracked( vecChosenObjectives[i] ) )
			continue;

		CBaseQuestObjectiveTracker* pNewTracker = new CBaseQuestObjectiveTracker( vecChosenObjectives[i], this );
		m_vecObjectiveTrackers.AddToTail( pNewTracker );
	}

	if ( m_vecObjectiveTrackers.IsEmpty() )
	{
		SO_TRACKER_SPEW( CFmtStr( "Did not create any objective trackers for quest %d on item %llu for user %s with %dsp and %dbp\n",
								  GetItem()->GetItemDefinition()->GetDefinitionIndex(),
								  GetItem()->GetID(),
								  GetOwnerSteamID().Render(),
								  GetEarnedStandardPoints(),
								  GetEarnedBonusPoints() ),
						 SO_TRACKER_SPEW_OBJECTIVE_TRACKER_MANAGEMENT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CQuestItemTracker::~CQuestItemTracker()
{
#ifdef CLIENT_DLL
	SO_TRACKER_SPEW( CFmtStr( "Deleting tracker for quest %u on item %llu with %usp and %ubp\n",
							   m_pItem->GetItemDefinition()->GetDefinitionIndex(),
							   m_pItem->GetItemID(),
							   m_nStandardPoints,
							   m_nBonusPoints ),
					 SO_TRACKER_SPEW_ITEM_TRACKER_MANAGEMENT );
#else
	SO_TRACKER_SPEW( CFmtStr( "Deleting tracker for quest %u on item %llu with %usp %ussp %ubp %usbp\n",
							   m_pItem->GetItemDefinition()->GetDefinitionIndex(),
							   m_pItem->GetItemID(),
							   m_nStandardPoints,
							   m_nStartingStandardPoints,
							   m_nBonusPoints,
							   m_nStartingBonusPoints ),
					 SO_TRACKER_SPEW_ITEM_TRACKER_MANAGEMENT );
#endif
	m_vecObjectiveTrackers.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: Take a look at our item and update what we think our points are
//			based on the attributes on the item IF they are greater.  We NEVER
//			want to lose progress for any reason.
//-----------------------------------------------------------------------------
void CQuestItemTracker::UpdatePointsFromSOItem()
{
	uint32 nNewPoints = 0;
	static CSchemaAttributeDefHandle pAttribDef_EarnedStandardPoints( "quest earned standard points" );
	m_pItem->FindAttribute( pAttribDef_EarnedStandardPoints, &nNewPoints );
#ifdef GAME_DLL
	m_nStartingStandardPoints = Max( nNewPoints, m_nStartingStandardPoints );
#else
	m_nStandardPoints = Max( nNewPoints, m_nStandardPoints );
#endif

	nNewPoints = 0;
	static CSchemaAttributeDefHandle pAttribDef_EarnedBonusPoints( "quest earned bonus points" );
	m_pItem->FindAttribute( pAttribDef_EarnedBonusPoints, &nNewPoints );
#ifdef GAME_DLL
	m_nStartingBonusPoints = Max( nNewPoints, m_nStartingBonusPoints );
#else
	m_nBonusPoints = Max( nNewPoints, m_nBonusPoints );
#endif

#ifdef GAME_DLL
	SendUpdateToClient( NULL );

	SO_TRACKER_SPEW( CFmtStr( "Updated points from item.  CS:%d S:%d  CB:%d B:%d\n", m_nStandardPoints, m_nStartingStandardPoints, m_nBonusPoints, m_nStartingBonusPoints ), SO_TRACKER_SPEW_OBJECTIVES );
#else
	SO_TRACKER_SPEW( CFmtStr( "Updated points from item.  S:%d B:%d\n", m_nStandardPoints, m_nBonusPoints ), SO_TRACKER_SPEW_OBJECTIVES );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const CBaseQuestObjectiveTracker* CQuestItemTracker::FindTrackerForDefIndex( uint32 nDefIndex ) const
{
	FOR_EACH_VEC( m_vecObjectiveTrackers, i )
	{
		if ( m_vecObjectiveTrackers[ i ]->GetObjectiveDefIndex() == nDefIndex )
		{
			return m_vecObjectiveTrackers[ i ];
		}
	}

	return NULL;
}

uint32 CQuestItemTracker::GetEarnedStandardPoints() const
{
#ifdef GAME_DLL
	return m_nStartingStandardPoints + m_nStandardPoints;
#else
	return m_nStandardPoints;
#endif
}
uint32 CQuestItemTracker::GetEarnedBonusPoints() const 
{ 
#ifdef GAME_DLL
	return m_nStartingBonusPoints + m_nBonusPoints;
#else
	return m_nBonusPoints;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestItemTracker::IncrementCount( uint32 nIncrementValue, const CQuestObjectiveDefinition* pObjective )
{
#if defined( DEBUG ) || defined( STAGING_ONLY )
	if ( !tf_quests_progress_enabled.GetBool() )
		return;
#endif

#ifdef GAME_DLL
	Assert( pObjective );
	Assert( m_pItem );
	if ( !pObjective || !m_pItem )
		return;

	auto pQuestDef = m_pItem->GetItemDefinition()->GetQuestDef();
	Assert( pQuestDef );
	if ( !pQuestDef )
		return;

	if ( g_pVGuiLocalize && ( g_nQuestSpewFlags & SO_TRACKER_SPEW_OBJECTIVES ) )
	{
		locchar_t loc_IntermediateName[ MAX_ITEM_NAME_LENGTH ];
		locchar_t locValue[ MAX_ITEM_NAME_LENGTH ];
		loc_sprintf_safe( locValue, LOCCHAR( "%d" ), pObjective->GetPoints() );
		loc_scpy_safe( loc_IntermediateName, CConstructLocalizedString( g_pVGuiLocalize->Find( pObjective->GetDescriptionToken() ), locValue ) );
		char szTempObjectiveName[256];
		::ILocalize::ConvertUnicodeToANSI( loc_IntermediateName, szTempObjectiveName, sizeof( szTempObjectiveName ));

		SO_TRACKER_SPEW( CFmtStr( "Increment for quest: %llu Objective: \"%s\" %d->%d (+%d)\n"
			, m_pItem->GetItemID()
			, szTempObjectiveName
			, m_nStandardPoints + m_nBonusPoints
			, m_nStandardPoints + m_nBonusPoints + nIncrementValue
			, nIncrementValue ), SO_TRACKER_SPEW_OBJECTIVES );
	}

	// Regardless of standard or bonus, we fill the standard gauge first
	uint32 nMaxStandardPoints = pQuestDef->GetMaxStandardPoints() - GetEarnedStandardPoints();
	int nAmountToAdd = Min( nMaxStandardPoints, nIncrementValue );
	m_nStandardPoints += nAmountToAdd;
	nIncrementValue -= nAmountToAdd;

	// If any bonus points left, fill in bonus points
	if ( pObjective->IsAdvanced() && nIncrementValue > 0 )
	{
		uint32 nMaxBonusPoints = pQuestDef->GetMaxBonusPoints() + pQuestDef->GetMaxStandardPoints() - GetEarnedStandardPoints() - GetEarnedBonusPoints();
		m_nBonusPoints += Min( nMaxBonusPoints, nIncrementValue );
	}

	bool bShouldCommit = IsQuestItemReadyToTurnIn( m_pItem );
#if defined( DEBUG ) || defined( STAGING_ONLY )
	bShouldCommit |= tf_quests_commit_every_point.GetBool();
#endif

	// Once we're over the turn-in threshhold, we need to record every point made.
	if ( bShouldCommit )
	{
		CommitChangesToDB();
	}

	SendUpdateToClient( pObjective );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Remove and delete any objective trackers that are no longer needed.
//			One is considered not needed if it's a tracker for a "standard"
//			objective and we're done getting standard points, or if we're at
//			full bonus points, then there's no way for us to get points anymore
//-----------------------------------------------------------------------------
void CQuestItemTracker::OnUpdate()
{
	FOR_EACH_VEC_BACK( m_vecObjectiveTrackers, i )
	{
		const CQuestObjectiveDefinition *pObjective = GEconItemSchema().GetQuestObjectiveByDefIndex( m_vecObjectiveTrackers[ i ]->GetObjectiveDefIndex() );
		Assert( pObjective );
		if ( !pObjective || !DoesObjectiveNeedToBeTracked( pObjective ) )
		{
			delete m_vecObjectiveTrackers[ i ];
			m_vecObjectiveTrackers.Remove( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestItemTracker::OnRemove()
{
#ifdef GAME_DLL
	CommitRecord_t* pRecord = m_pManager->GetCommitRecord( m_pItem->GetItemID() );
	if ( pRecord )
	{
		CMsgGCQuestObjective_PointsChange* pProto = assert_cast< CMsgGCQuestObjective_PointsChange* >( pRecord->m_pProtoMsg );
		pProto->set_update_base_points( true );
	}
#endif
}

void CQuestItemTracker::Spew() const 
{
	CBaseSOTracker::Spew();

	FOR_EACH_VEC( m_vecObjectiveTrackers, i )
	{
		DevMsg( "Tracking objective: %d\n", m_vecObjectiveTrackers[ i ]->GetObjectiveDefIndex() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CQuestItemTracker::DoesObjectiveNeedToBeTracked( const CQuestObjectiveDefinition* pObjective ) const
{
	auto pQuestDef = m_pItem->GetItemDefinition()->GetQuestDef();

	Assert( pObjective );
	if ( pObjective && pQuestDef )
	{
		// If there's standard points to be earned, all objectives need to be tracked
		if ( pQuestDef->GetMaxStandardPoints() > 0 && GetEarnedStandardPoints() < pQuestDef->GetMaxStandardPoints() )
		{
			return true;
		}

		// If this objective is advanced, only track it if there's bonus points to be earned
		if ( pObjective->IsAdvanced() )
		{
			return pQuestDef->GetMaxBonusPoints() > 0 && GetEarnedBonusPoints() < pQuestDef->GetMaxBonusPoints();
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestItemTracker::CommitChangesToDB()
{
#ifdef CLIENT_DLL
	if ( GetQuestLog() && GetTrackedPlayer() == C_TFPlayer::GetLocalTFPlayer() )
	{
		GetQuestLog()->MarkQuestsDirty();
	}
#else // GAME_DLL

	// Nothing to commit?  Bail
	if ( m_nStandardPoints == 0 && m_nBonusPoints == 0 )
		return;

	SO_TRACKER_SPEW( CFmtStr( "CommitChangesToDB: %llu S:%d B:%d\n"
			, m_pItem->GetItemID()
			, GetEarnedStandardPoints()
			, GetEarnedBonusPoints() ), 0 );
	
	CSteamID ownerSteamID( m_pItem->GetAccountID(), GetUniverse(), k_EAccountTypeIndividual );

	CMsgGCQuestObjective_PointsChange record;

	// Cook up our message
	record.set_owner_steamid( ownerSteamID.ConvertToUint64() );
	record.set_quest_item_id( m_pItem->GetItemID() );
	record.set_standard_points( GetEarnedStandardPoints() );
	record.set_bonus_points( GetEarnedBonusPoints() ); // Here's the meat

	m_pManager->AddCommitRecord( &record, record.quest_item_id(), true );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CQuestItemTracker::IsValidForPlayer( const CTFPlayer *pOwner, InvalidReasonsContainer_t& invalidReasons ) const
{
	int nNumInvalid = 0;
	FOR_EACH_VEC( m_vecObjectiveTrackers, i )
	{
		m_vecObjectiveTrackers[ i ]->IsValidForPlayer( pOwner, invalidReasons );

		if ( !invalidReasons.IsValid() )
		{
			++nNumInvalid;
		}
	}

	return nNumInvalid;
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: The server has changed scores.  Apply those changes here
//-----------------------------------------------------------------------------
void CQuestItemTracker::UpdateFromServer( uint32 nStandardPoints, uint32 nBonusPoints )
{
	SO_TRACKER_SPEW( CFmtStr( "Updating \"%s's\" standard points: %d->%d bonus points: %d->%d\n"
					  , m_pItem->GetItemDefinition()->GetQuestDef()->GetRolledNameForItem( m_pItem )
					  , m_nStandardPoints
					  , nStandardPoints
					  , m_nBonusPoints
					  , nBonusPoints )
					  , SO_TRACKER_SPEW_OBJECTIVES );

	m_nStandardPoints = nStandardPoints;
	m_nBonusPoints = nBonusPoints;
}
#else
void CQuestItemTracker::SendUpdateToClient( const CQuestObjectiveDefinition* pObjective )
{
	const CTFPlayer* pPlayer = GetTrackedPlayer();

	// They might've disconnected, so let's check if they're still around
	if ( pPlayer )
	{
		// Update the user on their progress
		CSingleUserRecipientFilter filter( GetTrackedPlayer() );
		filter.MakeReliable();
		UserMessageBegin( filter, "QuestObjectiveCompleted" );
		itemid_t nID = m_pItem->GetItemID();
		WRITE_BITS( &nID, 64 );
		WRITE_WORD( GetEarnedStandardPoints() );
		WRITE_WORD( GetEarnedBonusPoints() );
		WRITE_WORD( pObjective ? pObjective->GetDefinitionIndex() : (uint32)-1 );
		MessageEnd();
	}
}
#endif

#if defined( DEBUG ) || defined( STAGING_ONLY )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestItemTracker::DBG_CompleteQuest()
{
#ifdef GAME_DLL

	auto pQuestDef = m_pItem->GetItemDefinition()->GetQuestDef();
	uint32 nStandardPointsDelta = pQuestDef->GetMaxStandardPoints() - GetEarnedStandardPoints();

	// Cheat!
	if ( m_vecObjectiveTrackers.Count() )
	{
		const_cast< CBaseQuestObjectiveTracker* >( m_vecObjectiveTrackers[0] )->EvaluateCondition( NULL, nStandardPointsDelta );
	}

	CommitChangesToDB();
#endif
}
#endif
