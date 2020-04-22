//========= Copyright Valve Corporation, All rights reserved. ============//
//
// TF Nail
//
//=============================================================================
#include "cbase.h"
#include "tf_projectile_nail.h"
#include "tf_gamerules.h"

#ifdef CLIENT_DLL
#include "c_basetempentity.h"
#include "c_te_legacytempents.h"
#include "c_te_effect_dispatch.h"
#include "input.h"
#include "c_tf_player.h"
#include "cliententitylist.h"
#endif

#ifdef GAME_DLL
#include "tf_player.h"
#endif


//=============================================================================
//
// TF Syringe Projectile functions (Server specific).
//
#define SYRINGE_MODEL				"models/weapons/w_models/w_syringe_proj.mdl"
#define SYRINGE_DISPATCH_EFFECT		"ClientProjectile_Syringe"

LINK_ENTITY_TO_CLASS( tf_projectile_syringe, CTFProjectile_Syringe );
PRECACHE_REGISTER( tf_projectile_syringe );

#ifdef STAGING_ONLY
LINK_ENTITY_TO_CLASS( tf_projectile_tranq, CTFProjectile_Tranq );
PRECACHE_REGISTER( tf_projectile_tranq );
#endif // STAGING_ONLY

short g_sModelIndexSyringe;
void PrecacheSyringe(void *pUser)
{
	g_sModelIndexSyringe = modelinfo->GetModelIndex( SYRINGE_MODEL );
}

PRECACHE_REGISTER_FN(PrecacheSyringe);

//-----------------------------------------------------------------------------
// CTFProjectile_Syringe
//-----------------------------------------------------------------------------
#define SYRINGE_GRAVITY		0.3f
#define SYRINGE_VELOCITY	1000.0f
// Purpose:
//-----------------------------------------------------------------------------
CTFBaseProjectile *CTFProjectile_Syringe::Create( 
	const Vector &vecOrigin, 
	const QAngle &vecAngles, 
	CTFWeaponBaseGun *pLauncher /*= NULL*/,
	CBaseEntity *pOwner /*= NULL*/, 
	CBaseEntity *pScorer /*= NULL*/, 
	bool bCritical /*= false */
) {
	return CTFBaseProjectile::Create( "tf_projectile_syringe", vecOrigin, vecAngles, pOwner, SYRINGE_VELOCITY, g_sModelIndexSyringe, SYRINGE_DISPATCH_EFFECT, pScorer, bCritical );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
unsigned int CTFProjectile_Syringe::PhysicsSolidMaskForEntity( void ) const
{ 
	return BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_REDTEAM | CONTENTS_BLUETEAM;
}

//-----------------------------------------------------------------------------
float CTFProjectile_Syringe::GetGravity( void )
{
	return SYRINGE_GRAVITY;
}

#ifdef STAGING_ONLY
//-----------------------------------------------------------------------------
// CTFProjectile_Tranq
#define TRANQ_GRAVITY	0.1f
#define TRANQ_VELOCITY	2000.0f
#define TRANQ_STUN		0.50f
#define TRANQ_DURATION	1.5f

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFBaseProjectile *CTFProjectile_Tranq::Create(
	const Vector &vecOrigin,
	const QAngle &vecAngles,
	CTFWeaponBaseGun *pLauncher /*= NULL*/,
	CBaseEntity *pOwner /*= NULL*/,
	CBaseEntity *pScorer /*= NULL*/,
	bool bCritical /*= false */
) {
	return CTFBaseProjectile::Create( "tf_projectile_tranq", vecOrigin, vecAngles, pOwner, TRANQ_VELOCITY, g_sModelIndexSyringe, SYRINGE_DISPATCH_EFFECT, pScorer, bCritical );
}

//-----------------------------------------------------------------------------
float CTFProjectile_Tranq::GetGravity( void )
{
	return TRANQ_GRAVITY;
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
void CTFProjectile_Tranq::ProjectileTouch( CBaseEntity *pOther )
{
	// Verify a correct "other."
	Assert( pOther );
	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet( FSOLID_VOLUME_CONTENTS ) )
		return;

	// Handle hitting skybox (disappear).
	const trace_t *pTrace = &CBaseEntity::GetTouchTrace();
	trace_t *pNewTrace = const_cast<trace_t*>( pTrace );

	if( pTrace->surface.flags & SURF_SKY )
	{
		UTIL_Remove( this );
		return;
	}

	// pass through ladders
	if( pTrace->surface.flags & CONTENTS_LADDER )
		return;

	if ( TFGameRules() && TFGameRules()->GameModeUsesUpgrades() )
	{
		// Projectile shields
		if ( InSameTeam( pOther ) && pOther->IsCombatItem() )
			return;
	}

	if ( pOther->IsWorld() )
	{
		SetAbsVelocity( vec3_origin	);
		AddSolidFlags( FSOLID_NOT_SOLID );

		// Remove immediately. Clientside projectiles will stick in the wall for a bit.
		UTIL_Remove( this );
		return;
	}

	// determine the inflictor, which is the weapon which fired this projectile
	CBaseEntity *pInflictor = GetLauncher();

	CTakeDamageInfo info;
	info.SetAttacker( GetOwnerEntity() );		// the player who operated the thing that emitted nails
	info.SetInflictor( pInflictor );	// the weapon that emitted this projectile
	info.SetWeapon( pInflictor );
	info.SetDamage( GetDamage() );
	info.SetDamageForce( GetDamageForce() );
	info.SetDamagePosition( GetAbsOrigin() );
	info.SetDamageType( GetDamageType() );

	Vector dir;
	AngleVectors( GetAbsAngles(), &dir );

	pOther->DispatchTraceAttack( info, dir, pNewTrace );
	ApplyMultiDamage();

	CTFPlayer *pTFVictim = ToTFPlayer( pOther );
	CTFPlayer *pTFOwner = ToTFPlayer( GetOwnerEntity() );

	if ( pTFVictim && pTFOwner && !InSameTeam( pTFVictim ) )
	{
		// Apply Slow Condition
		pTFVictim->m_Shared.StunPlayer( TRANQ_DURATION, TRANQ_STUN, TF_STUN_MOVEMENT, pTFOwner );
		pTFVictim->m_Shared.AddCond( TF_COND_TRANQ_MARKED, PERMANENT_CONDITION, pTFOwner );	// Tranq marked until you die
		pTFVictim->ApplyPunchImpulseX( -2.0f );		// Apply a flinch

		// Apply a boost to the attacker
		//pTFOwner->m_Shared.AddCond( TF_COND_SPEED_BOOST, 3.0f );
		pTFOwner->m_Shared.AddCond( TF_COND_TRANQ_SPY_BOOST, 3.0f );
	}

	UTIL_Remove( this );
}
#endif // GAME_DLL
#endif // STAGING_ONLY

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
void GetSyringeTrailParticleName( CTFPlayer *pPlayer, CAttribute_String *attrParticleName, bool bCritical )
{
	int iTeamNumber = TF_TEAM_RED;	
	if ( pPlayer )
	{
		iTeamNumber = pPlayer->GetTeamNumber();
		CTFWeaponBase *pWeapon = pPlayer->GetActiveTFWeapon();
		if ( pWeapon )
		{
			static CSchemaAttributeDefHandle pAttrDef_ParticleName( "projectile particle name" );
			CEconItemView *pItem = pWeapon->GetAttributeContainer()->GetItem();
			if ( pAttrDef_ParticleName && pItem )
			{
				if ( pItem->FindAttribute( pAttrDef_ParticleName, attrParticleName ) )
				{
					const char * pParticleName = attrParticleName->value().c_str();
					if ( iTeamNumber == TF_TEAM_BLUE && V_stristr( pParticleName, "_teamcolor_red" ))
					{
						static char pBlue[256];
						V_StrSubst( attrParticleName->value().c_str(), "_teamcolor_red", "_teamcolor_blue", pBlue, 256 );
						attrParticleName->set_value( pBlue );
					}
					return;
				}
			}
		}
	}
	
	if ( iTeamNumber == TF_TEAM_BLUE )
	{
		attrParticleName->set_value( bCritical ? "nailtrails_medic_blue_crit" : "nailtrails_medic_blue" );
	}
	else
	{
		attrParticleName->set_value( bCritical ? "nailtrails_medic_red_crit" : "nailtrails_medic_red" );
	}
	return;
}

//-----------------------------------------------------------------------------
// Purpose: For Synrgine Projectiles, Add effects
//-----------------------------------------------------------------------------
void ClientsideProjectileSyringeCallback( const CEffectData &data )
{
	// Get the syringe and add it to the client entity list, so we can attach a particle system to it.
	C_TFPlayer *pPlayer = dynamic_cast<C_TFPlayer*>( ClientEntityList().GetBaseEntityFromHandle( data.m_hEntity ) );
	if ( pPlayer )
	{
		C_LocalTempEntity *pSyringe = ClientsideProjectileCallback( data, SYRINGE_GRAVITY );
		if ( pSyringe )
		{
			CAttribute_String attrParticleName;
			
			pSyringe->m_nSkin = ( pPlayer->GetTeamNumber() == TF_TEAM_RED ) ? 0 : 1;
			bool bCritical = ( ( data.m_nDamageType & DMG_CRITICAL ) != 0 );
			GetSyringeTrailParticleName( pPlayer, &attrParticleName, bCritical );

			pSyringe->AddParticleEffect( attrParticleName.value().c_str() );
			pSyringe->AddEffects( EF_NOSHADOW );
			pSyringe->flags |= FTENT_USEFASTCOLLISIONS;
		}
	}
}

DECLARE_CLIENT_EFFECT( SYRINGE_DISPATCH_EFFECT, ClientsideProjectileSyringeCallback );

#endif
