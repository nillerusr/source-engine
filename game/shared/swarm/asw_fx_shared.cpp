#include "cbase.h"
#include "mathlib/mathlib.h"
#include "util_shared.h"
//#include "model_types.h"
#include "convar.h"
#include "IEffects.h"
//#include "vphysics/object_hash.h"
//#include "IceKey.H"
//#include "checksum_crc.h"
#include "asw_fx_shared.h"
#include "particle_parse.h"

#ifdef CLIENT_DLL
	#include "c_te_effect_dispatch.h"
#else
	#include "te_effect_dispatch.h"
	#include "Sprite.h"
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef GAME_DLL
// link clientside sprite to CSprite - can remove this if we fixup all the maps to not have _clientside ones (do when we're sure we don't need clientside sprites)
LINK_ENTITY_TO_CLASS( env_sprite_clientside, CSprite );
#endif

void UTIL_ASW_EggGibs( const Vector &pos, int iFlags, int iEntIndex )
{
#ifdef GAME_DLL
	Vector vecOrigin = pos;
	CPASFilter filter( vecOrigin );
	UserMessageBegin( filter, "ASWEggEffects" );
	WRITE_FLOAT( vecOrigin.x );
	WRITE_FLOAT( vecOrigin.y );
	WRITE_FLOAT( vecOrigin.z );
	WRITE_SHORT( iFlags );
	WRITE_SHORT( iEntIndex );
	MessageEnd();
#endif
}

void UTIL_ASW_BuzzerDeath( const Vector &pos )
{
#ifdef GAME_DLL
	Vector vecExplosionPos = pos;
	CPASFilter filter( vecExplosionPos );
	UserMessageBegin( filter, "ASWBuzzerDeath" );
	WRITE_FLOAT( vecExplosionPos.x );
	WRITE_FLOAT( vecExplosionPos.y );
	WRITE_FLOAT( vecExplosionPos.z );
	MessageEnd();
#endif
}

void UTIL_ASW_DroneBleed( const Vector &pos, const Vector &dir, int amount )
{
	CEffectData	data;

	data.m_vOrigin = pos;
	data.m_vNormal = dir;
	//data.m_flScale = (float)amount;

	// todo: use filter?
	QAngle	vecAngles;
	VectorAngles( data.m_vNormal, vecAngles );
	DispatchParticleEffect( "drone_shot", data.m_vOrigin, vecAngles );

	//DispatchEffect( "DroneBleed", data );
}

void UTIL_ASW_BloodImpact( const Vector &pos, const Vector &dir, int color, int amount )
{
	CEffectData	data;

	data.m_vOrigin = pos;
	data.m_vNormal = dir;
	data.m_flScale = (float)amount;
	data.m_nColor = (unsigned char)color;

	DispatchEffect( "ASWBloodImpact", data );
}

void UTIL_ASW_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( color == DONT_BLEED || amount == 0 )
		return;

	if ( g_Language.GetInt() == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	if ( amount > 255 )
		amount = 255;

	if (color == BLOOD_COLOR_MECH)
	{
		g_pEffects->Sparks(origin);
		if (random->RandomFloat(0, 2) >= 1)
		{
			UTIL_Smoke(origin, random->RandomInt(10, 15), 10);
		}
	}
	else
	{
		// Normal blood impact
		//UTIL_ASW_BloodImpact( origin, direction, color, amount );
		QAngle	vecAngles;
		VectorAngles( direction, vecAngles );
		if ( amount < 4 )
			DispatchParticleEffect( "marine_bloodsplat_light", origin, vecAngles );
		else
			DispatchParticleEffect( "marine_bloodsplat_heavy", origin, vecAngles );
	}
}

void UTIL_ASW_MarineTakeDamage( const Vector &origin, const Vector &direction, int color, int amount, CASW_Marine *pMarine, bool bFriendly )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( color == DONT_BLEED || amount == 0 )
		return;

	if ( g_Language.GetInt() == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	if ( amount > 255 )
		amount = 255;

	// TODO: use amount to determine large versus small attacks taken?
	QAngle	vecAngles;
	VectorAngles( -direction, vecAngles );
	Vector vecForward, vecRight, vecUp;
	AngleVectors( vecAngles, &vecForward, &vecRight, &vecUp );

#ifdef CLIENT_DLL
	const char *pchEffectName = NULL;
	if ( bFriendly )
		pchEffectName = "marine_hit_blood_ff";
	else
		pchEffectName = "marine_hit_blood";

	CUtlReference< CNewParticleEffect > pEffect;
	pEffect = pMarine->ParticleProp()->Create( pchEffectName, PATTACH_CUSTOMORIGIN );

	if ( pEffect )
	{
		pMarine->ParticleProp()->AddControlPoint( pEffect, 2, pMarine, PATTACH_ABSORIGIN_FOLLOW );
		pEffect->SetControlPoint( 0, origin );//origin - pMarine->GetAbsOrigin()
		pEffect->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );
	}
	else
	{
		Warning( "Could not create effect for marine hurt: %s", pchEffectName );
	}
#endif
}

void UTIL_ASW_GrenadeExplosion( const Vector &vecPos, float flRadius )
{
#ifdef GAME_DLL
	Vector vecExplosionPos = vecPos;
	CPASFilter filter( vecExplosionPos );
	UserMessageBegin( filter, "ASWGrenadeExplosion" );
	WRITE_FLOAT( vecExplosionPos.x );
	WRITE_FLOAT( vecExplosionPos.y );
	WRITE_FLOAT( vecExplosionPos.z );
	WRITE_FLOAT( flRadius );
	MessageEnd();
#endif
}

void UTIL_ASW_EnvExplosionFX( const Vector &vecPos, float flRadius, bool bOnGround )
{
#ifdef GAME_DLL
	Vector vecExplosionPos = vecPos;
	CPASFilter filter( vecExplosionPos );
	UserMessageBegin( filter, "ASWEnvExplosionFX" );
	WRITE_FLOAT( vecExplosionPos.x );
	WRITE_FLOAT( vecExplosionPos.y );
	WRITE_FLOAT( vecExplosionPos.z );
	WRITE_FLOAT( flRadius );
	WRITE_BOOL( bOnGround );
	//damage.bFriendlyFire = msg.ReadOneBit() ? true : false;
	MessageEnd();
#endif
}