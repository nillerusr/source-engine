//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_CLASS_COMMANDO_H
#define TF_CLASS_COMMANDO_H

#ifdef _WIN32
#pragma once
#endif

#include "tf_playerclass.h"
#include "TFClassData_Shared.h"
#include "basetfcombatweapon_shared.h"

//=====================================================================
// Commando
class CPlayerClassCommando : public CPlayerClass
{
	DECLARE_CLASS( CPlayerClassCommando, CPlayerClass );
public:
	CPlayerClassCommando( CBaseTFPlayer *pPlayer, TFClass iClass  );
	virtual ~CPlayerClassCommando();

	virtual void	ClassActivate( void );
	virtual void	ClassDeactivate( void );

	virtual const char*	GetClassModelString( int nTeam );

	// Class Initialization
	virtual void	CreateClass( void );			// Create the class upon initial spawn
	virtual void	RespawnClass( void );			// Called upon all respawns
	virtual bool	ResupplyAmmo( float flPercentage, ResupplyReason_t reason );
	virtual void	SetupMoveData( void );			// Override class specific movement data here.
	virtual void	SetupSizeData( void );			// Override class specific size data here.
	virtual void	ResetViewOffset( void );

	// Should we take damage-based force?
	virtual bool ShouldApplyDamageForce( const CTakeDamageInfo &info );

	PlayerClassCommandoData_t *GetClassData( void ) { return &m_ClassData; }

	// Class Abilities
	virtual void	ClassThink( void );

	// Resources
	int				ClassCostAdjustment( ResupplyBuyType_t nType );

	// Objects
	virtual int		CanBuild( int iObjectType );
	virtual void	FinishedObject( CBaseObject *pObject );

	virtual bool	ClientCommand( const CCommand &args );
	virtual void	GainedNewTechnology( CBaseTechnology *pTechnology );

	// Adrenalin Rush
	virtual void	CalculateRush( void );
	virtual void	StartAdrenalinRush( void );

	// Automatic Melee Attack
	virtual void	Boot( CBaseTFPlayer *pTarget );

	// Hooks
	virtual void	SetPlayerHull( void );
	virtual void	GetPlayerHull( bool bDucking, Vector &vecMin, Vector &vecMax );

	// Orders.
	virtual void	CreatePersonalOrder( void );

	// Bull Rush.
	bool			InBullRush( void );
	bool			CanBullRush( void );
	void			BullRushTouch( CBaseEntity *pTouched );

	CNetworkVarEmbedded( PlayerClassCommandoData_t, m_ClassData );

	// Player physics shadow.
	void			InitVCollision( void );

	// Vehicle
	bool			CanGetInVehicle( void );

protected:
	// BullRush
	void			PreBullRush( void );
	void			PostBullRush( void );

protected:
	// Adrenalin Rush
	bool						m_bCanRush;				// True if he has the ability to rush
	bool						m_bPersonalRush;		// True if this he started his current rush, or outside effect
	bool						m_bHasBattlecry;		// True if he has the ability to battlecry

	// Weapons
	CHandle<CBaseTFCombatWeapon> m_hWpnPlasma;
//	CHandle<CBaseTFCombatWeapon> m_hWpnGrenade;

	// Automatic Melee Attack
	bool						m_bCanBoot;				// True if he has the ability to boot
	float						m_flNextBootCheck;		// Time at which to recheck for the automatic melee attack

	bool						m_bOldBullRush;

	CUtlVector<CBaseTFPlayer*>	m_aHitPlayers;			// Player I have hit during this bullrush.
};

EXTERN_SEND_TABLE( DT_PlayerClassCommandoData )

#endif // TF_CLASS_COMMANDO_H
