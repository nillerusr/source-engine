#include "cbase.h"
#include "asw_pickup_money.h"
#include "asw_marine.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "asw_game_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ITEM_PICKUP_BOX_BLOAT		24

#define MONEY_MODEL "models/swarm/MortarBugProjectile/MortarBugProjectile.mdl"

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Money, DT_ASW_Pickup_Money)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( asw_pickup_money, CASW_Pickup_Money );

PRECACHE_REGISTER( asw_pickup_money );

ConVar asw_money_friction("asw_money_friction", "-1.0f", FCVAR_CHEAT);
ConVar asw_money_gravity("asw_money_gravity", "0.8f", FCVAR_CHEAT);
ConVar asw_money_elasticity("asw_money_elasticity", "3.0f", FCVAR_CHEAT);

CASW_Pickup_Money::CASW_Pickup_Money()
{

}

CASW_Pickup_Money::~CASW_Pickup_Money()
{

}

void CASW_Pickup_Money::Spawn( void )
{
	BaseClass::Spawn();

	SetModel( MONEY_MODEL );

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	// This will make them not collide with the player, but will collide
	// against other items + weapons
	SetCollisionGroup( COLLISION_GROUP_WEAPON );
	CollisionProp()->UseTriggerBounds( true, ITEM_PICKUP_BOX_BLOAT );
	SetTouch(&CASW_Pickup_Money::ItemTouch);

	// Tumble in air
	QAngle vecAngVelocity( random->RandomFloat ( -100, -500 ), 0, 0 );
	SetLocalAngularVelocity( vecAngVelocity );

	CreateEffects();

	SetGravity( asw_money_gravity.GetFloat() );
	SetFriction( asw_money_friction.GetFloat() );
	SetElasticity( asw_money_elasticity.GetFloat() );
}

void CASW_Pickup_Money::Precache( void )
{
	BaseClass::Precache( );

	PrecacheModel( MONEY_MODEL );
	PrecacheModel("sprites/redglow1.vmt");
	PrecacheModel("sprites/bluelaser1.vmt");
	//PrecacheScriptSound("ASW_Money.Pickup");
}

void CASW_Pickup_Money::ItemTouch( CBaseEntity *pOther )
{
	if ( !pOther || pOther->Classify() != CLASS_ASW_MARINE )
		return;

	CASW_Marine *pMarine = CASW_Marine::AsMarine( pOther );
	if ( !pMarine )
		return;

	// Disappear with a satisfying sound and increment the player's money
	//EmitSound("ASW_Money.Pickup");	
	if ( ASWGameResource() )
	{
		ASWGameResource()->IncrementMoney( 10 );
	}

	UTIL_Remove( this );
}

void CASW_Pickup_Money::CreateEffects()
{
	// Start up the eye glow
	m_pMainGlow = CSprite::SpriteCreate( "sprites/redglow1.vmt", GetLocalOrigin(), false );

	int	nAttachment = LookupAttachment( "fuse" );

	if ( m_pMainGlow != NULL )
	{
		m_pMainGlow->FollowEntity( this );
		m_pMainGlow->SetAttachment( this, nAttachment );
		m_pMainGlow->SetTransparency( kRenderGlow, 255, 255, 255, 200, kRenderFxNoDissipation );
		m_pMainGlow->SetScale( 0.6f );
		m_pMainGlow->SetGlowProxySize( 4.0f );
	}

	// Start up the eye trail
	m_pGlowTrail	= CSpriteTrail::SpriteTrailCreate( "sprites/bluelaser1.vmt", GetLocalOrigin(), false );

	if ( m_pGlowTrail != NULL )
	{
		m_pGlowTrail->FollowEntity( this );
		m_pGlowTrail->SetAttachment( this, nAttachment );
		m_pGlowTrail->SetTransparency( kRenderTransAdd, 255, 0, 0, 255, kRenderFxNone );
		m_pGlowTrail->SetStartWidth( 8.0f );
		m_pGlowTrail->SetEndWidth( 1.0f );
		m_pGlowTrail->SetLifeTime( 0.5f );
	}
}