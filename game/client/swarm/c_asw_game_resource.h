#ifndef C_ASW_GAME_RESOURCE_H
#define C_ASW_GAME_RESOURCE_H
#pragma once

#include "c_baseentity.h"	
#include "asw_shareddefs.h"
#include "asw_marine_skills.h"

class C_ASW_Objective;
class C_ASW_Marine_Resource;
class C_ASW_Marine;
class C_ASW_Player;
class C_ASW_Scanner_Info;
class CASW_Campaign_Info;
class C_ASW_Campaign_Save;
class CASW_Marine_Profile;

// This entity networks various information about the game
//   such as list of selected marines, which player is leader, marine skills, etc.

class C_ASW_Game_Resource : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ASW_Game_Resource, C_BaseEntity );
	DECLARE_CLIENTCLASS();

					C_ASW_Game_Resource();
	virtual			~C_ASW_Game_Resource();

	CNetworkArray( EHANDLE, m_MarineResources, ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( EHANDLE, m_Objectives, ASW_MAX_OBJECTIVES );
	CNetworkArray( int, m_iRosterSelected, ASW_NUM_MARINE_PROFILES);
	CNetworkVar( bool, m_bOneMarineEach );
	CNetworkVar( int, m_iMaxMarines );

	// which player is the leader
	CNetworkHandle (C_ASW_Player, m_Leader);
	CNetworkVar(int, m_iLeaderIndex);
	CNetworkArray( bool, m_bPlayerReady, ASW_MAX_READY_PLAYERS );
	bool IsPlayerReady(C_ASW_Player *pPlayer);
	bool IsPlayerReady(int iPlayerEntIndex);

	C_ASW_Scanner_Info* GetScannerInfo();
	CNetworkHandle (C_ASW_Scanner_Info, m_hScannerInfo);

	bool IsOfflineGame() const { return m_bOfflineGame || ( gpGlobals->maxClients == 1 ); }
	CNetworkVar( bool, m_bOfflineGame );
		
	int IsCampaignGame() { return m_iCampaignGame; }
	CNetworkVar(int, m_iCampaignGame);	// is this a campaign game?  -1 = unknown, 0 = single mission, 1 = campaign

	C_ASW_Objective* GetObjective(int i);
	C_ASW_Marine_Resource* GetMarineResource(int i);
	int GetIndexFor(C_ASW_Marine_Resource* pMarineResource);
	bool IsRosterSelected(int i);
	bool IsRosterReserved(int i);
	int GetMarineResourceIndex( C_ASW_Marine_Resource *pMR );
	bool AtLeastOneMarine();	// is at least one marine selected?

	int GetMaxMarineResources() { return ASW_MAX_MARINE_RESOURCES; }
	int GetNumMarines(C_ASW_Player *pPlayer, bool bAliveOnly=false);	// returns how many marines this player has selected
	C_ASW_Player* GetLeader();
	int GetLeaderEntIndex() { return m_iLeaderIndex; }
	int GetNumMarineResources();
	C_ASW_Marine_Resource* GetFirstMarineResourceForPlayer( C_ASW_Player *pPlayer );	// returns the first marine resource controlled by this player

	C_ASW_Campaign_Save* GetCampaignSave();
	CNetworkHandle(C_ASW_Campaign_Save, m_hCampaignSave);

	bool AreAllOtherPlayersReady(int iPlayerEntIndex);
	
	// skills
	int GetSlotForSkill( int nProfileIndex, ASW_Skill nSkillIndex );
	int GetMarineSkill( int iProfileIndex, int nSkillSlot );
	int GetMarineSkill( C_ASW_Marine_Resource *m, int nSkillSlot );
	CNetworkArray(int, m_iSkillSlot0, ASW_NUM_MARINE_PROFILES);
	CNetworkArray(int, m_iSkillSlot1, ASW_NUM_MARINE_PROFILES);
	CNetworkArray(int, m_iSkillSlot2, ASW_NUM_MARINE_PROFILES);
	CNetworkArray(int, m_iSkillSlot3, ASW_NUM_MARINE_PROFILES);
	CNetworkArray(int, m_iSkillSlot4, ASW_NUM_MARINE_PROFILES);
	CNetworkArray(int, m_iSkillSlotSpare, ASW_NUM_MARINE_PROFILES);

	int m_iKickVotes[ASW_MAX_READY_PLAYERS];
	int m_iLeaderVotes[ASW_MAX_READY_PLAYERS];

	// player medals
	char	m_iszPlayerMedals[ ASW_MAX_READY_PLAYERS ][255];

	// returns current number of alive (non-KOed players)
	int CountAllAliveMarines( void );

	// returns count of all marines in these bounds;
	int EnumerateMarinesInBox(Vector &mins, Vector &maxs);	
	C_ASW_Marine* EnumeratedMarine(int i);
	C_ASW_Marine* m_pEnumeratedMarines[12];
	int m_iNumEnumeratedMarines;

	// a convenient means of finding which marines are closest
	// to the cursor; stores them in a sorted list,
	// and caches the computation so that it is performed
	// no more than once per frame.
	class CMarineToCrosshairInfo
	{
	public:
		// stores a handle to a marine and also that marine's distance to crosshair
		// (so you don't need to recompute it over and over again)
		struct tuple_t
		{
			tuple_t( C_ASW_Marine *pMarine, float fDist ) : m_hMarine(pMarine), m_fDistToCursor(fDist) {}
			tuple_t() : m_hMarine(), m_fDistToCursor(-FLT_MAX) {};
			CHandle<C_ASW_Marine> m_hMarine;
			float m_fDistToCursor;

			// an invalid handle returned for bad queries
			static tuple_t INVALID;
		};

		/// Get info on the closest marine.
		inline const tuple_t &GetClosestMarine() { return GetElement(0); }

		/// Returns as if a list of marines sorted by distance to crosshair, closest to furthest.
		/// the actual type of the list is the tuple_t below, which stores a handle
		/// and also the distance for convenience.
		inline const tuple_t &GetElement( int e );
		inline const tuple_t &operator[]( int i ) { return GetElement(i); }

		/// find the index corresponding to a given marine. returns -1 if marine isn't found (which should never happen)
		int FindIndexForMarine( C_ASW_Marine *pMarine );

		inline int Count();
	protected:
		inline void CheckCache();  // and recompute if necessary
		void RecomputeCache();

		int m_iLastFrameCached; /// the framecount of the last time my info was cached.
		CUtlVectorFixed< tuple_t, ASW_MAX_MARINE_RESOURCES > m_tMarines; // a list of marines sorted by distance to crosshair, closest to furthest
	};
	// access the singleton info struct giving info on marines close to crosshairs
	inline CMarineToCrosshairInfo *GetMarineCrosshairCache() { return &m_marineToCrosshairInfo; } 

	CASW_Campaign_Info* m_pCampaignInfo;

	// money
	int  GetMoney() { return m_iMoney; }
	CNetworkVar( int, m_iMoney );

	// map generation progress
	float m_fMapGenerationProgress;
	char	m_szMapGenerationStatus[ 128 ];
	int m_iRandomMapSeed;							// if set clients, will begin generating a random map based on this seed

	int GetNextCampaignMissionIndex() { return m_iNextCampaignMission.Get(); }
	CNetworkVar( int, m_iNextCampaignMission );

	CNetworkVar( int, m_nDifficultySuggestion );

protected:
	CMarineToCrosshairInfo m_marineToCrosshairInfo;

private:
	C_ASW_Game_Resource( const C_ASW_Game_Resource & ); // not defined, not accessible
};

extern C_ASW_Game_Resource *g_pASWGameResource;

inline C_ASW_Game_Resource* ASWGameResource()
{
	return g_pASWGameResource;
}

inline void C_ASW_Game_Resource::CMarineToCrosshairInfo::CheckCache()  // and recompute if necessary
{
	if (gpGlobals->framecount != m_iLastFrameCached) 
		RecomputeCache(); 
}

inline int C_ASW_Game_Resource::CMarineToCrosshairInfo::Count()
{
	CheckCache();
	return m_tMarines.Count();
}

inline const C_ASW_Game_Resource::CMarineToCrosshairInfo::tuple_t & C_ASW_Game_Resource::CMarineToCrosshairInfo::GetElement( int e )
{
	CheckCache();
	if ( e < Count() )
		return m_tMarines[e];
	else
		return tuple_t::INVALID;
}

#endif /* C_ASW_GAME_RESOURCE_H */
