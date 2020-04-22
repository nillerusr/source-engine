//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "in_buttons.h"
#include "weapon_combatshield.h"
#include "weapon_flame_thrower.h"
#include "gasoline_shared.h"
#include "ammodef.h"


#define FLAME_THROWER_FIRE_INTERVAL		0.3		// Eject a fire blob entity this often.

#define FLAMETHROWER_FLAME_DISTANCE		400.0

#define FLAMETHROWER_FLAME_SPEED		500.0	//

#define FLAMETHROWER_DAMAGE_PER_SEC		1000

// How far the flame particles will spread from the center.
#define FLAMETHROWER_SPREAD_ANGLE		15.0


// ------------------------------------------------------------------------------------------------ //
// Pretty little tables.
// ------------------------------------------------------------------------------------------------ //

#if defined( CLIENT_DLL )

	#include "vstdlib/random.h"
	#include "engine/IEngineSound.h"

	#define FLAMETHROWER_PARTICLES_PER_SEC	100

#else

	#include "gasoline_blob.h"
	#include "fire_damage_mgr.h"
	#include "tf_gamerules.h"
	
	#define FLAMETHROWER_DAMAGE_INTERVAL	0.2

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


PRECACHE_WEAPON_REGISTER( weapon_flame_thrower );

LINK_ENTITY_TO_CLASS( weapon_flame_thrower, CWeaponFlameThrower );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponFlameThrower, DT_WeaponFlameThrower )

BEGIN_NETWORK_TABLE( CWeaponFlameThrower, DT_WeaponFlameThrower )
	#if defined( CLIENT_DLL )
		RecvPropInt( RECVINFO( m_bFiring ) )
	#else
		SendPropInt( SENDINFO( m_bFiring ), 1, SPROP_UNSIGNED )
	#endif
END_NETWORK_TABLE()
		  
BEGIN_PREDICTION_DATA( CWeaponFlameThrower )

	DEFINE_PRED_FIELD( m_bFiring, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()


static inline void GenerateRandomFlameThrowerVelocity( Vector &vOut, const Vector &vForward, const Vector &vRight, const Vector &vUp )
{
	static float radians = DEG2RAD( 90 - FLAMETHROWER_SPREAD_ANGLE );
	static float v = cos( radians ) / sin( radians );
	
	vOut = vForward + vRight * RandomFloat( -v, v ) + vUp * RandomFloat( -v, v );
	VectorNormalize( vOut );
}

template< class T >
int FindInArray( const T pTest, const T *pArray, int arrayLen )
{
	for ( int i=0; i < arrayLen; i++ )
	{
		if ( pTest == pArray[i] )
			return i;
	}
	return -1;
}


// ------------------------------------------------------------------------------------------------ //
// CWeaponFlameThrower implementation.
// ------------------------------------------------------------------------------------------------ //

CWeaponFlameThrower::CWeaponFlameThrower()
{
	InternalConstructor( false );
}


CWeaponFlameThrower::CWeaponFlameThrower( bool bCanister )
{
	InternalConstructor( bCanister );
}

void CWeaponFlameThrower::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "FlameThrower.Sound" );
}

void CWeaponFlameThrower::InternalConstructor( bool bCanister )
{
	m_bCanister = bCanister;

	m_bFiring = false;							   	
	m_flNextPrimaryAttack = -1;

	#if defined( CLIENT_DLL )
	{
		m_hFlameEmitter = CSimpleEmitter::Create( "flamethrower" );
		
		m_hFireMaterial = INVALID_MATERIAL_HANDLE;
		if ( IsGasCanister() )
		{
			m_hFireMaterial = m_hFlameEmitter->GetPMaterial( "particle/particle_noisesphere" );
		}
		else
		{
			m_hFireMaterial = m_hFlameEmitter->GetPMaterial( "particle/fire" );
		}
	
		m_FlameEvent.Init( FLAMETHROWER_PARTICLES_PER_SEC );

		m_bSoundOn = false;
	}
	#endif
}


CWeaponFlameThrower::~CWeaponFlameThrower()
{
	#if defined( CLIENT_DLL )
		StopFlameSound();
	#endif
}


bool CWeaponFlameThrower::IsGasCanister() const
{
	return m_bCanister;
}


bool CWeaponFlameThrower::IsPredicted() const
{
	return true;
}


void CWeaponFlameThrower::ItemPostFrame()
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	if ( pOwner->IsAlive() && 
		(pOwner->m_nButtons & IN_ATTACK) && 
		GetShieldState() == SS_DOWN &&
		GetPrimaryAmmo() > 2 )
	{
		PrimaryAttack();
		m_bFiring = true;

		// Prevent shield post frame if we're not ready to attack, or we're healing
		AllowShieldPostFrame( false );
	}
	else
	{
		AllowShieldPostFrame( true );
		m_flNextPrimaryAttack = -1;
		m_bFiring = false;

#if defined( CLIENT_DLL )
#else
		m_hPrevBlob = NULL;

		// It's easy to lay down gasoline and forget to leave enough ammo to ignite it, so
		// this allows the pyro to ignite any nearby gasoline blobs for free.
		if ( !m_bCanister && GetPrimaryAmmo() <= 2 )
		{
			IgniteNearbyGasolineBlobs();
		}
#endif
	}
}


void CWeaponFlameThrower::PrimaryAttack()
{
	#if defined( CLIENT_DLL )
	
	#else

		CBasePlayer *pOwner = ToBaseTFPlayer( GetOwner() );
		if ( !pOwner )
			return;
		
		// Ok.. find eligible entities in a cone in front of us.
		Vector vOrigin = pOwner->Weapon_ShootPosition( );
		Vector vForward, vRight, vUp;
		AngleVectors( pOwner->GetAbsAngles(), &vForward, &vRight, &vUp );

		// Find some entities to burn.
		CBaseEntity *pHitEnts[64];
		int nHitEnts = 0;

		#define NUM_TEST_VECTORS	30
		for ( int iTest=0; iTest < NUM_TEST_VECTORS; iTest++ )
		{	
			Vector vVel;  
			GenerateRandomFlameThrowerVelocity( vVel, vForward, vRight, vUp );

			trace_t tr;
			UTIL_TraceLine( vOrigin, vOrigin + vVel * FLAMETHROWER_FLAME_DISTANCE, MASK_SHOT & (~CONTENTS_HITBOX), NULL, COLLISION_GROUP_NONE, &tr );
			if ( tr.m_pEnt )
			{
				if ( TFGameRules()->IsTraceBlockedByWorldOrShield( vOrigin, vOrigin + vVel * FLAMETHROWER_FLAME_DISTANCE, GetOwner(), DMG_BURN | DMG_PROBE, &tr ) == false )
				{
					CBaseEntity *pTestEnt = tr.m_pEnt;
					if ( pTestEnt && IsBurnableEnt( pTestEnt, GetTeamNumber() ) )
					{
						if ( FindInArray( pTestEnt, pHitEnts, nHitEnts ) == -1 )
						{
							pHitEnts[nHitEnts++] = pTestEnt;
							if ( nHitEnts >= ARRAYSIZE( pHitEnts ) )
								break;
						}
					}
				}	
			}
		}

		for ( int iHitEnt=0; iHitEnt < nHitEnts; iHitEnt++ )
		{
			CBaseEntity *pEnt = pHitEnts[iHitEnt];

			float flDist = (pEnt->GetAbsOrigin() - vOrigin).Length();
			float flPercent = 1.0 - flDist / FLAMETHROWER_FLAME_DISTANCE;
			if ( flPercent < 0.1 )
				flPercent = 0.1;

			float flDamage = flPercent * FLAMETHROWER_DAMAGE_PER_SEC;
			GetFireDamageMgr()->AddDamage( pEnt, GetOwner(), flDamage, !IsGasolineBlob( pEnt ) );
		}
		

		// Drop a new petrol blob.
		if ( gpGlobals->curtime >= m_flNextPrimaryAttack )
		{
			float flLifetime = MAX_LIT_GASOLINE_BLOB_LIFETIME;
			if ( IsGasCanister() )
				flLifetime = MAX_UNLIT_GASOLINE_BLOB_LIFETIME;

			CGasolineBlob *pBlob = CGasolineBlob::Create( GetOwner(), vOrigin, vForward * FLAMETHROWER_FLAME_SPEED, false, FLAMETHROWER_FLAME_DISTANCE / FLAMETHROWER_FLAME_SPEED, flLifetime );
			if ( pBlob )
			{
				if ( IsGasCanister() )
				{
					// Link the previous blob to this one.
					pBlob->AddAutoBurnBlob( m_hPrevBlob );
					if ( m_hPrevBlob.Get() )
						m_hPrevBlob->AddAutoBurnBlob( pBlob );

					m_hPrevBlob = pBlob;
				}
				else
				{
					pBlob->SetLit( true );
				}
			}

			pOwner->RemoveAmmo( 2, m_iPrimaryAmmoType );

			// Drop a blob every half second.
			m_flNextPrimaryAttack = gpGlobals->curtime + FLAME_THROWER_FIRE_INTERVAL;
		}

	#endif
}


#if defined( CLIENT_DLL )

	bool CWeaponFlameThrower::ShouldPredict()
	{
		if ( GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}


	void CWeaponFlameThrower::NotifyShouldTransmit( ShouldTransmitState_t state )
	{
		BaseClass::NotifyShouldTransmit( state );

		if ( state == SHOULDTRANSMIT_START )
		{
			SetNextClientThink( CLIENT_THINK_ALWAYS );
		}		
		else if ( state == SHOULDTRANSMIT_END )
		{
			SetNextClientThink( CLIENT_THINK_NEVER );
			StopFlameSound();
		}
	}

	void CWeaponFlameThrower::ClientThink()
	{
		// Spew some particles out.
		if ( !IsDormant() && IsCarrierAlive() && m_bFiring && m_hFlameEmitter.IsValid() )
		{
			unsigned char color[4] = { 255, 128, 0, 255 };
			if ( IsGasCanister() )
			{
				color[0] = color[1] = color[2] = 200;
				color[3] = 255;
			}

			StartSound();
			
			Vector vForward, vUp, vRight, vOrigin;
			QAngle vAngles;
			GetShootPosition( vOrigin, vAngles );
			AngleVectors( vAngles, &vForward, &vRight, &vUp );
			
			// Spew out flame particles.
			float dt = gpGlobals->frametime;
			while ( m_FlameEvent.NextEvent( dt ) )
			{
				SimpleParticle *p = m_hFlameEmitter->AddSimpleParticle( 
					m_hFireMaterial, 
					vOrigin + RandomVector( -3, 3 ),
					FLAMETHROWER_FLAME_DISTANCE / FLAMETHROWER_FLAME_SPEED,	// lifetime,
					9	// size
					);

				if ( p )
				{
					p->m_uchColor[0] = color[0];
					p->m_uchColor[1] = color[1];
					p->m_uchColor[2] = color[2];
					p->m_uchStartAlpha = color[3];
					p->m_uchEndAlpha = 0;
					GenerateRandomFlameThrowerVelocity( p->m_vecVelocity, vForward, vRight, vUp );
					p->m_vecVelocity *= RandomFloat( FLAMETHROWER_FLAME_SPEED * 0.9, FLAMETHROWER_FLAME_SPEED * 1.1 );
				}
			}
		}
		else
		{
			StopFlameSound();
		}
	}


	void CWeaponFlameThrower::StartSound()
	{
		if ( !m_bSoundOn )
		{
			CLocalPlayerFilter filter;
			EmitSound( filter, entindex(), "FlameThrower.Sound" );

			m_bSoundOn = true;
		}
	}


	void CWeaponFlameThrower::StopFlameSound()
	{
		if ( m_bSoundOn )
		{
			StopSound( entindex(), "FlameThrower.Sound" );
			m_bSoundOn = false;
		}
	}


#else

	bool CWeaponFlameThrower::Holster( CBaseCombatWeapon *pSwitchingTo )
	{
		m_bFiring = false;
		
		return BaseClass::Holster( pSwitchingTo );
	}


	void CWeaponFlameThrower::IgniteNearbyGasolineBlobs()
	{
		CBasePlayer *pOwner = ToBaseTFPlayer( GetOwner() );
		if ( !pOwner )
			return;

		Vector vOrigin = pOwner->Weapon_ShootPosition( );
		CBaseEntity *ents[128];
		float dists[128];
		int nEnts = FindBurnableEntsInSphere(
			ents,
			dists,
			ARRAYSIZE( dists ),
			vOrigin,
			50,
			pOwner );

		for ( int i=0; i < nEnts; i++ )
		{
			CGasolineBlob *pBlob = dynamic_cast< CGasolineBlob* >( ents[i] );
			if ( pBlob )
			{
				GetFireDamageMgr()->AddDamage( pBlob, pOwner, 500, false );
			}
		}
	}

#endif


