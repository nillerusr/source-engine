//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "tf_walker_strider.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "tf_gamerules.h"
#include "shake.h"
#include "in_buttons.h"
#include "studio.h"


#define STRIDER_AE_FOOTSTEP_LEFT		1
#define STRIDER_AE_FOOTSTEP_RIGHT		2
#define STRIDER_AE_FOOTSTEP_BACK		3
#define STRIDER_AE_FOOTSTEP_LEFTM		4
#define STRIDER_AE_FOOTSTEP_RIGHTM		5
#define STRIDER_AE_FOOTSTEP_BACKM		6
#define STRIDER_AE_FOOTSTEP_LEFTL		7
#define STRIDER_AE_FOOTSTEP_RIGHTL		8
#define STRIDER_AE_FOOTSTEP_BACKL		9


// How many inches/sec the strider's torso moves up and down to get the torso at the right height.
#define STRIDER_TORSO_VERTICAL_SLIDE_SPEED	100
#define STRIDER_FIRE_TIME			5
#define STRIDER_FIRE_INTERVAL		0.3
#define STRIDER_FIRE_ANGLE_ERROR	6	// N degrees of error when he fires.
#define WALKER_STRIDER_MODEL		"models/objects/walker_strider.mdl"


IMPLEMENT_SERVERCLASS_ST( CWalkerStrider, DT_WalkerStrider )
	SendPropInt( SENDINFO( m_bCrouched ), 1, SPROP_UNSIGNED )
END_SEND_TABLE()
LINK_ENTITY_TO_CLASS( walker_strider, CWalkerStrider );
PRECACHE_REGISTER( walker_strider );


char *g_StriderFeet[] =
{
	"left foot",
	"right foot",
	"back foot"
};
#define NUM_STRIDER_FEET ARRAYSIZE( g_StriderFeet )


CWalkerStrider::CWalkerStrider()
{
	m_bCrouched = false;
	m_flOriginToLowestLegHeight = -1;
	m_flWantedZ = -1;
	m_bFiring = false;
	m_vDriverAngles.Init();
}


void CWalkerStrider::Precache()
{
	PrecacheModel( WALKER_STRIDER_MODEL );

	PrecacheScriptSound( "Brush.Footstep" );
	PrecacheScriptSound( "Brush.Fire" );

	BaseClass::Precache();
}


void CWalkerStrider::Spawn()
{
	SpawnWalker( 
		WALKER_STRIDER_MODEL,		// Model name.
		OBJ_WALKER_STRIDER,			// Object type.
		Vector( -186, -157, -504 ), // Placement dimensions.
		Vector( 194, 159, 41 ),
		200,						// Health.
		1,							// Max passengers.
		2.0							// 2x animation speed boost
		);
}

bool CWalkerStrider::IsPassengerVisible( int nRole )
{
	if ( nRole == VEHICLE_ROLE_DRIVER )
		return false;

	return true;
}


void CWalkerStrider::Fire()
{
	EnableWalkMode( false );
	m_bFiring = true;
	m_flFireEndTime = gpGlobals->curtime + STRIDER_FIRE_TIME;
	ResetSequence( LookupSequence( "shoot01a" ) );
	m_flAnimTime = m_flPrevAnimTime = 0;
	SetCycle( 0 );
	m_flNextShootTime = gpGlobals->curtime;
	
	m_vFireAngles[ROLL] = 0;
	m_vFireAngles[PITCH] = m_vDriverAngles[PITCH];
	m_vFireAngles[YAW] = GetAbsAngles()[YAW];
	
	// Play the fire sound.
	CPASAttenuationFilter filter( this, "Brush.Fire" );
	EmitSound( filter, 0, "Brush.Fire", &GetAbsOrigin() );
}


void CWalkerStrider::Crouch()
{
	// Disable the base class's walking functionality while we're crouched.
	EnableWalkMode( false );

	m_bCrouched = true;
	m_flNextCrouchTime = gpGlobals->curtime + 0.3;
	ResetSequence( LookupSequence( "low" ) );
	
	// HACK: there should be a better way to this.. like CBaseAnimating::ResetAnimation,
	// or ResetSequence should do it.
	m_flAnimTime = m_flPrevAnimTime = 0;
	SetCycle( 0 );
	
	// HACK CITY.. This forces it to invalidate the abs origins of the gun bases.
	Vector vPos = GetLocalOrigin();
	SetLocalOrigin( vPos + Vector( 100, 0, 0 ) );
	SetLocalOrigin( vPos );
}


void CWalkerStrider::UnCrouch()
{
	EnableWalkMode( true );
	m_flNextCrouchTime = gpGlobals->curtime + 0.3;
	m_bCrouched = false;

	// HACK CITY.. This forces it to invalidate the abs origins of the gun bases.
	Vector vPos = GetLocalOrigin();
	SetLocalOrigin( vPos + Vector( 100, 0, 0 ) );
	SetLocalOrigin( vPos );
}


void CWalkerStrider::WalkerThink()
{
	float dt = GetTimeDelta();

	BaseClass::WalkerThink();

	if ( m_bCrouched )
	{
		// Just sit there until they hit IN_DUCK again.
		if ( (m_LastButtons & IN_DUCK) && gpGlobals->curtime > m_flNextCrouchTime )
		{
			UnCrouch();
		}
	}
	else
	{
		if ( gpGlobals->curtime > m_flNextCrouchTime )
		{
			if ( m_LastButtons & IN_DUCK )
			{
				// They want to crouch.
				Crouch();
			}
			else if ( !m_bFiring && (m_LastButtons & IN_ATTACK) )
			{
				Fire();
			}
		}
	}

	m_vDriverAngles = m_vLastCmdViewAngles;
	if ( m_bCrouched )
	{
		// The "low" animation gets back up at the end so don't let that happen.
		SetCycle( MIN( GetCycle(), 0.5f ) );
	}
	else if ( m_bFiring )
	{
		if ( gpGlobals->curtime > m_flFireEndTime )
		{
			m_bFiring = false;
			EnableWalkMode( true );
		}
		else
		{
			// Shoot?
			if ( gpGlobals->curtime >= m_flNextShootTime )
			{
				Vector vSrc = GetAbsOrigin();
				Vector vForward;
				
				QAngle vFireAngles = m_vFireAngles;
				vFireAngles[YAW] += RandomFloat( -STRIDER_FIRE_ANGLE_ERROR, STRIDER_FIRE_ANGLE_ERROR );
				vFireAngles[PITCH] += RandomFloat( -STRIDER_FIRE_ANGLE_ERROR, STRIDER_FIRE_ANGLE_ERROR );

				AngleVectors( vFireAngles, &vForward );
				trace_t trace;
				UTIL_TraceLine( vSrc, vSrc + vForward * 2000, MASK_SOLID, this, COLLISION_GROUP_NONE, &trace );

				Vector vHitPos = trace.endpos;
				float flDamageRadius = 100;
				float flDamage = 50;
				
				CBasePlayer *pDriver = GetPassenger( VEHICLE_ROLE_DRIVER );
				if ( pDriver )
				{
					UTIL_ImpactTrace( &trace, DMG_ENERGYBEAM, "Strider" );

					// Tell the client entity to make a beam effect.
					EntityMessageBegin( this, true );
						WRITE_VEC3COORD( vHitPos );
					MessageEnd();

					UTIL_ScreenShake( vHitPos, 10.0, 150.0, 1.0, 100, SHAKE_START );
					RadiusDamage( CTakeDamageInfo( this, pDriver, flDamage, DMG_BLAST ), vHitPos, flDamageRadius, CLASS_NONE, NULL );
				}

				m_flNextShootTime = gpGlobals->curtime + STRIDER_FIRE_INTERVAL;
			}
		}
	}
	else
	{
		// Move our torso within range of our feet.
		if ( m_flOriginToLowestLegHeight != -1 )
		{
			trace_t trace;
			UTIL_TraceLine( 
				GetAbsOrigin(), 
				GetAbsOrigin() - Vector( 0, 0, 2000 ),
				MASK_SOLID_BRUSHONLY, 
				this, 
				COLLISION_GROUP_NONE, 
				&trace );

			if ( trace.fraction < 1 )
			{
				m_flWantedZ = trace.endpos.z + m_flOriginToLowestLegHeight;
			}
			
			// Move our Z towards the wanted Z.
			if ( m_flWantedZ != -1 )
			{
				Vector vCur = GetAbsOrigin();
				vCur.z = Approach( m_flWantedZ, vCur.z, STRIDER_TORSO_VERTICAL_SLIDE_SPEED * dt );
				SetAbsOrigin( vCur );
			}		
		}
	}
}


void CWalkerStrider::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Sapper removal
	if ( RemoveEnemyAttachments( pActivator ) )
		return;

	CBaseTFPlayer *pPlayer = dynamic_cast<CBaseTFPlayer*>(pActivator);
	if ( !pPlayer || !InSameTeam( pPlayer ) )
		return;
	
	// Ok, put them in the driver role.
	AttemptToBoardVehicle( pPlayer );
}


void CWalkerStrider::SetupMove( CBasePlayer *pPlayer, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	BaseClass::SetupMove( pPlayer, ucmd, pHelper, move );
}


//
//
// This is a TOTAL hack, but we don't have any nodes that work well at all for mounted guns.
// This all goes away when we get a new model.
//
//
float sideDist = 90;
float downDist = -400;
bool CWalkerStrider::GetAttachment( int iAttachment, matrix3x4_t &attachmentToWorld )
{
	CStudioHdr *pStudioHdr = GetModelPtr( );
	if ( !pStudioHdr || iAttachment < 1 || iAttachment > pStudioHdr->GetNumAttachments() )
	{
		return false;
	}

	Vector vLocalPos( 0, 0, 0 );
	const mstudioattachment_t &pAttachment = pStudioHdr->pAttachment( iAttachment-1 );
	if ( stricmp( pAttachment.pszName(), "build_point_left_gun" ) == 0 )
	{
		vLocalPos.y = sideDist;
	}
	else if ( stricmp( pAttachment.pszName(), "build_point_right_gun" ) == 0 )
	{
		vLocalPos.y = -sideDist;
	}
	else if ( stricmp( pAttachment.pszName(), "ThirdPersonCameraOrigin" ) == 0 )
	{
	}
	else
	{
		// Ok, it's not one of our magical attachments. Use the regular attachment setup stuff.
		return BaseClass::GetAttachment( iAttachment, attachmentToWorld );
	}

	if ( m_bCrouched )
	{
		vLocalPos.z += downDist;
	}

	// Now build the output matrix.
	matrix3x4_t localMatrix;
	SetIdentityMatrix( localMatrix );
	PositionMatrix( vLocalPos, localMatrix );

	ConcatTransforms( EntityToWorldTransform(), localMatrix, attachmentToWorld );
	return true;
}


bool CWalkerStrider::StartBuilding( CBaseEntity *pBuilder )
{
	if ( !BaseClass::StartBuilding( pBuilder ) )
		return false;

	// Now figure out our ideal Z distance from our lowest foot to our torso.
	Vector vOrigin;
	QAngle vAngles;
	BaseClass::GetAttachment( "left foot", vOrigin, vAngles );
	m_flOriginToLowestLegHeight = GetAbsOrigin().z - vOrigin.z;
	m_flOriginToLowestLegHeight -= 40; // fudge it a little so the legs have room to stretch.
	return true;
}


void CWalkerStrider::FootHit( const char *pFootName )
{
	if ( m_bCrouched || gpGlobals->curtime > m_flDontMakeSoundsUntil )
	{
		Vector footPosition;
		QAngle angles;

		((BaseClass*)this)->GetAttachment( pFootName, footPosition, angles );
		CPASAttenuationFilter filter( this, "Brush.Footstep" );
		EmitSound( filter, entindex(), "Brush.Footstep", &footPosition );

		UTIL_ScreenShake( footPosition, 20.0, 1.0, 0.5, 200, SHAKE_START, false );

		CBasePlayer *pPlayer = GetPassenger( VEHICLE_ROLE_DRIVER );
		if ( pPlayer )
		{
			CTakeDamageInfo info( this, pPlayer, 20, DMG_CLUB );
			TFGameRules()->RadiusDamage( info, footPosition, 200, CLASS_NONE );
		}			

		m_flDontMakeSoundsUntil = gpGlobals->curtime + 0.4;
	}
}


void CWalkerStrider::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
		case STRIDER_AE_FOOTSTEP_LEFT:
		case STRIDER_AE_FOOTSTEP_LEFTM:
		case STRIDER_AE_FOOTSTEP_LEFTL:
			FootHit( "left foot" );
			break;
		
		case STRIDER_AE_FOOTSTEP_RIGHT:
		case STRIDER_AE_FOOTSTEP_RIGHTM:
		case STRIDER_AE_FOOTSTEP_RIGHTL:
			FootHit( "right foot" );
			break;
		
		case STRIDER_AE_FOOTSTEP_BACK:
		case STRIDER_AE_FOOTSTEP_BACKM:
		case STRIDER_AE_FOOTSTEP_BACKL:
			FootHit( "back foot" );
			break;
	}
}

