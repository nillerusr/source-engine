//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_PLAYER_SHARED_H
#define DOD_PLAYER_SHARED_H
#ifdef _WIN32
#pragma once
#endif


#include "networkvar.h"
#include "weapon_dodbase.h"


#ifdef CLIENT_DLL
	class C_DODPlayer;
#else
	class CDODPlayer;
#endif

// Entity Messages
#define DOD_PLAYER_POP_HELMET		1
#define DOD_PLAYER_REMOVE_DECALS	2

class CViewOffsetAnimation
{
public:
	CViewOffsetAnimation( CBasePlayer *pPlayer )
	{
		m_pPlayer = pPlayer;
		m_vecStart = vec3_origin;
		m_vecDest = vec3_origin;
		m_flLength = 0.0;
		m_flEndTime = 0.0;
		m_eViewAnimType = VIEW_ANIM_LINEAR_Z_ONLY;
		m_bActive = false;
	}

	static CViewOffsetAnimation *CreateViewOffsetAnim( CBasePlayer *pPlayer )
	{
		CViewOffsetAnimation *p = new CViewOffsetAnimation( pPlayer );

		Assert( p );

		return p;
	}

	void StartAnimation( Vector vecStart, Vector vecDest, float flTime, ViewAnimationType type )
	{
		m_vecStart = vecStart;
		m_vecDest = vecDest;
		m_flLength = flTime;
		m_flEndTime = gpGlobals->curtime + flTime;
		m_eViewAnimType = type;
		m_bActive = true;
	}

	void Reset( void ) 
	{
		m_bActive = false;
	}

	void Think( void )
	{
		if ( !m_bActive )
			return;

		if ( IsFinished() )
		{
			m_bActive = false;
			return;
		}

		float flFraction = ( m_flEndTime - gpGlobals->curtime ) / m_flLength;

		Assert( m_pPlayer );

		if ( m_pPlayer )
		{
			Vector vecCurrentView = m_pPlayer->GetViewOffset();

			switch ( m_eViewAnimType )
			{
			case VIEW_ANIM_LINEAR_Z_ONLY:
				vecCurrentView.z = flFraction * m_vecStart.z + ( 1.0 - flFraction ) * m_vecDest.z;
				break;

			case VIEW_ANIM_SPLINE_Z_ONLY:
				vecCurrentView.z = SimpleSplineRemapVal( flFraction, 1.0, 0.0, m_vecStart.z, m_vecDest.z );
				break;

			case VIEW_ANIM_EXPONENTIAL_Z_ONLY:
				{
					float flBias = Bias( flFraction, 0.2 );
					vecCurrentView.z = flBias * m_vecStart.z + ( 1.0 - flBias ) * m_vecDest.z;
				}
				break;
			}

			m_pPlayer->SetViewOffset( vecCurrentView );
		}		
	}

	bool IsFinished( void )
	{
		return ( gpGlobals->curtime > m_flEndTime || m_pPlayer == NULL );
	}

private:
	CBasePlayer *m_pPlayer;
	Vector		m_vecStart;
	Vector		m_vecDest;
	float		m_flEndTime;
	float		m_flLength;
	ViewAnimationType m_eViewAnimType;
	bool		m_bActive;
};


// Data in the DoD player that is accessed by shared code.
// This data isn't necessarily transmitted between client and server.
class CDODPlayerShared
{
public:

#ifdef CLIENT_DLL
	friend class C_DODPlayer;
	typedef C_DODPlayer OuterClass;
	DECLARE_PREDICTABLE();
#else
	friend class CDODPlayer;
	typedef CDODPlayer OuterClass;
#endif
	
	DECLARE_EMBEDDED_NETWORKVAR()
	DECLARE_CLASS_NOBASE( CDODPlayerShared );


	CDODPlayerShared();
	~CDODPlayerShared();

	void	SetStamina( float stamina );
	float	GetStamina( void ) { return m_flStamina; }
	
	void	Init( OuterClass *pOuter );

	bool	IsProne() const;
	bool	IsGettingUpFromProne() const;	
	bool	IsGoingProne() const;
	void	SetProne( bool bProne, bool bNoAnimation = false );

	bool	IsBazookaDeployed( void ) const;
	bool	IsBazookaOnlyDeployed( void ) const;
	bool	IsSniperZoomed( void ) const;
	bool	IsInMGDeploy( void ) const;
	bool	IsProneDeployed( void ) const;
	bool	IsSandbagDeployed( void ) const;
	bool	IsDucking( void ) const; 
	
	void	SetDesiredPlayerClass( int playerclass );
	int		DesiredPlayerClass( void );

	void	SetPlayerClass( int playerclass );
	int		PlayerClass( void );

	CWeaponDODBase* GetActiveDODWeapon() const;

	void	SetDeployed( bool bDeployed, float flHeight = -1 );

	QAngle	GetDeployedAngles( void ) const;
	float	GetDeployedHeight( void ) const;

	void	SetDeployedYawLimits( float flLeftYaw, float flRightYaw );
	void	ClampDeployedAngles( QAngle *vecTestAngles );

	void	SetSlowedTime( float t );
	float	GetSlowedTime( void ) const;

	void	StartGoingProne( void );
	void	StandUpFromProne( void );

	bool	CanChangePosition( void );

	bool	IsJumping( void ) { return m_bJumping; }
	void	SetJumping( bool bJumping );

	bool	IsSprinting( void ) { return m_bIsSprinting; }

	void	ForceUnzoom( void );

	void	SetSprinting( bool bSprinting );
	void	StartSprinting( void );
	void	StopSprinting( void );

	void	SetCPIndex( int index );
	int		GetCPIndex( void ) { return m_iCPIndex; }

	void SetLastViewAnimTime( float flTime );
	float GetLastViewAnimTime( void );

	void ViewAnimThink( void );

	void ResetViewOffsetAnimation( void );
	void ViewOffsetAnimation( Vector vecDest, float flTime, ViewAnimationType type );

	void ResetSprintPenalty( void );

	void SetPlanting( bool bPlanting )
	{
		m_bPlanting = bPlanting;
	}

	bool IsPlanting( void ) { return m_bPlanting; } 

	void SetDefusing( bool bDefusing )
	{
		m_bDefusing = bDefusing;
	}

	bool IsDefusing( void ) { return m_bDefusing; }

	void ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	void	SetPlayerDominated( CDODPlayer *pPlayer, bool bDominated );
	bool	IsPlayerDominated( int iPlayerIndex );
	bool	IsPlayerDominatingMe( int iPlayerIndex );
	void	SetPlayerDominatingMe( CDODPlayer *pPlayer, bool bDominated );

private:

	CNetworkVar( bool, m_bProne );

	CNetworkVar( int, m_iPlayerClass );
	CNetworkVar( int, m_iDesiredPlayerClass );

	CNetworkVar( float, m_flStamina );

	CNetworkVar( float, m_flSlowedUntilTime );

	CNetworkVar( bool, m_bIsSprinting );

	CNetworkVar( float, m_flDeployedYawLimitLeft );
	CNetworkVar( float, m_flDeployedYawLimitRight );

	CNetworkVar( bool, m_bPlanting );
	CNetworkVar( bool, m_bDefusing );

	bool m_bGaveSprintPenalty;

public:
	float m_flNextProneCheck; // Prevent it switching their prone state constantly.

	QAngle m_vecDeployedAngles;
	//float m_flDeployedHeight;
	CNetworkVar( float, m_flDeployedHeight );

	CNetworkVar( float, m_flUnProneTime );
	CNetworkVar( float, m_flGoProneTime );

	CNetworkVar( float, m_flDeployChangeTime );
	
	CNetworkVar( bool, m_bForceProneChange );

	CNetworkVar( int, m_iCPIndex );

	bool m_bJumping;

	float m_flLastViewAnimationTime;

	CViewOffsetAnimation *m_pViewOffsetAnim;

	CNetworkArray( bool, m_bPlayerDominated, MAX_PLAYERS+1 );		// array of state per other player whether player is dominating other players
	CNetworkArray( bool, m_bPlayerDominatingMe, MAX_PLAYERS+1 );	// array of state per other player whether other players are dominating this player

private:
	
	OuterClass *m_pOuter;
};			   



#endif // DOD_PLAYER_SHARED_H
