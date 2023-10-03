#include "cbase.h"
#include "asw_bait.h"
#include "soundent.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#include "decals.h"
#include "asw_shareddefs.h"
#include "ai_senses.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define BAIT_MODEL "models/swarm/Flare/flareweapon.mdl"
#define ASW_BAIT_LIFETIME 30.0f

LINK_ENTITY_TO_CLASS( asw_bait, CASW_Bait );

BEGIN_DATADESC( CASW_Bait )
	DEFINE_FUNCTION( BaitTouch ),
	DEFINE_FUNCTION( BaitThink ),

	DEFINE_FIELD( m_flTimeBurnOut, FIELD_TIME ),
END_DATADESC()

// flares maintain a linked list of themselves, for quick checking for autoaim
CASW_Bait* g_pHeadFlare = NULL;

CASW_Bait::CASW_Bait()
{
	m_lifeState		= LIFE_ALIVE;
	m_iHealth		= 100;
	m_nBounces		= 0;
}

CASW_Bait::~CASW_Bait( void )
{
}

void CASW_Bait::Spawn( void )
{
	Precache( );

	SetModel( BAIT_MODEL );
	UTIL_SetSize( this, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ) );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_SOLID );

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	m_takedamage	= DAMAGE_NO;

	SetFriction( 0.6f );
	m_flTimeBurnOut = gpGlobals->curtime + 30;

	AddEffects( EF_NOSHADOW|EF_NORECEIVESHADOW );

	AddFlag( FL_OBJECT );
	
	SetCollisionGroup( ASW_COLLISION_GROUP_IGNORE_NPCS );
	//CreateVPhysics();

	// Tumble in air
	QAngle vecAngVelocity( 0, random->RandomFloat ( -100, -500 ), 0 );
	SetLocalAngularVelocity( vecAngVelocity );

	SetTouch( &CASW_Bait::BaitTouch );	
	SetThink( &CASW_Bait::BaitThink );

	// join the flares team so that aliens will hate us
	ChangeFaction( FACTION_BAIT );

	if ( ASW_BAIT_LIFETIME > 0 )
	{
		m_flTimeBurnOut = gpGlobals->curtime + ASW_BAIT_LIFETIME;
	}
	else
	{
		m_flTimeBurnOut = -1.0f;
	}		
	SetNextThink( gpGlobals->curtime + 0.1f );

	g_AI_SensedObjectsManager.AddEntity( this );
}

void CASW_Bait::BaitThink( void )
{
	if ( m_flTimeBurnOut != -1.0f )
	{
		//Burned out
		if ( m_flTimeBurnOut < gpGlobals->curtime )
		{
			UTIL_Remove( this );
			return;
		}
	}

	//Next update
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CASW_Bait::Precache( void )
{
	PrecacheModel( BAIT_MODEL );

	BaseClass::Precache();
}



CASW_Bait* CASW_Bait::Bait_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner )
{
	CASW_Bait *pEnt = (CASW_Bait *)CreateEntityByName( "asw_bait" );
	pEnt->SetAbsAngles( angles );
	pEnt->Spawn();
	pEnt->SetOwnerEntity( pOwner );
	UTIL_SetOrigin( pEnt, position );
	pEnt->SetAbsVelocity( velocity );

	return pEnt;
}

void CASW_Bait::BaitTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	if ( pOther && pOther->m_takedamage )
	{
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
					if ( ( tr.plane.normal.z > -0.5f ) && ( surfDot < -0.9f ) )
					{
						RemoveSolidFlags( FSOLID_NOT_SOLID );
						AddSolidFlags( FSOLID_TRIGGER );
						LayFlat();
						UTIL_SetOrigin( this, tr.endpos + ( tr.plane.normal * 2.0f ) );
						SetAbsVelocity( vec3_origin );
						SetMoveType( MOVETYPE_NONE );
						SetTouch( NULL );
						return;
					}
				}
			}
		}

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
			SetTouch( NULL );
		}
	}
}

void CASW_Bait::LayFlat()
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