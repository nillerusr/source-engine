#ifndef _INCLUDED_C_ASW_MARINE_RESOURCE_H
#define _INCLUDED_C_ASW_MARINE_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseentity.h"	
#include "c_asw_player.h"

class CASW_Marine_Profile;
class C_ASW_Marine;
class C_ASW_Door;

// This class holds information about a particular ingame marine
// most of the data is set on the server and sent to the clients

class C_ASW_Marine_Resource : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ASW_Marine_Resource, C_BaseEntity );
	DECLARE_CLIENTCLASS();

					C_ASW_Marine_Resource();
	virtual			~C_ASW_Marine_Resource();

	CASW_Marine_Profile* GetProfile(void);
	int GetProfileIndex() {	return m_MarineProfileIndex; }
	int	m_MarineProfileIndex;
	C_ASW_Marine* GetMarineEntity();
	C_ASW_Player* GetCommander();
	int GetCommanderIndex() { return m_iCommanderIndex; }

	void GetDisplayName( char *pchDisplayName, int nMaxBytes );
	void GetDisplayName( wchar_t *pwchDisplayName, int nMaxBytes );

	int m_iWeaponsInSlots[ ASW_MAX_EQUIP_SLOTS ];	// index of equipment selected in loadout for primary inventory slot

	// stats
	float m_fDamageTaken;
	int m_iAliensKilled;

	CNetworkVarEmbedded( CTimeline, m_TimelineFriendlyFire );
	CNetworkVarEmbedded( CTimeline, m_TimelineKillsTotal );
	CNetworkVarEmbedded( CTimeline, m_TimelineHealth );
	CNetworkVarEmbedded( CTimeline, m_TimelineAmmo );
	CNetworkVarEmbedded( CTimeline, m_TimelinePosX );
	CNetworkVarEmbedded( CTimeline, m_TimelinePosY );

	bool m_bTakenWoundDamage;
	CNetworkVar( bool, m_bHealthHalved );	

	bool IsInfested() { return m_bInfested; }
	bool m_bInfested;
	float GetInfestedPercent();

	bool IsInhabited() { return m_bInhabited; }
	bool m_bInhabited;

	// is this marine's commander local to my client
	inline bool IsLocal() { return GetCommander() && GetCommander()->IsLocalPlayer(); }
	
	bool IsFiring();
	int m_iServerFiring;	// server's opinion on if we're firing or not
	float GetFiringTimer();
	float m_fFiring;	// countdown for firing
	bool IsReloading();

	// info accessors used by the HUD portrait bars  ('percent' is a bad term here, they return a number from 0 to 1, as a fraction of the total health/ammo/whatever)	
	float GetHealthPercent();
	float GetAmmoPercent();
	float GetClipsPercent();
	bool IsLowAmmo() { return (GetAmmoPercent() <= 0.2f); }
	
	float m_fScannerTime;	// if we're a tech marine, this stores the status of our ring for the local player
	bool m_bPlayedBlipSound;
	int m_iScannerSoundSkip;

	void OnDataChanged(DataUpdateType_t updateType);
	void ClientThink();

	// used for flashing the marine portraits red
	float GetHurtPulse() { return m_fHurtPulse; }
	float m_fLastHealthPercent;
	float m_fHurtPulse;

	EHANDLE m_MarineEntity;
	CNetworkHandle( C_ASW_Player, m_Commander );
	CNetworkVar( int, m_iCommanderIndex );

	// leadership effect bonus
	void UpdateLeadershipBonus();
	float GetLeadershipResist() { return m_fLeadershipResist; }	// chance of nearby leadership skill reducing damage
	float m_fLeadershipResist;
	float m_fNextLeadershipTest;

	// counting medical charges
	float GetMedsPercent();
	float m_fCachedMedsPercent;
	float m_fNextMedsCountTime;

	// medals
	char		m_MedalsAwarded[255];

	CNetworkHandle(C_ASW_Door, m_hWeldingDoor);	// networks down which door this marine tried to cut/seal last, so all players can see its progress on the HUD

	CNetworkVar(bool, m_bUsingEngineeringAura);	

private:
	C_ASW_Marine_Resource( const C_ASW_Marine_Resource & ); // not defined, not accessible
};

#endif /* _INCLUDED_C_ASW_MARINE_RESOURCE_H */