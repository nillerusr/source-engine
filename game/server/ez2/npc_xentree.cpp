//=============================================================================//
//
// Purpose:		Plant-like Xen creature that swings a cruel scythe-like
//				appendage to defend itself.
//
//				These things really made me nervous when I first played Half-Life.
//
// Author:		1upD
//
//=============================================================================//

#include "cbase.h"
#include "ai_squad.h"
#include "npcevent.h"
#include "npc_xentree.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


LINK_ENTITY_TO_CLASS( npc_xentree, CNPC_XenTree );

#define AE_TREE_MELEE			( 1 )

ConVar sk_xentree_health( "sk_xentree_health", "1000" );
ConVar sk_xentree_dmg( "sk_xentree_dmg", "25" );

void CNPC_XenTree::Spawn()
{
	BaseClass::Spawn();
	// Xen trees are bigger than other flora - hunter sized
	SetHullType( HULL_MEDIUM_TALL );

	// Xen trees have an innate melee attack
	CapabilitiesAdd( bits_CAP_INNATE_MELEE_ATTACK1 );

	m_iMaxHealth = sk_xentree_health.GetFloat();
	m_iHealth = m_iMaxHealth;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_XenTree::Precache()
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/tree.mdl" ) );
	}

	m_bIsRetracted = false;

	PrecacheScriptSound( "npc_xentree.swing" );
	PrecacheScriptSound( "npc_xentree.hit" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_XenTree::OnChangeActivity( Activity eNewActivity )
{
	BaseClass::OnChangeActivity( eNewActivity );

	switch (eNewActivity)
	{
		case ACT_MELEE_ATTACK1:
		{
			if (GetEnemy())
			{
				variant_t Val;
				Val.Set( FIELD_EHANDLE, GetEnemy() );
				m_OnLower.CBaseEntityOutput::FireOutput( Val, GetEnemy(), this );
			}
			EmitSound( "npc_xentree.swing" );
		}
		break;
	}
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_XenTree::HandleAnimEvent( animevent_t *pEvent )
{
	switch (pEvent->event)
	{
	case AE_TREE_MELEE:
	{
		CBaseEntity *pHurt = CheckTraceHullAttack( 128, Vector( -32, -32, -32 ), Vector( 32, 32, 32 ), sk_xentree_dmg.GetFloat(), DMG_SLASH );
		if ( pHurt )
		{
			EmitSound( "npc_xentree.hit" );
			// Vector right, up;
			// AngleVectors( GetAbsAngles(), NULL, &right, &up );

			if (pHurt->GetFlags() & (FL_NPC | FL_CLIENT))
				pHurt->ViewPunch( QAngle( 20, 0, -20 ) );

			// pHurt->ApplyAbsVelocityImpulse( 100 * (up+2*right) * GetModelScale() );
		}
	}
	break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
	}
}