#ifndef _INCLUDED_ASW_DEBRIEF_STATS_H
#define _INCLUDED_ASW_DEBRIEF_STATS_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"

// this class networks marine and team stats to show to players during the debriefing

class CASW_Debrief_Stats : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Debrief_Stats, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Debrief_Stats();
	virtual ~CASW_Debrief_Stats();
	void Spawn( void );	
	
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo );
	int UpdateTransmitState();

	int GetSkillPointsAwarded(int iProfileIndex);

	int GetKills(int iMarineIndex) { return m_iKills[iMarineIndex]; }
	float GetAccuracy(int iMarineIndex) { return m_fAccuracy[iMarineIndex]; }
	int GetFriendlyFire(int iMarineIndex) { return m_iFF[iMarineIndex]; }
	int GetDamageTaken(int iMarineIndex) { return m_iDamage[iMarineIndex]; }
	int GetShotsFired(int iMarineIndex) { return m_iShotsFired[iMarineIndex]; }
	int GetShotsHit(int iMarineIndex) { return m_iShotsHit[iMarineIndex]; }
	int GetAliensBurned(int iMarineIndex) { return m_iAliensBurned[iMarineIndex]; }
	int GetHealthHealed(int iMarineIndex) { return m_iHealthHealed[iMarineIndex]; }
	int GetFastHacks(int iMarineIndex) { return m_iFastHacks[iMarineIndex]; }
	bool IsWounded(int iMarineIndex) { return (m_iWounded[iMarineIndex] > 0); }
	int	GetStartingPrimaryEquip( int iMarineIndex ) { return m_iStartingEquip0[iMarineIndex]; }
	int	GetStartingSecondaryEquip( int iMarineIndex ) { return m_iStartingEquip1[iMarineIndex]; }
	int	GetStartingExtraEquip( int iMarineIndex ) { return m_iStartingEquip2[iMarineIndex]; }

	int GetTotalKills() { return m_iTotalKills; }
	float GetTimeTaken() { return m_fTimeTaken; }

	int GetBestKills() { return m_iBestKills; }
	float GetBestTime() { return m_fBestTimeTaken; }
	int GetSpeedrunTime() { return m_iSpeedrunTime; }
	void SetWeaponStats( int nMarineIndex, int nWeaponIndex, int nWeaponClass, int nDamage, int nFFDamage, int nShotsFired, int nShotsHit, int nKills );
	
	// per marine
	CNetworkArray( int,		m_iKills,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( float,	m_fAccuracy,	ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iFF,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iDamage,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iShotsFired,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iShotsHit,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iWounded,		ASW_MAX_MARINE_RESOURCES );	
	CNetworkArray( int,		m_iAliensBurned,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iHealthHealed,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iFastHacks,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iSkillPointsAwarded,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iStartingEquip0,			ASW_MAX_MARINE_RESOURCES );	
	CNetworkArray( int,		m_iStartingEquip1,			ASW_MAX_MARINE_RESOURCES );	
	CNetworkArray( int,		m_iStartingEquip2,			ASW_MAX_MARINE_RESOURCES );	
	CNetworkArray( int,		m_iAmmoDeployed,			ASW_MAX_MARINE_RESOURCES );	
	CNetworkArray( int,		m_iSentryGunsDeployed,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iSentryFlamerDeployed,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iSentryFreezeDeployed,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iSentryCannonDeployed,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iMedkitsUsed,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iFlaresUsed,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iAdrenalineUsed,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iTeslaTrapsDeployed,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iFreezeGrenadesThrown,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iElectricArmorUsed,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iHealGunHeals,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iHealBeaconHeals,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iHealGunHeals_Self,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iHealBeaconHeals_Self,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iDamageAmpsUsed,			ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( int,		m_iHealBeaconsDeployed,			ASW_MAX_MARINE_RESOURCES );

	// Weapon stats for the marine (8 weapons max)
	CNetworkArray( unsigned int,		m_iWeaponClassAndKills0,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iDamageAndFF0,				ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iShotsFiredAndHit0,			ASW_MAX_MARINE_RESOURCES );

	CNetworkArray( unsigned int,		m_iWeaponClassAndKills1,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iDamageAndFF1,				ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iShotsFiredAndHit1,			ASW_MAX_MARINE_RESOURCES );

	CNetworkArray( unsigned int,		m_iWeaponClassAndKills2,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iDamageAndFF2,				ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iShotsFiredAndHit2,			ASW_MAX_MARINE_RESOURCES );

	CNetworkArray( unsigned int,		m_iWeaponClassAndKills3,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iDamageAndFF3,				ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iShotsFiredAndHit3,			ASW_MAX_MARINE_RESOURCES );

	CNetworkArray( unsigned int,		m_iWeaponClassAndKills4,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iDamageAndFF4,				ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iShotsFiredAndHit4,			ASW_MAX_MARINE_RESOURCES );

	CNetworkArray( unsigned int,		m_iWeaponClassAndKills5,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iDamageAndFF5,				ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iShotsFiredAndHit5,			ASW_MAX_MARINE_RESOURCES );

	CNetworkArray( unsigned int,		m_iWeaponClassAndKills6,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iDamageAndFF6,				ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iShotsFiredAndHit6,			ASW_MAX_MARINE_RESOURCES );

	CNetworkArray( unsigned int,		m_iWeaponClassAndKills7,		ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iDamageAndFF7,				ASW_MAX_MARINE_RESOURCES );
	CNetworkArray( unsigned int,		m_iShotsFiredAndHit7,			ASW_MAX_MARINE_RESOURCES );

	// for the team
	CNetworkVar( float,		m_fTimeTaken );
	CNetworkVar( int,		m_iTotalKills );
	CNetworkVar( int,		m_iEggKills );
	CNetworkVar( int,		m_iParasiteKills );
	CNetworkVar( int,		m_iDroneKills );
	CNetworkVar( int,		m_iShieldbugKills );

	// debrief text
	CNetworkString( m_DebriefText1, 255 );
	CNetworkString( m_DebriefText2, 255 );
	CNetworkString( m_DebriefText3, 255 );

	CNetworkVar( bool, m_bJustUnlockedCarnage );
	CNetworkVar( bool, m_bJustUnlockedUber );
	CNetworkVar( bool, m_bJustUnlockedHardcore );
	CNetworkVar( bool, m_bBeatSpeedrunTime );

	CNetworkVar( float, m_fBestTimeTaken );
	CNetworkVar( int, m_iBestKills );
	CNetworkVar( int, m_iSpeedrunTime );	
};

CASW_Debrief_Stats* GetDebriefStats();

#endif /* _INCLUDED_ASW_DEBRIEF_STATS_H */