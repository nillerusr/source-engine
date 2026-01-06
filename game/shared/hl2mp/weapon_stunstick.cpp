//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Stun Stick- beating stick with a zappy end
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "weapon_stunstick.h"
#ifdef HL2MP
#include "weapon_hl2mpbasebasebludgeon.h"
#endif
#include "IEffects.h"
#include "debugoverlay_shared.h"

#ifndef CLIENT_DLL
	#include "npc_metropolice.h"
	#include "te_effect_dispatch.h"
	#include "in_buttons.h"
	#ifdef EZ
		#include "RagdollBoogie.h"
	#include "rumble_shared.h"
	#include "gamestats.h"
	#endif
#endif

#ifdef CLIENT_DLL
	
	#include "iviewrender_beams.h"
	#include "beam_shared.h"
	#include "materialsystem/imaterial.h"
	#include "model_types.h"
	#include "c_te_effect_dispatch.h"
	#include "fx_quad.h"
	#include "fx.h"

	extern void DrawHalo( IMaterial* pMaterial, const Vector &source, float scale, float const *color, float flHDRColorScale = 1.0f );
	extern void FormatViewModelAttachment( Vector &vOrigin, bool bInverse );

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar metropolice_move_and_melee;

#ifdef MAPBASE
ConVar    sk_plr_dmg_stunstick	( "sk_plr_dmg_stunstick","0");
ConVar    sk_npc_dmg_stunstick	( "sk_npc_dmg_stunstick","0");
#endif

//-----------------------------------------------------------------------------
// CWeaponStunStick
//-----------------------------------------------------------------------------
IMPLEMENT_NETWORKCLASS_ALIASED( WeaponStunStick, DT_WeaponStunStick )

BEGIN_NETWORK_TABLE( CWeaponStunStick, DT_WeaponStunStick )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_bActive ) ),
	#ifdef EZ
		RecvPropInt( RECVINFO( m_bInSwing ) ),
	#endif
#else
	SendPropInt( SENDINFO( m_bActive ), 1, SPROP_UNSIGNED ),
	#ifdef EZ
		SendPropInt( SENDINFO( m_bInSwing ), 1, SPROP_UNSIGNED ),
	#endif
#endif

END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponStunStick )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_stunstick, CWeaponStunStick );
PRECACHE_WEAPON_REGISTER( weapon_stunstick );


#ifndef CLIENT_DLL

acttable_t	CWeaponStunStick::m_acttable[] = 
{
#ifdef HL2MP
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_SLAM, true },
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_MELEE,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_MELEE,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_MELEE,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_MELEE,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_MELEE,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_MELEE,			false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_MELEE,					false },
#endif
	{ ACT_MELEE_ATTACK1,				ACT_MELEE_ATTACK_SWING,	true },
	{ ACT_IDLE_ANGRY,					ACT_IDLE_ANGRY_MELEE,	true },
#if EXPANDED_HL2_WEAPON_ACTIVITIES
	{ ACT_IDLE,							ACT_IDLE_MELEE,		false },
	{ ACT_RUN,							ACT_RUN_MELEE,		false },
	{ ACT_WALK,							ACT_WALK_MELEE,		false },
#endif

#ifdef MAPBASE
	// HL2:DM activities (for third-person animations in SP)
	{ ACT_RANGE_ATTACK1,                ACT_RANGE_ATTACK_SLAM, true },
	{ ACT_HL2MP_IDLE,                    ACT_HL2MP_IDLE_MELEE,                    false },
	{ ACT_HL2MP_RUN,                    ACT_HL2MP_RUN_MELEE,                    false },
	{ ACT_HL2MP_IDLE_CROUCH,            ACT_HL2MP_IDLE_CROUCH_MELEE,            false },
	{ ACT_HL2MP_WALK_CROUCH,            ACT_HL2MP_WALK_CROUCH_MELEE,            false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,    ACT_HL2MP_GESTURE_RANGE_ATTACK_MELEE,    false },
	{ ACT_HL2MP_GESTURE_RELOAD,            ACT_HL2MP_GESTURE_RELOAD_MELEE,            false },
	{ ACT_HL2MP_JUMP,                    ACT_HL2MP_JUMP_MELEE,                    false },
#if EXPANDED_HL2DM_ACTIVITIES
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK2,	ACT_HL2MP_GESTURE_RANGE_ATTACK2_MELEE,		false },
	{ ACT_HL2MP_WALK,					ACT_HL2MP_WALK_MELEE,						false },
#endif
#endif
};

IMPLEMENT_ACTTABLE(CWeaponStunStick);

#endif


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponStunStick::CWeaponStunStick( void )
{
	// HACK:  Don't call SetStunState because this tried to Emit a sound before
	//  any players are connected which is a bug
	m_bActive = false;

#ifdef CLIENT_DLL
	m_bSwungLastFrame = false;
	m_flFadeTime = FADE_DURATION;	// Start off past the fade point
#ifdef EZ
	m_pStunstickLight = NULL;
#endif
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWeaponStunStick::Spawn()
{
	Precache();

	BaseClass::Spawn();
	AddSolidFlags( FSOLID_NOT_STANDABLE );
}

void CWeaponStunStick::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "Weapon_StunStick.Activate" );
	PrecacheScriptSound( "Weapon_StunStick.Deactivate" );

	PrecacheModel( STUNSTICK_BEAM_MATERIAL );
	PrecacheModel( "sprites/light_glow02_add.vmt" );
	PrecacheModel( "effects/blueflare1.vmt" );
	PrecacheModel( "sprites/light_glow02_add_noz.vmt" );
}

//-----------------------------------------------------------------------------
// Purpose: Get the damage amount for the animation we're doing
// Input  : hitActivity - currently played activity
// Output : Damage amount
//-----------------------------------------------------------------------------
float CWeaponStunStick::GetDamageForActivity( Activity hitActivity )
{
#ifdef MAPBASE
	if ( ( GetOwner() != NULL ) && ( GetOwner()->IsPlayer() ) )
		return sk_plr_dmg_stunstick.GetFloat();
	
	return sk_npc_dmg_stunstick.GetFloat();
#else
	return 40.0f;
#endif
}

//-----------------------------------------------------------------------------
// Attempt to lead the target (needed because citizens can't hit manhacks with the crowbar!)
//-----------------------------------------------------------------------------
extern ConVar sk_crowbar_lead_time;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponStunStick::ImpactEffect( trace_t &traceHit )
{
	
//#ifndef CLIENT_DLL
	
	CEffectData	data;

	data.m_vNormal = traceHit.plane.normal;
	data.m_vOrigin = traceHit.endpos + ( data.m_vNormal * 4.0f );

	DispatchEffect( "StunstickImpact", data );

//#endif

	//FIXME: need new decals
	UTIL_ImpactTrace( &traceHit, DMG_CLUB );
}

#ifndef CLIENT_DLL


int CWeaponStunStick::WeaponMeleeAttack1Condition( float flDot, float flDist )
{
	// Attempt to lead the target (needed because citizens can't hit manhacks with the crowbar!)
	CAI_BaseNPC *pNPC	= GetOwner()->MyNPCPointer();
	CBaseEntity *pEnemy = pNPC->GetEnemy();
	if (!pEnemy)
		return COND_NONE;

	Vector vecVelocity;
	AngularImpulse angVelocity;
	pEnemy->GetVelocity( &vecVelocity, &angVelocity );

	// Project where the enemy will be in a little while, add some randomness so he doesn't always hit
	float dt = sk_crowbar_lead_time.GetFloat();
	dt += random->RandomFloat( -0.3f, 0.2f );
	if ( dt < 0.0f )
		dt = 0.0f;

	Vector vecExtrapolatedPos;
	VectorMA( pEnemy->WorldSpaceCenter(), dt, vecVelocity, vecExtrapolatedPos );

	Vector vecDelta;
	VectorSubtract( vecExtrapolatedPos, pNPC->WorldSpaceCenter(), vecDelta );

	if ( fabs( vecDelta.z ) > 70 )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	Vector vecForward = pNPC->BodyDirection2D( );
	vecDelta.z = 0.0f;
	float flExtrapolatedDot = DotProduct2D( vecDelta.AsVector2D(), vecForward.AsVector2D() );
	if ((flDot < 0.7) && (flExtrapolatedDot < 0.7))
	{
		return COND_NOT_FACING_ATTACK;
	}

	float flExtrapolatedDist = Vector2DNormalize( vecDelta.AsVector2D() );

	if( pEnemy->IsPlayer() )
	{
		//Vector vecDir = pEnemy->GetSmoothedVelocity();
		//float flSpeed = VectorNormalize( vecDir );

		// If player will be in front of me in one-half second, clock his arse.
		Vector vecProjectEnemy = pEnemy->GetAbsOrigin() + (pEnemy->GetAbsVelocity() * 0.35);
		Vector vecProjectMe = GetAbsOrigin();

		if( (vecProjectMe - vecProjectEnemy).Length2D() <= 48.0f )
		{
			return COND_CAN_MELEE_ATTACK1;
		}
	}
/*
	if( metropolice_move_and_melee.GetBool() )
	{
		if( pNPC->IsMoving() )
		{
			flTargetDist *= 1.5f;
		}
	}
*/
	float flTargetDist = 48.0f;
	if ((flDist > flTargetDist) && (flExtrapolatedDist > flTargetDist))
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	return COND_CAN_MELEE_ATTACK1;
}


void CWeaponStunStick::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_MELEE_HIT:
		{
			// Trace up or down based on where the enemy is...
			// But only if we're basically facing that direction
			Vector vecDirection;
			AngleVectors( GetAbsAngles(), &vecDirection );

			CBaseEntity *pEnemy = pOperator->MyNPCPointer() ? pOperator->MyNPCPointer()->GetEnemy() : NULL;
			if ( pEnemy )
			{
				Vector vecDelta;
				VectorSubtract( pEnemy->WorldSpaceCenter(), pOperator->Weapon_ShootPosition(), vecDelta );
				VectorNormalize( vecDelta );
				
				Vector2D vecDelta2D = vecDelta.AsVector2D();
				Vector2DNormalize( vecDelta2D );
				if ( DotProduct2D( vecDelta2D, vecDirection.AsVector2D() ) > 0.8f )
				{
					vecDirection = vecDelta;
				}
			}

			Vector vecEnd;
			VectorMA( pOperator->Weapon_ShootPosition(), 32, vecDirection, vecEnd );
			// Stretch the swing box down to catch low level physics objects
			CBaseEntity *pHurt = pOperator->CheckTraceHullAttack( pOperator->Weapon_ShootPosition(), vecEnd, 
				Vector(-16,-16,-40), Vector(16,16,16), GetDamageForActivity( GetActivity() ), DMG_CLUB, 0.5f, false );
			
			// did I hit someone?
			if ( pHurt )
			{
				// play sound
#ifndef EZ
				WeaponSound( MELEE_HIT );
#else
				// If the stunstick is not charged, play the normal hit sound
				if (m_flChargeAmount <= 1.0f)
				{
					WeaponSound( MELEE_HIT );
				}
				// If the stunstick has any change, play the new alternate hit sound
				else
				{
					WeaponSound( SPECIAL2 );
				}
#endif

				CBasePlayer *pPlayer = ToBasePlayer( pHurt );

				bool bFlashed = false;

#ifdef MAPBASE
				CNPC_MetroPolice *pCop = dynamic_cast<CNPC_MetroPolice *>(pOperator);

				if ( pCop != NULL && pPlayer != NULL )
				{
					// See if we need to knock out this target
					if ( pCop->ShouldKnockOutTarget( pHurt ) )
					{
						float yawKick = random->RandomFloat( -48, -24 );

						//Kick the player angles
						pPlayer->ViewPunch( QAngle( -16, yawKick, 2 ) );

						color32 white = {255,255,255,255};
						UTIL_ScreenFade( pPlayer, white, 0.2f, 1.0f, FFADE_OUT|FFADE_PURGE|FFADE_STAYOUT );
						bFlashed = true;
						
						pCop->KnockOutTarget( pHurt );

						break;
					}
					else
					{
						// Notify that we've stunned a target
						pCop->StunnedTarget( pHurt );
					}
				}
#endif
				
				// Punch angles
				if ( pPlayer != NULL && !(pPlayer->GetFlags() & FL_GODMODE) )
				{
					float yawKick = random->RandomFloat( -48, -24 );

					//Kick the player angles
					pPlayer->ViewPunch( QAngle( -16, yawKick, 2 ) );

					Vector	dir = pHurt->GetAbsOrigin() - GetAbsOrigin();

					// If the player's on my head, don't knock him up
					if ( pPlayer->GetGroundEntity() == pOperator )
					{
						dir = vecDirection;
						dir.z = 0;
					}

					VectorNormalize(dir);

					dir *= 500.0f;

					//If not on ground, then don't make them fly!
					if ( !(pPlayer->GetFlags() & FL_ONGROUND ) )
						 dir.z = 0.0f;

					//Push the target back
					pHurt->ApplyAbsVelocityImpulse( dir );

					if ( !bFlashed )
					{
						color32 red = {128,0,0,128};
						UTIL_ScreenFade( pPlayer, red, 0.5f, 0.1f, FFADE_IN );
					}
					
					// Force the player to drop anyting they were holding
					pPlayer->ForceDropOfCarriedPhysObjects();
				}
				
				// do effect?
			}
			else
			{
				WeaponSound( MELEE_MISS );
			}
		}
		break;
		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}

	#ifdef EZ
//-----------------------------------------------------------------------------
// Purpose: Tells us we're always a translucent entity
//	1upD - Moved from client to server because m_iActivity is not transmitted
//-----------------------------------------------------------------------------
bool CWeaponStunStick::InSwing(void)
{
	// If the alternate fire is charging up, glow
	return m_flChargeAmount > 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: After every frame, check and store if we are mid-swring
//-----------------------------------------------------------------------------
void CWeaponStunStick::ItemPostFrame(void)
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if (  m_flChargeAmount > 0.0f && ( !( pOwner->m_nButtons & IN_ATTACK2 ) || ( pOwner->m_nButtons & IN_ATTACK ) ||  m_flChargeAmount > 3.0f ) )
	{
		if ( m_flNextPrimaryAttack <= gpGlobals->curtime )
		{
			PrimaryAttack();
		}
		else
		{
			m_flChargeAmount = 0.0f;
			m_flLastChargeTime = 0.0f;
		}
	}

	BaseClass::ItemPostFrame();
	m_bInSwing = InSwing();
}

//-----------------------------------------------------------------------------
// Purpose: Charge up the swing
//-----------------------------------------------------------------------------
void CWeaponStunStick::SecondaryAttack()
{
	// Increase charge
	if (m_flLastChargeTime > 0.0f)
	{
		float flChargeInterval = gpGlobals->curtime - m_flLastChargeTime;
		m_flChargeAmount += flChargeInterval;
	}
	else {
		// The charge is just starting, play the sound
		WeaponSound( SPECIAL1 );
	}
	m_flLastChargeTime = gpGlobals->curtime;


}

//------------------------------------------------------------------------------
// Purpose: Implement impact function
//------------------------------------------------------------------------------
void CWeaponStunStick::Hit( trace_t &traceHit, Activity nHitActivity, bool bIsSecondary )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	//Do view kick
	AddViewKick();

	//Make sound for the AI
	CSoundEnt::InsertSound( SOUND_BULLET_IMPACT, traceHit.endpos, 400, 0.2f, pPlayer );

	// This isn't great, but it's something for when the crowbar hits.
	pPlayer->RumbleEffect( RUMBLE_AR2, 0, RUMBLE_FLAG_RESTART );

	CBaseEntity	*pHitEntity = traceHit.m_pEnt;

	//Apply damage to a hit target
	if (pHitEntity != NULL)
	{
		Vector hitDirection;
		pPlayer->EyeVectors( &hitDirection, NULL, NULL );
		VectorNormalize( hitDirection );

		float flBaseDamage = GetDamageForActivity( nHitActivity );
		// Stunstick charge damage function
		// y = 1.5 * (x ^ 2) with a floor of flBaseDamage and a ceiling of 100
		float flDamage = flBaseDamage * MIN( MAX( (m_flChargeAmount * m_flChargeAmount) * 1.5f, 1.0f ), 100.0f);

		CTakeDamageInfo info( GetOwner(), GetOwner(), flDamage, DMG_CLUB );

		if (pPlayer && pHitEntity->IsNPC())
		{
			// If bonking an NPC, adjust damage.
			info.AdjustPlayerDamageInflictedForSkillLevel();
		}

		CalculateMeleeDamageForce( &info, hitDirection, traceHit.endpos );

		// If the hit object is an NPC, and that NPC is now dead - become a server ragdoll and electrify!
		CAI_BaseNPC * pNPC = pHitEntity->MyNPCPointer();
		if ( flDamage > flBaseDamage && pHitEntity->IsNPC() && pNPC != NULL && pNPC->CanBecomeServerRagdoll() && !pNPC->IsEFlagSet( EFL_NO_MEGAPHYSCANNON_RAGDOLL ) && pNPC->m_iHealth - info.GetDamage() <= 0.0f)
		{
			pNPC->BecomeRagdollBoogie( GetOwner(), info.GetDamageForce(), 5.0f, SF_RAGDOLL_BOOGIE_ELECTRICAL );
		}
		else
		{
			pHitEntity->DispatchTraceAttack( info, hitDirection, &traceHit );
			ApplyMultiDamage();
		}

		// Now hit all triggers along the ray that... 
		TraceAttackToTriggers( info, traceHit.startpos, traceHit.endpos, hitDirection );

		if (ToBaseCombatCharacter( pHitEntity ))
		{
			gamestats->Event_WeaponHit( pPlayer, !bIsSecondary, GetClassname(), info );
		}
	}

	// Apply an impact effect
	ImpactEffect( traceHit );
}

BEGIN_DATADESC( CWeaponStunStick )
	DEFINE_FIELD( m_flLastChargeTime, FIELD_TIME ),
	DEFINE_FIELD( m_flChargeAmount, FIELD_INTEGER ),
END_DATADESC()

	#endif
#endif

//-----------------------------------------------------------------------------
// Purpose: Sets the state of the stun stick
//-----------------------------------------------------------------------------
void CWeaponStunStick::SetStunState( bool state )
{
	m_bActive = state;

	if ( m_bActive )
	{
		//FIXME: START - Move to client-side

		Vector vecAttachment;
		QAngle vecAttachmentAngles;

		if (GetAttachment(1, vecAttachment, vecAttachmentAngles))
			g_pEffects->Sparks(vecAttachment);

		//FIXME: END - Move to client-side

		EmitSound( "Weapon_StunStick.Activate" );
	}
	else
	{
		EmitSound( "Weapon_StunStick.Deactivate" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponStunStick::Deploy( void )
{
	SetStunState( true );

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponStunStick::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( BaseClass::Holster( pSwitchingTo ) == false )
		return false;

	SetStunState( false );
	SetWeaponVisible( false );

#ifdef EZ
#ifndef CLIENT_DLL
	// Reset charge
	m_flChargeAmount = 0.0f;
	m_flLastChargeTime = 0.0f;
#endif
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecVelocity - 
//-----------------------------------------------------------------------------
void CWeaponStunStick::Drop( const Vector &vecVelocity )
{
	SetStunState( false );

#ifndef CLIENT_DLL
#ifdef MAPBASE
#ifdef HL2MP
	if (!GetOwner() || GetOwner()->IsNPC())
		BaseClass::Drop(vecVelocity);
	else
		UTIL_Remove( this );
#else
	BaseClass::Drop(vecVelocity);
#endif
#else
	UTIL_Remove( this );
#endif
#endif

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponStunStick::GetStunState( void )
{
	return m_bActive;
}

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Get the attachment point on a viewmodel that a base weapon is using
//-----------------------------------------------------------------------------
bool UTIL_GetWeaponAttachment( C_BaseCombatWeapon *pWeapon, int attachmentID, Vector &absOrigin, QAngle &absAngles )
{
	// This is already correct in third-person
	if ( pWeapon && pWeapon->ShouldDrawUsingViewModel() == false )
	{
		return pWeapon->GetAttachment( attachmentID, absOrigin, absAngles );
	}

	// Otherwise we need to translate the attachment to the viewmodel's version and reformat it
	CBasePlayer *pOwner = ToBasePlayer( pWeapon->GetOwner() );
	
	if ( pOwner != NULL )
	{
		int ret = pOwner->GetViewModel()->GetAttachment( attachmentID, absOrigin, absAngles );
		FormatViewModelAttachment( absOrigin, true );

		return ret;
	}

	// Wasn't found
	return false;
}

#define	BEAM_ATTACH_CORE_NAME	"sparkrear"

//-----------------------------------------------------------------------------
// Purpose: Sets up the attachment point lookup for the model
//-----------------------------------------------------------------------------
void C_WeaponStunStick::SetupAttachmentPoints( void )
{
	// Setup points for both types of views
	if ( ShouldDrawUsingViewModel() )
	{
		const char *szBeamAttachNamesTop[NUM_BEAM_ATTACHMENTS] =
		{
			"spark1a","spark2a","spark3a","spark4a",
			"spark5a","spark6a","spark7a","spark8a",
			"spark9a",
		};

		const char *szBeamAttachNamesBottom[NUM_BEAM_ATTACHMENTS] =
		{
			"spark1b","spark2b","spark3b","spark4b",
			"spark5b","spark6b","spark7b","spark8b",
			"spark9b",
		};
		
		// Lookup and store all connections
		for ( int i = 0; i < NUM_BEAM_ATTACHMENTS; i++ )
		{
			m_BeamAttachments[i].IDs[0] = LookupAttachment( szBeamAttachNamesTop[i] );
			m_BeamAttachments[i].IDs[1] = LookupAttachment( szBeamAttachNamesBottom[i] );
		}

		// Setup the center beam point
		m_BeamCenterAttachment = LookupAttachment( BEAM_ATTACH_CORE_NAME );
	}
	else
	{
		// Setup the center beam point
		m_BeamCenterAttachment = 1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws the stunstick model (with extra effects)
//-----------------------------------------------------------------------------
int C_WeaponStunStick::DrawModel( int flags )
{
	if ( ShouldDraw() == false )
		return 0;

	// Only render these on the transparent pass
#ifdef MAPBASE
	if ( m_bActive && flags & STUDIO_TRANSPARENCY )
#else
	if ( flags & STUDIO_TRANSPARENCY )
#endif
	{
		DrawEffects();
		return 1;
	}

	return BaseClass::DrawModel( flags );
}

//-----------------------------------------------------------------------------
// Purpose: Randomly adds extra effects
//-----------------------------------------------------------------------------
void C_WeaponStunStick::ClientThink( void )
{
	if ( InSwing() == false )
	{
		if ( m_bSwungLastFrame )
		{
			// Start fading
			m_flFadeTime = gpGlobals->curtime;
			m_bSwungLastFrame = false;
		}

		return;
	}

	// Remember if we were swinging last frame
	m_bSwungLastFrame = InSwing();

	if ( IsEffectActive( EF_NODRAW ) )
		return;

	if ( ShouldDrawUsingViewModel() )
	{
		// Update our effects
		if ( gpGlobals->frametime != 0.0f && ( random->RandomInt( 0, 3 ) == 0 ) )
		{		
			Vector	vecOrigin;
			QAngle	vecAngles;

			// Inner beams
			BeamInfo_t beamInfo;

			int attachment = random->RandomInt( 0, 15 );

#ifdef EZ
			bool foundAttachment = UTIL_GetWeaponAttachment(this, attachment, vecOrigin, vecAngles);
			// Don't draw an effect if no attachment data was found
			if (!foundAttachment) {
				return;
			}
#else
			UTIL_GetWeaponAttachment( this, attachment, vecOrigin, vecAngles );
#endif			
			
			::FormatViewModelAttachment( vecOrigin, false );

			CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
			CBaseEntity *pBeamEnt = pOwner->GetViewModel();

			beamInfo.m_vecStart = vec3_origin;
			beamInfo.m_pStartEnt= pBeamEnt;
			beamInfo.m_nStartAttachment = attachment;

			beamInfo.m_pEndEnt	= NULL;
			beamInfo.m_nEndAttachment = -1;
			beamInfo.m_vecEnd = vecOrigin + RandomVector( -8, 8 );

			beamInfo.m_pszModelName = STUNSTICK_BEAM_MATERIAL;
			beamInfo.m_flHaloScale = 0.0f;
			beamInfo.m_flLife = 0.05f;
			beamInfo.m_flWidth = random->RandomFloat( 1.0f, 2.0f );
			beamInfo.m_flEndWidth = 0;
			beamInfo.m_flFadeLength = 0.0f;
			beamInfo.m_flAmplitude = random->RandomFloat( 16, 32 );
			beamInfo.m_flBrightness = 255.0;
			beamInfo.m_flSpeed = 0.0;
			beamInfo.m_nStartFrame = 0.0;
			beamInfo.m_flFrameRate = 1.0f;
			beamInfo.m_flRed = 255.0f;;
			beamInfo.m_flGreen = 255.0f;
			beamInfo.m_flBlue = 255.0f;
			beamInfo.m_nSegments = 16;
			beamInfo.m_bRenderable = true;
			beamInfo.m_nFlags = 0;
			
			beams->CreateBeamEntPoint( beamInfo );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Starts the client-side version thinking
//-----------------------------------------------------------------------------
void C_WeaponStunStick::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		SetupAttachmentPoints();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tells us we're always a translucent entity
//-----------------------------------------------------------------------------
RenderGroup_t C_WeaponStunStick::GetRenderGroup( void )
{
	return RENDER_GROUP_TWOPASS;
}

//-----------------------------------------------------------------------------
// Purpose: Tells us we're always a translucent entity
//-----------------------------------------------------------------------------
bool C_WeaponStunStick::InSwing( void )
{
#ifdef EZ
	return m_bInSwing;
#else
	int activity = GetActivity();

	// FIXME: This is needed until the actual animation works
	if ( ShouldDrawUsingViewModel() == false )
		return true;

	// These are the swing activities this weapon can play
	if ( activity == GetPrimaryAttackActivity() || 
		 activity == GetSecondaryAttackActivity() ||
		 activity == ACT_VM_MISSCENTER ||
		 activity == ACT_VM_MISSCENTER2 )
		return true;

	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Draw our special effects
//-----------------------------------------------------------------------------
void C_WeaponStunStick::DrawThirdPersonEffects( void )
{
	Vector	vecOrigin;
	QAngle	vecAngles;
	float	color[3];
	float	scale;

	CMatRenderContextPtr pRenderContext( materials );
	IMaterial *pMaterial = materials->FindMaterial( STUNSTICK_GLOW_MATERIAL, NULL, false );
	pRenderContext->Bind( pMaterial );

	// Get bright when swung
	if ( InSwing() )
	{
		color[0] = color[1] = color[2] = 0.4f;
		scale = 22.0f;
	}
	else
	{
		color[0] = color[1] = color[2] = 0.1f;
		scale = 20.0f;
	}
	
	// Draw an all encompassing glow around the entire head
	UTIL_GetWeaponAttachment( this, m_BeamCenterAttachment, vecOrigin, vecAngles );
	DrawHalo( pMaterial, vecOrigin, scale, color );

	if ( InSwing() )
	{
		pMaterial = materials->FindMaterial( STUNSTICK_GLOW_MATERIAL2, NULL, false );
		pRenderContext->Bind( pMaterial );

		color[0] = color[1] = color[2] = random->RandomFloat( 0.6f, 0.8f );
		scale = random->RandomFloat( 4.0f, 6.0f );

		// Draw an all encompassing glow around the entire head
		UTIL_GetWeaponAttachment( this, m_BeamCenterAttachment, vecOrigin, vecAngles );
		DrawHalo( pMaterial, vecOrigin, scale, color );

		// Update our effects
		if ( gpGlobals->frametime != 0.0f && ( random->RandomInt( 0, 5 ) == 0 ) )
		{
			Vector	vecOrigin;
			QAngle	vecAngles;

			GetAttachment( 1, vecOrigin, vecAngles );

			Vector	vForward;
			AngleVectors( vecAngles, &vForward );

			Vector vEnd = vecOrigin - vForward * 1.0f;

			// Inner beams
			BeamInfo_t beamInfo;

			beamInfo.m_vecStart = vEnd;
			Vector	offset = RandomVector( -12, 8 );

			offset += Vector(4,4,4);
			beamInfo.m_vecEnd = vecOrigin + offset;

			beamInfo.m_pStartEnt= cl_entitylist->GetEnt( BEAMENT_ENTITY( entindex() ) );
			beamInfo.m_pEndEnt	= cl_entitylist->GetEnt( BEAMENT_ENTITY( entindex() ) );
			beamInfo.m_nStartAttachment = 1;
			beamInfo.m_nEndAttachment = -1;
			
			beamInfo.m_nType = TE_BEAMTESLA;
			beamInfo.m_pszModelName = STUNSTICK_BEAM_MATERIAL;
			beamInfo.m_flHaloScale = 0.0f;
			beamInfo.m_flLife = 0.01f;
			beamInfo.m_flWidth = random->RandomFloat( 1.0f, 3.0f );
			beamInfo.m_flEndWidth = 0;
			beamInfo.m_flFadeLength = 0.0f;
			beamInfo.m_flAmplitude = random->RandomFloat( 1, 2 );
			beamInfo.m_flBrightness = 255.0;
			beamInfo.m_flSpeed = 0.0;
			beamInfo.m_nStartFrame = 0.0;
			beamInfo.m_flFrameRate = 1.0f;
			beamInfo.m_flRed = 255.0f;;
			beamInfo.m_flGreen = 255.0f;
			beamInfo.m_flBlue = 255.0f;
			beamInfo.m_nSegments = 16;
			beamInfo.m_bRenderable = true;
			beamInfo.m_nFlags = FBEAM_SHADEOUT;
			
			beams->CreateBeamPoints( beamInfo );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw our special effects
//-----------------------------------------------------------------------------
void C_WeaponStunStick::DrawFirstPersonEffects( void )
{
	Vector	vecOrigin;
	QAngle	vecAngles;
	float	color[3];
	float	scale;

	CMatRenderContextPtr pRenderContext( materials );
	IMaterial *pMaterial = materials->FindMaterial( STUNSTICK_GLOW_MATERIAL_NOZ, NULL, false );
	// FIXME: Needs to work with new IMaterial system!
	pRenderContext->Bind( pMaterial );

	// Find where we are in the fade
	float fadeAmount = RemapValClamped( gpGlobals->curtime, m_flFadeTime, m_flFadeTime + FADE_DURATION, 1.0f, 0.1f );

	// Get bright when swung
	if ( InSwing() )
	{
		color[0] = color[1] = color[2] = 0.4f;
		scale = 22.0f;
	}
	else
	{
		color[0] = color[1] = color[2] = 0.4f * fadeAmount;
		scale = 20.0f;
	}
	
	if ( color[0] > 0.0f && UTIL_GetWeaponAttachment(this, m_BeamCenterAttachment, vecOrigin, vecAngles))
	{
		// Draw an all encompassing glow around the entire head
		DrawHalo(pMaterial, vecOrigin, scale, color);
	}

	// Draw bright points at each attachment location
	for ( int i = 0; i < (NUM_BEAM_ATTACHMENTS*2)+1; i++ )
	{
		if ( InSwing() )
		{
			color[0] = color[1] = color[2] = random->RandomFloat( 0.05f, 0.5f );
			scale = random->RandomFloat( 4.0f, 5.0f );
		}
		else
		{
			color[0] = color[1] = color[2] = random->RandomFloat( 0.05f, 0.5f ) * fadeAmount;
			scale = random->RandomFloat( 4.0f, 5.0f ) * fadeAmount;
		}

		if ( color[0] > 0.0f && UTIL_GetWeaponAttachment(this, i, vecOrigin, vecAngles))
		{
			DrawHalo( pMaterial, vecOrigin, scale, color );
		}
	}

#ifdef EZ
	// Create a "muzzle flash" light when the stunstick is "in swing" (glowing)
	if ( InSwing() && GetOwner()->IsPlayer() )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if ( pPlayer != NULL )
		{
			m_flLastMuzzleFlashTime = gpGlobals->curtime;
			//// Not using actual muzzle flashes for now
			//pPlayer->DoMuzzleFlash();
		}
	}
#endif
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: Draw our special effects
//-----------------------------------------------------------------------------
void C_WeaponStunStick::DrawNPCEffects( void )
{
	if ( m_bActive )
	{
		Vector	vecOrigin;
		QAngle	vecAngles;
		float	color[3];

		color[0] = color[1] = color[2] = random->RandomFloat( 0.1f, 0.2f );

		GetAttachment( 1, vecOrigin, vecAngles );

		Vector	vForward;
		AngleVectors( vecAngles, &vForward );

		Vector vEnd = vecOrigin - vForward * 1.0f;

		IMaterial *pMaterial = materials->FindMaterial( "effects/stunstick", NULL, false );

		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( pMaterial );
		DrawHalo( pMaterial, vEnd, random->RandomFloat( 4.0f, 6.0f ), color );

		color[0] = color[1] = color[2] = random->RandomFloat( 0.9f, 1.0f );

		DrawHalo( pMaterial, vEnd, random->RandomFloat( 2.0f, 3.0f ), color );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Draw our special effects
//-----------------------------------------------------------------------------
void C_WeaponStunStick::DrawEffects( void )
{
	if ( ShouldDrawUsingViewModel() )
	{
		DrawFirstPersonEffects();
	}
#ifdef MAPBASE
	else if ( GetOwner() && GetOwner()->IsNPC() )
	{
		// Original HL2 stunstick FX
		DrawNPCEffects();
	}
#endif
	else
	{
		DrawThirdPersonEffects();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Viewmodel was drawn
//-----------------------------------------------------------------------------
void C_WeaponStunStick::ViewModelDrawn( C_BaseViewModel *pBaseViewModel )
{
	// Don't bother when we're not deployed
	if ( IsWeaponVisible() )
	{
		// Do all our special effects
		DrawEffects();
	}

	BaseClass::ViewModelDrawn( pBaseViewModel );
}

#ifdef EZ

ConVar    cl_stunstick_flashlight			( "cl_stunstick_flashlight", "1" );
ConVar    cl_stunstick_flashlight_distance	( "cl_stunstick_flashlight", "512" );
ConVar    cl_stunstick_flashlight_intensity	( "cl_stunstick_flashlight_intensity", "0.1" );

//// Not using actual muzzle flashes for now
//void C_WeaponStunStick::ProcessMuzzleFlashEvent()
//{
//	if (ShouldDrawUsingViewModel())
//	{
//		m_flLastMuzzleFlashTime = gpGlobals->curtime;
//		return;
//	}
//
//	BaseClass::ProcessMuzzleFlashEvent();
//}

// Override Simulate() to handle stunstick projected texture
void C_WeaponStunStick::Simulate( void )
{
	if ( cl_stunstick_flashlight.GetBool() )
	{
		bool stunstickLight = gpGlobals->curtime < m_flLastMuzzleFlashTime +  0.05f;

		// The dim light is the flashlight.
		if (stunstickLight)
		{
			if (m_pStunstickLight == NULL)
			{
				// Turned on the headlight; create it.
				m_pStunstickLight = new CFlashlightEffect( 0, MUZZLEFLASH );

				if (m_pStunstickLight == NULL)
					return;

				m_pStunstickLight->TurnOn();
			}

			if ( GetOwner() != NULL && GetOwner()->IsPlayer() )
			{
				CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
				if (pPlayer != NULL)
				{
					Vector vecForward, vecRight, vecUp;
					pPlayer->EyeVectors( &vecForward, &vecRight, &vecUp );
					m_pStunstickLight->UpdateLight( EyePosition(), vecForward, vecRight, vecUp, cl_stunstick_flashlight_distance.GetFloat() );
				}
			}
		}
		else if (m_pStunstickLight)
		{
			m_pStunstickLight->TurnOff();
			// Turned off the flashlight; delete it.
			delete m_pStunstickLight;
			m_pStunstickLight = NULL;
		}
	}

	BaseClass::Simulate();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Draw a cheap glow quad at our impact point (with sparks)
//-----------------------------------------------------------------------------
void StunstickImpactCallback( const CEffectData &data )
{
	float scale = random->RandomFloat( 16, 32 );

	FX_AddQuad( data.m_vOrigin, 
				data.m_vNormal, 
				scale,
				scale*2.0f,
				1.0f, 
				1.0f,
				0.0f,
				0.0f,
				random->RandomInt( 0, 360 ), 
				0,
				Vector( 1.0f, 1.0f, 1.0f ), 
				0.1f, 
				"sprites/light_glow02_add",
				0 );

	FX_Sparks( data.m_vOrigin, 1, 2, data.m_vNormal, 6, 64, 256 );
}

DECLARE_CLIENT_EFFECT( "StunstickImpact", StunstickImpactCallback );

#endif
