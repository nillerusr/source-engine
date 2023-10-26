#include "cbase.h"
#include "asw_flare_projectile.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "soundent.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#include "weapon_flaregun.h"
#include "decals.h"
#include "asw_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern ConVar sk_plr_dmg_asw_flares;
extern ConVar sk_npc_dmg_asw_flares;

#define FLARE_MODEL "models/swarm/Flare/flareweapon.mdl"
	//"models/weapons/flare.mdl"
#define ASW_FLARE_LIFETIME 30.0f

LINK_ENTITY_TO_CLASS( asw_flare_projectile, CASW_Flare_Projectile );

BEGIN_DATADESC( CASW_Flare_Projectile )
	DEFINE_FUNCTION( FlareTouch ),
	DEFINE_FUNCTION( FlareBurnTouch ),
	DEFINE_FUNCTION( FlareThink ),

	// Fields
	DEFINE_FIELD( m_pMainGlow, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pGlowTrail, FIELD_EHANDLE ),
	DEFINE_FIELD( m_inSolid, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flDamage, FIELD_FLOAT ),
	DEFINE_FIELD( m_nBounces, FIELD_INTEGER ),
	DEFINE_FIELD( m_flTimeBurnOut, FIELD_TIME ),
	DEFINE_FIELD( m_flScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDuration, FIELD_FLOAT ),
	DEFINE_FIELD( m_bFading, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bSmoke, FIELD_BOOLEAN ),
	// m_pNextFlare -  not saved, link list will be recreated as CASW_Flare_Projectile instances are created
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CASW_Flare_Projectile, DT_ASW_Flare_Projectile )
	SendPropFloat( SENDINFO( m_flTimeBurnOut ), 0,	SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flScale ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_bLight ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bSmoke ), 1, SPROP_UNSIGNED ),
END_SEND_TABLE()

// flares maintain a linked list of themselves, for quick checking for autoaim
CASW_Flare_Projectile* g_pHeadFlare = NULL;

CASW_Flare_Projectile::CASW_Flare_Projectile()
{
	m_flScale		= 2.0f;
	m_nBounces		= 0;
	m_bFading		= false;
	m_bLight		= true;
	m_bSmoke		= true;
	m_flNextDamage	= gpGlobals->curtime;
	m_lifeState		= LIFE_ALIVE;
	m_iHealth		= 100;

	if (g_pHeadFlare)
	{
		m_pNextFlare = g_pHeadFlare;
		g_pHeadFlare = this;
	}
	else
	{
		g_pHeadFlare = this;
		m_pNextFlare = NULL;
	}
}

CASW_Flare_Projectile::~CASW_Flare_Projectile( void )
{	
	// remove ourselves from the linked list of flares
	if (g_pHeadFlare == this)
	{
		g_pHeadFlare = m_pNextFlare;
	}
	else
	{
		CASW_Flare_Projectile* pFlare = g_pHeadFlare;
		int k=0;
		while (pFlare != this && pFlare != NULL && k < 256)	// some paranoid checks (should always break out of the while anyway)
		{
			k++;
			if (pFlare->m_pNextFlare == this)
			{
				pFlare->m_pNextFlare = m_pNextFlare;		// pulled ourselves out of the list
				break;
			}
			pFlare = pFlare->m_pNextFlare;			
		}
	}
}

Class_T CASW_Flare_Projectile::Classify( void )
{
	return CLASS_FLARE; 
}

int CASW_Flare_Projectile::Restore( IRestore &restore )
{
	int result = BaseClass::Restore( restore );

	if ( m_spawnflags & SF_FLARE_NO_DLIGHT )
	{
		m_bLight = false;
	}

	if ( m_spawnflags & SF_FLARE_NO_SMOKE )
	{
		m_bSmoke = false;
	}

	return result;
}

void CASW_Flare_Projectile::Spawn( void )
{
	Precache( );

	SetModel( FLARE_MODEL );
	UTIL_SetSize( this, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ) );
	SetSolid( SOLID_BBOX );
	//AddSolidFlags( FSOLID_NOT_SOLID );

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	m_flDamage		= 0;
	m_takedamage	= DAMAGE_NO;

	SetFriction( 0.6f );
	m_flTimeBurnOut = gpGlobals->curtime + 30;

	AddEffects( EF_NOSHADOW|EF_NORECEIVESHADOW );

	if ( m_spawnflags & SF_FLARE_NO_DLIGHT )
	{
		m_bLight = false;
	}

	if ( m_spawnflags & SF_FLARE_NO_SMOKE )
	{
		m_bSmoke = false;
	}

	if ( m_spawnflags & SF_FLARE_INFINITE )
	{
		m_flTimeBurnOut = -1.0f;
	}

	if ( m_spawnflags & SF_FLARE_START_OFF )
	{
		AddEffects( EF_NODRAW );
	}

	AddFlag( FL_OBJECT );
	
	SetCollisionGroup( ASW_COLLISION_GROUP_IGNORE_NPCS );
	//CreateVPhysics();

	// Tumble in air
	QAngle vecAngVelocity( 0, random->RandomFloat ( -100, -500 ), 0 );
	SetLocalAngularVelocity( vecAngVelocity );

	SetTouch( &CASW_Flare_Projectile::FlareTouch );

	//AddSolidFlags( FSOLID_NOT_STANDABLE );	
	
	SetThink( &CASW_Flare_Projectile::FlareThink );

	if ( ASW_FLARE_LIFETIME > 0 )
	{
		m_flTimeBurnOut = gpGlobals->curtime + ASW_FLARE_LIFETIME;
	}
	else
	{
		m_flTimeBurnOut = -1.0f;
	}		
	SetNextThink( gpGlobals->curtime + 0.1f );
}

unsigned int CASW_Flare_Projectile::PhysicsSolidMaskForEntity( void ) const
{
	return MASK_NPCSOLID;
}

void CASW_Flare_Projectile::FlareThink( void )
{
	float	deltaTime = ( m_flTimeBurnOut - gpGlobals->curtime );

	if ( m_flTimeBurnOut != -1.0f )
	{
		//Fading away
		if ( ( deltaTime <= FLARE_DECAY_TIME ) && ( m_bFading == false ) )
		{
			m_bFading = true;
		}

		//Burned out
		if ( m_flTimeBurnOut < gpGlobals->curtime )
		{
			UTIL_Remove( this );
			return;
		}
	}
	
	//Act differently underwater
	if ( GetWaterLevel() > 1 )
	{
		UTIL_Bubbles( GetEffectOrigin() + Vector( -2, -2, -2 ), GetEffectOrigin() + Vector( 2, 2, 2 ), 1 );
		m_bSmoke = false;
	}
	else
	{
		//Shoot sparks
		if ( random->RandomInt( 0, 6 ) == 1 )
		{
			g_pEffects->Sparks( GetEffectOrigin() );
		}
	}

	//Next update
	SetNextThink( gpGlobals->curtime + 0.1f );
}

const Vector& CASW_Flare_Projectile::GetEffectOrigin()
{
	static Vector s_vecEffectPos;
	Vector forward, right, up;
	AngleVectors(GetAbsAngles(), &forward, &right, &up);
	s_vecEffectPos = GetAbsOrigin() + up * 5;
	return s_vecEffectPos;
}

void CASW_Flare_Projectile::Precache( void )
{
	PrecacheModel( FLARE_MODEL );

	PrecacheScriptSound( "ASW_Flare.IgniteFlare" );
	PrecacheScriptSound( "ASW_Flare.FlareLoop" );
	PrecacheScriptSound( "ASW_Flare.Touch" );
	//PrecacheScriptSound( "Weapon_FlareGun.Burn" );
	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );

	BaseClass::Precache();
}



CASW_Flare_Projectile* CASW_Flare_Projectile::Flare_Projectile_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner )
{
	CASW_Flare_Projectile *pFlare = (CASW_Flare_Projectile *)CreateEntityByName( "asw_flare_projectile" );
	pFlare->SetAbsAngles( angles );
	pFlare->Spawn();
	pFlare->SetOwnerEntity( pOwner );
	//Msg("making pFlare with velocity %f,%f,%f\n", velocity.x, velocity.y, velocity.z);
	UTIL_SetOrigin( pFlare, position );
	pFlare->SetAbsVelocity( velocity );

	return pFlare;
}

void CASW_Flare_Projectile::FlareTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	if ( ( m_nBounces < 10 ) && ( GetWaterLevel() < 1 ) )
	{
		// Throw some real chunks here
		g_pEffects->Sparks( GetEffectOrigin() );
	}

	//If the flare hit a person or NPC, do damage here.
	if ( pOther && pOther->m_takedamage )
	{
		/*
			The Flare is the iRifle round right now. No damage, just ignite. (sjb)

		//Damage is a function of how fast the flare is flying.
		int iDamage = GetAbsVelocity().Length() / 50.0f;

		if ( iDamage < 5 )
		{
			//Clamp minimum damage
			iDamage = 5;
		}

		//Use m_pOwner, not GetOwnerEntity()
		pOther->TakeDamage( CTakeDamageInfo( this, m_pOwner, iDamage, (DMG_BULLET|DMG_BURN) ) );
		m_flNextDamage = gpGlobals->curtime + 1.0f;
		*/

		//CBaseAnimating *pAnim;

		//pAnim = dynamic_cast<CBaseAnimating*>(pOther);
		//if( pAnim )
		//{
			//pAnim->Ignite( 30.0f );
		//}

		Vector vecNewVelocity = GetAbsVelocity();
		vecNewVelocity	*= 0.1f;
		SetAbsVelocity( vecNewVelocity );

		SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
		SetGravity(1.0f);

		return;
	}
	else
	{
		// hit the world, check the material type here, see if the flare should stick.
		trace_t tr;
		tr = CBaseEntity::GetTouchTrace();

		//Only do this on the first bounce
		if ( m_nBounces == 0 )
		{
			surfacedata_t *pdata = physprops->GetSurfaceData( tr.surface.surfaceProps );	

			if ( pdata != NULL )
			{
				//Only embed into concrete and wood (jdw: too obscure for players?)
				//if ( ( pdata->gameMaterial == 'C' ) || ( pdata->gameMaterial == 'W' ) )
				{
					Vector	impactDir = ( tr.endpos - tr.startpos );
					VectorNormalize( impactDir );

					float	surfDot = tr.plane.normal.Dot( impactDir );

					//Do not stick to ceilings or on shallow impacts
					if ( ( tr.plane.normal.z > 0.3f ) && ( surfDot < -0.9f ) )
					{
						RemoveSolidFlags( FSOLID_NOT_SOLID );
						AddSolidFlags( FSOLID_TRIGGER );
						LayFlat();
						UTIL_SetOrigin( this, tr.endpos + ( tr.plane.normal * 2.0f ) );
						SetAbsVelocity( vec3_origin );
						SetMoveType( MOVETYPE_NONE );
						
						SetTouch( &CASW_Flare_Projectile::FlareBurnTouch );
						
						//int index = decalsystem->GetDecalIndexForName( "SmallScorch" );
						//if ( index >= 0 )
						//{
							//CBroadcastRecipientFilter filter;
							//te->Decal( filter, 0.0, &tr.endpos, &tr.startpos, ENTINDEX( tr.m_pEnt ), tr.hitbox, index );
						//}
						
						CPASAttenuationFilter filter2( this, "ASW_Flare.Touch" );
						EmitSound( filter2, entindex(), "ASW_Flare.Touch" );

						return;
					}
				}
			}
		}

		//Scorch decal
		//if ( GetAbsVelocity().LengthSqr() > (250*250) )
		//{
			//int index = decalsystem->GetDecalIndexForName( "FadingScorch" );
			//if ( index >= 0 )
			//{
				//CBroadcastRecipientFilter filter;
				//te->Decal( filter, 0.0, &tr.endpos, &tr.startpos, ENTINDEX( tr.m_pEnt ), tr.hitbox, index );
			//}
		//}

		// Change our flight characteristics
		SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
		SetGravity( UTIL_ScaleForGravity( 640 ) );
		
		m_nBounces++;

		//After the first bounce, smacking into whoever fired the flare is fair game
		SetOwnerEntity( NULL );	

		// Slow down
		Vector vecNewVelocity = GetAbsVelocity();
		vecNewVelocity.x *= 0.8f;
		vecNewVelocity.y *= 0.8f;
		SetAbsVelocity( vecNewVelocity );

		//Stopped?
		if ( GetAbsVelocity().Length() < 64.0f )
		{
			LayFlat();
			SetAbsVelocity( vec3_origin );
			SetMoveType( MOVETYPE_NONE );
			RemoveSolidFlags( FSOLID_NOT_SOLID );
			AddSolidFlags( FSOLID_TRIGGER );
			SetTouch( &CASW_Flare_Projectile::FlareBurnTouch );
		}
	}
}

void CASW_Flare_Projectile::LayFlat()
{
	return;
	QAngle angFacing = GetAbsAngles();
	if (angFacing[PITCH] > 0 && angFacing[PITCH] < 180.0f)
		angFacing[PITCH] = 90;
	else
		angFacing[PITCH] = 270;
	SetAbsAngles(angFacing);
	//Msg("Laying flat to %f, %f, %f\n", angFacing[PITCH], angFacing[YAW], angFacing[ROLL]);
}

void CASW_Flare_Projectile::FlareBurnTouch( CBaseEntity *pOther )
{
	// asw no burning flares for now
	//if ( pOther && pOther->m_takedamage && ( m_flNextDamage < gpGlobals->curtime ) )
	//{
		//pOther->TakeDamage( CTakeDamageInfo( this, m_pFirer, 1, (DMG_BULLET|DMG_BURN) ) );
		//m_flNextDamage = gpGlobals->curtime + 1.0f;
	//}
}

extern ConVar asw_flare_autoaim_radius;
void CASW_Flare_Projectile::DrawDebugGeometryOverlays()
{
	// draw arrows showing the extent of our autoaim
	for (int i=0;i<360;i+=45)
	{
		float flBaseSize = 10;
		float flHeight = asw_flare_autoaim_radius.GetFloat();

		Vector vBasePos = GetAbsOrigin() + Vector( 0, 0, 5 );
		QAngle angles( 0, 0, 0 );
		Vector vForward, vRight, vUp;
		angles[YAW] = i;
		AngleVectors( angles, &vForward, &vRight, &vUp );
		NDebugOverlay::Triangle( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 0, 255, 0, 255, false, 10 );		
	}

	BaseClass::DrawDebugGeometryOverlays();
}