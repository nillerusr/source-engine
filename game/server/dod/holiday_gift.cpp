//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "holiday_gift.h"
#include "dod_shareddefs.h"

#define CHRISTMAS_MODEL "models/items/dod_gift.mdl"
												  
LINK_ENTITY_TO_CLASS( holiday_gift, CHolidayGift );
PRECACHE_WEAPON_REGISTER( holiday_gift );

//-----------------------------------------------------------------------------
CHolidayGift* CHolidayGift::Create( const Vector &position, const QAngle &angles, const QAngle &eyeAngles, const Vector &velocity, CBaseCombatCharacter *pOwner )
{
	CHolidayGift *pGift = (CHolidayGift*)CBaseEntity::Create( "holiday_gift", position, angles, pOwner );
	
	if ( pGift )
	{
		pGift->AddSpawnFlags( SF_NORESPAWN );

		Vector vecRight, vecUp;
		AngleVectors( eyeAngles, NULL, &vecRight, &vecUp );

		// Calculate the initial impulse on the gift.
		Vector vecImpulse( 0.0f, 0.0f, 0.0f );
		vecImpulse += vecUp * random->RandomFloat( 0, 0.25 );
		vecImpulse += vecRight * random->RandomFloat( -0.25, 0.25 );
		VectorNormalize( vecImpulse );
		vecImpulse *= random->RandomFloat( 100.0, 150.0 );
		vecImpulse += velocity;

		// Cap the impulse.
		float flSpeed = vecImpulse.Length();
		if ( flSpeed > 300.0 )
		{
			VectorScale( vecImpulse, 300.0 / flSpeed, vecImpulse );
		}

		pGift->SetMoveType( MOVETYPE_FLYGRAVITY );
		pGift->SetAbsVelocity( vecImpulse * 2.f + Vector(0,0,200) );
		pGift->SetAbsAngles( QAngle(0,0,0) );
		pGift->UseClientSideAnimation();
		pGift->ResetSequence( pGift->LookupSequence("idle") );

		pGift->EmitSound( "Christmas.GiftDrop" );

		pGift->ActivateWhenAtRest();
	}

	return pGift;
}

//-----------------------------------------------------------------------------
void CHolidayGift::Precache()
{
	BaseClass::Precache();

	PrecacheModel( CHRISTMAS_MODEL );
	PrecacheScriptSound( "Christmas.GiftDrop" );
	PrecacheScriptSound( "Christmas.GiftPickup" );
}

//-----------------------------------------------------------------------------
void CHolidayGift::Spawn( void )
{ 
	BaseClass::Spawn();

	SetModel( CHRISTMAS_MODEL );

	// Die in 30 seconds
	SetContextThink( &CBaseEntity::SUB_Remove, gpGlobals->curtime + 30, "DIE_THINK" );
	SetContextThink( &CHolidayGift::DropSoundThink, gpGlobals->curtime + 0.2f, "SOUND_THINK" );
}

//-----------------------------------------------------------------------------
void CHolidayGift::DropSoundThink( void )
{
	EmitSound( "Christmas.GiftDrop" );
}

//-----------------------------------------------------------------------------
bool CHolidayGift::MyTouch( CBasePlayer *pPlayer )
{
	if( !pPlayer )
		return false;

	if( !pPlayer->IsAlive() )
		return false;

	if ( pPlayer->IsBot() )
		return false;

	if ( ( pPlayer->GetTeamNumber() != TEAM_ALLIES ) && ( pPlayer->GetTeamNumber() != TEAM_AXIS ) )
		return false;

	// Send a message for the achievement tracking.
	IGameEvent *event = gameeventmanager->CreateEvent( "christmas_gift_grab" );
	if ( event )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		gameeventmanager->FireEvent( event );
	}
	pPlayer->EmitSound( "Christmas.GiftPickup" );

	return true;
}

//-----------------------------------------------------------------------------
void CHolidayGift::ItemTouch( CBaseEntity *pOther )
{
	if ( pOther->IsWorld() )
	{
		Vector absVel = GetAbsVelocity();
		SetAbsVelocity( Vector( 0,0,absVel.z ) );
		return;
	}

	BaseClass::ItemTouch( pOther );
}
