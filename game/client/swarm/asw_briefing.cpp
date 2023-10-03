#include "cbase.h"
#include "asw_briefing.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "asw_equipment_list.h"
#include "asw_marine_profile.h"
#include "asw_weapon_parse.h"
#include <vgui/ILocalize.h>
#include "c_playerresource.h"
#include "asw_gamerules.h"
#include "c_asw_campaign_save.h"
#define CASW_Equip_Req C_ASW_Equip_Req
#include "asw_equip_req.h"
#include "voice_status.h"
#include "asw_campaign_info.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_ignore_need_two_player_requirement;

CASW_Briefing* g_pBriefing = NULL;

IBriefing* Briefing()
{
	if ( !g_pBriefing )
	{
		// could return different briefings here to support briefing outside of a map
		g_pBriefing = new CASW_Briefing();
	}
	return g_pBriefing;
}

CASW_Briefing::CASW_Briefing()
{
	m_nLastLobbySlotMappingFrame = -1;
	m_flLastSelectionChatterTime = 0.0f;
}

CASW_Briefing::~CASW_Briefing()
{
	if ( g_pBriefing == this )
	{
		g_pBriefing = NULL;
	}
}

void CASW_Briefing::UpdateLobbySlotMapping()
{
	if ( m_nLastLobbySlotMappingFrame == gpGlobals->framecount )		// don't update twice in one frame
		return;

	m_nLastLobbySlotMappingFrame = gpGlobals->framecount;

	if ( !ASWGameResource() )
		return;

	C_ASW_Player *pLocalPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pLocalPlayer )
		return;

	if ( IsOfflineGame() )
	{
		// just map marine resources to slots directly		
		for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources() && i < NUM_BRIEFING_LOBBY_SLOTS; i++ )
		{
			C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
			if ( !pMR )
			{
				if ( i == 0 )
				{
					m_LobbySlotMapping[ i ].m_nPlayerEntIndex = pLocalPlayer->entindex();
					m_LobbySlotMapping[ i ].m_hPlayer = pLocalPlayer;
				}
				else
				{
					m_LobbySlotMapping[ i ].m_nPlayerEntIndex = -1;
					m_LobbySlotMapping[ i ].m_hPlayer = NULL;
				}
				m_LobbySlotMapping[ i ].m_nMarineResourceIndex = -1;
				m_LobbySlotMapping[ i ].m_hMR = NULL;
			}
			else
			{
				m_LobbySlotMapping[ i ].m_nPlayerEntIndex = pLocalPlayer->entindex();
				m_LobbySlotMapping[ i ].m_hPlayer = pLocalPlayer;
				m_LobbySlotMapping[ i ].m_nMarineResourceIndex = i;
				m_LobbySlotMapping[ i ].m_hMR = pMR;
			}
		}
		return;
	}

	// lobby slot 0 is always reserved for the local player
	m_LobbySlotMapping[ 0 ].m_nPlayerEntIndex = pLocalPlayer->entindex();
	m_LobbySlotMapping[ 0 ].m_hPlayer = pLocalPlayer;
	m_LobbySlotMapping[ 0 ].m_nMarineResourceIndex = -1;
	m_LobbySlotMapping[ 0 ].m_hMR = NULL;

	for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
	{
		C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
		if ( !pMR || pMR->GetCommander() != pLocalPlayer )
			continue;

		m_LobbySlotMapping[ 0 ].m_nMarineResourceIndex = i;
		m_LobbySlotMapping[ 0 ].m_hMR = pMR;
		break;
	}

	int nSlot = 1;
	
	// if the player has any other marines selected, they come first
	for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
	{
		C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
		if ( !pMR || pMR->GetCommander() != pLocalPlayer )
			continue;
		
		bool bAlreadyInList = false;
		for ( int k = 0; k < nSlot; k++ )
		{
			if ( pMR == m_LobbySlotMapping[ k ].m_hMR.Get() )
			{
				bAlreadyInList = true;
				break;
			}
		}

		if ( bAlreadyInList )
			continue;

		m_LobbySlotMapping[ nSlot ].m_nPlayerEntIndex = pLocalPlayer->entindex();
		m_LobbySlotMapping[ nSlot ].m_hPlayer = pLocalPlayer;
		m_LobbySlotMapping[ nSlot ].m_hMR = pMR;
		m_LobbySlotMapping[ nSlot ].m_nMarineResourceIndex = i;

		nSlot++;
		if ( nSlot >= NUM_BRIEFING_LOBBY_SLOTS )
			break;
	}

	if ( nSlot >= NUM_BRIEFING_LOBBY_SLOTS )
		return;

	// now add marines for other players in order
	for( int iClient = 1; iClient < MAX_PLAYERS; iClient++ )
	{
		if ( !g_PR->IsConnected( iClient ) )
			continue;

		if ( iClient == pLocalPlayer->entindex() )
			continue;

		for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
		{
			C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
			if ( !pMR || pMR->m_iCommanderIndex != iClient )
				continue;

			bool bAlreadyInList = false;
			for ( int k = 0; k < nSlot; k++ )
			{
				if ( pMR == m_LobbySlotMapping[ k ].m_hMR.Get() )
				{
					bAlreadyInList = true;
					break;
				}
			}

			if ( bAlreadyInList )
				continue;

			m_LobbySlotMapping[ nSlot ].m_nPlayerEntIndex = iClient;
			m_LobbySlotMapping[ nSlot ].m_hPlayer = static_cast<C_ASW_Player*>( UTIL_PlayerByIndex( iClient ) );
			m_LobbySlotMapping[ nSlot ].m_hMR = pMR;
			m_LobbySlotMapping[ nSlot ].m_nMarineResourceIndex = i;

			nSlot++;
			if ( nSlot >= NUM_BRIEFING_LOBBY_SLOTS )
				break;
		}
	}

	if ( nSlot >= NUM_BRIEFING_LOBBY_SLOTS )
		return;

	// now add any players who don't have any marines
	for( int iClient = 1; iClient < MAX_PLAYERS; iClient++ )
	{
		if ( !g_PR->IsConnected( iClient ) )
			continue;

		if ( iClient == pLocalPlayer->entindex() )
			continue;

		int nMarines = 0;
		for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
		{
			C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
			if ( !pMR || pMR->m_iCommanderIndex != iClient )
				continue;

			nMarines++;
		}

		if ( nMarines == 0)
		{
			m_LobbySlotMapping[ nSlot ].m_nPlayerEntIndex = iClient;
			m_LobbySlotMapping[ nSlot ].m_hPlayer = static_cast<C_ASW_Player*>( UTIL_PlayerByIndex( iClient ) );
			m_LobbySlotMapping[ nSlot ].m_hMR = NULL;
			m_LobbySlotMapping[ nSlot ].m_nMarineResourceIndex = -1;

			nSlot++;
			if ( nSlot >= NUM_BRIEFING_LOBBY_SLOTS )
				break;
		}
	}

	for ( int k = nSlot; k < NUM_BRIEFING_LOBBY_SLOTS; k++ )
	{
		m_LobbySlotMapping[ k ].m_nPlayerEntIndex = -1;
		m_LobbySlotMapping[ k ].m_hPlayer = NULL;
		m_LobbySlotMapping[ k ].m_hMR = NULL;
		m_LobbySlotMapping[ k ].m_nMarineResourceIndex = -1;
	}
}

int CASW_Briefing::LobbySlotToMarineResourceIndex( int nLobbySlot )
{
	if ( nLobbySlot < 0 || nLobbySlot >= NUM_BRIEFING_LOBBY_SLOTS )
		return -1;

	UpdateLobbySlotMapping();

	return m_LobbySlotMapping[ nLobbySlot ].m_nMarineResourceIndex;
}

const char* CASW_Briefing::GetLeaderName()
{
	if ( !ASWGameResource() )
		return "";

	C_ASW_Player *pLeader = ASWGameResource()->GetLeader();
	if ( !pLeader )
		return "";

	return pLeader->GetPlayerName();
}

bool CASW_Briefing::IsLocalPlayerLeader()
{
	if ( !ASWGameResource() )
		return false;

	C_ASW_Player *pLeader = ASWGameResource()->GetLeader();
	if ( !pLeader )
		return false;

	return pLeader == C_ASW_Player::GetLocalASWPlayer();
}

bool CASW_Briefing::IsLobbySlotOccupied( int nLobbySlot )
{
	UpdateLobbySlotMapping();

	return m_LobbySlotMapping[ nLobbySlot ].m_nPlayerEntIndex != -1;
}

bool CASW_Briefing::IsLobbySlotLocal( int nLobbySlot )
{
	if ( nLobbySlot == 0 || IsOfflineGame() )		// first slot is always the local player
		return true;

	int nMarineResourceIndex = LobbySlotToMarineResourceIndex( nLobbySlot );
	C_ASW_Marine_Resource *pMR = ASWGameResource() ? ASWGameResource()->GetMarineResource( nMarineResourceIndex ) : NULL;
	if ( !pMR )
		return false;

	return pMR->GetCommander() && ( pMR->GetCommander() == C_ASW_Player::GetLocalASWPlayer() );
}

bool CASW_Briefing::IsLobbySlotBot( int nLobbySlot )
{
	if ( nLobbySlot < 0 || nLobbySlot >= NUM_BRIEFING_LOBBY_SLOTS || !IsLobbySlotOccupied( nLobbySlot ) )
		return false;

	int nMarineResourceIndex = LobbySlotToMarineResourceIndex( nLobbySlot );
	C_ASW_Marine_Resource *pMR = ASWGameResource() ? ASWGameResource()->GetMarineResource( nMarineResourceIndex ) : NULL;

	bool bHuman = ( pMR == NULL );

	if ( pMR )
	{
		C_ASW_Player *pPlayer = m_LobbySlotMapping[ nLobbySlot ].m_hPlayer.Get();
		C_ASW_Marine_Resource *pFirstMR = pPlayer ? ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer ) : NULL;

		if ( pFirstMR == pMR )
		{
			bHuman = true;
		}
	}
	return !bHuman;
}

wchar_t* CASW_Briefing::GetMarineOrPlayerName( int nLobbySlot )
{
	if ( nLobbySlot < 0 || nLobbySlot >= NUM_BRIEFING_LOBBY_SLOTS || !IsLobbySlotOccupied( nLobbySlot ) )
		return L"";

	int nMarineResourceIndex = LobbySlotToMarineResourceIndex( nLobbySlot );
	C_ASW_Marine_Resource *pMR = ASWGameResource() ? ASWGameResource()->GetMarineResource( nMarineResourceIndex ) : NULL;

	bool bUsePlayerName = ( pMR == NULL );

	if ( pMR )
	{
		C_ASW_Player *pPlayer = m_LobbySlotMapping[ nLobbySlot ].m_hPlayer.Get();
		C_ASW_Marine_Resource *pFirstMR = pPlayer ? ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer ) : NULL;

		if ( pFirstMR == pMR )
		{
			bUsePlayerName = true;
		}
	}
	else if ( !bUsePlayerName )
	{
		// no marine and no player name to use, return blank
		return L"";
	}

	
	static wchar_t wszMarineNameResult[ 32 ];

	// if it's their first marine, show the commander name instead
	if ( bUsePlayerName )
	{		
		C_ASW_Player *pPlayer = m_LobbySlotMapping[ nLobbySlot ].m_hPlayer.Get();
		if ( !pPlayer )
			return L"";

		const char *pszPlayerName = pPlayer->GetPlayerName();
		g_pVGuiLocalize->ConvertANSIToUnicode( pszPlayerName ? pszPlayerName : "", wszMarineNameResult, sizeof( wszMarineNameResult ) );
		return wszMarineNameResult;
	}
	
	pMR->GetDisplayName( wszMarineNameResult, sizeof( wszMarineNameResult ) );
	return wszMarineNameResult;
}

wchar_t* CASW_Briefing::GetMarineName( int nLobbySlot )
{
	if ( nLobbySlot < 0 || nLobbySlot >= NUM_BRIEFING_LOBBY_SLOTS || !IsLobbySlotOccupied( nLobbySlot ) )
		return L"";

	int nMarineResourceIndex = LobbySlotToMarineResourceIndex( nLobbySlot );
	C_ASW_Marine_Resource *pMR = ASWGameResource() ? ASWGameResource()->GetMarineResource( nMarineResourceIndex ) : NULL;
	if ( !pMR )
		return L"";

	static wchar_t wszMarineNameResult[ 32 ];	

	pMR->GetDisplayName( wszMarineNameResult, sizeof( wszMarineNameResult ) );
	return wszMarineNameResult;
}

const char* CASW_Briefing::GetPlayerNameForMarineProfile( int nProfileIndex )
{
	for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
	{
		C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
		if ( !pMR )
			continue;

		if ( pMR->GetProfileIndex() == nProfileIndex )
		{
			C_ASW_Player *pPlayer = pMR->GetCommander();
			if ( pPlayer )
			{
				return pPlayer->GetPlayerName();
			}
			break;
		}
	}
	return "";
}
#if !defined(NO_STEAM)
CSteamID CASW_Briefing::GetCommanderSteamID( int nLobbySlot )
{
	CSteamID invalid_result;
	if ( nLobbySlot < 0 || nLobbySlot >= NUM_BRIEFING_LOBBY_SLOTS )
		return invalid_result;

	UpdateLobbySlotMapping();

	C_ASW_Player *pPlayer = m_LobbySlotMapping[ nLobbySlot ].m_hPlayer.Get();
	if ( !pPlayer )
		return invalid_result;

	int iIndex = pPlayer->entindex();
	player_info_t pi;
	if ( engine->GetPlayerInfo(iIndex, &pi) )
	{
		if ( pi.friendsID )
		{
			CSteamID steamIDForPlayer( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
			return steamIDForPlayer;
		}
	}
	
	return invalid_result;
}
#endif

int CASW_Briefing::GetCommanderLevel( int nLobbySlot )
{
	if ( nLobbySlot < 0 || nLobbySlot >= NUM_BRIEFING_LOBBY_SLOTS )
		return -1;

	UpdateLobbySlotMapping();

	C_ASW_Player *pPlayer = m_LobbySlotMapping[ nLobbySlot ].m_hPlayer.Get();
	if ( !pPlayer )
		return -1;

	return pPlayer->GetLevel();
}

int CASW_Briefing::GetCommanderXP( int nLobbySlot )
{
	if ( nLobbySlot < 0 || nLobbySlot >= NUM_BRIEFING_LOBBY_SLOTS )
		return -1;

	UpdateLobbySlotMapping();

	C_ASW_Player *pPlayer = m_LobbySlotMapping[ nLobbySlot ].m_hPlayer.Get();
	if ( !pPlayer )
		return -1;

	return pPlayer->GetExperience();
}

int CASW_Briefing::GetCommanderPromotion( int nLobbySlot )
{
	if ( nLobbySlot < 0 || nLobbySlot >= NUM_BRIEFING_LOBBY_SLOTS )
		return -1;

	UpdateLobbySlotMapping();

	C_ASW_Player *pPlayer = m_LobbySlotMapping[ nLobbySlot ].m_hPlayer.Get();
	if ( !pPlayer )
		return -1;

	return pPlayer->GetPromotion();
}

ASW_Marine_Class CASW_Briefing::GetMarineClass( int nLobbySlot )
{	
	int nMarineResourceIndex = LobbySlotToMarineResourceIndex( nLobbySlot );
	C_ASW_Marine_Resource *pMR = ASWGameResource() ? ASWGameResource()->GetMarineResource( nMarineResourceIndex ) : NULL;
	if ( !pMR || !pMR->GetProfile() )
		return MARINE_CLASS_UNDEFINED;

	return pMR->GetProfile()->GetMarineClass();
}

CASW_Marine_Profile *CASW_Briefing::GetMarineProfile( int nLobbySlot )
{
	int nMarineResourceIndex = LobbySlotToMarineResourceIndex( nLobbySlot );
	C_ASW_Marine_Resource *pMR = ASWGameResource() ? ASWGameResource()->GetMarineResource( nMarineResourceIndex ) : NULL;
	if ( !pMR )
		return NULL;

	return pMR->GetProfile();
}

CASW_Marine_Profile *CASW_Briefing::GetMarineProfileByProfileIndex( int nProfileIndex )
{
	if ( !MarineProfileList() )
		return NULL;

	return MarineProfileList()->GetProfile( nProfileIndex );
}

int CASW_Briefing::GetProfileSelectedWeapon( int nProfileIndex, int nWeaponSlot )
{
	for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
	{
		C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
		if ( !pMR )
			continue;

		if ( pMR->GetProfileIndex() == nProfileIndex )
		{
			return pMR->m_iWeaponsInSlots[ nWeaponSlot ];
		}
	}
	return -1;
}

int CASW_Briefing::GetMarineSelectedWeapon( int nLobbySlot, int nWeaponSlot )
{
	int nMarineResourceIndex = LobbySlotToMarineResourceIndex( nLobbySlot );
	C_ASW_Marine_Resource *pMR = ASWGameResource() ? ASWGameResource()->GetMarineResource( nMarineResourceIndex ) : NULL;
	if ( !pMR || nWeaponSlot < 0 || nWeaponSlot >= ASW_NUM_INVENTORY_SLOTS )
	{
		return -1;
	}

	return pMR->m_iWeaponsInSlots[ nWeaponSlot ];
}

const char* CASW_Briefing::GetMarineWeaponClass( int nLobbySlot, int nWeaponSlot )
{
	int nMarineResourceIndex = LobbySlotToMarineResourceIndex( nLobbySlot );
	C_ASW_Marine_Resource *pMR = ASWGameResource() ? ASWGameResource()->GetMarineResource( nMarineResourceIndex ) : NULL;
	if ( !pMR || nWeaponSlot < 0 || nWeaponSlot >= ASW_NUM_INVENTORY_SLOTS || !ASWEquipmentList() )
	{
		return "";
	}

	CASW_EquipItem *pItem = ASWEquipmentList()->GetItemForSlot( nWeaponSlot, pMR->m_iWeaponsInSlots[ nWeaponSlot ] );
	if ( !pItem )
	{
		return "";
	}
	
	return STRING( pItem->m_EquipClass );
}

int CASW_Briefing::GetCommanderReady( int nLobbySlot )
{
	if ( nLobbySlot < 0 || nLobbySlot >= NUM_BRIEFING_LOBBY_SLOTS )
		return -1;

	UpdateLobbySlotMapping();

	C_ASW_Player *pPlayer = m_LobbySlotMapping[ nLobbySlot ].m_hPlayer.Get();
	if ( !pPlayer )
		return true;

	return ASWGameResource()->IsPlayerReady( pPlayer );
}

bool CASW_Briefing::IsLeader( int nLobbySlot )
{
	if ( nLobbySlot < 0 || nLobbySlot >= NUM_BRIEFING_LOBBY_SLOTS )
		return -1;

	UpdateLobbySlotMapping();

	C_ASW_Player *pPlayer = m_LobbySlotMapping[ nLobbySlot ].m_hPlayer.Get();
	if ( !pPlayer )
		return true;

	return ( pPlayer == ASWGameResource()->GetLeader() );
}

int CASW_Briefing::GetMarineSkillPoints( int nLobbySlot, int nSkillSlot )
{
	if ( nLobbySlot < 0 || nLobbySlot >= NUM_BRIEFING_LOBBY_SLOTS || !ASWGameResource() )
		return -1;

	int nMarineResourceIndex = LobbySlotToMarineResourceIndex( nLobbySlot );
	C_ASW_Marine_Resource *pMR = ASWGameResource() ? ASWGameResource()->GetMarineResource( nMarineResourceIndex ) : NULL;
	if ( !pMR )
		return -1;

	return GetProfileSkillPoints( pMR->GetProfileIndex(), nSkillSlot );
	
}
int CASW_Briefing::GetProfileSkillPoints( int nProfileIndex, int nSkillSlot )
{
	if ( !ASWGameResource() )
		return -1;

	return ASWGameResource()->GetMarineSkill( nProfileIndex, nSkillSlot );
}

void CASW_Briefing::AutoSelectFullSquadForSingleplayer( int nFirstSelectedProfileIndex )
{
	if ( !MarineProfileList() )
		return;

	CASW_Marine_Profile* pFirstSelectedProfile = MarineProfileList()->GetProfile( nFirstSelectedProfileIndex );
	if ( !pFirstSelectedProfile )
		return;

	ASW_Marine_Class nMarineClasses[]=
	{
		MARINE_CLASS_NCO, 
		MARINE_CLASS_SPECIAL_WEAPONS,
		MARINE_CLASS_MEDIC,
		MARINE_CLASS_TECH
	};

	// select one of each class
	for ( int i = 0; i < NELEMS( nMarineClasses ); i++ )
	{
		if ( nMarineClasses[ i ] == pFirstSelectedProfile->GetMarineClass() )
			continue;

		CASW_Marine_Profile* pProfile = NULL;
		for ( int p = 0; p < MarineProfileList()->m_NumProfiles; p++ )
		{
			pProfile = MarineProfileList()->GetProfile( p );
			if ( pProfile && pProfile->GetMarineClass() == nMarineClasses[i] )
			{
				break;
			}
		}

		if ( !pProfile )
			continue;

		SelectMarine( 0, pProfile->m_ProfileIndex, -1 );
	}
}

void CASW_Briefing::SelectMarine( int nOrder, int nProfileIndex, int nPreferredLobbySlot )
{
	// for now, just select him
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	if ( IsOfflineGame() )
	{
		pPlayer->RosterSelectMarineForSlot( nProfileIndex, nPreferredLobbySlot );
	}
	else
	{
		pPlayer->RosterSelectSingleMarine( nProfileIndex );
	}

	if ( gpGlobals->curtime - m_flLastSelectionChatterTime < 1.0f )
		return;

	CASW_Marine_Profile *pProfile = Briefing()->GetMarineProfileByProfileIndex( nProfileIndex );
	if ( pProfile )
	{
		char szSelectionSound[ CHATTER_STRING_SIZE ];
		V_snprintf( szSelectionSound, sizeof( szSelectionSound ), "%s%d", pProfile->m_Chatter[ CHATTER_SELECTION ], RandomInt( 0, pProfile->m_iChatterCount[ CHATTER_SELECTION ] - 1 ) );

		CSoundParameters params;
		if ( C_BaseEntity::GetParametersForSound( szSelectionSound, params, NULL ) )
		{
			EmitSound_t playparams( params );
			playparams.m_nChannel = CHAN_STATIC;
			playparams.m_bEmitCloseCaption = false;

			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, -1, playparams );

			m_flLastSelectionChatterTime = gpGlobals->curtime;
		}
	}
}

bool CASW_Briefing::IsWeaponUnlocked( const char *szWeaponClass )
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return true;

	return pPlayer->IsWeaponUnlocked( szWeaponClass );
}

void CASW_Briefing::SelectWeapon( int nProfileIndex, int nInventorySlot, int nEquipIndex )
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
	{
		C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
		if ( !pMR )
			continue;

		if ( pMR->GetProfileIndex() == nProfileIndex )
		{
			int nMarineResourceIndex = ASWGameResource()->GetIndexFor( pMR );
			pPlayer->LoadoutSelectEquip( nMarineResourceIndex, nInventorySlot, nEquipIndex );
			return;
		}
	}
}

void CASW_Briefing::ToggleLocalPlayerReady()
{
	engine->ClientCmd("cl_ready");
}

bool CASW_Briefing::CheckMissionRequirements()
{
	if ( ASWGameRules() && ASWGameRules()->GetGameState() < ASW_GS_DEBRIEF && ASWGameResource() )
	{	
		if ( ASWGameRules()->m_bMissionRequiresTech )
		{
			bool bTech = false;
			for (int i=0;i<ASWGameResource()->GetMaxMarineResources();i++)
			{
				C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(i);
				if (pMR && pMR->GetProfile() && pMR->GetProfile()->CanHack())
					bTech = true;
			}
			if (!bTech)
			{
				// have the server print a message about needing a tech, so all can see
				engine->ClientCmd("cl_needtech");
				return false;
			}
		}
		C_ASW_Equip_Req* pReq = C_ASW_Equip_Req::FindEquipReq();
		if (pReq)
		{
			if (pReq && !pReq->AreRequirementsMet())
			{
				// have the server print a message about needing equip, so all can see
				engine->ClientCmd("cl_needequip");
				return false;
			}
		}
		if ( !ASWGameResource()->AtLeastOneMarine() )
		{
			return false;
		}

		if ( ASWGameResource() && !asw_ignore_need_two_player_requirement.GetBool() )
		{
			CASW_Campaign_Info *pCampaign = ASWGameRules()->GetCampaignInfo();

			char mapname[64];
			V_FileBase( engine->GetLevelName(), mapname, sizeof( mapname ) );

			if ( pCampaign && pCampaign->GetMissionByMapName( mapname ) )
			{
				bool bNeedsMoreThanOneMarine = pCampaign->GetMissionByMapName( mapname )->m_bNeedsMoreThanOneMarine;
				if ( bNeedsMoreThanOneMarine )
				{
					// how many marines do we have?
					int numMarines = 0;
					for (int i=0;i<ASWGameResource()->GetMaxMarineResources();i++)
					{
						C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(i);
						if ( pMR && pMR->GetProfileIndex() >= 0 )
							numMarines++;
					}

					if ( numMarines < 2 )
					{
						engine->ClientCmd("cl_needtwoplayers");
						return false;
					}
				}
			}
		}
	}
	return true;
}

bool CASW_Briefing::AreOtherPlayersReady()
{
	if ( !ASWGameResource() )
		return false;

	C_ASW_Player *pLeader = ASWGameResource()->GetLeader();
	if ( !pLeader )
		return false;

	return ASWGameResource()->AreAllOtherPlayersReady( pLeader->entindex() );
}

void CASW_Briefing::StartMission()
{
	engine->ClientCmd("cl_start");
}

bool CASW_Briefing::IsProfileSelectedBySomeoneElse( int nProfileIndex )
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return false;
	
	for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
	{
		C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
		if ( !pMR )
			continue;

		if ( pMR->GetProfileIndex() == nProfileIndex )
		{
			return ( pMR->GetCommander() != pPlayer );
		}
	}

	return false;
}

bool CASW_Briefing::IsProfileSelected( int nProfileIndex )
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return false;

	for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
	{
		C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
		if ( !pMR )
			continue;

		if ( pMR->GetProfileIndex() == nProfileIndex )
		{
			return true;
		}
	}

	return false;
}

int CASW_Briefing::GetMaxPlayers()
{
	return gpGlobals->maxClients;
}

bool CASW_Briefing::IsOfflineGame()
{
	if ( !ASWGameResource() )
		return true;

	return ASWGameResource()->IsOfflineGame();
}

bool CASW_Briefing::IsCampaignGame()
{
	return ASWGameResource() && ASWGameResource()->IsCampaignGame();
}

bool CASW_Briefing::UsingFixedSkillPoints()
{
	if ( !IsCampaignGame() || !ASWGameRules() || !ASWGameRules()->GetCampaignSave() )
	{
		return false;
	}
	return ASWGameRules()->GetCampaignSave()->UsingFixedSkillPoints();
}

void CASW_Briefing::SetChangingWeaponSlot( int nWeaponSlot )
{
	engine->ClientCmd( VarArgs( "cl_editing_slot %d", nWeaponSlot ) );
}

int CASW_Briefing::GetChangingWeaponSlot( int nLobbySlot )
{
	UpdateLobbySlotMapping();

	C_ASW_Player *pPlayer = m_LobbySlotMapping[ nLobbySlot ].m_hPlayer.Get();
	if ( !pPlayer )
		return 0;

	return pPlayer->m_nChangingSlot.Get();
}

bool CASW_Briefing::IsCommanderSpeaking( int nLobbySlot )
{
	if ( gpGlobals->maxClients <= 1 )
		return false;

	UpdateLobbySlotMapping();

	C_ASW_Player *pPlayer = m_LobbySlotMapping[ nLobbySlot ].m_hPlayer.Get();
	if ( !pPlayer )
		return false;

	CVoiceStatus *pVoiceMgr = GetClientVoiceMgr();	
	if ( !pVoiceMgr )
		return false;

	int index = pPlayer->entindex();
	bool bTalking = false;
	if ( pPlayer == C_ASW_Player::GetLocalASWPlayer() )
	{
		bTalking = pVoiceMgr->IsLocalPlayerSpeakingAboveThreshold( FirstValidSplitScreenSlot() );
	}
	else
	{
		bTalking = pVoiceMgr->IsPlayerSpeaking( index );
	}
	return bTalking;
}