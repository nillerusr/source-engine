//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "soundenvelope.h"
#include "entitylist.h"
#include "ai_basenpc.h"
#include "soundent.h"
#include "physics.h"
#include "physics_saverestore.h"
#include "asw_tesla_trap.h"
#include "movevars_shared.h"
#include "vphysics/constraints.h"
#include "ai_hint.h"
#include "particle_parse.h"
#include "world.h"
#include "asw_gamerules.h"
#include "asw_util_shared.h"
#include "asw_trace_filter_shot.h"
#include "asw_alien.h"
#include "asw_marine.h"

enum
{
	TESLATRAP_STATE_DORMANT = 0,
	TESLATRAP_STATE_DEPLOY,		// Try to lock down and arm
	TESLATRAP_STATE_CHARGING,		// Held in the physgun
	TESLATRAP_STATE_CHARGED,		// Locked down and looking for targets
};

// for the Modification keyfield
enum
{
	TESLATRAP_MODIFICATION_NORMAL  = 0,
	TESLATRAP_MODIFICATION_CAVERN,
};

// the citizen modified skins for the mine (inclusive):
#define TESLATRAP_CITIZEN_SKIN_MIN 1
#define TESLATRAP_CITIZEN_SKIN_MAX 2

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Approximate radius of the bomb's model
#define ASW_TESLATRAP_RADIUS		24

IMPLEMENT_SERVERCLASS_ST( CASW_TeslaTrap, DT_ASW_TeslaTrap )
	SendPropInt( SENDINFO( m_iTrapState )),
	SendPropInt( SENDINFO( m_iAmmo )),
	SendPropInt( SENDINFO( m_iMaxAmmo )),
	SendPropFloat( SENDINFO( m_flRadius )),
	SendPropFloat( SENDINFO( m_flDamage )),
	SendPropFloat( SENDINFO( m_flNextFullChargeTime )),
	SendPropBool( SENDINFO( m_bAssembled )),
END_SEND_TABLE()

BEGIN_DATADESC( CASW_TeslaTrap )
	DEFINE_ENTITYFUNC( TeslaTouch ),
	DEFINE_THINKFUNC( TeslaSearchThink ),
	DEFINE_THINKFUNC( TeslaChargeThink ),
	DEFINE_THINKFUNC( TeslaSettleThink ),

	DEFINE_SOUNDPATCH( m_pWarnSound ),

	//DEFINE_KEYFIELD( m_flExplosionDelay,	FIELD_FLOAT, "ExplosionDelay" ),

	//DEFINE_FIELD( m_bCharged, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hNearestNPC, FIELD_EHANDLE ),

	DEFINE_KEYFIELD( m_bLockSilently, FIELD_BOOLEAN, "LockSilently" ),
	DEFINE_FIELD( m_bFoeNearest, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flIgnoreWorldTime, FIELD_TIME ),
	DEFINE_KEYFIELD( m_bDisarmed, FIELD_BOOLEAN, "StartDisarmed" ),
	//DEFINE_KEYFIELD( m_iModification, FIELD_INTEGER, "Modification" ),

	DEFINE_FIELD( m_bPlacedByPlayer, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_iTrapState, FIELD_INTEGER ),

	DEFINE_FIELD( m_flRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDamage, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextFullChargeTime, FIELD_FLOAT ),

END_DATADESC()

CUtlVector<CASW_TeslaTrap*> g_aTeslaTraps;

CASW_TeslaTrap::CASW_TeslaTrap()
{
	m_pWarnSound = NULL;
	m_bPlacedByPlayer = true;
	g_aTeslaTraps.AddToTail( this );
	m_hMarineDeployer = NULL;
	m_CreatorWeaponClass = (Class_T)CLASS_ASW_UNKNOWN;
}

CASW_TeslaTrap::~CASW_TeslaTrap()
{
	g_aTeslaTraps.FindAndRemove( this );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CASW_TeslaTrap::Precache()
{
	PrecacheModel( "models/items/teslaCoil/teslacoil.mdl" );

	PrecacheScriptSound( "ASW_Tesla_Trap.Zap" );
}

static const char *s_pTeslaAnimThink = "TeslaAnimThink";

//---------------------------------------------------------
//---------------------------------------------------------
void CASW_TeslaTrap::Spawn()
{
	Precache();

	SetModel("models/items/teslaCoil/teslacoil.mdl");

	UTIL_SetSize( this, Vector( -24, -24, -1 ), Vector( 24, 24, 80 ) );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	m_takedamage = DAMAGE_YES;

	SetFriction( 0.9f );
	SetGravity( UTIL_ScaleForGravity( 1000 ) );

	AddEffects( EF_NOSHADOW|EF_NORECEIVESHADOW );
	AddFlag( FL_OBJECT );
	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );

	SetCollisionGroup( ASW_COLLISION_GROUP_PASSABLE );


	m_takedamage = DAMAGE_EVENTS_ONLY;

	SetBloodColor( DONT_BLEED );
	SetHealth( 100 );

	ResetSequence( LookupSequence( "closed" ) );	

	//m_iModification = TESLATRAP_MODIFICATION_CAVERN;

	if( !GetParent() )
	{
		// Create vphysics now if I'm not being carried.
		CreateVPhysics();
	}

	if( m_bDisarmed )
	{
		SetTrapState( TESLATRAP_STATE_DORMANT );
	}
	else
	{
		SetTrapState( TESLATRAP_STATE_DEPLOY );
	}	


	m_flRadius = 200.0f;
	m_flDamage = 5.0f;
	m_iAmmo = 30;
	m_iMaxAmmo = 30;
	m_flChargeInterval = 0.3f;
	m_bAssembled = false;	
	m_bActive = false;
	m_flNextFullChargeTime = gpGlobals->curtime + 1.0f;

	SetContextThink( &CASW_TeslaTrap::RunAnimation, gpGlobals->curtime + 0.1f, s_pTeslaAnimThink );
}


void CASW_TeslaTrap::RunAnimation()
{
	m_flPlaybackRate = 1.0f;
	StudioFrameAdvance();
	DispatchAnimEvents( this );
	if ( m_bAssembled && !m_bActive )
	{
		ResetSequence( LookupSequence( "active" ) );
		m_bActive = true;

	}
	SetContextThink( &CASW_TeslaTrap::RunAnimation, gpGlobals->curtime + 0.1f, s_pTeslaAnimThink );
} 

void CASW_TeslaTrap::ReachedEndOfSequence()
{
	if (  ( GetSequence() == LookupSequence( "deploy") )  )
	{
		m_bAssembled = true;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CASW_TeslaTrap::OnRestore()
{
	BaseClass::OnRestore();

	//if( VPhysicsGetObject() )
	//{
	//	VPhysicsGetObject()->SetCharged();
	//}
}


//---------------------------------------------------------
//---------------------------------------------------------
void CASW_TeslaTrap::SetTrapState( int iState )
{
	m_iTrapState = iState;

	switch( iState )
	{
	case TESLATRAP_STATE_DEPLOY:
		SetThink( &CASW_TeslaTrap::TeslaSettleThink );
		SetTouch( &CASW_TeslaTrap::TeslaTouch );
		SetNextThink( gpGlobals->curtime + 0.1f );
		break;

	case TESLATRAP_STATE_CHARGING:
		m_flNextFullChargeTime = gpGlobals->curtime + m_flChargeInterval;
		SetThink( &CASW_TeslaTrap::TeslaChargeThink );
		SetTouch( NULL );
		SetNextThink( gpGlobals->curtime + 0.05f );
		break;

	case TESLATRAP_STATE_CHARGED:
		SetThink( &CASW_TeslaTrap::TeslaSearchThink );
		SetTouch( NULL );
		SetNextThink( gpGlobals->curtime + 0.05f );
		break;

	default:
		DevMsg("**Unknown Trap State: %d\n", iState );
		break;
	}
}

/*
//---------------------------------------------------------
//---------------------------------------------------------
void CASW_TeslaTrap::UpdateLight( bool bTurnOn, unsigned int r, unsigned int g, unsigned int b, unsigned int a )
{
	if( bTurnOn )
	{
		Assert( a > 0 );

		// Throw the old sprite away
		if( m_hSprite )
		{
			UTIL_Remove( m_hSprite );
			m_hSprite.Set( NULL );
		}

		if( !m_hSprite.Get() )
		{
			Vector up;
			GetVectors( NULL, NULL, &up );

			// Light isn't on.
			m_hSprite = CSprite::SpriteCreate( "sprites/glow01.vmt", GetAbsOrigin() + up * 10.0f, false );
			CSprite *pSprite = (CSprite *)m_hSprite.Get();

			if( m_hSprite )
			{
				pSprite->SetParent( this );		
				pSprite->SetTransparency( kRenderTransAdd, r, g, b, a, kRenderFxNone );
				pSprite->SetScale( 0.95, 0.0 );
			}
		}
		else
		{
			// Update color
			CSprite *pSprite = (CSprite *)m_hSprite.Get();
			pSprite->SetTransparency( kRenderTransAdd, r, g, b, a, kRenderFxNone );
		}
	}

	if( !bTurnOn )
	{
		if( m_hSprite )
		{
			UTIL_Remove( m_hSprite );
			m_hSprite.Set( NULL );
		}
	}
	
	if ( !m_hSprite )
	{
		m_LastSpriteColor.SetRawColor( 0 );
	}
	else
	{
		m_LastSpriteColor.SetColor( r, g, b, a );
	}
}
*/
/*
//---------------------------------------------------------
//---------------------------------------------------------
void CASW_TeslaTrap::SetCharged( bool bCharged )
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	CReliableBroadcastRecipientFilter filter;
	
	if ( bCharged )
		SetTrapState( TESLATRAP_STATE_CHARGED );

	if( !m_pWarnSound )
	{
		m_pWarnSound = controller.SoundCreate( filter, entindex(), "Combine_TeslaTrap.ActiveLoop" );
		controller.Play( m_pWarnSound, 1.0, PITCH_NORM  );
	}

	if( bCharged )
	{
		// Turning on
		if( m_bFoeNearest )
		{
			SetTrapState( TESLATRAP_STATE_CHARGED );
			EmitSound( "Combine_TeslaTrap.TurnOn" );
			controller.SoundChangeVolume( m_pWarnSound, 1.0, 0.1 );
		}
	}
	else
	{
		// Turning off
		if( m_bFoeNearest )
		{
			EmitSound( "Combine_TeslaTrap.TurnOff" );
		}

		SetNearestNPC( NULL );
		controller.SoundChangeVolume( m_pWarnSound, 0.0, 0.1 );
	}

	m_bCharged = bCharged;
}
*/
//---------------------------------------------------------
// Returns distance to the nearest BaseCombatCharacter.
//---------------------------------------------------------
float CASW_TeslaTrap::FindNearestNPC()
{
	float flNearest = (m_flRadius+100.0f * m_flRadius+100.0f) + 1.0;

	// Assume this search won't find anyone.
	SetNearestNPC( NULL );

	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	int nAIs = g_AI_Manager.NumAIs();

	for ( int i = 0; i < nAIs; i++ )
	{
		CAI_BaseNPC *pNPC = ppAIs[ i ];

		if( pNPC->IsAlive() )
		{
			// ignore hidden objects
			if ( pNPC->IsEffectActive( EF_NODRAW ) )
				continue;

			// Don't bother with NPC's that are below me.
			if( (pNPC->EyePosition().z + 4.0f) < GetAbsOrigin().z )
				continue;

			// Disregard things that want to be disregarded
			if( pNPC->Classify() == CLASS_NONE )
				continue; 

			// Disregard bullseyes
			if( pNPC->Classify() == CLASS_BULLSEYE )
				continue;

			// ignore marines
			if( pNPC->Classify() == CLASS_ASW_MARINE || pNPC->Classify() == CLASS_ASW_COLONIST )
				continue;

			float flDist = (GetAbsOrigin() - pNPC->GetAbsOrigin()).LengthSqr();

			if( flDist < flNearest )
			{
				// Now do a visibility test.
				//UTIL_TraceLine( EyePosition(), pNPC->WorldSpaceCenter(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_)
				if( FVisible( pNPC, MASK_SOLID_BRUSHONLY ) )
				{
					CASW_Alien *pAlien = dynamic_cast<CASW_Alien*>( pNPC );
					bool bAlreadyZapped = pAlien && pAlien->IsElectroStunned();

					if ( bAlreadyZapped && m_hNearestNPC.Get() != NULL )
						continue;

					flNearest = flDist;
					SetNearestNPC( pNPC );
				}
			}
		}
	}

	if( m_hNearestNPC.Get() )
	{
		// If sprite is active, update its color to reflect who is nearest.
		if( IsFriend( m_hNearestNPC ) )
		{
			if( m_bFoeNearest )
			{
				// Changing state to where a friend is nearest.

				if( IsFriend( m_hNearestNPC ) )
				{
					// Friend
					//UpdateLight( true, 0, 255, 0, 190 );
					m_bFoeNearest = false;
				}
			}
		}
		else // it's a foe
		{
			if( !m_bFoeNearest )
			{
				// Changing state to where a foe is nearest.
				//UpdateLight( true, 255, 0, 0, 190 );
				m_bFoeNearest = true;
			}
		}
	}

	return sqrt( flNearest );
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CASW_TeslaTrap::IsFriend( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return true;

	int classify = pEntity->Classify();
	bool bIsFriendly = false;

  	if( classify == CLASS_ASW_MARINE )
	{
		bIsFriendly = true;
	}

	if( !m_bPlacedByPlayer )
	{
		return !bIsFriendly;
	}
	else
	{
		return bIsFriendly;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CASW_TeslaTrap::TeslaSearchThink()
{

	if( !UTIL_FindClientInPVS(edict()) )
	{
		// Sleep!
		SetNextThink( gpGlobals->curtime + 0.5 );
		return;
	}

	/*
	if(	(CAI_BaseNPC::m_nDebugBits & bits_debugDisableAI) )
	{
		if( IsCharged() )
		{
			SetCharged(false);
		}

		SetNextThink( gpGlobals->curtime + 0.5 );
		return;
	}*/

	float flNearestNPCDist = FindNearestNPC();

	if( flNearestNPCDist <= m_flRadius && !IsFriend( m_hNearestNPC ) )
	{
		// Don't pop up in the air, just explode if the NPC gets closer than explode radius.
		if ( ZapTarget( m_hNearestNPC ) )
		{
			m_iAmmo = m_iAmmo - 1;
			if ( m_iAmmo.Get() <= 0 )
			{
				SetThink( &CASW_TeslaTrap::SUB_Remove );
			}
		}
	}

	SetNextThink( gpGlobals->curtime + 0.05 );
}

void CASW_TeslaTrap::LayFlat()
{
	QAngle angFacing = GetAbsAngles();
	//if (angFacing[PITCH] > 0 && angFacing[PITCH] < 180.0f)
	angFacing[PITCH] = 0;  //90
	//else
	//	angFacing[PITCH] = 270;
	SetAbsAngles(angFacing);
	//Msg("Laying flat to %f, %f, %f\n", angFacing[PITCH], angFacing[YAW], angFacing[ROLL]);
}

void CASW_TeslaTrap::TeslaTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	// don't touch npcs
	if ( pOther->IsNPC() )
		return;

	// Change our flight characteristics
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetGravity( UTIL_ScaleForGravity( 640 ) );

	// Slow down
	Vector vecNewVelocity = GetAbsVelocity();
	vecNewVelocity.x *= 0.8f;
	vecNewVelocity.y *= 0.8f;
	SetAbsVelocity( vecNewVelocity );

	//Stopped?
	if ( GetAbsVelocity().Length() < 128.0f )
	{
		LayFlat();
		SetAbsVelocity( vec3_origin );
		SetMoveType( MOVETYPE_NONE );
		//RemoveSolidFlags( FSOLID_NOT_SOLID );
		//AddSolidFlags( FSOLID_TRIGGER );
		SetTouch( NULL );
		ResetSequence( LookupSequence( "deploy" ) );
		SetTrapState( TESLATRAP_STATE_CHARGING );
	}
}

int	CASW_TeslaTrap::DrawDebugTextOverlays()
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		NDebugOverlay::EntityText( entindex(),text_offset,CFmtStr( "State: %d", m_iTrapState.Get() ),0 );
		text_offset++;
	}
	return text_offset;
}


void CASW_TeslaTrap::TeslaSettleThink( void )
{
	//float	deltaTime = ( m_flTimeBurnOut - gpGlobals->curtime );
	//if ( m_flTimeBurnOut != -1.0f )
	//{
		//Burned out
		//if ( m_flTimeBurnOut < gpGlobals->curtime )
		//{
		//	UTIL_Remove( this );
		//	return;
		//}
	//}

	if ( GetAbsVelocity().Length() < 128.0f && GetGroundEntity() )//!m_bSettled && 
	{
		TeslaTouch( GetGroundEntity() );	
	}
	
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CASW_TeslaTrap::TeslaChargeThink( void )
{
	if ( m_flNextFullChargeTime <= gpGlobals->curtime )
	{
		SetTrapState( TESLATRAP_STATE_CHARGED );
		return;
	}
	SetNextThink( gpGlobals->curtime + 0.05f );
}

bool CASW_TeslaTrap::ZapTarget( CBaseEntity *pEntity )
{	
	if ( !pEntity )
		return false;	

	if ( !m_bAssembled )
		return false;

	// trace from the device position to the victim
	trace_t shockTR;
	Vector vecShockSrc = WorldSpaceCenter();
	Vector vecAIPos = pEntity->WorldSpaceCenter();
	CASWTraceFilterShot traceFilter( this, NULL, COLLISION_GROUP_NONE );
	AI_TraceLine( vecShockSrc, vecAIPos, MASK_SHOT, &traceFilter, &shockTR );

	if ( shockTR.fraction != 1.0 && shockTR.m_pEnt )
	{
		Vector	vecFXSrc;
		QAngle	vecFXAng;

		GetAttachment( "effects", vecFXSrc, vecFXAng );

		vecAIPos = shockTR.endpos;
		ClearMultiDamage();	
		CTakeDamageInfo shockDmgInfo( this, this, m_flDamage, DMG_SHOCK );					
		Vector vecDir = vecAIPos - vecShockSrc;
		VectorNormalize( vecDir );
		shockDmgInfo.SetDamagePosition( shockTR.endpos );
		shockDmgInfo.SetDamageForce( vecDir );
		shockDmgInfo.ScaleDamageForce( 1.0 );	
		shockDmgInfo.SetWeapon( m_hCreatorWeapon );

		shockTR.m_pEnt->DispatchTraceAttack( shockDmgInfo, vecDir, &shockTR );
		ApplyMultiDamage();

		//shockTR.m_pEnt->EmitSound( "Electricity.Zap" );
		CASW_Alien *pAlien = dynamic_cast<CASW_Alien*>( shockTR.m_pEnt );
		if ( pAlien )
		{
			// pAlien->ElectroShockBig( vecDir * 5, pAlien->GetAbsOrigin() - GetAbsOrigin() );
		}

		// spawn a shock effect
		/*
		CBaseEntity *pHelpHelpImBeingSupressed = (CBaseEntity*) te->GetSuppressHost();
		te->SetSuppressHost( NULL );
		DispatchParticleEffect( "tracer_electricity", vecFXSrc, vecAIPos, vec3_angle );
		te->SetSuppressHost( pHelpHelpImBeingSupressed );
		*/

		CRecipientFilter filter;
		filter.AddAllPlayers();
		UserMessageBegin( filter, "ASWEnemyZappedByTesla" );
		WRITE_FLOAT( vecShockSrc.x );
		WRITE_FLOAT( vecShockSrc.y );
		WRITE_FLOAT( vecShockSrc.z );
		WRITE_SHORT( pEntity->entindex() );
		MessageEnd();

		//UTIL_ASW_ScreenShake( vecShockSrc, 10.0, 100.0, 2.0, 350, SHAKE_START );

		SetTrapState( TESLATRAP_STATE_CHARGING );
		if( IsPlayerPlaced() && m_hMarineDeployer.Get() )
			m_hMarineDeployer->OnWeaponFired( this, 1 );

		return true;
	}

	return false;
}

CASW_TeslaTrap* CASW_TeslaTrap::Tesla_Trap_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon )
{
	CASW_TeslaTrap *pEnt = (CASW_TeslaTrap*)CreateEntityByName( "asw_tesla_trap" );
	pEnt->SetAbsAngles( angles );
	pEnt->Spawn();
	pEnt->SetOwnerEntity( pOwner );
	UTIL_SetOrigin( pEnt, position );
	pEnt->SetAbsVelocity( velocity );
	pEnt->m_hCreatorWeapon.Set( pCreatorWeapon );
	if( pCreatorWeapon )
		pEnt->m_CreatorWeaponClass = pCreatorWeapon->Classify();

	return pEnt;
}

LINK_ENTITY_TO_CLASS( asw_tesla_trap, CASW_TeslaTrap );
