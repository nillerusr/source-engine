//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_obj.h"
#include "tf_obj_mapdefined.h"
#include "engine/IEngineSound.h"
#include "entityoutput.h"
#include "tf_shareddefs.h"
#include "triggers.h"
#include "shake.h"
#include "tf_player.h"
#include "tf_gamerules.h"

#define TUNNEL_THINK_INTERVAL	0.1f
#define TUNNEL_FADE_TIME		1.0f
#define MAX_TUNNEL_DURATION		30.0f
// If tunneling takes longer than this, use a countdown
#define TUNNEL_DURATION_MESSAGE_NEEDED	3.0f

// It takes this long to tunnel
static ConVar tf_tunnel_time( "tf_tunnel_time", "2", 0, "Takes this long to traverse a tunnel." );

class CObjectTunnel : public CObjectMapDefined
{
	DECLARE_CLASS( CObjectTunnel, CObjectMapDefined );
public:

	DECLARE_SERVERCLASS();

	virtual void Spawn( void );
	virtual void Killed( void );

	int UpdateTransmitState();
	
private:
};

IMPLEMENT_SERVERCLASS_ST(CObjectTunnel, DT_ObjectTunnel)
END_SEND_TABLE();

int CObjectTunnel::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CObjectTunnel::Spawn( void )
{
	BaseClass::Spawn();

	AddFlag( FL_NOTARGET );
	SetType( OBJ_TUNNEL );
}

LINK_ENTITY_TO_CLASS(obj_tunnel,CObjectTunnel);
LINK_ENTITY_TO_CLASS(obj_tunnel_prop,CObjectTunnel);

//-----------------------------------------------------------------------------
// Purpose: Object has been blown up. Tunnels are never fully destroyed, so they stay on the minimap.
//-----------------------------------------------------------------------------
void CObjectTunnel::Killed( void )
{
	m_bDying = true;

	RemoveAllSappers( this );

	// Do an explosion.
	CPASFilter filter( GetAbsOrigin() );
	te->Explosion( 
		filter, 
		0.0,				
		&GetAbsOrigin(),
		g_sModelIndexFireball,
		5.4,		// radius
		15,
		TE_EXPLFLAG_NODLIGHTS,
		256,
		200);

	// Become non-solid and invisible
	VPhysicsDestroyObject();
	AddSolidFlags( FSOLID_NOT_SOLID );
	m_takedamage = DAMAGE_NO;
	AddEffects( EF_NODRAW );
}

class CInfoTunnelExit : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoTunnelExit, CPointEntity );
private:
};

LINK_ENTITY_TO_CLASS(info_tunnel_exit,CInfoTunnelExit);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CObjectTunnelTrigger : public CBaseTrigger
{
	DECLARE_CLASS( CObjectTunnelTrigger, CBaseTrigger );
public:
	CObjectTunnelTrigger();

	DECLARE_DATADESC();

	virtual void Precache();
	virtual void Spawn();
	virtual void Activate();

	virtual void StartTouch( CBaseEntity *pOther );

	void		SetActive( bool active );
	bool		GetActive( void ) const;

	void	InputSetActive( inputdata_t &inputdata );
	void	InputSetInactive( inputdata_t &inputdata );
	void	InputToggleActive( inputdata_t &inputdata );

	void	InputSetTarget( inputdata_t &inputdata );
	void	InputSetTeleportDuration( inputdata_t &inputdata );
	void	InputSetTeleportVelocity( inputdata_t &inputdata );

	virtual void	TunnelThink();
private:
	float						GetTeleportDuration( void );

	bool m_bActive;
	CHandle< CInfoTunnelExit > m_hTunnelExit;

	COutputEvent	OnTunnelTriggerStart;
	COutputEvent	OnTunnelTriggerEnd;

	struct TunnelPlayer
	{
		CHandle< CBaseTFPlayer >	player;
		Vector						startpos;
		float						tunnelstarted;
		float						duration;
		float						teleporttime;
		float						fadeintime;
		bool						exitstarted;
		float						fadetime;
		int							iremaining;
		int							ilastremaining;
		bool						needremainigcounter;
	};

	CUtlVector< TunnelPlayer > m_Tunneling;

	void		StartTunneling( CBaseTFPlayer *player );
	bool		KeepTunneling( TunnelPlayer *tunnel );
	

	float							m_flTeleportDuration;
	float							m_flTeleportVelocity;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectTunnelTrigger::CObjectTunnelTrigger()
{
	m_flTeleportDuration = -1.0f;
	m_flTeleportVelocity = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *player - 
//-----------------------------------------------------------------------------
void CObjectTunnelTrigger::StartTunneling( CBaseTFPlayer *player )
{
	if ( !player )
		return;

	// Ignore if it's already in the list
	int c = m_Tunneling.Count();
	for ( int i = 0 ; i < c; i++ )
	{
		TunnelPlayer *tp = &m_Tunneling[ i ];
		if ( tp->player == player )
		{
			return;
		}
	}

	TunnelPlayer tunnel;
	tunnel.player = player;
	tunnel.tunnelstarted = gpGlobals->curtime;
	tunnel.duration =  GetTeleportDuration();
	tunnel.teleporttime = tunnel.tunnelstarted + tunnel.duration;
	tunnel.exitstarted = false;
	tunnel.startpos = player->GetAbsOrigin();
	tunnel.iremaining = (int)tunnel.duration;
	tunnel.ilastremaining = tunnel.iremaining;
	tunnel.needremainigcounter = ( tunnel.iremaining > TUNNEL_DURATION_MESSAGE_NEEDED ) ? true : false;

	// Fade user screen to black
	color32 black = {0,0,0,255};

	float duration = tunnel.duration;
	float fadeouttime = TUNNEL_FADE_TIME;
	float holdtime = 0.0f;
	if ( duration < 2 * TUNNEL_FADE_TIME )
	{
		fadeouttime = duration * 0.5f;
	}
	else
	{
		fadeouttime = TUNNEL_FADE_TIME;
		holdtime = duration - 2 * fadeouttime;
	}

	tunnel.fadetime = fadeouttime;
	tunnel.fadeintime = tunnel.tunnelstarted + fadeouttime + holdtime;

	UTIL_ScreenFade( player, black, fadeouttime, holdtime, FFADE_OUT | FFADE_STAYOUT | FFADE_PURGE );

	m_Tunneling.AddToTail( tunnel );

	player->SetMoveType( MOVETYPE_NONE );
	player->EnableControl( false );
	player->AddEffects( EF_NODRAW );
	player->AddSolidFlags( FSOLID_NOT_SOLID );

	CPASAttenuationFilter filter( player, "ObjectTunnelTrigger.TeleportSound" );
	EmitSound( filter, player->entindex(), "ObjectTunnelTrigger.TeleportSound" );

	OnTunnelTriggerStart.FireOutput( player, this );
}

float CObjectTunnelTrigger::GetTeleportDuration( void )
{
	float duration = m_flTeleportDuration;

	if ( m_flTeleportVelocity > 0.0f && m_hTunnelExit != NULL )
	{
		Vector delta = m_hTunnelExit->GetAbsOrigin() - GetAbsOrigin();
		float dist = delta.Length();
		duration = dist / m_flTeleportVelocity;
	}
	else if ( m_flTeleportDuration == -1.0f )
	{
		Msg( "obj_tunnel_trigger:  must set TeleportVelocity or TeleportDuration" );
		m_flTeleportDuration = tf_tunnel_time.GetFloat();
	}

	duration = MIN( duration, MAX_TUNNEL_DURATION );
	return duration;
}

bool CObjectTunnelTrigger::KeepTunneling( TunnelPlayer *tunnel )
{
	if ( !tunnel || ( tunnel->player == NULL ) )
	{
		return false;
	}

	float remaining = tunnel->teleporttime - gpGlobals->curtime + 0.5f;
	remaining = MAX( 0.0f, remaining );

	tunnel->iremaining = (int)( remaining );

	if ( !tunnel->exitstarted )
	{
		if ( gpGlobals->curtime > tunnel->fadeintime )
		{
			tunnel->exitstarted = true;
			// Fade user screen to black
			color32 black = {0,0,0,255};
			UTIL_ScreenFade( tunnel->player, black, tunnel->fadetime, 0.0, FFADE_IN | FFADE_PURGE );

			// Move to tunnel exit spot now that we're half-way through teleport
			if ( m_hTunnelExit != NULL )
			{
				tunnel->player->EnableControl( true );
				tunnel->player->RemoveEffects( EF_NODRAW );

				// Change the player to non-solid before the teleport, so the physics system doesn't think he
				// actually moved this distance:
				int OriginalSolidFlags = tunnel->player->GetSolidFlags();
				tunnel->player->AddSolidFlags( FSOLID_NOT_SOLID);

				// Do a placement test to prevent the player from teleporting inside another player, the ground, or just to help
				// prevent badly placed tunnels from causing stuck situations.
				Vector vTarget = m_hTunnelExit->GetAbsOrigin();
				Vector vOriginal = vTarget; 

				if ( !EntityPlacementTest( tunnel->player, vOriginal, vTarget, true ) )
				{
					Warning("Couldn't place entity after tunnel teleport.\n");
				}


				tunnel->player->Teleport( &vTarget /*m_hTunnelExit->GetAbsOrigin()*/, &m_hTunnelExit->GetAbsAngles(), NULL );
				tunnel->player->SnapEyeAngles( m_hTunnelExit->GetAbsAngles() );
				tunnel->player->SetAbsVelocity( vec3_origin );

				// Restore the player's solid flags.
				tunnel->player->SetSolidFlags(OriginalSolidFlags);

			}
		}
// Can't quite do this because the player's weapons are still visible flying across the map even if
//  he is hidden
#if 0
		else if ( gpGlobals->curtime > tunnel->tunnelstarted + tunnel->fadetime )
		{
			float travel_time = tunnel->duration - 2 * tunnel->fadetime;
			if ( travel_time > 0.0f )
			{
				float f = ( gpGlobals->curtime - tunnel->tunnelstarted - tunnel->fadetime  ) / travel_time;
				f = clamp( f, 0.0f, 1.0f );
				if ( m_hTunnelExit != NULL )
				{
					Vector delta = m_hTunnelExit->GetAbsOrigin() - tunnel->startpos;
					Vector currentPos;
					VectorMA( tunnel->startpos, f, delta, currentPos );

					tunnel->player->Teleport( &currentPos, NULL, NULL );
				}
			}
		}
#endif
	}

	if ( tunnel->ilastremaining != tunnel->iremaining &&
		tunnel->needremainigcounter &&
		tunnel->iremaining >= 1 &&
		tunnel->player != NULL )
	{
		// Counter
		ClientPrint( tunnel->player, HUD_PRINTCENTER, UTIL_VarArgs("\nExiting tunnel in %d %s\n", tunnel->iremaining, tunnel->iremaining > 1 ? "seconds" : "second" ) );
	}

	tunnel->ilastremaining = tunnel->iremaining;

	// TODO: Play footstep or some other teleport sounds occasionaly to this player?

	bool done = ( gpGlobals->curtime > tunnel->teleporttime ) ? true : false;
	if ( done )
	{
		color32 black = {0,0,0,255};
		UTIL_ScreenFade( tunnel->player, black, 0.0f, 0.0f, FFADE_IN | FFADE_PURGE );

		tunnel->player->SetMoveType( MOVETYPE_WALK );
		tunnel->player->EnableControl( true );
		tunnel->player->RemoveEffects( EF_NODRAW );
		tunnel->player->RemoveSolidFlags( FSOLID_NOT_SOLID );

		// TODO:  Play an exit sound??
		OnTunnelTriggerEnd.FireOutput( tunnel->player, this );
	}

	return !done;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectTunnelTrigger::TunnelThink()
{
	// Make sure it's not already in the list
	int c = m_Tunneling.Count();
	for ( int i = c - 1; i >= 0; i-- )
	{
		TunnelPlayer *tp = &m_Tunneling[ i ];

		if ( !KeepTunneling( tp ) )
		{
			m_Tunneling.Remove( i );
		}
	}

	SetNextThink( gpGlobals->curtime + TUNNEL_THINK_INTERVAL );
}

void CObjectTunnelTrigger::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "ObjectTunnelTrigger.TeleportSound" );
	PrecacheScriptSound( "ObjectTunnelTrigger.DisabledSound" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectTunnelTrigger::Spawn()
{
	Precache();

	SetSolid( SOLID_BSP );
	AddSolidFlags( FSOLID_TRIGGER );
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
	SetModel( STRING( GetModelName() ) );
	AddFlag( FL_NOTARGET );

	m_bActive = false;
}

//-----------------------------------------------------------------------------
// Purpose: See if we've got a gather point specified 
//-----------------------------------------------------------------------------
void CObjectTunnelTrigger::Activate( void )
{
	BaseClass::Activate();

	if (m_target != NULL_STRING)
	{
		CBaseEntity	*pEnt = gEntList.FindEntityByName( NULL, m_target );
		if ( pEnt && FClassnameIs( pEnt, "info_tunnel_exit" ) )
		{
			m_hTunnelExit = static_cast< CInfoTunnelExit * >( pEnt );
		}
		else
		{
			Msg( "CObjectTunnelTrigger::Activate, unable to connect tunnel to target %s\n",
				STRING( m_target ) );
		}
	}
	else
	{
		Msg( "CObjectTunnelTrigger::Activate, missing target\n" );
	}

	SetActive( true );

	SetThink( TunnelThink );
	SetNextThink( gpGlobals->curtime + TUNNEL_THINK_INTERVAL );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : active - 
//-----------------------------------------------------------------------------
void CObjectTunnelTrigger::SetActive( bool active )
{
	m_bActive = active;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CObjectTunnelTrigger::GetActive( void ) const
{
	return m_bActive;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectTunnelTrigger::InputSetActive( inputdata_t &inputdata )
{
	SetActive( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectTunnelTrigger::InputSetInactive( inputdata_t &inputdata )
{
	SetActive( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectTunnelTrigger::InputToggleActive( inputdata_t &inputdata )
{
	if ( m_bActive )
	{
		SetActive( false );
	}
	else
	{
		SetActive( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CObjectTunnelTrigger::InputSetTarget( inputdata_t &inputdata )
{
	CBaseEntity	*pEnt = gEntList.FindEntityByName( NULL, inputdata.value.String() );
	if ( pEnt && FClassnameIs( pEnt, "info_tunnel_exit" ) )
	{
		m_hTunnelExit = static_cast< CInfoTunnelExit * >( pEnt );
	}
	else
	{
		Msg( "CObjectTunnelTrigger::InputSetTarget: Couldn't find info_tunnel_exit named %s\n",
			inputdata.value.String() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CObjectTunnelTrigger::InputSetTeleportDuration( inputdata_t &inputdata )
{
	m_flTeleportDuration = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CObjectTunnelTrigger::InputSetTeleportVelocity( inputdata_t &inputdata )
{
	m_flTeleportVelocity = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CObjectTunnelTrigger::StartTouch( CBaseEntity *pOther )
{
	if ( !pOther || !pOther->IsPlayer() )
		return;

	// Only works for my team, of course
	if ( !pOther->InSameTeam(  this ) )
		return;

	if ( m_hTunnelExit == NULL )
		return;

	// It's been damaged to the point of being disabled
	if ( !GetActive() )
	{
		// Play a deny sound
		CPASAttenuationFilter filter( pOther, "ObjectTunnelTrigger.DisabledSound" );
		EmitSound( filter, pOther->entindex(), "ObjectTunnelTrigger.DisabledSound" );
		return;
	}

	StartTunneling( (CBaseTFPlayer *)pOther );
}

LINK_ENTITY_TO_CLASS(obj_tunnel_trigger,CObjectTunnelTrigger);

BEGIN_DATADESC( CObjectTunnelTrigger )
	// inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "SetActive", InputSetActive ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetInactive", InputSetInactive ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ToggleActive", InputToggleActive ),

	DEFINE_INPUTFUNC( FIELD_STRING, "SetTarget", InputSetTarget ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTeleportDuration", InputSetTeleportDuration ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTeleportVelocity", InputSetTeleportVelocity ),

	// outputs
	DEFINE_OUTPUT( OnTunnelTriggerStart, "OnTunnelTriggerStart" ),
	DEFINE_OUTPUT( OnTunnelTriggerEnd, "OnTunnelTriggerEnd" ),

	// keyvalues
	DEFINE_KEYFIELD_NOT_SAVED( m_flTeleportDuration, FIELD_FLOAT, "TeleportDuration" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_flTeleportVelocity, FIELD_FLOAT, "TeleportVelocity" ),
END_DATADESC()
