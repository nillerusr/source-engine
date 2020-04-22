//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		barnacle - stationary ceiling mounted 'fishing' monster	
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl1_npc_barnacle.h"
#include "npcevent.h"
#include "gib.h"
#include "ai_default.h"
#include "activitylist.h"
#include "hl2_player.h"
#include "vstdlib/random.h"
#include "physics_saverestore.h"
#include "vcollide_parse.h"
#include "engine/IEngineSound.h"

ConVar	sk_barnacle_health( "sk_barnacle_health","25");

//-----------------------------------------------------------------------------
// Private activities.
//-----------------------------------------------------------------------------
static int ACT_EAT = 0;

//-----------------------------------------------------------------------------
// Interactions
//-----------------------------------------------------------------------------
int	g_interactionBarnacleVictimDangle	= 0;
int	g_interactionBarnacleVictimReleased	= 0;
int	g_interactionBarnacleVictimGrab		= 0;

LINK_ENTITY_TO_CLASS( monster_barnacle, CNPC_Barnacle );
IMPLEMENT_CUSTOM_AI( monster_barnacle, CNPC_Barnacle );

//-----------------------------------------------------------------------------
// Purpose: Initialize the custom schedules
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Barnacle::InitCustomSchedules(void) 
{
	INIT_CUSTOM_AI(CNPC_Barnacle);

	ADD_CUSTOM_ACTIVITY(CNPC_Barnacle, ACT_EAT);

	g_interactionBarnacleVictimDangle	= CBaseCombatCharacter::GetInteractionID();
	g_interactionBarnacleVictimReleased	= CBaseCombatCharacter::GetInteractionID();
	g_interactionBarnacleVictimGrab		= CBaseCombatCharacter::GetInteractionID();	
}


BEGIN_DATADESC( CNPC_Barnacle )

	DEFINE_FIELD( m_flAltitude, FIELD_FLOAT ),
	DEFINE_FIELD( m_flKillVictimTime, FIELD_TIME ),
	DEFINE_FIELD( m_cGibs, FIELD_INTEGER ),// barnacle loads up on gibs each time it kills something.
	DEFINE_FIELD( m_fLiftingPrey, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flTongueAdj, FIELD_FLOAT ),
	DEFINE_FIELD( m_flIgnoreTouchesUntil, FIELD_TIME ),

	// Function pointers
	DEFINE_THINKFUNC( BarnacleThink ),
	DEFINE_THINKFUNC( WaitTillDead ),
END_DATADESC()


//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
Class_T	CNPC_Barnacle::Classify ( void )
{
	return	CLASS_ALIEN_MONSTER;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CNPC_Barnacle::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
	case BARNACLE_AE_PUKEGIB:
		CGib::SpawnRandomGibs( this, 1, GIB_HUMAN );	
		break;
	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CNPC_Barnacle::Spawn()
{
	Precache( );

	SetModel( "models/barnacle.mdl" );
	UTIL_SetSize( this, Vector(-16, -16, -32), Vector(16, 16, 0) );

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_NONE );
	SetBloodColor( BLOOD_COLOR_GREEN );
	m_iHealth			= sk_barnacle_health.GetFloat();
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;
	m_flKillVictimTime	= 0;
	m_cGibs				= 0;
	m_fLiftingPrey		= FALSE;
	m_takedamage		= DAMAGE_YES;

	InitBoneControllers();
	InitTonguePosition();

	// set eye position
	SetDefaultEyeOffset();

	SetActivity ( ACT_IDLE );

	SetThink ( &CNPC_Barnacle::BarnacleThink );
	SetNextThink( gpGlobals->curtime + 0.5f );
	//Do not have a shadow
	AddEffects( EF_NOSHADOW );

	m_flIgnoreTouchesUntil = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CNPC_Barnacle::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;
	if ( info.GetDamageType() & DMG_CLUB )
	{
		info.SetDamage( m_iHealth );
	}

	return BaseClass::OnTakeDamage_Alive( info );
}

//-----------------------------------------------------------------------------
// Purpose: Initialize tongue position when first spawned
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Barnacle::InitTonguePosition( void )
{
	CBaseEntity *pTouchEnt;
	float flLength;

	pTouchEnt = TongueTouchEnt( &flLength );
	m_flAltitude = flLength;

	Vector origin;
	QAngle angle;

	GetAttachment( "TongueEnd", origin, angle );

	m_flTongueAdj = origin.z - GetAbsOrigin().z;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Barnacle::BarnacleThink ( void )
{
	CBaseEntity *pTouchEnt;
	float flLength;

	SetNextThink( gpGlobals->curtime + 0.1f );

	if (CAI_BaseNPC::m_nDebugBits & bits_debugDisableAI)
	{
		// AI Disabled, don't do anything
	}
	else if ( GetEnemy() != NULL )
	{
// barnacle has prey.

		if ( !GetEnemy()->IsAlive() )
		{
			// someone (maybe even the barnacle) killed the prey. Reset barnacle.
			m_fLiftingPrey = FALSE;// indicate that we're not lifting prey.
			SetEnemy( NULL );
			return;
		}

		CBaseCombatCharacter* pVictim = GetEnemyCombatCharacterPointer();
		Assert( pVictim );

		if ( m_fLiftingPrey )
		{	

			if ( GetEnemy() != NULL && pVictim->m_lifeState == LIFE_DEAD )
			{
				// crap, someone killed the prey on the way up.
				SetEnemy( NULL );
				m_fLiftingPrey = FALSE;
				return;
			}

	// still pulling prey.
			Vector vecNewEnemyOrigin = GetEnemy()->GetLocalOrigin();
			vecNewEnemyOrigin.x = GetLocalOrigin().x;
			vecNewEnemyOrigin.y = GetLocalOrigin().y;

			// guess as to where their neck is
			// FIXME: remove, ask victim where their neck is
			vecNewEnemyOrigin.x -= 6 * cos(GetEnemy()->GetLocalAngles().y * M_PI/180.0);	
			vecNewEnemyOrigin.y -= 6 * sin(GetEnemy()->GetLocalAngles().y * M_PI/180.0);

			m_flAltitude -= BARNACLE_PULL_SPEED;
			vecNewEnemyOrigin.z += BARNACLE_PULL_SPEED;

			if ( fabs( GetLocalOrigin().z - ( vecNewEnemyOrigin.z + GetEnemy()->GetViewOffset().z ) ) < BARNACLE_BODY_HEIGHT )
			{
		// prey has just been lifted into position ( if the victim origin + eye height + 8 is higher than the bottom of the barnacle, it is assumed that the head is within barnacle's body )
				m_fLiftingPrey = FALSE;

				CPASAttenuationFilter filter( this );
				EmitSound( filter, entindex(), "Barnacle.Bite");

				// Take a while to kill the player
				m_flKillVictimTime = gpGlobals->curtime + 10;
						
				if ( pVictim )
				{
					pVictim->DispatchInteraction( g_interactionBarnacleVictimDangle, NULL, this );
					SetActivity ( (Activity)ACT_EAT );
				}
			}

			CBaseEntity *pEnemy = GetEnemy();

			trace_t trace;
			UTIL_TraceEntity( pEnemy, pEnemy->GetAbsOrigin(), vecNewEnemyOrigin, MASK_SOLID_BRUSHONLY, pEnemy, COLLISION_GROUP_NONE, &trace );

			if( trace.fraction != 1.0 )
			{
				// The victim cannot be moved from their current origin to this new origin. So drop them.
				SetEnemy( NULL );
				m_fLiftingPrey = FALSE;

				if( pEnemy->MyCombatCharacterPointer() )
				{
					pEnemy->MyCombatCharacterPointer()->DispatchInteraction( g_interactionBarnacleVictimReleased, NULL, this );
				}

				// Ignore touches long enough to let the victim move away.
				m_flIgnoreTouchesUntil = gpGlobals->curtime + 1.5;

				SetActivity( ACT_IDLE );

				return;
			}

			UTIL_SetOrigin ( GetEnemy(), vecNewEnemyOrigin );
		}
		else
		{
	// prey is lifted fully into feeding position and is dangling there.

			if ( m_flKillVictimTime != -1 && gpGlobals->curtime > m_flKillVictimTime )
			{
				// kill!
				if ( pVictim )
				{
					// DMG_CRUSH added so no physics force is generated
					pVictim->TakeDamage( CTakeDamageInfo( this, this, pVictim->m_iHealth, DMG_SLASH | DMG_ALWAYSGIB | DMG_CRUSH ) );
					m_cGibs = 3;
				}

				return;
			}

			// bite prey every once in a while
			if ( pVictim && ( random->RandomInt( 0, 49 ) == 0 ) )
			{
				CPASAttenuationFilter filter( this );
				EmitSound( filter, entindex(), "Barnacle.Chew" );

				if ( pVictim )
				{
					pVictim->DispatchInteraction( g_interactionBarnacleVictimDangle, NULL, this );
				}
			}
		}
	}
	else
	{
// barnacle has no prey right now, so just idle and check to see if anything is touching the tongue.

		// If idle and no nearby client, don't think so often. Client should be out of PVS and not within 50 feet.
		if ( !UTIL_FindClientInPVS(edict()) )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex(1);

			if( pPlayer )
			{
				Vector vecDist = pPlayer->GetAbsOrigin() - GetAbsOrigin();

				if( vecDist.Length2DSqr() >= Square(600.0f) )
				{
					SetNextThink( gpGlobals->curtime + 1.5f );
				}
			}
		}

		if ( IsActivityFinished() )
		{// this is done so barnacle will fidget.
			SetActivity ( ACT_IDLE );
		}

		if ( m_cGibs && random->RandomInt(0,99) == 1 )
		{
			// cough up a gib.
			CGib::SpawnRandomGibs( this, 1, GIB_HUMAN );
			m_cGibs--;

			CPASAttenuationFilter filter( this );
			EmitSound( filter, entindex(), "Barnacle.Chew" );
		}

		pTouchEnt = TongueTouchEnt( &flLength );

		//NDebugOverlay::Box( GetAbsOrigin() - Vector( 0, 0, flLength ), Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), 255,0,0, 0, 0.1 );

		if ( pTouchEnt != NULL )
		{
			// tongue is fully extended, and is touching someone.
			CBaseCombatCharacter* pBCC = (CBaseCombatCharacter *)pTouchEnt;

			// FIXME: humans should return neck position
			Vector vecGrabPos = pTouchEnt->GetAbsOrigin();

			if ( pBCC && pBCC->DispatchInteraction( g_interactionBarnacleVictimGrab, &vecGrabPos, this ) )
			{
				CPASAttenuationFilter filter( this );
				EmitSound( filter, entindex(), "Barnacle.Alert" );

				SetSequenceByName ( "attack1" );

				SetEnemy( pTouchEnt );

				pTouchEnt->SetMoveType( MOVETYPE_FLY );
				pTouchEnt->SetAbsVelocity( vec3_origin );
				pTouchEnt->SetBaseVelocity( vec3_origin );
				Vector origin = GetAbsOrigin();
				origin.z = pTouchEnt->GetAbsOrigin().z;
				pTouchEnt->SetLocalOrigin( origin );
				
				m_fLiftingPrey = TRUE;// indicate that we should be lifting prey.
				m_flKillVictimTime = -1;// set this to a bogus time while the victim is lifted.

				m_flAltitude = (GetAbsOrigin().z - vecGrabPos.z);
			}
		}
		else
		{
			// calculate a new length for the tongue to be clear of anything else that moves under it. 
			if ( m_flAltitude < flLength )
			{
				// if tongue is higher than is should be, lower it kind of slowly.
				m_flAltitude += BARNACLE_PULL_SPEED;
			}
			else
			{
				m_flAltitude = flLength;
			}

		}

	}

	// ALERT( at_console, "tounge %f\n", m_flAltitude + m_flTongueAdj );
	//NDebugOverlay::Box( GetAbsOrigin() - Vector( 0, 0, m_flAltitude ), Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), 255,255,255, 0, 0.1 );

	SetBoneController( 0, -(m_flAltitude + m_flTongueAdj) );
	StudioFrameAdvance();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Barnacle::Event_Killed( const CTakeDamageInfo &info )
{
	AddSolidFlags( FSOLID_NOT_SOLID );
	m_takedamage		= DAMAGE_NO;
	m_lifeState			= LIFE_DEAD;
	if ( GetEnemy() != NULL )
	{
		CBaseCombatCharacter *pVictim = GetEnemyCombatCharacterPointer();

		if ( pVictim )
		{
			pVictim->DispatchInteraction( g_interactionBarnacleVictimReleased, NULL, this );
		}
	}

	CGib::SpawnRandomGibs( this, 4, GIB_HUMAN );

	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Barnacle.Die" );

	SetActivity ( ACT_DIESIMPLE );
	SetBoneController( 0, 0 );

	StudioFrameAdvance();

	SetNextThink( gpGlobals->curtime + 0.1f );
	SetThink ( &CNPC_Barnacle::WaitTillDead );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Barnacle::WaitTillDead ( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );

	StudioFrameAdvance();
	DispatchAnimEvents ( this );

	if ( IsActivityFinished() )
	{
		// death anim finished. 
		StopAnimation();
		SetThink ( NULL );
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_Barnacle::Precache()
{
	PrecacheModel("models/barnacle.mdl");

	PrecacheScriptSound( "Barnacle.Bite" );
	PrecacheScriptSound( "Barnacle.Chew" );
	PrecacheScriptSound( "Barnacle.Alert" );
	PrecacheScriptSound( "Barnacle.Die" );

	BaseClass::Precache();
}	

//=========================================================
// TongueTouchEnt - does a trace along the barnacle's tongue
// to see if any entity is touching it. Also stores the length
// of the trace in the int pointer provided.
//=========================================================
#define BARNACLE_CHECK_SPACING	8
CBaseEntity *CNPC_Barnacle::TongueTouchEnt ( float *pflLength )
{
	trace_t		tr;
	float		length;

	// trace once to hit architecture and see if the tongue needs to change position.
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() - Vector ( 0 , 0 , 2048 ), 
		MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
	
	length = fabs( GetAbsOrigin().z - tr.endpos.z );
	// Pull it up a tad
	length -= 16;
	if ( pflLength )
	{
		*pflLength = length;
	}

	// Don't try to touch any prey.
	if ( m_flIgnoreTouchesUntil > gpGlobals->curtime )
		return NULL;

	Vector delta = Vector( BARNACLE_CHECK_SPACING, BARNACLE_CHECK_SPACING, 0 );
	Vector mins = GetAbsOrigin() - delta;
	Vector maxs = GetAbsOrigin() + delta;
	maxs.z = GetAbsOrigin().z;
	
	// Take our current tongue's length or a point higher if we hit a wall 
	// NOTENOTE: (this relieves the need to know if the tongue is currently moving)
	mins.z -= MIN( m_flAltitude, length );

	CBaseEntity *pList[10];
	int count = UTIL_EntitiesInBox( pList, 10, mins, maxs, (FL_CLIENT|FL_NPC) );
	if ( count )
	{
		for ( int i = 0; i < count; i++ )
		{
			CBaseCombatCharacter *pVictim = ToBaseCombatCharacter( pList[ i ] );

			bool bCanHurt = false;

			if ( IRelationType( pList[i] ) == D_HT || IRelationType( pList[i] ) == D_FR )
				 bCanHurt = true;

			if ( pList[i] != this && bCanHurt == true && pVictim->m_lifeState == LIFE_ALIVE )
			{
				return pList[i];
			}
		}
	}

	return NULL;
}
