#include "cbase.h"
#include "asw_alien_shared_classmembers.h"
#include "c_asw_clientragdoll.h"
#include "asw_util_shared.h"
#include "c_asw_fx.h"
#include "c_asw_player.h"
#include "asw_input.h"
#include "props_shared.h"
#include "c_asw_alien.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_DATADESC( C_ASW_ClientRagdoll )
	DEFINE_FIELD( m_fASWGibTime, FIELD_TIME ),
	DEFINE_FIELD( m_iSourceEntityIndex, FIELD_INTEGER ),
END_DATADESC()

ConVar asw_drone_ridiculous( "asw_drone_ridiculous", "0", FCVAR_CHEAT, "If true, hurl drone ragdolls at camera in a ridiculous fashion." );
ConVar asw_drone_gib_velocity( "asw_drone_gib_velocity", "1.75", FCVAR_CHEAT, "Drone gibs will inherit the velocity of the parent ragdoll scaled by this" );
extern ConVar asw_breakable_aliens;
extern ConVar asw_alien_debug_death_style;

C_ASW_ClientRagdoll::C_ASW_ClientRagdoll( bool bRestoring ) : BaseClass( bRestoring )
{
	m_nDeathStyle = 0;
	m_bElectroShock = false;
	m_bHurled = false;

	pszGibParticleEffect = NULL;
}

/// @brief given an origin point, a destination, and a gravity, return a
/// velocity that will apex exactly at the destination.
/// @param flGravity should be positive, assumed to be aimed down -z
Vector ComputeParabolicTrajectoryToApex( const Vector &vOrigin, const Vector &vDestination, const float flGravity )
{
	// transform everything so the object's origin is at 0,0,0
	const Vector vApex = vDestination - vOrigin;
	/*
	V = <x,z> // z is up, x out in this formulation
	x(t) = tVx
	y(t) = tVy
	z(t) = tVz - 0.5gt^2
	|V| = sqrt(Vx^2 + Vz^2)
	at apex,
	0 = Vz - gt -> t = Vz/g
	X1 = tVx  for t = Vz/g
	Y1 = tVy  for t = Vz/g
	Z1 = tVz - 0.5gt^2  for t=Vz/g
	solving the linear system gets you
	Vz^2 = 2*Z1*g
	Vx = g * X1 / Vz
	Vy = g * Y1 / Vz
	*/

	// return value
	float flX, flY, flZ;

	float flZSquared = 2.0f * vApex.z * flGravity;
	float flRecipZ = FastRSqrt( flZSquared ); // 1/vZ
	flZ = flRecipZ * flZSquared; // Vz^2 / Vz = Vz
	flX = flRecipZ * flGravity * vApex.x;
	flY = flRecipZ * flGravity * vApex.y;
	
	return Vector( flX, flY, flZ );
}

extern ConVar cl_ragdoll_gravity;
void ASWHurlRagdollAtCamera( C_ASW_ClientRagdoll * RESTRICT pEntity, const Vector &vCameraPosition, const QAngle &vCameraAngles )
{
	Assert( pEntity );
	// AssertMsg1( pEntity->VPhysicsGetObject(), "Tried to throw %s at camera but it has no VPhysics\n", pEntity->GetDebugName() );
	pEntity->SetAbsVelocity( Vector(0,0,0) );

	Vector vViewForward; // =CurrentViewForward();
	AngleVectors( vCameraAngles, &vViewForward );
 
	float radius = MAX(pEntity->BoundingRadius(), 20.0f );
	Vector vApex = vCameraPosition + ( vViewForward * ( radius * 2 ) );
	// NDebugOverlay::Sphere( vApex, vCamAngles , pEntity->BoundingRadius() * 0.5f, 255, 0, 0, 255, false, 1.0f );
	// NDebugOverlay::Box( vApex, Vector(-6,-6,-6), Vector(6,6,6), 255, 0, 0, 55, 1.0f );

	// pEntity->ApplyAbsVelocityImpulse( ComputeParabolicTrajectoryToApex( pEntity->GetAbsOrigin(), vApex, cl_ragdoll_gravity.GetFloat() ) );
	Vector vel = ComputeParabolicTrajectoryToApex( pEntity->GetAbsOrigin(), vApex, cl_ragdoll_gravity.GetFloat() );
	if ( !vel.IsValid() )
		return;

	IPhysicsObject *ppPhysObjs[ VPHYSICS_MAX_OBJECT_LIST_COUNT ];
	int nNumPhysObjs = pEntity->VPhysicsGetObjectList( ppPhysObjs, VPHYSICS_MAX_OBJECT_LIST_COUNT );
	if ( nNumPhysObjs > 0 )
	{
		{
			AngularImpulse spin = RandomAngularImpulse(-720, 720);
			ppPhysObjs[ 0 ]->SetVelocity( &vel, &spin );
		}

		for ( int i = 1; i < nNumPhysObjs; i++ )
		{
			ppPhysObjs[ i ]->SetVelocity( &vel, NULL );
		}
	}
		
}

void ASWHurlRagdollAtCamera( C_ASW_ClientRagdoll * RESTRICT pEntity )
{
	// Verify that we have input.
	Assert( ASWInput() != NULL );
	if ( !ASWInput() )
		return;

	HACK_GETLOCALPLAYER_GUARD( "HurlObjectAtCamera needs to care about which camera" );
	Vector vCamPos; QAngle vCamAngles; 
	int omx, omy;
	// const Vector vCamPos = CurrentViewOrigin();
	ASWInput()->ASW_GetCameraLocation( C_ASW_Player::GetLocalASWPlayer(), vCamPos, vCamAngles, omx, omy, false );
	return ASWHurlRagdollAtCamera( pEntity, vCamPos, vCamAngles );
}


void ASWMeleeThrowRagdoll( C_ASW_ClientRagdoll * RESTRICT pEntity )
{
	Assert( pEntity );
	pEntity->SetAbsVelocity( Vector(0,0,0) );

	Vector vecThrowDir = pEntity->GetDeathForce();
	vecThrowDir.z = 0;
	vecThrowDir.NormalizeInPlace();

	Vector vApex = pEntity->GetAbsOrigin() + vecThrowDir * 170.0f + Vector( 0, 0, 50 );

	Vector vel = ComputeParabolicTrajectoryToApex( pEntity->GetAbsOrigin(), vApex, cl_ragdoll_gravity.GetFloat() );
	if ( !vel.IsValid() )
		return;

	IPhysicsObject *ppPhysObjs[ VPHYSICS_MAX_OBJECT_LIST_COUNT ];
	int nNumPhysObjs = pEntity->VPhysicsGetObjectList( ppPhysObjs, VPHYSICS_MAX_OBJECT_LIST_COUNT );
	if ( nNumPhysObjs > 0 )
	{
		{
			AngularImpulse spin = RandomAngularImpulse(-720, 720);
			ppPhysObjs[ 0 ]->SetVelocity( &vel, &spin );
		}

		for ( int i = 1; i < nNumPhysObjs; i++ )
		{
			ppPhysObjs[ i ]->SetVelocity( &vel, NULL );
		}
	}
}

void C_ASW_ClientRagdoll::BreakRagdoll()
{

	IPhysicsObject *pPhysics = VPhysicsGetObject();

	Vector	velocity;
	velocity = m_vecForce / 140.0f;
	if ( velocity == vec3_origin )
		velocity = Vector( RandomFloat( 1, 15 ), RandomFloat( 1, 15 ), 75.0f );
	velocity.z += 100.0;
	AngularImpulse	angVelocity = RandomAngularImpulse( -500.0f, 500.0f );
	breakablepropparams_t params( GetAbsOrigin(), GetAbsAngles(), velocity, angVelocity );
	params.impactEnergyScale = 1.25f;
	params.defBurstScale = 125.0f;
	params.defCollisionGroup = COLLISION_GROUP_DEBRIS;
	params.useThisRawVelocity = true;
	PropBreakableCreateAll( GetModelIndex(), pPhysics, params, this, -1, true, true ); 

	SUB_Remove(); // destroy ragdoll
}

//ConVar test_hurl( "test_hurl", "0.25" );
void C_ASW_ClientRagdoll::ClientThink( void )
{
	// if parent is going to delete us, make sure we do nothing else
	if ( m_bReleaseRagdoll )
	{
		BaseClass::ClientThink();
		return;
	}

	BaseClass::ClientThink();

	// OLD STUFF
	/*
	if ( m_fASWGibTime > 0 && gpGlobals->curtime > m_fASWGibTime )
	{
		// should gib this
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, entindex(), "ASW_Drone.GibSplatLight" );
		UTIL_ASW_ClientsideGib(this);
		Release();
	}
	*/

	if ( !m_pRagdoll || IsFadingOut() )
		return;

	// force-reset the velocity one frame after spawning in case the grenade explosion
	// has somehow accumulated again on top of it even though we told it not to.
	if ( m_nDeathStyle == kDIE_HURL && !m_bHurled && ( ( SpawnTime() + 0.05f ) < gpGlobals->curtime ) )
	{
		// SetMoveType( MOVETYPE_VPHYSICS );
		ASWHurlRagdollAtCamera( this );
		m_bHurled = true;
	}
	else if ( m_nDeathStyle == kDIE_MELEE_THROW && !m_bHurled && ( ( SpawnTime() + 0.05f ) < gpGlobals->curtime ) )
	{
		ASWMeleeThrowRagdoll( this );
		m_bHurled = true;
	}

	if ( m_fASWGibTime > 0 && gpGlobals->curtime > m_fASWGibTime )
	{
		if ( asw_alien_debug_death_style.GetBool() )
			Msg( "C_ASW_ClientRagdoll::ClientThink: m_nDeathStyle = %d\n",  m_nDeathStyle );

		if ( m_nDeathStyle == kDIE_BREAKABLE && asw_breakable_aliens.GetBool() )
		{
			BreakRagdoll();
			return;
		}
		// if we die and are on fire, the ragdoll burns away
		if ( GetFlags() & FL_ONFIRE )
		{
			CUtlReference<CNewParticleEffect> pBurnEffect = ParticleProp()->Create( "burned_alien_death", PATTACH_ABSORIGIN_FOLLOW );
		
			EmitSound( "ASW_Alien.Flame_Death_Flash" );

			// this tells the ragdoll to fade out.
			SUB_Remove();
			return;
		}

		// if we're set to fade, MAKE IT SO
		if ( m_nDeathStyle == kDIE_RAGDOLLFADE )
		{
			// this tells the ragdoll to fade out.
			SUB_Remove();

			//if ( pszGibParticleEffect )
			//	ParticleProp()->Create( pszGibParticleEffect, PATTACH_ABSORIGIN_FOLLOW );
			
			return;
		}

		// we might use this code int he future, but for now, we're using models in the particle system to spawn the instagib effects
		/*
		KeyValues * modelKeyValues = new KeyValues("");
		if ( modelKeyValues->LoadFromBuffer( GetModelName(), modelinfo->GetModelKeyValueText( GetModel() ) ) )
		{
			KeyValues *pkvMeshParticleEffect = modelKeyValues->FindKey("MeshParticles");
			if ( pkvMeshParticleEffect )
			{

				for ( KeyValues *pSingleEffect = pkvMeshParticleEffect->GetFirstSubKey(); pSingleEffect; pSingleEffect = pSingleEffect->GetNextKey() )
				{
					const char *pszParticleEffect = pSingleEffect->GetString( "effectName", "" );
					const char *pszModelName = pSingleEffect->GetString( "modelName", "" );
					const char *pszBoneName = pSingleEffect->GetString( "boneName", "" );
					//const char *pszBoneAxis = pSingleEffect->GetString( "boneAxis", "0 1 0" );

					Vector vGibOrigin;
					QAngle aGibAngles;

					int iBoneIdx = -1;
					if ( pszBoneName )
					{
						iBoneIdx = LookupBone( pszBoneName );	
					}

					// See if we can find the appropriate bone
					if ( iBoneIdx > -1 )
					{
						GetBonePosition( iBoneIdx, vGibOrigin, aGibAngles );
					}
					else
					{
						//if no bone was specified, pop a warning and then use the ragdoll's centre, or the worldspace origin...
						Warning("Failed to find the bone specified for particle effect in model '%s' keyvalues section. Trying to spawn effect '%s' on attachment named '%s'.  Spawning effect at origin of ragdoll.\n", GetModelName(), pszParticleEffect, pszBoneName );

						Vector vMins, vMaxs;
						if ( m_pRagdoll )
						{
							m_pRagdoll->GetRagdollBounds( vMins, vMaxs );
							vGibOrigin = m_pRagdoll->GetRagdollOrigin() + ( ( vMins + vMaxs ) / 2.0f );
						}
						else
						{
							vGibOrigin = WorldSpaceCenter();
						}
					}

					Vector vGibVelocity(0,0,0);
					if ( m_pRagdoll ) 
					{
						m_pRagdoll->GetElement(0)->GetVelocity(&vGibVelocity,NULL);
					}

					FX_GibMeshEmitter( pszModelName, pszParticleEffect, vGibOrigin, vGibVelocity * asw_drone_gib_velocity.GetFloat(), GetSkin() );
				}
			}
			else
			{
				UTIL_ASW_ClientsideGib( this );
			}

			modelKeyValues->deleteThis();
		}
		*/

		Release();
	}
}