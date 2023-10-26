//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements an explosion entity and a support spark shower entity.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "decals.h"
#include "asw_env_explosion.h"
#include "dlight.h"
#include "vstdlib/random.h"
#include "tier1/strtools.h"
#include "shareddefs.h"
#include "asw_gamerules.h"
#include "asw_fx_shared.h"
//#include "ieffects.h"
//#include "fx.h"
//#include "tempent.h"
#include "iefx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class CASWEnvExplosion : public CPointEntity
{
public:
	DECLARE_CLASS( CASWEnvExplosion, CPointEntity );

	CASWEnvExplosion( void )
	{
	};

	void Precache( void );
	void Spawn( );
	//void Smoke ( void );
	void SetCustomDamageType( int iType ) { m_iCustomDamageType = iType; }
	//bool KeyValue( const char *szKeyName, const char *szValue );

	int DrawDebugTextOverlays(void);

	// Input handlers
	void InputExplode( inputdata_t &inputdata );

	DECLARE_DATADESC();

	int m_iDamage;// how large is the fireball? how much damage?
	int m_iRadiusOverride;// For use when m_iDamage results in larger radius than designer desires.
	//int m_spriteScale; // what's the exact fireball sprite scale? 
	float m_flDamageForce;	// How much damage force should we use?
	string_t m_iszExplosionSound;
	EHANDLE m_hInflictor;
	int m_iCustomDamageType;

	// passed along to the RadiusDamage call
	int m_iClassIgnore;
	EHANDLE m_hEntityIgnore;

};

LINK_ENTITY_TO_CLASS( asw_env_explosion, CASWEnvExplosion );

BEGIN_DATADESC( CASWEnvExplosion )

	DEFINE_KEYFIELD( m_iDamage, FIELD_INTEGER, "iDamage" ),
	DEFINE_KEYFIELD( m_iRadiusOverride, FIELD_INTEGER, "iRadiusOverride" ),
	//DEFINE_FIELD( m_spriteScale, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_flDamageForce, FIELD_FLOAT, "DamageForce" ),
	DEFINE_FIELD( m_hInflictor, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iCustomDamageType, FIELD_INTEGER ),

	DEFINE_KEYFIELD( m_iClassIgnore, FIELD_INTEGER, "ignoredClass" ),
	DEFINE_KEYFIELD( m_hEntityIgnore, FIELD_EHANDLE, "ignoredEntity" ),

	DEFINE_KEYFIELD( m_iszExplosionSound, FIELD_SOUNDNAME, "ExplosionSound" ),

	// Function Pointers
	//DEFINE_THINKFUNC( Smoke ),

	// Inputs
	DEFINE_INPUTFUNC(FIELD_VOID, "Explode", InputExplode),

END_DATADESC()

/*
bool CASWEnvExplosion::KeyValue( const char *szKeyName, const char *szValue )
{
	return true;
}
*/

void CASWEnvExplosion::Precache( void )
{
	//PrecacheParticleSystem( "freeze_explosion" ) ;
	//PrecacheScriptSound( "explode_3" );

	PrecacheParticleSystem( "asw_env_explosion" );

	if( !( m_spawnflags & SF_ENVEXPLOSION_NOSOUND ) )
	{
		UTIL_ValidateSoundName( m_iszExplosionSound, "ASW_Explosion.Explosion_Default" );
		PrecacheScriptSound( (char *) STRING(m_iszExplosionSound) );
	}

}

void CASWEnvExplosion::Spawn( void )
{ 
	Precache();

	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );

	SetMoveType( MOVETYPE_NONE );

	//m_spriteScale = (int)flSpriteScale;
	m_iCustomDamageType = -1;
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for making the explosion explode.
//-----------------------------------------------------------------------------
void CASWEnvExplosion::InputExplode( inputdata_t &inputdata )
{ 
	trace_t tr;

	SetModelName( NULL_STRING );//invisible
	SetSolid( SOLID_NONE );// intangible

	Vector vecSpot = GetAbsOrigin() + Vector( 0 , 0 , 8 );
	UTIL_TraceLine( vecSpot, vecSpot + Vector( 0, 0, -40 ), (MASK_SOLID_BRUSHONLY | MASK_WATER), this, COLLISION_GROUP_NONE, &tr );
	
	// Pull out of the wall a bit. We used to move the explosion origin itself, but that seems unnecessary, not to mention a
	// little weird when you consider that it might be in hierarchy. Instead we just calculate a new virtual position at
	// which to place the explosion. We don't use that new position to calculate radius damage because according to Steve's
	// comment, that adversely affects the force vector imparted on explosion victims when they ragdoll.
	Vector vecExplodeOrigin = GetAbsOrigin();
	if ( tr.fraction != 1.0 )
	{
		vecExplodeOrigin = tr.endpos + (tr.plane.normal * 24 );
	}

	// draw decal
	if (! ( m_spawnflags & SF_ENVEXPLOSION_NODECAL ))
	{
		if ( ! ( m_spawnflags & SF_ENVEXPLOSION_ICE ))
			UTIL_DecalTrace( &tr, "Scorch" );
		else
			UTIL_DecalTrace( &tr, "Ice_Explosion_Decal" );
	}

	//Get the damage override if specified
	float	flRadius = ( m_iRadiusOverride > 0 ) ? ((float)m_iRadiusOverride/100.0f) : ( (float)m_iDamage / 100.0f );

	// do damage
	if ( !( m_spawnflags & SF_ENVEXPLOSION_NODAMAGE ) )
	{
		CBaseEntity *pAttacker = GetOwnerEntity() ? GetOwnerEntity() : this;

		// Only calculate damage type if we didn't get a custom one passed in
		int iDamageType = m_iCustomDamageType;
		if ( iDamageType == -1 )
		{
			iDamageType = HasSpawnFlags( SF_ENVEXPLOSION_GENERIC_DAMAGE ) ? DMG_GENERIC : DMG_BLAST;
		}

		CTakeDamageInfo info( m_hInflictor ? m_hInflictor : this, pAttacker, m_iDamage, iDamageType );

		if( HasSpawnFlags( SF_ENVEXPLOSION_SURFACEONLY ) )
		{
			info.AddDamageType( DMG_BLAST_SURFACE );
		}

		if ( m_flDamageForce )
		{
			// Not the right direction, but it'll be fixed up by RadiusDamage.
			info.SetDamagePosition( GetAbsOrigin() );
			info.SetDamageForce( Vector( m_flDamageForce, 0, 0 ) );
		}
		
		ASWGameRules()->RadiusDamage( info, GetAbsOrigin(), flRadius, m_iClassIgnore, m_hEntityIgnore.Get() );
	}

	// emit the sound
	if( !(m_spawnflags & SF_ENVEXPLOSION_NOSOUND) )
	{
		EmitSound( STRING(m_iszExplosionSound) );
	}

	// do the particle effect
	if( !(m_spawnflags & SF_ENVEXPLOSION_NOFIREBALL) )
	{
		bool bOnGround = false;
		if ( tr.fraction != 1.0 )
			bOnGround = true;

		UTIL_ASW_EnvExplosionFX( vecExplodeOrigin, flRadius, bOnGround );
	}

	/*
	// do a dlight
	if ( !( m_spawnflags & SF_ENVEXPLOSION_NODLIGHTS ) )
	{
		// Make an elight
		dlight_t *el = effects->CL_AllocDlight( LIGHT_INDEX_TE_DYNAMIC + entindex() );
		el->origin = vecExplodeOrigin;
		el->radius = random->RandomFloat( flRadius + 54.0f, flRadius + 74.0f ); 
		el->decay = el->radius / 0.05f;
		el->die = gpGlobals->curtime + 0.05f;
		Color c = Color( 255, 192, 64, 6 );
		el->color.r = c.r();
		el->color.g = c.g();
		el->color.b = c.b();
		el->color.exponent = c.a(); 
	}
	*/

	if ( !(m_spawnflags & SF_ENVEXPLOSION_REPEATABLE) )
	{
		SetThink( &CASWEnvExplosion::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 1.0f );
	}
}

/*
void CASWEnvExplosion::Smoke( void )
{
	if ( !(m_spawnflags & SF_ENVEXPLOSION_REPEATABLE) )
	{
		UTIL_Remove( this );
	}
}
*/

// HACKHACK -- create one of these and fake a keyvalue to get the right explosion setup
void ASWExplosionCreate( const Vector &center, const QAngle &angles, 
	CBaseEntity *pOwner, int magnitude, int radius, int nSpawnFlags, float flExplosionForce, CBaseEntity *pInflictor, int iCustomDamageType,
	const EHANDLE *ignoredEntity , Class_T ignoredClass )
{
	char			buf[128];

	CASWEnvExplosion *pExplosion = (CASWEnvExplosion*)CBaseEntity::Create( "asw_env_explosion", center, angles, pOwner );
	Q_snprintf( buf,sizeof(buf), "%3d", magnitude );
	char *szKeyName = "iDamage";
	char *szValue = buf;
	pExplosion->KeyValue( szKeyName, szValue );

	pExplosion->AddSpawnFlags( nSpawnFlags );

	if ( radius )
	{
		Q_snprintf( buf,sizeof(buf), "%d", radius );
		pExplosion->KeyValue( "iRadiusOverride", buf );
	}

	if ( flExplosionForce != 0.0f )
	{
		Q_snprintf( buf,sizeof(buf), "%.3f", flExplosionForce );
		pExplosion->KeyValue( "DamageForce", buf );
	}

	variant_t emptyVariant;
	pExplosion->m_nRenderMode = kRenderTransAdd;
	pExplosion->SetOwnerEntity( pOwner );
	pExplosion->Spawn();
	pExplosion->m_hInflictor = pInflictor;
	pExplosion->SetCustomDamageType( iCustomDamageType );
	if (ignoredEntity)
	{
		pExplosion->m_hEntityIgnore = *ignoredEntity;
	}
	pExplosion->m_iClassIgnore = ignoredClass;

	pExplosion->AcceptInput( "Explode", NULL, NULL, emptyVariant, 0 );
}


void ASWExplosionCreate( const Vector &center, const QAngle &angles, 
	CBaseEntity *pOwner, int iDamage, int radius, bool doDamage, float flExplosionForce, bool bSurfaceOnly, bool bSilent, int iCustomDamageType, bool bNoDlight )
{
	int nFlags = 0;
	if ( !bNoDlight )
	{
		nFlags |= SF_ENVEXPLOSION_NODLIGHTS;
	}

	if ( !doDamage )
	{
		nFlags |= SF_ENVEXPLOSION_NODAMAGE;
	}

	if( bSurfaceOnly )
	{
		nFlags |= SF_ENVEXPLOSION_SURFACEONLY;
	}

	if( bSilent )
	{
		nFlags |= SF_ENVEXPLOSION_NOSOUND;
	}

	ASWExplosionCreate( center, angles, pOwner, iDamage, radius, nFlags, flExplosionForce, NULL, iCustomDamageType );
}

// this version lets you specify classes or entities to be ignored
void ASWExplosionCreate( const Vector &center, const QAngle &angles, 
					 CBaseEntity *pOwner, int magnitude, int radius, bool doDamage, 
					 const EHANDLE *ignoredEntity, Class_T ignoredClass,
					 float flExplosionForce , bool bSurfaceOnly , bool bSilent , int iCustomDamageType, bool bNoDlight )
{
	// For E3, no sparks
	int nFlags = 0;
	if ( !bNoDlight )
	{
		nFlags |= SF_ENVEXPLOSION_NODLIGHTS;
	}

	if ( !doDamage )
	{
		nFlags |= SF_ENVEXPLOSION_NODAMAGE;
	}

	if( bSurfaceOnly )
	{
		nFlags |= SF_ENVEXPLOSION_SURFACEONLY;
	}

	if( bSilent )
	{
		nFlags |= SF_ENVEXPLOSION_NOSOUND;
	}

	ASWExplosionCreate( center, angles, pOwner, magnitude, radius, nFlags, flExplosionForce, NULL, iCustomDamageType, ignoredEntity, ignoredClass );
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CASWEnvExplosion::DrawDebugTextOverlays( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		Q_snprintf(tempstr,sizeof(tempstr),"    magnitude: %i", m_iDamage);
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}
