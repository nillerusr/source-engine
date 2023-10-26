#ifndef _INCLUDED_ASW_MARINE_RESOURCE_H
#define _INCLUDED_ASW_MARINE_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "asw_shareddefs.h"

class CASW_Marine_Profile;
class CASW_Marine;
class CASW_Player;
class CASW_Weapon;
class CASW_Door;

// This class holds information about a particular ingame marine
// most of the data is set on the server and sent to the clients

class CASW_Marine_Profile;
class CASW_Intensity;

class CASW_Marine_Resource : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Marine_Resource, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();	

	CASW_Marine_Resource();
	virtual ~CASW_Marine_Resource();

	CNetworkVar( int,  m_MarineProfileIndex );	

	CNetworkHandle (CASW_Marine, m_MarineEntity); 	// the actual marine
	CNetworkHandle (CASW_Player, m_Commander);		// the player in charge of this marine
	CNetworkVar( int, m_iCommanderIndex );

	// indices into the equipment list for currently selected equipment
	CNetworkArray( int, m_iWeaponsInSlots, ASW_MAX_EQUIP_SLOTS );
	int m_iInitialWeaponsInSlots[ ASW_MAX_EQUIP_SLOTS ];

	CNetworkVar( bool, m_bInfested );
	CNetworkVar( bool, m_bInhabited );
	CNetworkVar( bool, m_bTakenWoundDamage );
	CNetworkVar( bool, m_bHealthHalved );		
	
	CNetworkVar( int, m_iServerFiring );

	// stats
	int m_iShotsFired;	
	int m_iShotsMissed;
	int m_iPlayerShotsFired;	// NOTE: player shots are not increased if you hit a junk item (so accuracy is only based on alien hits and misses)
	int m_iPlayerShotsMissed;
	//CNetworkVar( float, m_fDamageTaken );
	float m_fDamageTaken;
	CNetworkVar( int, m_iAliensKilled );
	int m_iAliensKilledByBouncingBullets;
	int m_iAliensKilledSinceLastFriendlyFireIncident;
	bool m_bAwardedFFPartialAchievement;
	bool m_bAwardedHealBeaconAchievement;
	bool m_bDealtNonMeleeDamage;
	Vector m_vecOutOfAmmoSpot;
	int m_iHealCount;	// how many times this marine received a heal
	int m_iDamageTakenDuringHack;

	CNetworkVarEmbedded( CTimeline, m_TimelineFriendlyFire );
	CNetworkVarEmbedded( CTimeline, m_TimelineKillsTotal );
	CNetworkVarEmbedded( CTimeline, m_TimelineHealth );
	CNetworkVarEmbedded( CTimeline, m_TimelineAmmo );
	CNetworkVarEmbedded( CTimeline, m_TimelinePosX );
	CNetworkVarEmbedded( CTimeline, m_TimelinePosY );

	// skills
	CNetworkArray( int, m_index, ASW_SCANNER_MAX_BLIPS );

	virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo );
	void SetCommander(CASW_Player* commander);
	CASW_Player* GetCommander();

	void GetDisplayName( char *pwchDisplayName, int nMaxBytes );

	void SetMarineEntity(CASW_Marine* marine);
	CASW_Marine* GetMarineEntity();
	void SetProfileIndex(int ProfileIndex);
	int GetProfileIndex();
	CASW_Marine_Profile* GetProfile();
	bool IsInfested() { return m_bInfested; }
	void SetInfested(bool b) { m_bInfested = b; }
	bool IsInhabited();
	void SetInhabited(bool bInhabited);
	void UpdateWeaponIndices();
	float GetHealthPercent();
	bool IsFiring();
	void SetFiring(int iFiring) { m_iServerFiring = iFiring; }
	bool IsReloading();

	// leadership effects
	float OnFired_GetDamageScale();	// called whenever a weapon is fired.  Leadership and damage amp scaling is done here
	int m_iLeadershipCount;
	Vector m_vecDeathPosition;	// position of the marine when he died
	float m_fDeathTime;

	// medal stats collection
	void DebugMedalStats();
	void DebugMedalStatsOnScreen();
	float m_fFriendlyFireDamageDealt;	 // how much FF damage this marine dealt out
	int m_iSingleGrenadeKills;	// our max number of aliens killed with a single grenade this mission
	int m_iKickedGrenadeKills;	// how many aliens were killed by a kicked grenade
	int m_iShieldbugsKilled;
	int m_iParasitesKilled;
	int m_iSavedLife;	// has this marine killed an alien that was munching on a teammate?
	bool m_bHurt;	// has this marine been hurt at all?
	int m_iMadFiring;
	int m_iMadFiringAutogun;
	//int m_iLastStandKills;	// alien kills made while the NCO marine is bleeding to death
	int m_iMedicHealing;	// how much healing this medic has done (minus healing of FF damage)
	int m_iCuredInfestation;	// how many marines have survived infestation after a cure from us (not a heal)

	int m_iOnlyWeaponEquipIndex;	// equip index of the only weapon we've used (-1 means not used a weapon yet, -2 means we've used multiple weapons)
	bool m_bOnlyWeaponExtra;		// was the only weapon we've used an extra item?
	void UsedWeapon(CASW_Weapon *pWeapon, int iShots);
	void UsedWeapon(int iWeaponEquipIndex, bool bWeaponExtra, int iShots);
	int m_iMineKills;
	int m_iSentryKills;
	int m_iEggKills;
	int m_iGrubKills;
	int m_iBarrelKills;
	int m_iFiresPutOut;
	bool m_bDidFastReloadsInARow;
	bool m_bDamageAmpMedal;
	bool m_bKilledBoomerEarly;
	bool m_bStunGrenadeMedal;
	bool m_bFreezeGrenadeMedal;
	bool m_bDodgedRanger;
	bool m_bProtectedTech;
	bool m_bMeleeParasiteKill;
	int m_iAliensKicked;
	int m_iFastDoorHacks;
	int m_iFastComputerHacks;
	CNetworkHandle(CASW_Door, m_hWeldingDoor);	// networks down which door this marine tried to cut/seal last, so all players can see its progress on the HUD

	// medals awarded during the debrief
	CNetworkString( m_MedalsAwarded, 255 );
	bool m_bAwardedMedals;
	CUtlVector<int>	m_aAchievementsEarned;

	// char array of medals awarded (used for stats upload)
	CUtlVector<unsigned char> m_CharMedals;

	CNetworkVar(bool, m_bUsingEngineeringAura);

	// Intensity
	CASW_Intensity* GetIntensity() { return m_pIntensity; }

private:
	CASW_Intensity* m_pIntensity;
};

#endif /* _INCLUDED_ASW_MARINE_RESOURCE_H */