#include "cbase.h"
#include "props.h"
#include "asw_firewall_piece.h"
#include "asw_marine.h"
#include "asw_alien.h"
#include "asw_fire.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_firewall_piece, CASW_Firewall_Piece );
PRECACHE_REGISTER( asw_firewall_piece );
//---------------------------------------------------------
// 
//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST(CASW_Firewall_Piece, DT_ASW_Firewall_Piece)

END_SEND_TABLE()


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Firewall_Piece )
	DEFINE_THINKFUNC( FireThink ),
	DEFINE_ENTITYFUNC( FireTouch ),
	DEFINE_FIELD( m_iLeftFire, FIELD_INTEGER ),
	DEFINE_FIELD( m_iRightFire, FIELD_INTEGER ),
	DEFINE_FIELD( m_fSpawnTime, FIELD_TIME ),
	DEFINE_FIELD( m_fFireDuration, FIELD_FLOAT ),
END_DATADESC()


CASW_Firewall_Piece::CASW_Firewall_Piece()
{
	m_iLeftFire = 0;
	m_iRightFire = 0;
	m_fFireDuration = 30.0f;
	m_hCreatorWeapon = NULL;
}


CASW_Firewall_Piece::~CASW_Firewall_Piece()
{
	m_hCreatorWeapon = NULL;
}

#define ASW_FIREWALL_SPREAD_TIME 0.2f
#define ASW_FIREWALL_SPACING 38.0f

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Firewall_Piece::Spawn( void )
{
	Precache();
	
	SetSolid( SOLID_BBOX );
	SetCollisionBounds( Vector(-12,-12,0), Vector(12,12,60));
	//SetCollisionGroup( COLLISION_GROUP_WEAPON );
	SetCollisionGroup( ASW_COLLISION_GROUP_PASSABLE );
	CollisionProp()->UseTriggerBounds( true, 16 );
	
	
	//SetCollisionGroup( ASW_COLLISION_GROUP_PASSABLE );
	AddSolidFlags( FSOLID_TRIGGER);	  // FSOLID_NOT_SOLID || 
	//SetMoveType( MOVETYPE_FLYGRAVITY );
	
	//BaseClass::Spawn();

	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );	
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	AddEffects( EF_NODRAW );
	
	SetThink( &CASW_Firewall_Piece::FireThink );
	SetNextThink( gpGlobals->curtime + ASW_FIREWALL_SPREAD_TIME );
	m_fSpawnTime = gpGlobals->curtime;
	SetTouch( &CASW_Firewall_Piece::FireTouch );

	// spawn our env fire
	//CFire *pFire = (CFire *)CreateEntityByName( "env_fire" );	
	//pFire->SetAbsAngles( GetAbsAngles() );
	//UTIL_SetOrigin( pFire, GetAbsOrigin() );
	//pFire->Spawn();		
	//pFire->SetAbsVelocity( vec3_origin );

	trace_t tr;
	UTIL_TraceHull( GetAbsOrigin() + Vector( 0.0f,0.0f, 5.0f), GetAbsOrigin() + Vector( 0.0f, 0.0f, -100.0f ), 
					Vector( -30.0f, -30.0f, 0.0f ), Vector( 30.0f, 30.0f, 30.0f ), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

	int nFlags = ( SF_FIRE_START_ON | SF_FIRE_SMOKELESS );

	if ( m_iLeftFire <= 0 || m_iRightFire <= 0 )
	{
		nFlags |= SF_FIRE_NO_SOUND;
	}

	FireSystem_StartFire(tr.endpos, 64, 4, m_fFireDuration, nFlags, GetOwnerEntity(), FIRE_WALL_MINE, GetAbsAngles().y, m_hCreatorWeapon );
}

void CASW_Firewall_Piece::Precache()
{
	BaseClass::Precache();
}


int CASW_Firewall_Piece::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	//return FL_EDICT_ALWAYS;
	return FL_EDICT_DONTSEND;
}

int CASW_Firewall_Piece::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_FULLCHECK );
}

void CASW_Firewall_Piece::FireThink( void )
{
	//NDebugOverlay::Box( GetAbsOrigin(), CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs(), 255, 0, 255, 64, 1.0f );

	// check for spawning out fire to either side
	if (m_iLeftFire > 0 || m_iRightFire> 0)
	{
		if (gpGlobals->curtime - m_fSpawnTime >= ASW_FIREWALL_SPREAD_TIME)
		{
			if (m_iLeftFire > 0)
			{
				// spawn another firewall piece to the left
				CASW_Firewall_Piece* pFirewall = CreateAnotherPiece(false);
				if (pFirewall)
				{
					pFirewall->SetSideFire(m_iLeftFire-1, 0);
				}
				m_iLeftFire = 0;
			}
			if (m_iRightFire > 0)
			{
				// spawn another firewall piece to the right
				CASW_Firewall_Piece* pFirewall = CreateAnotherPiece(true);
				if (pFirewall)
				{
					pFirewall->SetSideFire(0, m_iRightFire-1);
				}
				m_iRightFire = 0;
			}
		}
		SetNextThink( gpGlobals->curtime + 0.001f );
	}
	else
	{
		// if we're done spawning all our children, remove ourself
		SetThink( &CASW_Firewall_Piece::SUB_Remove );
		SetNextThink( gpGlobals->curtime );
	}
}

CASW_Firewall_Piece* CASW_Firewall_Piece::CreateAnotherPiece(bool bRight)
{
	Vector offset;
	QAngle ang = GetAbsAngles();
	if (bRight)
		ang.y+=90;
	else
		ang.y-=90;
	AngleVectors(ang, &offset);
	offset *= ASW_FIREWALL_SPACING;
	Vector start = GetAbsOrigin() + Vector(0,0,20);
	Vector dest = start + offset;
	//todo: trace from abs to dest
	trace_t tr;
	UTIL_TraceLine(start, dest, MASK_SOLID, this, ASW_COLLISION_GROUP_IGNORE_NPCS, &tr);
	if (tr.fraction < 1.0f)
	{
		//Msg("firewall trace got %f startsolid=%d allsolid=%d\n", tr.fraction, tr.startsolid, tr.allsolid);
		//if (tr.m_pEnt)
			//Msg("firewall trace hit %s\n", tr.m_pEnt->GetClassname());
		return NULL;
	}
	//else if (tr.fraction < 1.0f)
		//dest = GetAbsOrigin() + (offset * tr.fraction);
	
	CASW_Firewall_Piece* pFirewall = (CASW_Firewall_Piece *)CreateEntityByName( "asw_firewall_piece" );	
	if (pFirewall)
	{
		pFirewall->SetAbsAngles( GetAbsAngles() );
		pFirewall->SetOwnerEntity(GetOwnerEntity());
		//if (GetOwnerEntity())
			//Msg("Creating another firewall piece with owner %s\n", GetOwnerEntity()->GetClassname());
		//UTIL_SetOrigin( pFirewall, GetAbsOrigin() + offset );
		pFirewall->SetAbsOrigin( dest - Vector(0,0,20) );
		pFirewall->SetDuration(m_fFireDuration);
		pFirewall->m_hCreatorWeapon = m_hCreatorWeapon;
		pFirewall->Spawn();		
		pFirewall->SetAbsVelocity( vec3_origin );
	}
	else
	{
		//Msg("Failed to create firewall piece!");
	}

	return pFirewall;
}

// redundant now, just let the env_fires set people alight
void CASW_Firewall_Piece::FireTouch(CBaseEntity* pOther)
{
	/*
	//Msg("fire touched by %s\n", pOther->GetClassname());
	//todo: set other on fire
	CASW_Marine* pMarine = CASW_Marine::AsMarine( pOther );
	if (pMarine)
	{
		pMarine->Ignite(30.0f);
		//Msg("OUCH OUCH BURNING MARINE\n");
	}
	CASW_Alien* pAlien = dynamic_cast<CASW_Alien*>(pOther);
	if (pAlien)
	{
		pAlien->Ignite(30.0f);
	}
	*/
}