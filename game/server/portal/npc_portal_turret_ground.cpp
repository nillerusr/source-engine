//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Combine gun turret that emerges from a trapdoor in the ground.
//
//=============================================================================//
#include "cbase.h"
#include "npc_turret_ground.h"
#include "ammodef.h"
#include "IEffects.h"
#include "te_effect_dispatch.h"

extern ConVar ai_newgroundturret;
ConVar	turret_ground_damage_multiplier( "turret_ground_damage_multiplier", "8.0", FCVAR_CHEAT );

class CNPC_Portal_GroundTurret : public CNPC_GroundTurret
{
	DECLARE_CLASS( CNPC_Portal_GroundTurret, CNPC_GroundTurret );
	DECLARE_DATADESC();

private:

	float	m_fViewconeDegrees;

public:

	virtual void	Spawn( void );

	virtual float	GetAttackDamageScale( CBaseEntity *pVictim );

	virtual void	Shoot();
	virtual void	Scan();

	virtual int		OnTakeDamage_Alive( const CTakeDamageInfo &info );

};

BEGIN_DATADESC( CNPC_Portal_GroundTurret )
	DEFINE_KEYFIELD( m_fViewconeDegrees,		FIELD_FLOAT,		"ConeOfFire" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_portal_turret_ground, CNPC_Portal_GroundTurret );


void CNPC_Portal_GroundTurret::Spawn( void )
{
	Precache();

	UTIL_SetModel( this, "models/combine_turrets/ground_turret.mdl" );

	SetNavType( NAV_FLY );
	SetSolid( SOLID_VPHYSICS );

	SetBloodColor( DONT_BLEED );
	m_iHealth			= 125;
	m_flFieldOfView		= cos( ((m_fViewconeDegrees / 2.0f) * M_PI / 180.0f) );
	m_NPCState			= NPC_STATE_NONE;

	m_vecSpread.x = 0.5;
	m_vecSpread.y = 0.5;
	m_vecSpread.z = 0.5;

	CapabilitiesClear();

	AddEFlags( EFL_NO_DISSOLVE );

	NPCInit();

	CapabilitiesAdd( bits_CAP_SIMPLE_RADIUS_DAMAGE );

	m_iAmmoType = GetAmmoDef()->Index( "PISTOL" );

	m_pSmoke = NULL;

	m_bHasExploded = false;
	m_bEnabled = false;

	if( ai_newgroundturret.GetBool() )
	{
		m_flSensingDist = 384;
		SetDistLook( m_flSensingDist );
	}
	else
	{
		m_flSensingDist = 2048;
	}

	if( !GetParent() )
	{
		DevMsg("ERROR! npc_ground_turret with no parent!\n");
		UTIL_Remove(this);
		return;
	}

	m_flTimeNextShoot = gpGlobals->curtime;
	m_flTimeNextPing = gpGlobals->curtime;

	m_vecClosedPos = GetAbsOrigin();

	StudioFrameAdvance();

	Vector vecPos;

	GetAttachment( "eyes", vecPos );
	SetViewOffset( vecPos - GetAbsOrigin() );

	GetAttachment( "light", vecPos );
	m_vecLightOffset = vecPos - GetAbsOrigin();
}

float CNPC_Portal_GroundTurret::GetAttackDamageScale( CBaseEntity *pVictim )
{
	CBaseCombatCharacter *pBCC = pVictim->MyCombatCharacterPointer();

	// Do extra damage to antlions & combine
	if ( pBCC )
	{
		if ( pBCC->Classify() == CLASS_PLAYER )
			return turret_ground_damage_multiplier.GetFloat();
	}

	return BaseClass::GetAttackDamageScale( pVictim );
}

void CNPC_Portal_GroundTurret::Shoot()
{
	FireBulletsInfo_t info;

	Vector vecSrc = EyePosition();
	Vector vecDir;

	GetVectors( &vecDir, NULL, NULL );

	for( int i = 0 ; i < 1 ; i++ )
	{
		info.m_vecSrc = vecSrc;

		if( i > 0 || !GetEnemy()->IsPlayer() )
		{
			// Subsequent shots or shots at non-players random
			GetVectors( &info.m_vecDirShooting, NULL, NULL );
			info.m_vecSpread = m_vecSpread;
		}
		else
		{
			// First shot is at the enemy.
			info.m_vecDirShooting = GetActualShootTrajectory( vecSrc );
			info.m_vecSpread = VECTOR_CONE_PRECALCULATED;
		}

		info.m_iTracerFreq = 1;
		info.m_iShots = 1;
		info.m_pAttacker = this;
		info.m_flDistance = MAX_COORD_RANGE;
		info.m_iAmmoType = m_iAmmoType;

		FireBullets( info );

		trace_t tr;
		CTraceFilterSkipTwoEntities traceFilter( this, info.m_pAdditionalIgnoreEnt, COLLISION_GROUP_NONE );
		Vector vecEnd = info.m_vecSrc + vecDir * info.m_flDistance;
		AI_TraceLine( info.m_vecSrc, vecEnd, MASK_SHOT, &traceFilter, &tr );

		if ( tr.m_pEnt && !tr.m_pEnt->IsPlayer() && ( vecDir * info.m_flDistance * tr.fraction ).Length() < 16.0f )
		{
			CTakeDamageInfo damageInfo;
			damageInfo.SetAttacker( this );
			damageInfo.SetDamageType( DMG_BULLET );
			damageInfo.SetDamage( 20.0f );

			TakeDamage( damageInfo );

			EmitSound( "NPC_FloorTurret.DryFire" );
		}
	}

	// Do the AR2 muzzle flash
	CEffectData data;
	data.m_nEntIndex = entindex();
	data.m_nAttachmentIndex = LookupAttachment( "eyes" );
	data.m_flScale = 1.0f;
	data.m_fFlags = MUZZLEFLASH_COMBINE;
	DispatchEffect( "MuzzleFlash", data );

	EmitSound( "NPC_FloorTurret.ShotSounds" );

	m_flTimeNextShoot = gpGlobals->curtime + 0.09;
}

void CNPC_Portal_GroundTurret::Scan( void )
{
	if( m_bSeeEnemy )
	{
		// Using a bool for this check because the condition gets wiped out by changing schedules.
		return;
	}

	if( IsOpeningOrClosing() )
	{
		// Moving.
		return;
	}

	if( !IsOpen() )
	{
		// Closed
		return;
	}

	if( !UTIL_FindClientInPVS(edict()) )
	{
		return;
	}

	if( gpGlobals->curtime >= m_flTimeNextPing )
	{
		EmitSound( "NPC_FloorTurret.Ping" );
		m_flTimeNextPing = gpGlobals->curtime + 1.0f;
	}

	QAngle	scanAngle;
	Vector	forward;
	Vector	vecEye = GetAbsOrigin() + m_vecLightOffset;

	// Draw the outer extents
	scanAngle = GetAbsAngles();
	scanAngle.y += (m_fViewconeDegrees / 2.0f);
	AngleVectors( scanAngle, &forward, NULL, NULL );
	ProjectBeam( vecEye, forward, 1, 30, 0.1 );

	scanAngle = GetAbsAngles();
	scanAngle.y -= (m_fViewconeDegrees / 2.0f);
	AngleVectors( scanAngle, &forward, NULL, NULL );
	ProjectBeam( vecEye, forward, 1, 30, 0.1 );

	// Draw a sweeping beam
	scanAngle = GetAbsAngles();
	scanAngle.y += (m_fViewconeDegrees / 2.0f) * sin( gpGlobals->curtime * 6.0f );
	AngleVectors( scanAngle, &forward, NULL, NULL );
	ProjectBeam( vecEye, forward, 1, 30, 0.3 );
}

int CNPC_Portal_GroundTurret::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// Taking damage from myself, make sure it's fatal.
	CTakeDamageInfo infoCopy = info;

	if ( infoCopy.GetDamageType() == DMG_CRUSH )
	{
		infoCopy.SetDamage( GetHealth() );
		infoCopy.SetDamageType( DMG_REMOVENORAGDOLL | DMG_GENERIC );
	}

	return BaseClass::BaseClass::OnTakeDamage_Alive( infoCopy );
}
