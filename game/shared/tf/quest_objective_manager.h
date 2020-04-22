//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef QUEST_OBJECTIVE_MANAGER_H
#define QUEST_OBJECTIVE_MANAGER_H

#include "GameEventListener.h"
#include "econ_item_constants.h"
#include "econ_item_inventory.h"
#include "tf_quest_restriction.h"
#include "econ_dynamic_recipe.h"
#include "shared_object_tracker.h"

#ifdef GAME_DLL
	#include "tf_player.h"
#else
	#include "c_tf_player.h"
#endif


#if defined( _WIN32 )
#pragma once
#endif

using namespace GCSDK;


class CQuestItemTracker;

class CBaseQuestObjectiveTracker : public CTFQuestEvaluator
{
public:
	DECLARE_CLASS( CBaseQuestObjectiveTracker, CBaseQuestObjectiveTracker )

	CBaseQuestObjectiveTracker( const CTFQuestObjectiveDefinition* pObjective, CQuestItemTracker* pParent );
	virtual ~CBaseQuestObjectiveTracker();

	uint32 GetObjectiveDefIndex() const { return m_nObjectiveDefIndex; }

	// CTFQuestConditionEvaluator specific
	virtual const char *GetConditionName() const OVERRIDE { return "tracker"; }
	virtual bool IsValidForPlayer( const CTFPlayer *pOwner, InvalidReasonsContainer_t& invalidReasons ) const;
	virtual const CTFPlayer *GetQuestOwner() const OVERRIDE;
	virtual void EvaluateCondition( CTFQuestEvaluator *pSender, int nScore ) OVERRIDE;
	virtual void ResetCondition() OVERRIDE;

	bool UpdateConditions();

protected:
	const CTFPlayer* GetTrackedPlayer() const;
	void IncrementCount( int nIncrementValue );

	uint32 m_nObjectiveDefIndex;

private:
	CTFQuestEvaluator *m_pEvaluator;
	CQuestItemTracker *m_pParent;
};


class CQuestItemTracker : public CBaseSOTracker
{
public:
	CQuestItemTracker( const CSharedObject* pItem, CSteamID SteamIDOwner, CSOTrackerManager* pManager );
	~CQuestItemTracker();

	virtual void OnUpdate() OVERRIDE;
	virtual void OnRemove() OVERRIDE;

	void UpdatePointsFromSOItem();

	const CBaseQuestObjectiveTracker* FindTrackerForDefIndex( uint32 nDefIndex ) const;
	inline const CUtlVector< const CBaseQuestObjectiveTracker* >& GetTrackers() const { return m_vecObjectiveTrackers; }

	uint32 GetEarnedStandardPoints() const;
	uint32 GetEarnedBonusPoints() const;
	const CEconItem* GetItem() const { return static_cast< const CEconItem* >( m_pSObject ); }

	void IncrementCount( uint32 nIncrementValue, const CQuestObjectiveDefinition* pObjective );
	virtual void CommitChangesToDB() OVERRIDE;

	int IsValidForPlayer( const CTFPlayer *pOwner, InvalidReasonsContainer_t& invalidReasons ) const;

#ifdef CLIENT_DLL
	void UpdateFromServer( uint32 nStandardPoints, uint32 nBonusPoints );
#else
	void SendUpdateToClient( const CQuestObjectiveDefinition* pObjective );
#endif

#if defined( DEBUG ) || defined( STAGING_ONLY )
	void DBG_CompleteQuest();
#endif

	virtual void Spew() const OVERRIDE;

private:

	bool DoesObjectiveNeedToBeTracked( const CQuestObjectiveDefinition* pObjective ) const;

#ifdef GAME_DLL
	uint32 m_nStartingStandardPoints;
	uint32 m_nStartingBonusPoints;
#endif

	uint32 m_nStandardPoints;
	uint32 m_nBonusPoints;

	const CEconItem* m_pItem;
	
	CUtlVector< const CBaseQuestObjectiveTracker* > m_vecObjectiveTrackers;
};

// A class to handle the creation and deletion of quest objective trackers. Automatically
// subscribes to the local player's SOCache and will subscribe to any connecting players'
// SOCaches when they connect.
class CQuestObjectiveManager : public CSOTrackerManager
{
public:
	DECLARE_CLASS( CQuestObjectiveManager, CSOTrackerManager )

	CQuestObjectiveManager();
	virtual ~CQuestObjectiveManager();

	virtual SOTrackerMap_t::KeyType_t GetKeyForObjectTracker( const CSharedObject* pItem, CSteamID steamIDOwner ) OVERRIDE;

#ifdef CLIENT_DLL
	void UpdateFromServer( itemid_t nID, uint32 nStandardPoints, uint32 nBonusPoints );
#endif


#if defined( DEBUG ) || defined( STAGING_ONLY )
	void DBG_CompleteQuests();
#endif

private:
#ifdef GAME_DLL
	void SendMessageForCommit( const ::google::protobuf::Message* pProtoMessage ) const;
#endif

	virtual int GetType() const OVERRIDE { return CEconItem::k_nTypeID; }
	virtual const char* GetName() const { return "QuestObjectiveManager"; }
	virtual CFmtStr GetDebugObjectDescription( const CSharedObject* pItem ) const;
	virtual CBaseSOTracker* AllocateNewTracker( const CSharedObject* pItem, CSteamID steamIDOwner, CSOTrackerManager* pManager ) const OVERRIDE;
	virtual ::google::protobuf::Message* AllocateNewProtoMessage() const OVERRIDE;
	virtual void OnCommitRecieved( const ::google::protobuf::Message* pProtoMsg ) OVERRIDE;
	virtual bool ShouldTrackObject( const CSteamID & steamIDOwner, const CSharedObject *pObject ) const OVERRIDE;
	virtual int CompareRecords( const ::google::protobuf::Message* pNewProtoMsg, const ::google::protobuf::Message* pExistingProtoMsg ) const OVERRIDE;
};

CQuestObjectiveManager* QuestObjectiveManager();

#endif // QUEST_OBJECTIVE_MANAGER_H
