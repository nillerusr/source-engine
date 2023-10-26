#ifndef _INCLUDED_IBRIEFING_H
#define _INCLUDED_IBRIEFING_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"

class CASW_Marine_Profile;

// interface for getting briefing state
abstract_class IBriefing
{
public:
	virtual const char* GetLeaderName() = 0;

	virtual bool IsLocalPlayerLeader() = 0;
	virtual void ToggleLocalPlayerReady() = 0;
	virtual bool CheckMissionRequirements() = 0;		// do we have all the required classes/equips to start the mission?
	virtual bool AreOtherPlayersReady() = 0;		// is everyone except the leader ready?
	virtual void StartMission() = 0;
	virtual bool IsLobbySlotOccupied( int nLobbySlot ) = 0;
	virtual bool IsLobbySlotLocal( int nLobbySlot ) = 0;
	virtual bool IsLobbySlotBot( int nLobbySlot ) = 0;
	virtual wchar_t* GetMarineOrPlayerName( int nLobbySlot ) = 0;
	virtual wchar_t* GetMarineName( int nLobbySlot ) = 0;		// always returns the marine's profile name
	virtual const char* GetPlayerNameForMarineProfile( int nProfileIndex ) = 0;
	virtual int GetCommanderLevel( int nLobbySlot ) = 0;
	virtual int GetCommanderXP( int nLobbySlot ) = 0;
	virtual int GetCommanderPromotion( int nLobbySlot ) = 0;
#if !defined(NO_STEAM)
	virtual CSteamID GetCommanderSteamID( int nLobbySlot ) = 0;
#endif
	virtual ASW_Marine_Class GetMarineClass( int nLobbySlot ) = 0;
	virtual CASW_Marine_Profile *GetMarineProfile( int nLobbySlot ) = 0;
	virtual CASW_Marine_Profile *GetMarineProfileByProfileIndex( int nProfileIndex ) = 0;
	virtual int GetProfileSelectedWeapon( int nProfileIndex, int nWeaponSlot ) = 0;
	virtual int GetMarineSelectedWeapon( int nLobbySlot, int nWeaponSlot ) = 0;
	virtual const char* GetMarineWeaponClass( int nLobbySlot, int nWeaponSlot ) = 0;
	virtual int GetCommanderReady( int nLobbySlot ) = 0;
	virtual bool IsLeader( int nLobbySlot ) = 0;
	virtual int GetMarineSkillPoints( int nLobbySlot, int nSkillSlot ) = 0;
	virtual int GetProfileSkillPoints( int nProfileIndex, int nSkillSlot ) = 0;
	virtual bool IsWeaponUnlocked( const char *szWeaponClass ) = 0;
	virtual bool IsProfileSelectedBySomeoneElse( int nProfileIndex ) = 0;
	virtual bool IsProfileSelected( int nProfileIndex ) = 0;
	virtual int GetMaxPlayers() = 0;
	virtual bool IsOfflineGame() = 0;
	virtual bool IsCampaignGame() = 0;
	virtual bool UsingFixedSkillPoints() = 0;
	virtual void SetChangingWeaponSlot( int nWeaponSlot ) = 0;
	virtual int GetChangingWeaponSlot( int nLobbySlot ) = 0;
	virtual bool IsCommanderSpeaking( int nLobbySlot ) = 0;

	virtual void SelectMarine( int nOrder, int nProfileIndex, int nPreferredLobbySlot ) = 0;
	virtual void SelectWeapon( int nProfileIndex, int nInventorySlot, int nEquipIndex ) = 0;
	virtual void AutoSelectFullSquadForSingleplayer( int nFirstSelectedProfileIndex ) = 0;


	virtual void ResetLastChatterTime() = 0;
};

#define NUM_BRIEFING_LOBBY_SLOTS 4

#endif // _INCLUDED_IBRIEFING_H