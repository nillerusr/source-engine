#include "cbase.h"
#include "asw_laser_mine.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "soundent.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#include "weapon_flaregun.h"
#include "decals.h"
#include "ai_basenpc.h"
#include "asw_marine.h"
#include "asw_firewall_piece.h"
#include "asw_marine_skills.h"
#include "world.h"
#include "asw_util_shared.h"
#include "asw_fx_shared.h"
#include "asw_gamerules.h"
#include "ndebugoverlay.h"
#include "asw_door.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_LASER_MINE_MODEL "models/items/Mine/mine.mdl"
#define ASW_MINE_EXPLODE_TIME 0.6f

extern ConVar asw_debug_mine;

LINK_ENTITY_TO_CLASS( asw_laser_mine, CASW_Laser_Mine );

BEGIN_DATADESC( CASW_Laser_Mine )
	//DEFINE_FUNCTION( LaserThink ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CASW_Laser_Mine, DT_ASW_Laser_Mine )
	SendPropVector( SENDINFO ( m_angLaserAim ), 0, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bFriendly ) ),
	SendPropBool( SENDINFO( m_bMineActive ) ),
END_SEND_TABLE()

CASW_Laser_Mine::CASW_Laser_Mine()
{
	m_bIsSpawnFlipping = false;
	m_bIsSpawnLanded = false;
	m_vecSpawnFlipStartPos = Vector( 0, 0, 0 );
	m_vecSpawnFlipEndPos = Vector( 0, 0, 0 );
	m_angSpawnFlipEndAngle = QAngle( 0, 0, 0 );
	m_flSpawnFlipStartTime = 0;
	m_flSpawnFlipEndTime = 0;
	m_CreatorWeaponClass = (Class_T)CLASS_ASW_UNKNOWN;
}

CASW_Laser_Mine::~CASW_Laser_Mine( void )
{

}

void CASW_Laser_Mine::Spawn( void )
{
	Precache( );
	SetModel( ASW_LASER_MINE_MODEL );
	SetSolid( SOLID_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	m_takedamage	= DAMAGE_NO;

	m_flDamageRadius = 256.0f;	// TODO: grab from marine skill
	m_flDamage = 100.0f;	// TODO: Grab from marine skill
	m_bMineActive = false;
	m_nSkin = 3;

	AddEffects( EF_NOSHADOW|EF_NORECEIVESHADOW );

	SetThink( &CASW_Laser_Mine::LaserThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CASW_Laser_Mine::LaserThink( void )
{
	if ( !m_bMineActive )
	{
		SetNextThink( gpGlobals->curtime );
		return;
	}
		
	UpdateLaser();

	// check for destroying the mine if we attach to a moving door
	if ( GetMoveParent() && GetMoveParent()->Classify() == CLASS_ASW_DOOR )
	{
		CASW_Door *pDoor = assert_cast<CASW_Door*>( GetMoveParent() );
		if ( !pDoor->IsDoorClosed() )
		{
			// do a small explosion when we die in this fashion
			m_flDamage = 10.0f;
			m_flDamageRadius = 64.0f;
			Explode();
		}
	}
	
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CASW_Laser_Mine::StartSpawnFlipping( Vector vecStart, Vector vecEnd, QAngle angEnd, float flDuration )
{
	m_vecSpawnFlipStartPos = vecStart;
	m_vecSpawnFlipEndPos = vecEnd;
	m_angSpawnFlipEndAngle = angEnd;

	StopFollowingEntity( );
	SetMoveType( MOVETYPE_NOCLIP );

	SetGroundEntity( NULL );
	SetTouch(NULL);
	SetSolidFlags( 0 );

	m_flSpawnFlipStartTime = gpGlobals->curtime;
	m_flSpawnFlipEndTime = gpGlobals->curtime + flDuration;
	SetContextThink( &CASW_Laser_Mine::SpawnFlipThink, gpGlobals->curtime, s_pLaserMineSpawnFlipThink );

	SetAbsAngles( m_angSpawnFlipEndAngle );

	QAngle angVel( 0, random->RandomFloat ( 200, 400 ), 0 );
	SetLocalAngularVelocity( angVel );

	m_bIsSpawnFlipping = true;
}

// this is the think that flips the weapon into the world when it is spawned
void CASW_Laser_Mine::SpawnFlipThink()
{
	// we are still flagged as spawn flipping in the air
	if ( m_bIsSpawnFlipping == false )
	{
		// we get here if we spawned, but haven't started spawn flipping yet, please try again!
		SetContextThink( &CASW_Laser_Mine::SpawnFlipThink, gpGlobals->curtime, s_pLaserMineSpawnFlipThink );		
		return;
	}

	// when we should hit the ground
	float flEndTime = m_flSpawnFlipEndTime;

	// the total time it takes for us to flip
	float flFlipTime = flEndTime - m_flSpawnFlipStartTime;
	float flFlipProgress = ( gpGlobals->curtime - m_flSpawnFlipStartTime ) / flFlipTime;
	flFlipProgress = clamp( flFlipProgress, 0.0f, 2.5f );

	if ( !m_bIsSpawnLanded )
	{
		// high gravity, it looks more satisfying
		float flGravity = 2200;

		float flInitialZVelocity = (m_vecSpawnFlipEndPos.z - m_vecSpawnFlipStartPos.z)/flFlipTime + (flGravity/2) * flFlipTime;
		float flZVelocity = flInitialZVelocity - flGravity * (gpGlobals->curtime - m_flSpawnFlipStartTime);

		float flXDiff = (m_vecSpawnFlipEndPos.x - m_vecSpawnFlipStartPos.x) / flFlipTime;
		float flYDiff = (m_vecSpawnFlipEndPos.y - m_vecSpawnFlipStartPos.y) / flFlipTime;

		Vector vecVelocity( flXDiff, flYDiff, flZVelocity );

		SetAbsVelocity( vecVelocity );

		// angular velocity
		QAngle angCurAngVel = GetLocalAngularVelocity();
		float flXAngDiff = 360 / flFlipTime;
		// keep the Y angular velocity that was given to it at the start (it's random)
		SetLocalAngularVelocity( QAngle( flXAngDiff, angCurAngVel.y, 0 ) );
	}

	if ( flFlipProgress >= 1.0f )
	{
		if ( !m_bIsSpawnLanded )
		{
			Vector vecVelStop( 0,0,0 );
			SetAbsVelocity( vecVelStop );

			SetAbsOrigin( m_vecSpawnFlipEndPos );


			QAngle angVel( 0, 0, 0 );
			SetLocalAngularVelocity( angVel );
			/*
			// get the current angles of the item so we can use them to determine the final angles
			QAngle angPrevAngles = GetAbsAngles();
			float flYAngles = angPrevAngles.y;
			QAngle angFlat( 0, flYAngles, 0 );
			*/
			SetAbsAngles( m_angSpawnFlipEndAngle );

			EmitSound("ASW_Laser_Mine.Lay");

			m_bIsSpawnLanded = true;
		}

		if ( flFlipProgress >= 2.5f )
		{
			SetContextThink( NULL, 0, s_pLaserMineSpawnFlipThink );

			EmitSound("ASW_Mine.Lay");

			m_bMineActive = true;
			return;
		}
	}

	SetContextThink( &CASW_Laser_Mine::SpawnFlipThink, gpGlobals->curtime, s_pLaserMineSpawnFlipThink );
}

void CASW_Laser_Mine::Explode( bool bRemove )
{
	// scorch the ground
	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -80 ), MASK_SHOT, 
		this, COLLISION_GROUP_NONE, &tr);

	if ((tr.m_pEnt != GetWorldEntity()) || (tr.hitbox != 0))
	{
		// non-world needs smaller decals
		if( tr.m_pEnt && !tr.m_pEnt->IsNPC() )
		{
			UTIL_DecalTrace( &tr, "SmallScorch" );
		}
	}
	else
	{
		UTIL_DecalTrace( &tr, "Scorch" );
	}

	UTIL_ASW_ScreenShake( GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START );

	UTIL_ASW_GrenadeExplosion( GetAbsOrigin(), m_flDamageRadius );

	EmitSound( "ASW_Laser_Mine.Explode" );

	// damage to nearby things
	CTakeDamageInfo info( this, GetOwnerEntity(), m_flDamage, DMG_BLAST );
	info.SetWeapon( m_hCreatorWeapon );
	ASWGameRules()->RadiusDamage( info, GetAbsOrigin(), m_flDamageRadius, CLASS_NONE, NULL );

	if ( bRemove )
	{
		UTIL_Remove( this );
	}
}

void CASW_Laser_Mine::Precache( void )
{
	PrecacheModel( ASW_LASER_MINE_MODEL );
	PrecacheScriptSound("ASW_Mine.Lay");
	PrecacheScriptSound( "ASW_Laser_Mine.Lay" );
	PrecacheScriptSound( "ASW_Laser_Mine.Explode" );
	PrecacheParticleSystem( "laser_mine" );
	PrecacheParticleSystem( "laser_mine_friendly" );

	BaseClass::Precache();
}

CASW_Laser_Mine* CASW_Laser_Mine::ASW_Laser_Mine_Create( const Vector &position, const QAngle &angles, const QAngle &angLaserAim, CBaseEntity *pOwner, CBaseEntity *pMoveParent, bool bFriendly, CBaseEntity *pCreatorWeapon )
{
	CASW_Laser_Mine *pMine = (CASW_Laser_Mine*)CreateEntityByName( "asw_laser_mine" );
	pMine->SetLaserAngle( angLaserAim );
	
	matrix3x4_t wallMatrix;
	AngleMatrix( angles, wallMatrix );

	QAngle angRotateMine( 0, 90, 90 );
	matrix3x4_t fRotateMatrix;
	AngleMatrix( angRotateMine, fRotateMatrix );

	matrix3x4_t finalMatrix;
	QAngle angMine;
	ConcatTransforms( wallMatrix, fRotateMatrix, finalMatrix );
	MatrixAngles( finalMatrix, angMine );

	Vector vecSrc = pOwner->WorldSpaceCenter();
	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>( pOwner );
	if ( pMarine )
		vecSrc = pMarine->GetOffhandThrowSource();
	pMine->SetAbsAngles( -angMine );
	pMine->Spawn();
	pMine->SetOwnerEntity( pOwner );
	UTIL_SetOrigin( pMine, vecSrc );
	pMine->m_bFriendly = bFriendly;
	pMine->m_hCreatorWeapon.Set( pCreatorWeapon );

	// adjust throw duration based on distance and some randomness
	float flDist = vecSrc.DistTo( position );
	const float flBaseDist = 90.0f;
	float flDistFraction = ( flDist / flBaseDist );
	flDistFraction = clamp<float>( flDistFraction, 0.5f, 2.0f );
	flDistFraction += RandomFloat( 0.0f, 0.2f );
	pMine->StartSpawnFlipping( vecSrc, position, angMine, 0.30f * flDistFraction );

	if( pCreatorWeapon )
		pMine->m_CreatorWeaponClass = pCreatorWeapon->Classify();

	if ( pMoveParent )
	{
		pMine->SetParent( pMoveParent );
		gEntList.AddListenerEntity( pMine );
	}

	return pMine;
}

bool CASW_Laser_Mine::ValidMineTarget(CBaseEntity *pOther)
{
	if ( !pOther )
		return false;

	if ( pOther->IsNPC() )
	{
		if ( m_bFriendly.Get() && pOther->Classify() == CLASS_ASW_MARINE )		// friendly mines don't trigger on marines
			return false;

		return true;
	}

	return false;
}

void CASW_Laser_Mine::StopLoopingSounds()
{
	BaseClass::StopLoopingSounds();

	gEntList.RemoveListenerEntity( this );
}

void CASW_Laser_Mine::OnEntityDeleted( CBaseEntity *pEntity )
{
	// detonate if the thing we're attached to is destroyed
	if ( pEntity == GetMoveParent() )
	{
		Explode( false );
	}
}