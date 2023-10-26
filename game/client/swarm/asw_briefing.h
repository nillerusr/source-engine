#ifndef _INCLUDED_ASW_BRIEFING_H
#define _INCLUDED_ASW_BRIEFING_H
#ifdef _WIN32
#pragma once
#endif

#include "ibriefing.h"

class C_ASW_Player;
class C_ASW_Marine_Resource;

struct LobbySlotMapping_t
{
	int m_nPlayerEntIndex;
	CHandle<C_ASW_Player> m_hPlayer;
	CHandle<C_ASW_Marine_Resource> m_hMR;
	int m_nMarineResourceIndex;
};

class CASW_Briefing : public IBriefing
{
public:
	CASW_Briefing();
	~CASW_Briefing();

	virtual const char* GetLeaderName();

	virtual bool IsLocalPlayerLeader();
	virtual void ToggleLocalPlayerReady();
	virtual bool CheckMissionRequirements();		// do we have all the required classes/equips to start the mission?
	virtual bool AreOtherPlayersReady();		// is everyone except the leader ready?
	virtual void StartMission();
	virtual bool IsLobbySlotOccupied( int nLobbySlot );
	virtual bool IsLobbySlotLocal( int nLobbySlot );
	virtual bool IsLobbySlotBot( int nLobbySlot );
	virtual wchar_t* GetMarineOrPlayerName( int nLobbySlot );
	virtual wchar_t* GetMarineName( int nLobbySlot );		// always returns the marine's profile name
	virtual const char* GetPlayerNameForMarineProfile( int nProfileIndex );
	virtual int GetCommanderLevel( int nLobbySlot );
	virtual int GetCommanderXP( int nLobbySlot );
	virtual int GetCommanderPromotion( int nLobbySlot );
#if !defined(NO_STEAM)
	CSteamID GetCommanderSteamID( int nLobbySlot );
#endif
	virtual ASW_Marine_Class GetMarineClass( int nLobbySlot );
	virtual CASW_Marine_Profile *GetMarineProfile( int nLobbySlot );
	virtual CASW_Marine_Profile *GetMarineProfileByProfileIndex( int nProfileIndex );
	virtual int GetProfileSelectedWeapon( int nProfileIndex, int nWeaponSlot );
	virtual int GetMarineSelectedWeapon( int nLobbySlot, int nWeaponSlot );
	virtual const char* GetMarineWeaponClass( int nLobbySlot, int nWeaponSlot );
	virtual int GetCommanderReady( int nLobbySlot );
	virtual bool IsLeader( int nLobbySlot );
	virtual int GetMarineSkillPoints( int nLobbySlot, int nSkillSlot );
	virtual int GetProfileSkillPoints( int nProfileIndex, int nSkillSlot );
	virtual bool IsWeaponUnlocked( const char *szWeaponClass );
	virtual bool IsProfileSelectedBySomeoneElse( int nProfileIndex );
	virtual bool IsProfileSelected( int nProfileIndex );
	virtual int GetMaxPlayers();
	virtual bool IsOfflineGame();
	virtual bool IsCampaignGame();
	virtual bool UsingFixedSkillPoints();
	virtual void SetChangingWeaponSlot( int nWeaponSlot );
	virtual int GetChangingWeaponSlot( int nLobbySlot );
	virtual bool IsCommanderSpeaking( int nLobbySlot );

	virtual void SelectMarine( int nOrder, int nProfileIndex, int nPreferredLobbySlot );
	virtual void SelectWeapon( int nProfileIndex, int nInventorySlot, int nEquipIndex );
	virtual void AutoSelectFullSquadForSingleplayer( int nFirstSelectedProfileIndex );

	virtual void ResetLastChatterTime() { m_flLastSelectionChatterTime = 0.0f; }

	int LobbySlotToMarineResourceIndex( int nLobbySlot );
	void UpdateLobbySlotMapping();

	int m_nLastLobbySlotMappingFrame;
	LobbySlotMapping_t m_LobbySlotMapping[ NUM_BRIEFING_LOBBY_SLOTS ];

	float m_flLastSelectionChatterTime;
};

IBriefing *Briefing();

#endif // _INCLUDED_ASW_BRIEFING_H