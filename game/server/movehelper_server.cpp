//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include <stdarg.h>
#include "gamerules.h"
#include "player.h"
#include "model_types.h"
#include "imovehelper.h"
#include "shake.h"				// For screen fade constants
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IPhysicsCollision *physcollision;


//-----------------------------------------------------------------------------
// Implementation of the movehelper on the server
//-----------------------------------------------------------------------------

class CMoveHelperServer : public IMoveHelper
{
public:
	CMoveHelperServer( void );
	virtual ~CMoveHelperServer();

	// Methods associated with a particular entity
	virtual	char const*		GetName( EntityHandle_t handle ) const;

	// Touch list...
	virtual void	ResetTouchList( void );
	virtual bool	AddToTouched( const trace_t &tr, const Vector& impactvelocity );
	virtual void	SetGroundNormal( const Vector& groundNormal );
 	virtual void	ProcessImpacts( void );

	virtual bool	PlayerFallingDamage( void );
	virtual void	PlayerSetAnimation( PLAYER_ANIM eAnim );

	// Numbered line printf
	virtual void	Con_NPrintf( int idx, char const* fmt, ... );
	
	// These have separate server vs client impementations
	virtual void	StartSound( const Vector& origin, int channel, char const* sample, float volume, soundlevel_t soundlevel, int fFlags, int pitch );
	virtual void	StartSound( const Vector& origin, const char *soundname ); 

	virtual void	PlaybackEventFull( int flags, int clientindex, unsigned short eventindex, float delay, Vector& origin, Vector& angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
	virtual IPhysicsSurfaceProps *GetSurfaceProps( void );

	void			SetHost( CBaseEntity *host );

	virtual bool IsWorldEntity( const CBaseHandle &handle );

private:
	CBaseEntity*	m_pHost;

	// results, tallied on client and server, but only used by server to run SV_Impact.
	// we store off our velocity in the trace_t structure so that we can determine results
	// of shoving boxes etc. around.
	struct touchlist_t
	{
		Vector	deltavelocity;
		trace_t trace;
	};

	CUtlVector<touchlist_t>	m_TouchList;

	Vector m_collisionNormal;
	Vector m_groundNormal;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------

IMPLEMENT_MOVEHELPER();

static CMoveHelperServer s_MoveHelperServer;

//-----------------------------------------------------------------------------
// Converts the entity handle into a edict_t
//-----------------------------------------------------------------------------

static inline edict_t* GetEdict( EntityHandle_t handle )
{
	return gEntList.GetEdict( handle );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

CMoveHelperServer::CMoveHelperServer( void ) : m_TouchList( 0, 128 )
{
	m_pHost = 0;
	SetSingleton( this );
}

CMoveHelperServer::~CMoveHelperServer( void )
{
	SetSingleton( 0 );
}

//-----------------------------------------------------------------------------
// Indicates which player we're going to move
//-----------------------------------------------------------------------------

void CMoveHelperServer::SetHost( CBaseEntity *host )
{
	m_pHost = host;

	// In case any stuff is ever left over, sigh...
	ResetTouchList();
}


//-----------------------------------------------------------------------------
// Returns the name for debugging purposes
//-----------------------------------------------------------------------------
char const* CMoveHelperServer::GetName( EntityHandle_t handle ) const
{
	// This ain't pertickulerly fast, but it's for debugging anyways
	edict_t* pEdict = GetEdict(handle);
	CBaseEntity *ent = CBaseEntity::Instance( pEdict );
	
	// Is it the world?
	if (ENTINDEX(pEdict) == 0)
		return STRING(gpGlobals->mapname);

	// Is it a model?
	if ( ent && ent->GetModelName() != NULL_STRING )
		return STRING( ent->GetModelName() );

	if ( ent->GetClassname() != NULL )
	{
		return ent->GetClassname();
	}

	return "?";
}	

//-----------------------------------------------------------------------------
// When we do a collision test, we report everything we hit.. 
//-----------------------------------------------------------------------------

void CMoveHelperServer::ResetTouchList( void )
{
	m_TouchList.RemoveAll();

	// Track collision normal
	m_collisionNormal.Init();
	m_groundNormal.Init();
}


//-----------------------------------------------------------------------------
// When a collision occurs, we add it to the touched list
//-----------------------------------------------------------------------------

bool CMoveHelperServer::AddToTouched( const trace_t &tr, const Vector& impactvelocity )
{
	Assert( m_pHost );

	// Trace missed
	if ( !tr.m_pEnt )
		return false;

	if ( tr.m_pEnt == m_pHost )
	{
		Assert( !"CMoveHelperServer::AddToTouched:  Tried to add self to touchlist!!!" );
		return false;
	}

	// Track collision normal
	m_collisionNormal += tr.plane.normal;

	// Check for duplicate entities
	for ( int j = m_TouchList.Count(); --j >= 0; )
	{
		if ( m_TouchList[j].trace.m_pEnt == tr.m_pEnt )
		{
			return false;
		}
	}
	
	int i = m_TouchList.AddToTail();
	m_TouchList[i].trace = tr;
	VectorCopy( impactvelocity, m_TouchList[i].deltavelocity );

	return true;
}


//-----------------------------------------------------------------------------
// When the ground is hit, update the normal
//-----------------------------------------------------------------------------

void CMoveHelperServer::SetGroundNormal( const Vector& groundNormal )
{
	m_groundNormal = groundNormal;
}


//-----------------------------------------------------------------------------
// After we built the touch list, deal with all the impacts...
//-----------------------------------------------------------------------------
void CMoveHelperServer::ProcessImpacts( void )
{
	Assert( m_pHost );

	m_pHost->PhysicsTouchTriggers();

	// Don't bother if the player ain't solid
	if ( m_pHost->IsSolidFlagSet( FSOLID_NOT_SOLID ) )
		return;

	// Save off the velocity, cause we need to temporarily reset it
	Vector vel = m_pHost->GetAbsVelocity();

	// Touch other objects that were intersected during the movement.
	for (int i = 0 ; i < m_TouchList.Count(); i++)
	{
		CBaseHandle entindex = m_TouchList[i].trace.m_pEnt->GetRefEHandle();

		// We should have culled negative indices by now
		Assert( entindex.IsValid() );

		edict_t* ent = GetEdict( entindex );
		if (!ent)
			continue;

		// Run the impact function as if we had run it during movement.
		CBaseEntity *entity = GetContainingEntity( ent );
		if ( !entity )
			continue;

		Assert( entity != m_pHost );
		// Don't ever collide with self!!!!
		if ( entity == m_pHost )
			continue;

		// Reconstruct trace results.
		m_TouchList[i].trace.m_pEnt = CBaseEntity::Instance( ent );

		// Use the velocity we had when we collided, so boxes will move, etc.
		m_pHost->SetAbsVelocity( m_TouchList[i].deltavelocity );
		
		entity->PhysicsImpact( m_pHost, m_TouchList[i].trace );
	}

	// Restore the velocity
	m_pHost->SetAbsVelocity( vel );

	// Track collision normal
	if ( m_pHost && m_pHost->IsPlayer() )
	{
		CBasePlayer *pPlayerHost = static_cast< CBasePlayer * >( m_pHost );
		Assert( pPlayerHost );
		if ( !m_collisionNormal.IsZero() )
		{
			m_collisionNormal.NormalizeInPlace();
			pPlayerHost->m_movementCollisionNormal = m_collisionNormal;
		}		  

		if ( !m_groundNormal.IsZero() )
		{
			pPlayerHost->m_groundNormal = m_groundNormal;
		}
	}

	// So no stuff is ever left over, sigh...
	ResetTouchList();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			*soundname - 
//-----------------------------------------------------------------------------
void CMoveHelperServer::StartSound( const Vector& origin, const char *soundname )
{
	//MDB - Changing this to send to PAS, as the overloaded function below has done.
	//Also removed the UsePredictionRules, client does not yet play the equivalent sound

	CRecipientFilter filter;
	filter.AddRecipientsByPAS( origin );

	CBaseEntity::EmitSound( filter, m_pHost->entindex(), soundname );
}

//-----------------------------------------------------------------------------
// plays a sound
//-----------------------------------------------------------------------------
void CMoveHelperServer::StartSound( const Vector& origin, int channel, char const* sample, 
						float volume, soundlevel_t soundlevel, int fFlags, int pitch )
{

	CRecipientFilter filter;
	filter.AddRecipientsByPAS( origin );
	// FIXME, these sounds should not go to the host entity ( SND_NOTHOST )
	if ( gpGlobals->maxClients == 1 )
	{
		// Always send sounds down in SP

		EmitSound_t ep;
		ep.m_nChannel = channel;
		ep.m_pSoundName = sample;
		ep.m_flVolume = volume;
		ep.m_SoundLevel = soundlevel;
		ep.m_nFlags = fFlags;
		ep.m_nPitch = pitch;
		ep.m_pOrigin = &origin;

		CBaseEntity::EmitSound( filter, m_pHost->entindex(), ep );
	}
	else
	{
		filter.UsePredictionRules();

		EmitSound_t ep;
		ep.m_nChannel = channel;
		ep.m_pSoundName = sample;
		ep.m_flVolume = volume;
		ep.m_SoundLevel = soundlevel;
		ep.m_nFlags = fFlags;
		ep.m_nPitch = pitch;
		ep.m_pOrigin = &origin;

		CBaseEntity::EmitSound( filter, m_pHost->entindex(), ep );
	}
}


//-----------------------------------------------------------------------------
// Umm...
//-----------------------------------------------------------------------------
void CMoveHelperServer::PlaybackEventFull( int flags, int clientindex, unsigned short eventindex, float delay, Vector& origin, Vector& angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	// FIXME, Redo with new event system parameter stuff
}

IPhysicsSurfaceProps *CMoveHelperServer::GetSurfaceProps( void )
{
	extern IPhysicsSurfaceProps *physprops;
	return physprops;
}

//-----------------------------------------------------------------------------
// Purpose: Note that this only works on a listen server (since it requires graphical output)
//			*pFormat - 
//			... - 
//-----------------------------------------------------------------------------
void CMoveHelperServer::Con_NPrintf( int idx, char const* pFormat, ...)
{
	va_list marker;
	char msg[8192];

	va_start(marker, pFormat);
	Q_vsnprintf(msg, sizeof( msg ), pFormat, marker);
	va_end(marker);
	
	engine->Con_NPrintf( idx, msg );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the player falls onto a surface fast enough to take
//			damage, according to the rules in CGameMovement::CheckFalling.
// Output : Returns true if the player survived the fall, false if they died.
//-----------------------------------------------------------------------------
bool CMoveHelperServer::PlayerFallingDamage( void )
{
	if ( m_pHost->IsPlayer() )
	{
		CBasePlayer *pPlayer = static_cast< CBasePlayer * >( m_pHost );

		float flFallDamage = g_pGameRules->FlPlayerFallDamage( pPlayer );	
		if ( flFallDamage > 0 )
		{
			pPlayer->TakeDamage( CTakeDamageInfo( GetContainingEntity(INDEXENT(0)), GetContainingEntity(INDEXENT(0)), flFallDamage, DMG_FALL ) ); 
			StartSound( pPlayer->GetAbsOrigin(), "Player.FallDamage" );
		}

		if ( pPlayer->m_iHealth <= 0 )
		{
			if ( g_pGameRules->FlPlayerFallDeathDoesScreenFade( pPlayer ) )
			{
				color32 black = {0, 0, 0, 255};
				UTIL_ScreenFade( pPlayer, black, 0, 9999, FFADE_OUT | FFADE_STAYOUT );
			}
			return(false);
		}
	}

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Sets an animation in the player.
// Input  : eAnim - Animation to set.
//-----------------------------------------------------------------------------
void CMoveHelperServer::PlayerSetAnimation( PLAYER_ANIM eAnim )
{
	if ( m_pHost && m_pHost->IsPlayer() )
	{
		static_cast< CBasePlayer * >( m_pHost )->SetAnimation( eAnim );
	}
}

bool CMoveHelperServer::IsWorldEntity( const CBaseHandle &handle )
{
	return handle == CBaseEntity::Instance( 0 );
}


// todo: remove this and find/replace all the MoveHelperServer() calls   (server no longer uses a specialized version of the IMoveHelper interface)
IMoveHelper* MoveHelperServer()
{
	return MoveHelper();
}
