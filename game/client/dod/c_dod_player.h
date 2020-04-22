//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_DOD_PLAYER_H
#define C_DOD_PLAYER_H
#ifdef _WIN32
#pragma once
#endif


#include "dod_playeranimstate.h"
#include "c_baseplayer.h"
#include "dod_shareddefs.h"
#include "baseparticleentity.h"
#include "dod_player_shared.h"
#include "beamdraw.h"
#include "hintsystem.h"

#define PRONE_MAX_SWAY		3.0
#define PRONE_SWAY_AMOUNT	4.0
#define RECOIL_DURATION 0.1

class CViewAngleAnimation;

class C_DODPlayer : public C_BasePlayer
{
public:
	friend class CDODPlayerShared;

	DECLARE_CLASS( C_DODPlayer, C_BasePlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_INTERPOLATION();

	C_DODPlayer();
	~C_DODPlayer();

	static C_DODPlayer* GetLocalDODPlayer();

	virtual const QAngle& GetRenderAngles();
	virtual void UpdateClientSideAnimation();
	virtual void ProcessMuzzleFlashEvent();
	virtual void PostDataUpdate( DataUpdateType_t updateType );

	virtual bool CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	
	virtual void FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );

	virtual void OnDataChanged( DataUpdateType_t type );
	virtual void NotifyShouldTransmit( ShouldTransmitState_t state );
	virtual void ClientThink( void );
	virtual void GetToolRecordingState( KeyValues *msg );

	void LocalPlayerRespawn( void );

	// Returns true if we're allowed to show the menus right now.
	bool CanShowTeamMenu() const  {  return true; }

	bool CanShowClassMenu();

	bool ShouldDraw( void );

	virtual C_BaseAnimating * BecomeRagdollOnClient();
	virtual IRagdoll* GetRepresentativeRagdoll() const;

	virtual void ReceiveMessage( int classID, bf_read &msg );
	void PopHelmet( Vector vecDir, Vector vecForceOffset, int model );

	// Get the ID target entity index. The ID target is the player that is behind our crosshairs, used to
	// display the player's name.
	int GetIDTarget() const
	{
		return m_iIDEntIndex;
	}

	void UpdateIDTarget();

	bool ShouldAutoReload( void );
	bool ShouldAutoRezoom( void );

	void SetSprinting( bool bIsSprinting );
	bool IsSprinting( void );

	void LowerWeapon( void );
	void RaiseWeapon( void );
	bool IsWeaponLowered( void );

	virtual ShadowType_t	ShadowCastType( void );
	virtual void			GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual void			GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType );
	virtual bool			GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const;

	virtual int				DrawModel( int flags );
	virtual void Simulate();

	virtual float GetMinFOV() const { return 20; }
	virtual bool SetFOV( CBaseEntity *pRequester, int FOV, float zoomRate = 0.0f );

	virtual void CreateLightEffects( void ) {}	//no dimlight effects

	virtual Vector GetChaseCamViewOffset( CBaseEntity *target );
	virtual const QAngle& EyeAngles();
	virtual const Vector& GetRenderOrigin();  // return ragdoll origin if dead

	bool dod_IsInterpolationEnabled( void ) { return IsInterpolationEnabled(); }

	void CreateViewAngleAnimations( void );

	void CheckGrenadeHint( Vector vecGrenadeOrigin );
	void CheckBombTargetPlantHint( void );
	void CheckBombTargetDefuseHint( void );

	virtual void ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );
	
	// Hints
	virtual CHintSystem	*Hints( void ) { return &m_Hints; }

	// Player avoidance
	bool ShouldCollide( int collisionGroup, int contentsMask ) const;
	void AvoidPlayers( CUserCmd *pCmd );

	virtual bool ShouldReceiveProjectedTextures( int flags )
	{
		return ( this != C_BasePlayer::GetLocalPlayer() );
	}

	bool IsNemesisOfLocalPlayer();
	bool ShouldShowNemesisIcon();

	virtual void OnAchievementAchieved( int iAchievement );

	int GetActiveAchievementAward( void );
	IMaterial *GetHeadIconMaterial( void );

protected:
	virtual void		CalcObserverView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void				CalcDODDeathCamView( Vector &eyeOrigin, QAngle& eyeAngles, float& fov );
	void				CalcChaseCamView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov);
	virtual void CalcFreezeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );


	// Called by shared code.
public:

	void DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );
	
	DODPlayerState State_Get() const;

	IDODPlayerAnimState *m_PlayerAnimState;


	QAngle	m_angEyeAngles;
	CInterpolatedVar< QAngle >	m_iv_angEyeAngles;

	void FireBullets( const FireBulletsInfo_t &info );
	bool CanAttack( void );

	void SetBazookaDeployed( bool bDeployed ) {}

	// the control point index the player is near
	int GetCPIndex( void ) { return m_Shared.GetCPIndex(); }

//Implemented in shared code
public:
	virtual void SharedSpawn();

	void CheckProneMoveSound( int groundspeed, bool onground );
	virtual void UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity  );
	virtual void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );

	virtual const Vector	GetPlayerMins( void ) const; // uses local player
	virtual const Vector	GetPlayerMaxs( void ) const; // uses local player

	// Returns true if the player is allowed to move.
	bool CanMove() const;
	float GetStamina( void ) const;
	void SetStamina( float flStamina );

	virtual void SetAnimation( PLAYER_ANIM playerAnim );

	CDODPlayerShared m_Shared;

	float m_flRecoilTimeRemaining;
	float m_flPitchRecoilAccumulator;
	float m_flYawRecoilAccumulator;

	void GetWeaponRecoilAmount( int weaponId, float &flPitchRecoil, float &flYawRecoil );
	void DoRecoil( int iWpnID, float flWpnRecoil );
	void SetRecoilAmount( float flPitchRecoil, float flYawRecoil );
	void GetRecoilToAddThisFrame( float &flPitchRecoil, float &flYawRecoil );

	EHANDLE	m_hRagdoll;

	CWeaponDODBase* GetActiveDODWeapon() const;

	Activity TranslateActivity( Activity baseAct, bool *pRequired = NULL );	

	Vector m_lastStandingPos; // used by the gamemovement code for finding ladders

	// for stun effect
	float m_flStunEffectTime;
	float m_flStunAlpha;
	CNetworkVar( float, m_flStunMaxAlpha );
	CNetworkVar( float, m_flStunDuration );	

	float GetDeathTime( void ) { return m_flDeathTime; }

	// How long the progress bar takes to get to the end. If this is 0, then the progress bar
	// should not be drawn.
	CNetworkVar( int, m_iProgressBarDuration );

	// When the progress bar should start.
	CNetworkVar( float, m_flProgressBarStartTime );

	float m_flLastRespawnTime;

private:
	C_DODPlayer( const C_DODPlayer & );

	CNetworkVar( DODPlayerState, m_iPlayerState );

	CNetworkVar( float, m_flStamina );

	float	m_flProneViewOffset;
	bool	m_bProneSwayingRight;
	Vector m_vecRagdollVelocity;

	// ID Target
	int					m_iIDEntIndex;

	bool	m_bWeaponLowered;	// should our weapon be lowered right now

	CNetworkVar( bool, m_bSpawnInterpCounter );
	bool m_bSpawnInterpCounterCache;

	CHintSystem		m_Hints;

	void ReleaseFlashlight( void );
	Beam_t	*m_pFlashlightBeam;

	float m_flMinNextStepSoundTime;
	
	float m_fNextThinkPushAway;

	bool m_bPlayingProneMoveSound;

	void StaminaSoundThink( void );
	CSoundPatch		*m_pStaminaSound;
	bool m_bPlayingLowStaminaSound;

	// Cold Breath
	bool			CreateColdBreathEmitter( void );
	void			DestroyColdBreathEmitter( void );
	void			UpdateColdBreath( void );
	void			EmitColdBreathParticles( void );

	bool						m_bColdBreathOn;
	float						m_flColdBreathTimeStart;
	float						m_flColdBreathTimeEnd;
	CSmartPtr<CSimpleEmitter>	m_hColdBreathEmitter;	
	PMaterialHandle				m_hColdBreathMaterial;
	int							m_iHeadAttach;

	float						m_flHideHeadIconUntilTime;

	virtual void CalculateIKLocks( float currentTime );

	int m_iAchievementAwardsMask;
	IMaterial *m_pHeadIconMaterial;
};

inline C_DODPlayer *ToDODPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<C_DODPlayer*>( pEntity );
}


#endif // C_DOD_PLAYER_H
