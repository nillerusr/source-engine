//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "tf_ladder_data.h"
#include "gcsdk/enumutils.h"

#ifdef CLIENT_DLL
#include "econ/confirm_dialog.h"
#include "tf_matchmaking_shared.h"
#include "c_tf_player.h"
#endif

using namespace GCSDK;


//-----------------------------------------------------------------------------
// Purpose: Get player's ladder stat data by steamID.  Returns nullptr if it doesn't exist
//-----------------------------------------------------------------------------
CSOTFLadderData *YieldingGetPlayerLadderDataBySteamID( const CSteamID &steamID, EMatchGroup nMatchGroup )
{
#ifdef GC_DLL
	GCSDK::CGCSharedObjectCache *pSOCache = GGCEcon()->YieldingFindOrLoadSOCache( steamID );
#else
	GCSDK::CGCClientSharedObjectCache *pSOCache = GCClientSystem()->GetSOCache( steamID );
#endif
	if ( pSOCache )
	{
		auto *pTypeCache = pSOCache->FindTypeCache( CSOTFLadderData::k_nTypeID );
		if ( pTypeCache )
		{
			for ( uint32 i = 0; i < pTypeCache->GetCount(); ++i )
			{
				CSOTFLadderData *pLadderData = (CSOTFLadderData*)pTypeCache->GetObject( i );
				if ( nMatchGroup == (EMatchGroup)pLadderData->Obj().match_group() )
				{
					return pLadderData;
				}
			}
		}
	}

	return nullptr;
}

#if !defined( GC )
//-----------------------------------------------------------------------------
// Purpose: Get the local player's Ladder Data.  Returns NULL if it doesn't exist (no GC)
//-----------------------------------------------------------------------------
CSOTFLadderData *GetLocalPlayerLadderData( EMatchGroup nMatchGroup )
{
	if ( steamapicontext && steamapicontext->SteamUser() )
	{
		return YieldingGetPlayerLadderDataBySteamID( steamapicontext->SteamUser()->GetSteamID(), nMatchGroup );
	}

	return NULL;
}
#endif // !GC

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSOTFLadderData::CSOTFLadderData()
{
	Obj().set_account_id( 0 );
	Obj().set_match_group( k_nMatchGroup_Invalid );
	Obj().set_season_id( 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSOTFLadderData::CSOTFLadderData( uint32 unAccountID, EMatchGroup eMatchGroup  )
{
	Obj().set_account_id( unAccountID );
	Obj().set_match_group( eMatchGroup );
	Obj().set_season_id( 1 );
}
#ifdef GC

IMPLEMENT_CLASS_MEMPOOL( CSOTFLadderData, 1000, UTLMEMORYPOOL_GROW_SLOW );
IMPLEMENT_CLASS_MEMPOOL( CSOTFMatchResultPlayerInfo, 1000, UTLMEMORYPOOL_GROW_SLOW );

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool CSOTFLadderData::BIsKeyLess( const CSharedObject & soRHS ) const
{
	Assert( GetTypeID() == soRHS.GetTypeID() );
	const CSOTFLadderPlayerStats &obj = Obj();
	const CSOTFLadderPlayerStats &rhs = ( static_cast< const CSOTFLadderData & >( soRHS ) ).Obj();

	if ( obj.account_id() < rhs.account_id() ) return true;
	if ( obj.account_id() > rhs.account_id() ) return false;
	if ( obj.match_group() < rhs.match_group() ) return true;
	if ( obj.match_group() > rhs.match_group() ) return false;

	return obj.season_id() < rhs.season_id();
}

//-----------------------------------------------------------------------------
bool CSOTFLadderData::BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchLadderData schLadderData;
	WriteToRecord( &schLadderData );
	return CSchemaSharedObjectHelper::BYieldingAddInsertToTransaction( sqlAccess, &schLadderData );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CSOTFLadderData::BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields )
{
	CSchLadderData schLadderData;
	WriteToRecord( &schLadderData );
	CColumnSet csDatabaseDirty( schLadderData.GetPSchema()->GetRecordInfo() );
	csDatabaseDirty.MakeEmpty();
	FOR_EACH_VEC( fields, nField )
	{
		switch ( fields[nField] )
		{
		case CSOTFLadderPlayerStats::kAccountIdFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unAccountID ); break;
		case CSOTFLadderPlayerStats::kMatchGroupFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_nMatchGroup ); break;
		case CSOTFLadderPlayerStats::kSeasonIdFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unSeasonID ); break;

		case CSOTFLadderPlayerStats::kRankFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unRank ); break;
		case CSOTFLadderPlayerStats::kHighestRankFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unHighestRank ); break;
		case CSOTFLadderPlayerStats::kExperienceFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unExperience ); break;
		case CSOTFLadderPlayerStats::kLastAckdExperienceFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unLastAckdExperience ); break;

		case CSOTFLadderPlayerStats::kGamesFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unGames ); break;
		case CSOTFLadderPlayerStats::kScoreFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unScore ); break;
		case CSOTFLadderPlayerStats::kKillsFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unKills ); break;
		case CSOTFLadderPlayerStats::kDeathsFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unDeaths ); break;
		case CSOTFLadderPlayerStats::kDamageFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unDamage ); break;
		case CSOTFLadderPlayerStats::kHealingFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unHealing ); break;
		case CSOTFLadderPlayerStats::kSupportFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unSupport ); break;

		case CSOTFLadderPlayerStats::kScoreBronzeFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unScoreBronze ); break;
		case CSOTFLadderPlayerStats::kScoreSilverFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unScoreSilver ); break;
		case CSOTFLadderPlayerStats::kScoreGoldFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unScoreGold ); break;
		case CSOTFLadderPlayerStats::kKillsBronzeFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unKillsBronze ); break;
		case CSOTFLadderPlayerStats::kKillsSilverFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unKillsSilver ); break;
		case CSOTFLadderPlayerStats::kKillsGoldFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unKillsGold ); break;
		case CSOTFLadderPlayerStats::kDamageBronzeFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unDamageBronze ); break;
		case CSOTFLadderPlayerStats::kDamageSilverFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unDamageSilver ); break;
		case CSOTFLadderPlayerStats::kDamageGoldFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unDamageGold ); break;
		case CSOTFLadderPlayerStats::kHealingBronzeFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unHealingBronze ); break;
		case CSOTFLadderPlayerStats::kHealingSilverFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unHealingSilver ); break;
		case CSOTFLadderPlayerStats::kHealingGoldFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unHealingGold ); break;
		case CSOTFLadderPlayerStats::kSupportBronzeFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unSupportBronze ); break;
		case CSOTFLadderPlayerStats::kSupportSilverFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unSupportSilver ); break;
		case CSOTFLadderPlayerStats::kSupportGoldFieldNumber: csDatabaseDirty.BAddColumn( CSchLadderData::k_iField_unSupportGold ); break;

		default:
			Assert( false );
		}
	}
	return CSchemaSharedObjectHelper::BYieldingAddWriteToTransaction( sqlAccess, &schLadderData, csDatabaseDirty );
}

//-----------------------------------------------------------------------------
bool CSOTFLadderData::BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchLadderData schLadderData;
	WriteToRecord( &schLadderData );
	return CSchemaSharedObjectHelper::BYieldingAddRemoveToTransaction( sqlAccess, &schLadderData );
}

//-----------------------------------------------------------------------------
void CSOTFLadderData::WriteToRecord( CSchLadderData *pLadderData ) const
{
	pLadderData->m_unAccountID = Obj().account_id();

	pLadderData->m_nMatchGroup = (int16)Obj().match_group();
	pLadderData->m_unSeasonID = (uint16)Obj().season_id();

	pLadderData->m_unRank = (uint16)Obj().rank();
	pLadderData->m_unHighestRank = (uint16)Obj().highest_rank();
	pLadderData->m_unExperience = Obj().experience();
	pLadderData->m_unLastAckdExperience = Obj().last_ackd_experience();

	pLadderData->m_unGames = Obj().games();
	pLadderData->m_unScore = Obj().score();
	pLadderData->m_unKills = Obj().kills();
	pLadderData->m_unDeaths = Obj().deaths();
	pLadderData->m_unDamage = Obj().damage();
	pLadderData->m_unHealing = Obj().healing();
	pLadderData->m_unSupport = Obj().support();

	pLadderData->m_unScoreBronze = (uint16)Obj().score_bronze();
	pLadderData->m_unScoreSilver = (uint16)Obj().score_silver();
	pLadderData->m_unScoreGold = (uint16)Obj().score_gold();
	pLadderData->m_unKillsBronze = (uint16)Obj().score_bronze();
	pLadderData->m_unKillsSilver = (uint16)Obj().score_silver();
	pLadderData->m_unKillsGold = (uint16)Obj().score_gold();
	pLadderData->m_unDamageBronze = (uint16)Obj().damage_bronze();
	pLadderData->m_unDamageSilver = (uint16)Obj().damage_silver();
	pLadderData->m_unDamageGold = (uint16)Obj().damage_gold();
	pLadderData->m_unHealingBronze = (uint16)Obj().healing_bronze();
	pLadderData->m_unHealingSilver = (uint16)Obj().healing_silver();
	pLadderData->m_unHealingGold = (uint16)Obj().healing_gold();
	pLadderData->m_unSupportBronze = (uint16)Obj().support_bronze();
	pLadderData->m_unSupportSilver = (uint16)Obj().support_silver();
	pLadderData->m_unSupportGold = (uint16)Obj().support_gold();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSOTFLadderData::ReadFromRecord( const CSchLadderData &ladderData )
{
	Obj().set_account_id( ladderData.m_unAccountID );
	Obj().set_match_group( ladderData.m_nMatchGroup );
	Obj().set_season_id( 1 );	// TODO: GetSeasonID()

	Obj().set_rank( ladderData.m_unRank );
	Obj().set_highest_rank( ladderData.m_unHighestRank );
	Obj().set_experience( ladderData.m_unExperience );
	Obj().set_last_ackd_experience( ladderData.m_unLastAckdExperience );

	Obj().set_games( ladderData.m_unGames );
	Obj().set_score( ladderData.m_unScore );
	Obj().set_kills( ladderData.m_unKills );
	Obj().set_deaths( ladderData.m_unDeaths );
	Obj().set_damage( ladderData.m_unDamage );
	Obj().set_healing( ladderData.m_unHealing );
	Obj().set_support( ladderData.m_unSupport );

	Obj().set_score_bronze( ladderData.m_unScoreBronze );
	Obj().set_score_silver( ladderData.m_unScoreSilver );
	Obj().set_score_gold( ladderData.m_unScoreGold );
	Obj().set_kills_bronze( ladderData.m_unKillsBronze );
	Obj().set_kills_silver( ladderData.m_unKillsSilver );
	Obj().set_kills_gold( ladderData.m_unKillsGold );
	Obj().set_damage_bronze( ladderData.m_unDamageBronze );
	Obj().set_damage_silver( ladderData.m_unDamageSilver);
	Obj().set_damage_gold( ladderData.m_unDamageGold );
	Obj().set_healing_bronze( ladderData.m_unHealingBronze );
	Obj().set_healing_silver( ladderData.m_unHealingSilver );
	Obj().set_healing_gold( ladderData.m_unHealingGold );
	Obj().set_support_bronze( ladderData.m_unSupportBronze );
	Obj().set_support_silver( ladderData.m_unSupportSilver );
	Obj().set_support_gold( ladderData.m_unSupportGold );
}
#endif	// GC

#if !defined( GC )
void GetLocalPlayerMatchHistory( EMatchGroup nMatchGroup, CUtlVector < CSOTFMatchResultPlayerStats > &vecMatchesOut )
{
	if ( steamapicontext && steamapicontext->SteamUser() )
	{
		CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();
		GCSDK::CGCClientSharedObjectCache *pSOCache = GCClientSystem()->GetSOCache( steamID );
		if ( pSOCache )
		{
			GCSDK::CGCClientSharedObjectTypeCache *pTypeCache = pSOCache->FindTypeCache( CSOTFMatchResultPlayerInfo::k_nTypeID );
			if ( pTypeCache )
			{
				for ( uint32 i = 0; i < pTypeCache->GetCount(); ++i )
				{
					CSOTFMatchResultPlayerInfo *pMatchStats = (CSOTFMatchResultPlayerInfo*)pTypeCache->GetObject( i );
					if ( nMatchGroup == (EMatchGroup)pMatchStats->Obj().match_group() )
					{
						vecMatchesOut.AddToTail( pMatchStats->Obj() );
					}
				}
			}
		}
	}
}
#endif // !GC
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSOTFMatchResultPlayerInfo::CSOTFMatchResultPlayerInfo()
{
	Obj().set_account_id( 0 );
}

#ifdef GC

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSOTFMatchResultPlayerInfo::CSOTFMatchResultPlayerInfo( uint32 unAccountID )
{
 	Obj().set_match_id( 0 );
	Obj().set_account_id( unAccountID );
 	Obj().set_match_group( k_nMatchGroup_Invalid );
	Obj().set_endtime( 0 );
	Obj().set_season_id( 1 );
	Obj().set_status( 0 );

	Obj().set_party_id( 0 );
	Obj().set_team( 0 );
	Obj().set_score( 0 );
	Obj().set_ping( 0 );
	Obj().set_flags( 0 );
	Obj().set_display_rating( 0 );
	Obj().set_display_rating_change( 0 );
	Obj().set_rank( 0 );
	Obj().set_classes_played( 0 );

	Obj().set_kills( 0 );
	Obj().set_deaths( 0 );
	Obj().set_damage( 0 );
	Obj().set_healing( 0 );
	Obj().set_support( 0 );

	Obj().set_score_medal( 0 );
	Obj().set_kills_medal( 0 );
	Obj().set_damage_medal( 0 );
	Obj().set_healing_medal( 0 );
	Obj().set_support_medal( 0 );

	Obj().set_map_index( 0 );
}

bool CSOTFMatchResultPlayerInfo::BIsKeyLess( const CSharedObject & soRHS ) const
{
	Assert( GetTypeID() == soRHS.GetTypeID() );
	const CSOTFMatchResultPlayerStats &obj = Obj();
	const CSOTFMatchResultPlayerStats &rhs = ( static_cast<const CSOTFMatchResultPlayerInfo &>( soRHS ) ).Obj();
	Assert( obj.account_id() == obj.account_id() );

	if ( obj.match_group() < rhs.match_group() ) return true;
	if ( obj.match_group() > rhs.match_group() ) return false;

	return obj.season_id() < rhs.season_id();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSOTFMatchResultPlayerInfo::BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchMatchResultPlayerInfo schMatchInfo;
	WriteToRecord( &schMatchInfo );
	return CSchemaSharedObjectHelper::BYieldingAddInsertToTransaction( sqlAccess, &schMatchInfo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSOTFMatchResultPlayerInfo::BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields )
{
	CSchMatchResultPlayerInfo schMatchInfo;
	WriteToRecord( &schMatchInfo );
	CColumnSet csDatabaseDirty( schMatchInfo.GetPSchema()->GetRecordInfo() );
	csDatabaseDirty.MakeEmpty();
	FOR_EACH_VEC( fields, nField )
	{
		switch ( fields[nField] )
		{
		case CSOTFMatchResultPlayerStats::kMatchIdFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unMatchID ); break;
		case CSOTFMatchResultPlayerStats::kAccountIdFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unAccountID ); break;
		case CSOTFMatchResultPlayerStats::kMatchGroupFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_nMatchGroup ); break;
		case CSOTFMatchResultPlayerStats::kEndtimeFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_RTime32Stamp ); break;
		case CSOTFMatchResultPlayerStats::kSeasonIdFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unSeasonID ); break;
		case CSOTFMatchResultPlayerStats::kStatusFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unStatus ); break;

		case CSOTFMatchResultPlayerStats::kPartyIdFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unPartyID ); break;
		case CSOTFMatchResultPlayerStats::kTeamFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unTeam ); break;
		case CSOTFMatchResultPlayerStats::kScoreFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unScore ); break;
		case CSOTFMatchResultPlayerStats::kPingFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unPing ); break;
		case CSOTFMatchResultPlayerStats::kFlagsFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unFlags ); break;
		case CSOTFMatchResultPlayerStats::kDisplayRatingFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unDisplayRating ); break;
		case CSOTFMatchResultPlayerStats::kDisplayRatingChangeFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_nDisplayRatingChange ); break;
		case CSOTFMatchResultPlayerStats::kRankFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unRank ); break;
		case CSOTFMatchResultPlayerStats::kClassesPlayedFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unClassesPlayed ); break;

		case CSOTFMatchResultPlayerStats::kKillsFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unKills ); break;
		case CSOTFMatchResultPlayerStats::kDeathsFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unDeaths ); break;
		case CSOTFMatchResultPlayerStats::kDamageFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unDamage ); break;
		case CSOTFMatchResultPlayerStats::kHealingFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unHealing ); break;
		case CSOTFMatchResultPlayerStats::kSupportFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unSupport ); break;

		case CSOTFMatchResultPlayerStats::kScoreMedalFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unScoreMedal ); break;
		case CSOTFMatchResultPlayerStats::kKillsMedalFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unKillsMedal ); break;
		case CSOTFMatchResultPlayerStats::kDamageMedalFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unDamageMedal ); break;
		case CSOTFMatchResultPlayerStats::kHealingMedalFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unHealingMedal ); break;
		case CSOTFMatchResultPlayerStats::kSupportMedalFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unSupportMedal ); break;

		case CSOTFMatchResultPlayerStats::kMapIndexFieldNumber: csDatabaseDirty.BAddColumn( CSchMatchResultPlayerInfo::k_iField_unMapIndex ); break;

		default:
			Assert( false );
		}
	}
	return CSchemaSharedObjectHelper::BYieldingAddWriteToTransaction( sqlAccess, &schMatchInfo, csDatabaseDirty );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSOTFMatchResultPlayerInfo::BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchMatchResultPlayerInfo schMatchInfo;
	WriteToRecord( &schMatchInfo );
	return CSchemaSharedObjectHelper::BYieldingAddRemoveToTransaction( sqlAccess, &schMatchInfo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSOTFMatchResultPlayerInfo::WriteToRecord( CSchMatchResultPlayerInfo *pMatchInfo ) const
{
	pMatchInfo->m_unMatchID = Obj().match_id();
	pMatchInfo->m_unAccountID = Obj().account_id();
	pMatchInfo->m_nMatchGroup = (int16)Obj().match_group();
	pMatchInfo->m_RTime32Stamp = Obj().endtime();
	pMatchInfo->m_unSeasonID = (uint16)Obj().season_id();
	pMatchInfo->m_unStatus = (uint16)Obj().status();

	pMatchInfo->m_unPartyID = Obj().party_id();
	pMatchInfo->m_unTeam = (uint16)Obj().team();
	pMatchInfo->m_unScore = (uint16)Obj().score();
	pMatchInfo->m_unPing = (uint16)Obj().ping();
	pMatchInfo->m_unFlags = Obj().flags();
	pMatchInfo->m_unDisplayRating = Obj().display_rating();
	pMatchInfo->m_nDisplayRatingChange = Obj().display_rating_change();
	pMatchInfo->m_unRank = (uint16)Obj().rank();
	pMatchInfo->m_unClassesPlayed = Obj().classes_played();

	pMatchInfo->m_unKills = (uint16)Obj().kills();
	pMatchInfo->m_unDeaths = (uint16)Obj().deaths();
	pMatchInfo->m_unDamage = Obj().damage();
	pMatchInfo->m_unHealing = Obj().healing();
	pMatchInfo->m_unSupport = Obj().support();

	pMatchInfo->m_unScoreMedal = (uint8)Obj().score_medal();
	pMatchInfo->m_unKillsMedal = (uint8)Obj().kills_medal();
	pMatchInfo->m_unDamageMedal = (uint8)Obj().damage_medal();
	pMatchInfo->m_unHealingMedal = (uint8)Obj().healing_medal();
	pMatchInfo->m_unSupportMedal = (uint8)Obj().support_medal();

	pMatchInfo->m_unMapIndex = (uint16)Obj().map_index();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSOTFMatchResultPlayerInfo::ReadFromRecord( const CSchMatchResultPlayerInfo &matchInfo )
{
	Obj().set_match_id( matchInfo.m_unMatchID );
	Obj().set_account_id( matchInfo.m_unAccountID );
	Obj().set_match_group( matchInfo.m_nMatchGroup );
	Obj().set_endtime( matchInfo.m_RTime32Stamp );
	Obj().set_season_id( matchInfo.m_unSeasonID );
	Obj().set_status( matchInfo.m_unStatus );

	Obj().set_party_id( matchInfo.m_unPartyID );
	Obj().set_team( matchInfo.m_unTeam );
	Obj().set_score( matchInfo.m_unScore );
	Obj().set_ping( matchInfo.m_unPing );
	Obj().set_flags( matchInfo.m_unFlags );
	Obj().set_display_rating( matchInfo.m_unDisplayRating );
	Obj().set_display_rating_change( matchInfo.m_nDisplayRatingChange );
	Obj().set_rank( matchInfo.m_unRank );
	Obj().set_classes_played( matchInfo.m_unClassesPlayed );

	Obj().set_kills( matchInfo.m_unKills );
	Obj().set_deaths( matchInfo.m_unDeaths );
	Obj().set_damage( matchInfo.m_unDamage );
	Obj().set_healing( matchInfo.m_unHealing );
	Obj().set_support( matchInfo.m_unSupport );

	Obj().set_score_medal( matchInfo.m_unScoreMedal );
	Obj().set_kills_medal( matchInfo.m_unKillsMedal );
	Obj().set_damage_medal( matchInfo.m_unDamageMedal );
	Obj().set_healing_medal( matchInfo.m_unHealingMedal );
	Obj().set_support_medal( matchInfo.m_unSupportMedal );

	Obj().set_map_index( matchInfo.m_unMapIndex );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CGCMatchHistoryLoad : public CGCEconJob
{
public:
	CGCMatchHistoryLoad( CGCEcon *pGC ) : CGCEconJob( pGC ) {}
	bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket );
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CGCMatchHistoryLoad::BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
{
	CProtoBufMsg < CMsgGCMatchHistoryLoad > msg( pNetPacket );
	const CSteamID steamID( msg.Hdr().client_steam_id() );
	if ( !steamID.IsValid() || !steamID.BIndividualAccount() )
		return true;

	CTFSharedObjectCache *pSOCache = GGCTF()->YieldingGetLockedTFSOCache( steamID, __FILE__, __LINE__ );
	if ( !pSOCache )
		return true;

	CScopedSteamIDLock playerLock;
	playerLock.MarkLocked( steamID );

	pSOCache->BYieldingLoadMatchHistoryObjects( true );

	return true;
}

GC_REG_JOB( CGCEcon, CGCMatchHistoryLoad, "CGCMatchHistoryLoad", k_EMsgGCMatchHistoryLoad, k_EServerTypeGC );

#endif	// GC
