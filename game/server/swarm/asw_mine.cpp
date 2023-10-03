#include "cbase.h"
#include "asw_mine.h"
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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_MINE_MODEL "models/items/Mine/mine.mdl"
#define ASW_MINE_EXPLODE_TIME 0.6f

ConVar asw_debug_mine("asw_debug_mine", "0", FCVAR_CHEAT, "Turn on debug messages for Incendiary Mines");

LINK_ENTITY_TO_CLASS( asw_mine, CASW_Mine );

BEGIN_DATADESC( CASW_Mine )
	DEFINE_FUNCTION( MineTouch ),
	DEFINE_FUNCTION( MineThink ),
	DEFINE_FIELD( m_bPrimed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMineTriggered, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fForcePrimeTime, FIELD_FLOAT ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CASW_Mine, DT_ASW_Mine )
	SendPropBool( SENDINFO( m_bMineTriggered ) ),
END_SEND_TABLE()

CASW_Mine::CASW_Mine()
{
	m_flDurationScale = 1.0f;
	m_iExtraFires = 0;
}

CASW_Mine::~CASW_Mine( void )
{

}

void CASW_Mine::Spawn( void )
{
	Precache( );
	SetModel( ASW_MINE_MODEL );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_SOLID || FSOLID_TRIGGER);
	SetMoveType( MOVETYPE_FLYGRAVITY );
	m_takedamage	= DAMAGE_NO;
	m_bPrimed = false;

	AddEffects( EF_NOSHADOW|EF_NORECEIVESHADOW );

	m_fForcePrimeTime = gpGlobals->curtime + 5.0f;
		
	SetThink( &CASW_Mine::MineThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
	SetTouch( &CASW_Mine::MineTouch );
}

unsigned int CASW_Mine::PhysicsSolidMaskForEntity( void ) const
{
	return MASK_NPCSOLID;
}

void CASW_Mine::MineThink( void )
{
	if (m_bMineTriggered)
	{
		Explode();
	}
	// if we've stopped moving, prime ourselves
	else if (!m_bPrimed && (GetAbsVelocity().Length() < 1.0f || gpGlobals->curtime > m_fForcePrimeTime))
	{
		Prime();
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

void CASW_Mine::Explode()
{
	if (asw_debug_mine.GetBool())
		Msg("OMG MINE ASPLODE!\n");
	CASW_Firewall_Piece* pFirewall = (CASW_Firewall_Piece *)CreateEntityByName( "asw_firewall_piece" );	
	if (pFirewall)
	{
		pFirewall->SetAbsAngles( GetAbsAngles() );
		if (GetOwnerEntity())
			Msg("Creating firewall with owner %s\n", GetOwnerEntity()->GetClassname());
		pFirewall->SetOwnerEntity(GetOwnerEntity());
		pFirewall->SetAbsOrigin( GetAbsOrigin() );			

		CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(GetOwnerEntity());
		if (!pMarine)
		{
			pFirewall->SetSideFire( 3 + m_iExtraFires , 3+ m_iExtraFires );
		}
		else
		{
			int iSideFires = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_MINES_FIRES) + m_iExtraFires;
			float fDuration = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_MINES_DURATION);
			pFirewall->SetSideFire(iSideFires,iSideFires);
			pFirewall->SetDuration(fDuration * m_flDurationScale);
		}

		pFirewall->m_hCreatorWeapon = m_hCreatorWeapon;
		pFirewall->Spawn();
		pFirewall->SetAbsVelocity( vec3_origin );
	}
	EmitSound("ASW_Mine.Explode");	
	UTIL_Remove( this );
}

void CASW_Mine::Precache( void )
{
	PrecacheModel( ASW_MINE_MODEL );
	
	PrecacheScriptSound("ASW_Mine.Lay");
	PrecacheScriptSound("ASW_Mine.Flash");
	PrecacheScriptSound("ASW_Mine.Explode");
	BaseClass::Precache();
}

CASW_Mine* CASW_Mine::ASW_Mine_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon )
{
	CASW_Mine *pMine = (CASW_Mine*)CreateEntityByName( "asw_mine" );
	pMine->SetAbsAngles( angles );
	pMine->Spawn();
	pMine->SetOwnerEntity( pOwner );
	UTIL_SetOrigin( pMine, position );
	pMine->SetAbsVelocity( velocity );
	pMine->m_hCreatorWeapon.Set( pCreatorWeapon );

	return pMine;
}

void CASW_Mine::MineTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if (pOther->entindex() != 0 && asw_debug_mine.GetBool())
		Msg("Touched by %s\n", pOther->GetClassname());

	if (!m_bMineTriggered && m_bPrimed)
	{
		// if other is an alien, trigger the mine
		if (ValidMineTarget(pOther))
		{
			// Ensure clear path to the target
			trace_t tr;
			UTIL_TraceLine( GetAbsOrigin(), pOther->WorldSpaceCenter(), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

			if ( tr.fraction >= 1.0f || tr.m_pEnt == pOther )
			{
				if (asw_debug_mine.GetBool())
					Msg("Mine triggered!\n");
				m_bMineTriggered = true;
				EmitSound("ASW_Mine.Flash");
				SetNextThink( gpGlobals->curtime + ASW_MINE_EXPLODE_TIME );
			}
		}
	}

	// Slow down
	Vector vecNewVelocity = GetAbsVelocity();
	vecNewVelocity.x *= 0.8f;
	vecNewVelocity.y *= 0.8f;
	SetAbsVelocity( vecNewVelocity );	
}

// set the mine up for exploding
void CASW_Mine::Prime()
{
	if (asw_debug_mine.GetBool())
		Msg("Mine primed!\n");
	EmitSound("ASW_Mine.Lay");
	SetSolid( SOLID_BBOX );
	float boxWidth = 150;
	UTIL_SetSize(this, Vector(-boxWidth,-boxWidth,-boxWidth),Vector(boxWidth,boxWidth,boxWidth * 2));
	SetCollisionGroup( ASW_COLLISION_GROUP_PASSABLE );
	CollisionProp()->UseTriggerBounds( true, 24 );
	AddSolidFlags(FSOLID_TRIGGER);
	AddSolidFlags(FSOLID_NOT_SOLID);
	SetTouch( &CASW_Mine::MineTouch );
	m_bPrimed = true;

	// attach to whatever we're standing on
	CBaseEntity *pGround = GetGroundEntity();
	if ( pGround && !pGround->IsWorld() )
	{
		if (asw_debug_mine.GetBool())
			Msg( "Parenting mine to %s\n", GetGroundEntity()->GetClassname() );
		SetParent( GetGroundEntity() );
		SetMoveType( MOVETYPE_NONE );
	}
}

bool CASW_Mine::ValidMineTarget(CBaseEntity *pOther)
{
	CASW_Marine* pMarine = CASW_Marine::AsMarine( pOther );
	if (pMarine)
		return false;

	CAI_BaseNPC* pNPC = dynamic_cast<CAI_BaseNPC*>(pOther);
	if (pNPC)
		return true;

	return false;
}

void CASW_Mine::StopLoopingSounds()
{
	BaseClass::StopLoopingSounds();
}