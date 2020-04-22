//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_weapon_bottle.h"
#include "decals.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
// Server specific.
#else
#include "tf_player.h"
#include "tf_fx.h"
#include "tf_gamerules.h"
#endif

//=============================================================================
//
// Weapon Bottle tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFBottle, DT_TFWeaponBottle )

BEGIN_NETWORK_TABLE( CTFBottle, DT_TFWeaponBottle )
#if defined( CLIENT_DLL )
	RecvPropBool( RECVINFO( m_bBroken ) )
#else
	SendPropBool( SENDINFO( m_bBroken ) )
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFBottle )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_bottle, CTFBottle );
PRECACHE_WEAPON_REGISTER( tf_weapon_bottle );

//=============================================================================
//
// Weapon Stickbomb tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFStickBomb, DT_TFWeaponStickBomb )

#ifdef CLIENT_DLL
void RecvProxy_Detonated( const CRecvProxyData *pData, void *pStruct, void *pOut );
#endif

BEGIN_NETWORK_TABLE( CTFStickBomb, DT_TFWeaponStickBomb )
#if defined( CLIENT_DLL )
	RecvPropInt( RECVINFO( m_iDetonated ), 0, RecvProxy_Detonated )
#else
	SendPropInt( SENDINFO( m_iDetonated ), 1, SPROP_UNSIGNED )
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFStickBomb )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_stickbomb, CTFStickBomb );
PRECACHE_WEAPON_REGISTER( tf_weapon_stickbomb );

#ifdef STAGING_ONLY
#define TF_WEAPON_STICKBOMB_NORMAL_MODEL	"models/workshop/weapons/c_models/c_caber/c_caber.mdl"
#define TF_WEAPON_STICKBOMB_BROKEN_MODEL	"models/workshop/weapons/c_models/c_caber/c_caber_exploded.mdl"
#else
#define TF_WEAPON_STICKBOMB_NORMAL_MODEL	"models/weapons/c_models/c_caber/c_caber.mdl"
#define TF_WEAPON_STICKBOMB_BROKEN_MODEL	"models/weapons/c_models/c_caber/c_caber_exploded.mdl"
#endif

//=============================================================================

#define TF_BOTTLE_SWITCHGROUP 1
#define TF_BOTTLE_NOTBROKEN 0
#define TF_BOTTLE_BROKEN 1

//=============================================================================
//
// Weapon Bottle functions.
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFBottle::CTFBottle()
{
}

void CTFBottle::WeaponReset( void )
{
	BaseClass::WeaponReset();

	m_bBroken = false;
}

bool CTFBottle::DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt )
{
	bool bRet = BaseClass::DefaultDeploy( szViewModel, szWeaponModel, iActivity, szAnimExt );

	if ( bRet )
	{
		SwitchBodyGroups();
	}

	return bRet;
}

void CTFBottle::SwitchBodyGroups( void )
{
	int iState = 0;

	if ( m_bBroken == true )
	{
		iState = 1;
	}

	SetBodygroup( TF_BOTTLE_SWITCHGROUP, iState );

	CTFPlayer *pTFPlayer = ToTFPlayer( GetOwner() );

	if ( pTFPlayer && pTFPlayer->GetActiveWeapon() == this )
	{
		if ( pTFPlayer->GetViewModel() )
		{
			pTFPlayer->GetViewModel()->SetBodygroup( TF_BOTTLE_SWITCHGROUP, iState );
		}
	}
}

void CTFBottle::Smack( void )
{
	BaseClass::Smack();

	if ( ConnectedHit() && IsCurrentAttackACrit() )
	{
		m_bBroken = true;
		SwitchBodyGroups();
	}
}

CTFStickBomb::CTFStickBomb()
: CTFBottle()
{
	m_iDetonated = 0;
}

void CTFStickBomb::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( TF_WEAPON_STICKBOMB_NORMAL_MODEL );
	PrecacheModel( TF_WEAPON_STICKBOMB_BROKEN_MODEL );
}

void CTFStickBomb::Smack( void )
{
	CTFWeaponBaseMelee::Smack();

	// Stick bombs detonate once, on impact.
	if ( m_iDetonated == 0 && ConnectedHit() )
	{
		m_iDetonated = 1;
		m_bBroken = true;
		SwitchBodyGroups();

#ifdef GAME_DLL
		CTFPlayer *pTFPlayer = ToTFPlayer( GetOwner() );
		if ( pTFPlayer )
		{
			Vector vecForward; 
			AngleVectors( pTFPlayer->EyeAngles(), &vecForward );
			Vector vecSwingStart = pTFPlayer->Weapon_ShootPosition();
			Vector vecSwingEnd = vecSwingStart + vecForward * GetSwingRange();

			Vector explosion = vecSwingStart;

			CPVSFilter filter( explosion );
			
			// Halloween Spell
			int iHalloweenSpell = 0;
			int iCustomParticleIndex = INVALID_STRING_INDEX;
			if ( TF_IsHolidayActive( kHoliday_HalloweenOrFullMoon ) )
			{
				CALL_ATTRIB_HOOK_INT_ON_OTHER( this, iHalloweenSpell, halloween_pumpkin_explosions );
				if ( iHalloweenSpell > 0 )
				{
					iCustomParticleIndex = GetParticleSystemIndex( "halloween_explosion" );
				}
			}

			TE_TFExplosion( filter, 0.0f, explosion, Vector(0,0,1), TF_WEAPON_GRENADELAUNCHER, pTFPlayer->entindex(), -1, SPECIAL1, iCustomParticleIndex );

			int dmgType = DMG_BLAST | DMG_USEDISTANCEMOD;
			if ( IsCurrentAttackACrit() )
				dmgType |= DMG_CRITICAL;

			CTakeDamageInfo info( pTFPlayer, pTFPlayer, this, explosion, explosion, 75.0f, dmgType, TF_DMG_CUSTOM_STICKBOMB_EXPLOSION, &explosion );
			CTFRadiusDamageInfo radiusinfo( &info, explosion, 100.f );
			TFGameRules()->RadiusDamage( radiusinfo );
		}
#endif
	}
}

void CTFStickBomb::WeaponReset( void )
{
	BaseClass::WeaponReset();

	m_iDetonated = 0;

	SwitchBodyGroups();
}

void CTFStickBomb::WeaponRegenerate( void )
{
	BaseClass::WeaponRegenerate();

	m_iDetonated = 0;

	SetContextThink( &CTFStickBomb::SwitchBodyGroups, gpGlobals->curtime + 0.01f, "SwitchBodyGroups" );
}

void CTFStickBomb::SwitchBodyGroups( void )
{
#ifdef CLIENT_DLL
	if ( !GetViewmodelAttachment() )
		return;

	if ( m_iDetonated == 1 )
	{
		GetViewmodelAttachment()->SetModel( TF_WEAPON_STICKBOMB_BROKEN_MODEL );
	}
	else
	{
		GetViewmodelAttachment()->SetModel( TF_WEAPON_STICKBOMB_NORMAL_MODEL );
	}
#endif
}

const char *CTFStickBomb::GetWorldModel( void ) const
{
	if ( m_iDetonated == 1 )
	{
		return TF_WEAPON_STICKBOMB_BROKEN_MODEL;
	}
	else
	{
		return BaseClass::GetWorldModel();
	}
}

#ifdef CLIENT_DLL
int CTFStickBomb::GetWorldModelIndex( void )
{
	if ( !modelinfo )
		return BaseClass::GetWorldModelIndex();

	if ( m_iDetonated == 1 )
	{
		m_iWorldModelIndex = modelinfo->GetModelIndex( TF_WEAPON_STICKBOMB_BROKEN_MODEL );
		return m_iWorldModelIndex;
	}
	else
	{
		m_iWorldModelIndex = modelinfo->GetModelIndex( TF_WEAPON_STICKBOMB_NORMAL_MODEL );
		return m_iWorldModelIndex;
	}
}
#endif

#ifdef CLIENT_DLL

void RecvProxy_Detonated( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_TFStickBomb* pBomb = (C_TFStickBomb*) pStruct;

	if ( pData->m_Value.m_Int != pBomb->GetDetonated() )
	{
		pBomb->SetDetonated( pData->m_Value.m_Int );
		pBomb->SwitchBodyGroups();
	}
}

#endif
