//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASEENTITY_SHARED_H
#define BASEENTITY_SHARED_H
#ifdef _WIN32
#pragma once
#endif

extern ConVar hl2_episodic;

// Simple shared header file for common base entities

// entity capabilities
// These are caps bits to indicate what an object's capabilities (currently used for +USE, save/restore and level transitions)
// NOTE: (For portal 2 and paint) These +use related caps MUST be the first 6 bits!
#define		FCAP_IMPULSE_USE			0x00000001		// can be used by the player
#define		FCAP_CONTINUOUS_USE			0x00000002		// can be used by the player
#define		FCAP_ONOFF_USE				0x00000004		// can be used by the player
#define		FCAP_DIRECTIONAL_USE		0x00000008		// Player sends +/- 1 when using (currently only tracktrains)
// NOTE: Normally +USE only works in direct line of sight.  Add these caps for additional searches
#define		FCAP_USE_ONGROUND			0x00000010
#define		FCAP_USE_IN_RADIUS			0x00000020

#define		FCAP_MUST_SPAWN				0x00000040		// Spawn after restore
#define		FCAP_ACROSS_TRANSITION		0x00000080		// should transfer between transitions 
// UNDONE: This will ignore transition volumes (trigger_transition), but not the PVS!!!
#define		FCAP_FORCE_TRANSITION		0x00000100		// ALWAYS goes across transitions
#define		FCAP_NOTIFY_ON_TRANSITION	0x00000200		// Entity will receive Inside/Outside transition inputs when a transition occurs

#define		FCAP_SAVE_NON_NETWORKABLE	0x00000400

#define		FCAP_MASTER					0x10000000		// Can be used to "master" other entities (like multisource)
#define		FCAP_WCEDIT_POSITION		0x40000000		// Can change position and update Hammer in edit mode
#define		FCAP_DONT_SAVE				0x80000000		// Don't save this


// How many bits are used to transmit parent attachment indices?
#define NUM_PARENTATTACHMENT_BITS	6

// Maximum number of vphysics objects per entity
#define VPHYSICS_MAX_OBJECT_LIST_COUNT	1024

#if defined( CLIENT_DLL )
#include "c_baseentity.h"
#include "c_baseanimating.h"
#else
#include "baseentity.h"

#ifdef HL2_EPISODIC
	#include "info_darknessmode_lightsource.h"
#endif // HL2_EPISODIC

#endif

#include "vscript/ivscript.h"
#include "vscript_shared.h"

#if !defined( NO_ENTITY_PREDICTION )
// CBaseEntity inlines
inline bool CBaseEntity::IsPlayerSimulated( void ) const
{
	return m_bIsPlayerSimulated;
}

inline CBasePlayer *CBaseEntity::GetSimulatingPlayer( void )
{
	return m_hPlayerSimulationOwner.Get();
}
#endif

inline MoveType_t CBaseEntity::GetMoveType() const
{
	return (MoveType_t)(unsigned char)m_MoveType;
}

inline MoveCollide_t CBaseEntity::GetMoveCollide() const
{
	return (MoveCollide_t)(unsigned char)m_MoveCollide;
}

//-----------------------------------------------------------------------------
// Collision group accessors
//-----------------------------------------------------------------------------
inline int CBaseEntity::GetCollisionGroup() const
{
	return m_CollisionGroup;
}

inline int	CBaseEntity::GetFlags( void ) const
{
	return m_fFlags;
}

inline bool CBaseEntity::IsAlive( void )
{
	return m_lifeState == LIFE_ALIVE; 
}

inline CBaseEntity	*CBaseEntity::GetOwnerEntity() const
{
	return m_hOwnerEntity.Get();
}

inline CBaseEntity	*CBaseEntity::GetEffectEntity() const
{
	return m_hEffectEntity.Get();
}

inline int CBaseEntity::GetPredictionRandomSeed( void )
{
	return m_nPredictionRandomSeed;
}

inline CBasePlayer *CBaseEntity::GetPredictionPlayer( void )
{
	return m_pPredictionPlayer;
}

inline void CBaseEntity::SetPredictionPlayer( CBasePlayer *player )
{
	m_pPredictionPlayer = player;
}


inline bool CBaseEntity::IsSimulatedEveryTick() const
{
	return m_bSimulatedEveryTick;
}

inline bool CBaseEntity::IsAnimatedEveryTick() const
{
	return m_bAnimatedEveryTick;
}

inline void CBaseEntity::SetSimulatedEveryTick( bool sim )
{
	if ( m_bSimulatedEveryTick != sim )
	{
		m_bSimulatedEveryTick = sim;
#ifdef CLIENT_DLL
		Interp_UpdateInterpolationAmounts( GetVarMapping() );
#endif
	}
}

inline void CBaseEntity::SetAnimatedEveryTick( bool anim )
{
	if ( m_bAnimatedEveryTick != anim )
	{
		m_bAnimatedEveryTick = anim;
#ifdef CLIENT_DLL
		Interp_UpdateInterpolationAmounts( GetVarMapping() );
#endif
	}
}

inline float CBaseEntity::GetAnimTime() const
{
	return m_flAnimTime;
}

inline float CBaseEntity::GetSimulationTime() const
{
	return m_flSimulationTime;
}

inline void CBaseEntity::SetAnimTime( float at )
{
	m_flAnimTime = at;
}

inline void CBaseEntity::SetSimulationTime( float st )
{
	m_flSimulationTime = st;
}

inline int CBaseEntity::GetEffects( void ) const
{ 
	return m_fEffects; 
}

inline void CBaseEntity::RemoveEffects( int nEffects ) 
{ 
#if !defined( CLIENT_DLL )
#ifdef HL2_EPISODIC
	if ( nEffects & (EF_BRIGHTLIGHT|EF_DIMLIGHT) )
	{
		// Hack for now, to avoid player emitting radius with his flashlight
		if ( !IsPlayer() )
		{
			RemoveEntityFromDarknessCheck( this );
		}
	}
#endif // HL2_EPISODIC
#endif // !CLIENT_DLL

	m_fEffects &= ~nEffects;
	if ( nEffects & EF_NODRAW )
	{
#ifndef CLIENT_DLL
		NetworkProp()->MarkPVSInformationDirty();
		DispatchUpdateTransmitState();
#else
		UpdateVisibility();
#endif
	}
}

inline void CBaseEntity::ClearEffects( void ) 
{ 
#if !defined( CLIENT_DLL )
#ifdef HL2_EPISODIC
	if ( m_fEffects & (EF_BRIGHTLIGHT|EF_DIMLIGHT) )
	{
		// Hack for now, to avoid player emitting radius with his flashlight
		if ( !IsPlayer() )
		{
			RemoveEntityFromDarknessCheck( this );
		}
	}
#endif // HL2_EPISODIC
#endif // !CLIENT_DLL

	m_fEffects = 0;
#ifndef CLIENT_DLL
		DispatchUpdateTransmitState();
#else
		UpdateVisibility();
#endif
}

inline bool CBaseEntity::IsEffectActive( int nEffects ) const
{ 
	return (m_fEffects & nEffects) != 0; 
}


inline HSCRIPT ToHScript( CBaseEntity *pEnt )
{
	return ( pEnt ) ? pEnt->GetScriptInstance() : NULL;
}

template <> ScriptClassDesc_t *GetScriptDesc<CBaseEntity>( CBaseEntity * );
inline CBaseEntity *ToEnt( HSCRIPT hScript )
{

	return ( hScript ) ? (CBaseEntity *)g_pScriptVM->GetInstanceValue( hScript, GetScriptDescForClass(CBaseEntity) ) : NULL;
}

// convenience functions for fishing out the vectors of this object
// equivalent to GetVectors(), but doesn't need an intermediate stack 
// variable (which might cause an LHS anyway)
inline Vector	CBaseEntity::Forward() const RESTRICT  ///< get my forward (+x) vector
{
	const matrix3x4_t &mat = EntityToWorldTransform();
	return Vector( mat[0][0], mat[1][0], mat[2][0] );
}

inline Vector	CBaseEntity::Left() const RESTRICT     ///< get my left    (+y) vector
{
	const matrix3x4_t &mat = EntityToWorldTransform();
	return Vector( mat[0][1], mat[1][1], mat[2][1] );
}

inline Vector	CBaseEntity::Up() const  RESTRICT      ///< get my up      (+z) vector
{
	const matrix3x4_t &mat = EntityToWorldTransform();
	return Vector( mat[0][2], mat[1][2], mat[2][2] );
}

// Shared EntityMessage between game and client .dlls
#define BASEENTITY_MSG_REMOVE_DECALS	1

#endif // BASEENTITY_SHARED_H
