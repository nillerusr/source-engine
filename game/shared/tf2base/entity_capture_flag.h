//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: CTF Flag.
//
//=============================================================================//
#ifndef ENTITY_CAPTURE_FLAG_H
#define ENTITY_CAPTURE_FLAG_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_item.h"

#ifdef CLIENT_DLL
#define CCaptureFlag C_CaptureFlag
#endif

#define TF_FLAG_THINK_TIME			1.0f
#define	TF_FLAG_OWNER_PICKUP_TIME	3.0f

#define TF_FLAG_MODEL				"models/flag/briefcase.mdl"

//=============================================================================
//
// CTF Flag defines.
//

#define TF_CTF_ENEMY_STOLEN			"CaptureFlag.EnemyStolen"
#define TF_CTF_ENEMY_DROPPED		"CaptureFlag.EnemyDropped"
#define TF_CTF_ENEMY_CAPTURED		"CaptureFlag.EnemyCaptured"
#define TF_CTF_ENEMY_RETURNED		"CaptureFlag.EnemyReturned"

#define TF_CTF_TEAM_STOLEN			"CaptureFlag.TeamStolen"
#define TF_CTF_TEAM_DROPPED			"CaptureFlag.TeamDropped"
#define TF_CTF_TEAM_CAPTURED		"CaptureFlag.TeamCaptured"
#define TF_CTF_TEAM_RETURNED		"CaptureFlag.TeamReturned"

#define TF_CTF_FLAGSPAWN			"CaptureFlag.FlagSpawn"

#define TF_CTF_CAPTURED_TEAM_FRAGS	1

#define TF_CTF_RESET_TIME			60.0f

//=============================================================================
//
// Attack/Defend Flag defines.
//

#define TF_AD_ENEMY_STOLEN			"AttackDefend.EnemyStolen"
#define TF_AD_ENEMY_DROPPED			"AttackDefend.EnemyDropped"
#define TF_AD_ENEMY_CAPTURED		"AttackDefend.EnemyCaptured"
#define TF_AD_ENEMY_RETURNED		"AttackDefend.EnemyReturned"

#define TF_AD_TEAM_STOLEN			"AttackDefend.TeamStolen"
#define TF_AD_TEAM_DROPPED			"AttackDefend.TeamDropped"
#define TF_AD_TEAM_CAPTURED			"AttackDefend.TeamCaptured"
#define TF_AD_TEAM_RETURNED			"AttackDefend.TeamReturned"

#define TF_AD_CAPTURED_SOUND		"AttackDefend.Captured"

#define TF_AD_CAPTURED_FRAGS		30
#define TF_AD_RESET_TIME			60.0f

//=============================================================================
//
// Invade Flag defines.
//

#define TF_INVADE_ENEMY_STOLEN			"Invade.EnemyStolen"
#define TF_INVADE_ENEMY_DROPPED			"Invade.EnemyDropped"
#define TF_INVADE_ENEMY_CAPTURED		"Invade.EnemyCaptured"

#define TF_INVADE_TEAM_STOLEN			"Invade.TeamStolen"
#define TF_INVADE_TEAM_DROPPED			"Invade.TeamDropped"
#define TF_INVADE_TEAM_CAPTURED			"Invade.TeamCaptured"

#define TF_INVADE_FLAG_RETURNED			"Invade.FlagReturned"

#define TF_INVADE_CAPTURED_FRAGS		10
#define TF_INVADE_CAPTURED_TEAM_FRAGS	1

#define TF_INVADE_RESET_TIME			60.0f
#define TF_INVADE_NEUTRAL_TIME			30.0f

#ifdef CLIENT_DLL
	#define CCaptureFlagReturnIcon C_CaptureFlagReturnIcon
	#define CBaseAnimating C_BaseAnimating
#endif

class CCaptureFlagReturnIcon: public CBaseAnimating
{
public:
	DECLARE_CLASS( CCaptureFlagReturnIcon, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CCaptureFlagReturnIcon();

#ifdef CLIENT_DLL

	virtual int		DrawModel( int flags );
	void			DrawReturnProgressBar( void );

	virtual RenderGroup_t GetRenderGroup( void );
	virtual bool	ShouldDraw( void ) { return true; }

	virtual void GetRenderBounds( Vector& theMins, Vector& theMaxs );

private:

	IMaterial	*m_pReturnProgressMaterial_Empty;		// For labels above players' heads.
	IMaterial	*m_pReturnProgressMaterial_Full;

#else
public:
	virtual void Spawn( void );
	virtual int UpdateTransmitState( void );

#endif

};

//=============================================================================
//
// CTF Flag class.
//
class CCaptureFlag : public CTFItem
{
public:

	DECLARE_CLASS( CCaptureFlag, CTFItem );
	DECLARE_NETWORKCLASS();

	CCaptureFlag();

	unsigned int	GetItemID( void );

	void			Precache( void );
	void			Spawn( void );

	void			FlagTouch( CBaseEntity *pOther );

	bool			IsDisabled( void );
	void			SetDisabled( bool bDisabled );

	CBaseEntity		*GetPrevOwner( void ) { return m_hPrevOwner.Get(); }

// Game DLL Functions
#ifdef GAME_DLL
	virtual void	Activate( void );

	// Input handlers
	void			InputEnable( inputdata_t &inputdata );
	void			InputDisable( inputdata_t &inputdata );
	void			InputRoundActivate( inputdata_t &inputdata );

	void			Think( void );
	
	void			SetFlagStatus( int iStatus );
	void			ResetFlagReturnTime( void ) { m_flResetTime = 0; }
	void			SetFlagReturnIn( float flTime )
	{
		m_flResetTime = gpGlobals->curtime + flTime;
		m_flMaxResetTime = flTime;
	}

	void			ResetFlagNeutralTime( void ) { m_flNeutralTime = 0; }
	void			SetFlagNeutralIn( float flTime )
	{ 
		m_flNeutralTime = gpGlobals->curtime + flTime;
		m_flMaxResetTime = flTime;
	}
	bool			IsCaptured( void ){ return m_bCaptured; }

	int				UpdateTransmitState();

#else // CLIENT DLL Functions

	virtual const char	*GetIDString( void ) { return "entity_capture_flag"; };

	virtual void	OnPreDataChanged( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	CNewParticleEffect	*m_pGlowTrailEffect;
	CNewParticleEffect	*m_pPaperTrailEffect;

	void			ManageTrailEffects( void );
	virtual void	Simulate( void );

	float			GetMaxResetTime() { return m_flMaxResetTime; }
	float			GetReturnProgress( void );

#endif

	void			Capture( CTFPlayer *pPlayer, int nCapturePoint );
	virtual void	PickUp( CTFPlayer *pPlayer, bool bInvisible );
	virtual void	Drop( CTFPlayer *pPlayer, bool bVisible, bool bThrown = false, bool bMessage = true );

	int				GetGameType( void ){ return m_nGameType; }

	bool			IsDropped( void );
	bool			IsHome( void );
	bool			IsStolen( void );

private:

	void			Reset( void );
	void			ResetMessage( void );

private:

	CNetworkVar( bool,	m_bDisabled );	// Enabled/Disabled?
	CNetworkVar( int,	m_nGameType );	// Type of game this flag will be used for.

	CNetworkVar( int,	m_nFlagStatus );
	CNetworkVar( float,	m_flResetTime );		// Time until the flag is placed back at spawn.
	CNetworkVar( float, m_flMaxResetTime );		// Time the flag takes to return in the current mode
	CNetworkVar( float, m_flNeutralTime );	// Time until the flag becomes neutral (used for the invade gametype)
	CNetworkHandle( CBaseEntity, m_hPrevOwner );

	int				m_iOriginalTeam;
	float			m_flOwnerPickupTime;

	EHANDLE		m_hReturnIcon;

#ifdef GAME_DLL
	Vector			m_vecResetPos;		// The position the flag should respawn (reset) at.
	QAngle			m_vecResetAng;		// The angle the flag should respawn (reset) at.

	COutputEvent	m_outputOnReturn;	// Fired when the flag is returned via timer.
	COutputEvent	m_outputOnPickUp;	// Fired when the flag is picked up.
	COutputEvent	m_outputOnDrop;		// Fired when the flag is dropped.
	COutputEvent	m_outputOnCapture;	// Fired when the flag is captured.

	bool			m_bAllowOwnerPickup;


	bool			m_bCaptured;
#else

	IMaterial	*m_pReturnProgressMaterial_Empty;		// For labels above players' heads.
	IMaterial	*m_pReturnProgressMaterial_Full;		

	int			m_nOldFlagStatus;

#endif

	DECLARE_DATADESC();
};

#endif // ENTITY_CAPTURE_FLAG_H