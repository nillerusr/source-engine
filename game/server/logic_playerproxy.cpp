//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ====
//
// Purpose:		Player for HL2.
//
//=============================================================================

#include "cbase.h"
#include "logic_playerproxy.h"
#include "filters.h"

#if defined HL2_EPISODIC
#include "hl2_player.h"
#endif // HL2_EPISODIC

LINK_ENTITY_TO_CLASS( logic_playerproxy, CLogicPlayerProxy);

BEGIN_DATADESC( CLogicPlayerProxy )

// Base
DEFINE_OUTPUT( m_RequestedPlayerHealth,		"PlayerHealth" ),
DEFINE_OUTPUT( m_PlayerDied,				"PlayerDied" ),
DEFINE_FIELD( m_hPlayer, FIELD_EHANDLE ),



// HL2 / Episodic
#if defined HL2_EPISODIC
DEFINE_OUTPUT( m_OnFlashlightOn,			"OnFlashlightOn" ),
DEFINE_OUTPUT( m_OnFlashlightOff,			"OnFlashlightOff" ),
DEFINE_OUTPUT( m_PlayerMissedAR2AltFire,	"PlayerMissedAR2AltFire" ),
DEFINE_OUTPUT( m_PlayerHasAmmo,				"PlayerHasAmmo" ),
DEFINE_OUTPUT( m_PlayerHasNoAmmo,			"PlayerHasNoAmmo" ),

DEFINE_INPUTFUNC( FIELD_VOID,				"SetFlashlightSlowDrain",	InputSetFlashlightSlowDrain ),
DEFINE_INPUTFUNC( FIELD_VOID,				"SetFlashlightNormalDrain",	InputSetFlashlightNormalDrain ),
DEFINE_INPUTFUNC( FIELD_VOID,				"LowerWeapon", InputLowerWeapon ),
DEFINE_INPUTFUNC( FIELD_STRING,				"SetLocatorTargetEntity", InputSetLocatorTargetEntity ),
DEFINE_INPUTFUNC( FIELD_VOID,				"RequestPlayerHealth",	InputRequestPlayerHealth ),
DEFINE_INPUTFUNC( FIELD_INTEGER,			"SetPlayerHealth",	InputSetPlayerHealth ),
DEFINE_INPUTFUNC( FIELD_VOID,				"RequestAmmoState", InputRequestAmmoState ),
DEFINE_INPUTFUNC( FIELD_VOID,				"EnableCappedPhysicsDamage", InputEnableCappedPhysicsDamage ),
DEFINE_INPUTFUNC( FIELD_VOID,				"DisableCappedPhysicsDamage", InputDisableCappedPhysicsDamage ),
#endif // HL2_EPISODIC

END_DATADESC()

void CLogicPlayerProxy::Activate( void )
{
	BaseClass::Activate();

	if ( m_hPlayer == NULL )
	{
		m_hPlayer = AI_GetSinglePlayer();
	}
}

bool CLogicPlayerProxy::PassesDamageFilter( const CTakeDamageInfo &info )
{
	if (m_hDamageFilter)
	{
		CBaseFilter *pFilter = (CBaseFilter *)(m_hDamageFilter.Get());
		return pFilter->PassesDamageFilter(info);
	}

	return true;
}

void CLogicPlayerProxy::InputSetPlayerHealth( inputdata_t &inputdata )
{
	if ( m_hPlayer == NULL )
		return;

	m_hPlayer->SetHealth( inputdata.value.Int() );

}

void CLogicPlayerProxy::InputRequestPlayerHealth( inputdata_t &inputdata )
{
	if ( m_hPlayer == NULL )
		return;

	m_RequestedPlayerHealth.Set( m_hPlayer->GetHealth(), inputdata.pActivator, inputdata.pCaller );
}

#if defined HL2_EPISODIC

extern ConVar hl2_darkness_flashlight_factor;

void CLogicPlayerProxy::InputSetFlashlightSlowDrain( inputdata_t &inputdata )
{
	if( m_hPlayer == NULL )
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if( pPlayer )
		pPlayer->SetFlashlightPowerDrainScale( hl2_darkness_flashlight_factor.GetFloat() );
}

void CLogicPlayerProxy::InputSetFlashlightNormalDrain( inputdata_t &inputdata )
{
	if( m_hPlayer == NULL )
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if( pPlayer )
		pPlayer->SetFlashlightPowerDrainScale( 1.0f );
}

void CLogicPlayerProxy::InputLowerWeapon( inputdata_t &inputdata )
{
	if( m_hPlayer == NULL )
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	pPlayer->Weapon_Lower();
}

void CLogicPlayerProxy::InputSetLocatorTargetEntity( inputdata_t &inputdata )
{
	if( m_hPlayer == NULL )
		return;

	CBaseEntity *pTarget = NULL; // assume no target
	string_t iszTarget = MAKE_STRING( inputdata.value.String() );

	if( iszTarget != NULL_STRING )
	{
		pTarget = gEntList.FindEntityByName( NULL, iszTarget );
	}

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	pPlayer->SetLocatorTargetEntity(pTarget);
}

void CLogicPlayerProxy::InputRequestAmmoState( inputdata_t &inputdata )
{
	if( m_hPlayer == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( m_hPlayer );

	for ( int i = 0 ; i < pPlayer->WeaponCount(); ++i )
	{
		CBaseCombatWeapon* pCheck = pPlayer->GetWeapon( i );

		if ( pCheck )
		{
			if ( pCheck->HasAnyAmmo() && (pCheck->UsesPrimaryAmmo() || pCheck->UsesSecondaryAmmo()))
			{
				m_PlayerHasAmmo.FireOutput( this, this, 0 );
				return;
			}
		}
	}

	m_PlayerHasNoAmmo.FireOutput( this, this, 0 );
}

void CLogicPlayerProxy::InputEnableCappedPhysicsDamage( inputdata_t &inputdata )
{
	if ( m_hPlayer == NULL )
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	pPlayer->EnableCappedPhysicsDamage();
}

void CLogicPlayerProxy::InputDisableCappedPhysicsDamage( inputdata_t &inputdata )
{
	if ( m_hPlayer == NULL )
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	pPlayer->DisableCappedPhysicsDamage();
}

#endif // HL2_EPISODIC
