//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================//

#ifndef HL1_PLAYER_H
#define HL1_PLAYER_H
#pragma once


#include "player.h"

extern int TrainSpeed(int iSpeed, int iMax);
extern void CopyToBodyQue( CBaseAnimating *pCorpse );

enum HL1PlayerPhysFlag_e
{
	// 1 -- 5 are used by enum PlayerPhysFlag_e in player.h

	PFLAG_ONBARNACLE	= ( 1<<6 )		// player is hangning from the barnalce
};

class IPhysicsPlayerController;


//=============================================================================
//=============================================================================
class CSuitPowerDevice
{
public:
	CSuitPowerDevice( int bitsID, float flDrainRate ) { m_bitsDeviceID = bitsID; m_flDrainRate = flDrainRate; }
private:
	int		m_bitsDeviceID;	// tells what the device is. DEVICE_SPRINT, DEVICE_FLASHLIGHT, etc. BITMASK!!!!!
	float	m_flDrainRate;	// how quickly does this device deplete suit power? ( percent per second )

public:
	int		GetDeviceID( void ) const { return m_bitsDeviceID; }
	float	GetDeviceDrainRate( void ) const { return m_flDrainRate; }
};

//=============================================================================
// >> HL1_PLAYER
//=============================================================================
class CHL1_Player : public CBasePlayer
{
	DECLARE_CLASS( CHL1_Player, CBasePlayer );
	DECLARE_SERVERCLASS();
public:

	DECLARE_DATADESC();

	CHL1_Player();
	~CHL1_Player( void );

	static CHL1_Player *CreatePlayer( const char *className, edict_t *ed )
	{
		CHL1_Player::s_PlayerEdict = ed;
		return (CHL1_Player*)CreateEntityByName( className );
	}

	void		CreateCorpse( void ) { CopyToBodyQue( this ); };

	void		Precache( void );
	void		Spawn(void);
	void		Event_Killed( const CTakeDamageInfo &info );
	void		CheatImpulseCommands( int iImpulse );
	void		PlayerRunCommand( CUserCmd *ucmd, IMoveHelper *moveHelper );
	void		UpdateClientData( void );
	void		OnSave( IEntitySaveUtils *pUtils );
	
	void		CheckTimeBasedDamage( void );

	// from cbasecombatcharacter
	void		InitVCollision( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity );

	Class_T		Classify ( void );
	Class_T		m_nControlClass;			// Class when player is controlling another entity

	// from CBasePlayer
	void				SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize );

	// Aiming heuristics accessors
	float		GetIdleTime( void ) const { return ( m_flIdleTime - m_flMoveTime ); }
	float		GetMoveTime( void ) const { return ( m_flMoveTime - m_flIdleTime ); }
	float		GetLastDamageTime( void ) const { return m_flLastDamageTime; }
	bool		IsDucking( void ) const { return !!( GetFlags() & FL_DUCKING ); }

	int			OnTakeDamage( const CTakeDamageInfo &info );
	int			OnTakeDamage_Alive( const CTakeDamageInfo &info );
	void		FindMissTargets( void );
	bool		GetMissPosition( Vector *position );

	void		OnDamagedByExplosion( const CTakeDamageInfo &info ) { };
	void		PlayerPickupObject( CBasePlayer *pPlayer, CBaseEntity *pObject );

	virtual void CreateViewModel( int index /*=0*/ );

	virtual CBaseEntity	*GiveNamedItem( const char *pszName, int iSubType = 0 );

	virtual void OnRestore( void );

	bool		IsPullingObject() { return m_bIsPullingObject; }
	void		StartPullingObject( CBaseEntity *pObject );
	void		StopPullingObject();
	void		UpdatePullingObject();


protected:
	void		PreThink( void );
	bool		HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt);

private:
	Vector		m_vecMissPositions[16];
	int			m_nNumMissPositions;

	// Aiming heuristics code
	float		m_flIdleTime;		//Amount of time we've been motionless
	float		m_flMoveTime;		//Amount of time we've been in motion
	float		m_flLastDamageTime;	//Last time we took damage
	float		m_flTargetFindTime;

	EHANDLE					m_hPullObject;
	IPhysicsConstraint		*m_pPullConstraint;


public:

	// Flashlight Device
	int			FlashlightIsOn( void );
	void		FlashlightTurnOn( void );
	void		FlashlightTurnOff( void );
	float		m_flFlashLightTime;			// Time until next battery draw/Recharge
	CNetworkVar( int, m_nFlashBattery );	// Flashlight Battery Draw

	// For gauss weapon
//	float		m_flStartCharge;
//	float		m_flAmmoStartCharge;
//	float		m_flPlayAftershock;
//	float		m_flNextAmmoBurn;	// while charging, when to absorb another unit of player's ammo?

	CNetworkVar( float, m_flStartCharge );
	CNetworkVar( float, m_flAmmoStartCharge );
	CNetworkVar( float, m_flPlayAftershock );
	CNetworkVar( float, m_flNextAmmoBurn );	// while charging, when to absorb another unit of player's ammo?

	CNetworkVar( bool, m_bHasLongJump );
	CNetworkVar( bool, m_bIsPullingObject );
};


//-----------------------------------------------------------------------------
// Converts an entity to a HL1 player
//-----------------------------------------------------------------------------
inline CHL1_Player *ToHL1Player( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;
#if _DEBUG
	return dynamic_cast<CHL1_Player *>( pEntity );
#else
	return static_cast<CHL1_Player *>( pEntity );
#endif
}

#endif	//HL1_PLAYER_H
