//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "weapon_basecombatobject.h"
//====================================================================================================
// BASE COMBAT OBJECT WEAPON
//====================================================================================================

LINK_ENTITY_TO_CLASS( weapon_basecombatobject, CWeaponBaseCombatObject );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponBaseCombatObject, DT_WeaponBaseCombatObject )

BEGIN_NETWORK_TABLE( CWeaponBaseCombatObject, DT_WeaponBaseCombatObject )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponBaseCombatObject )

/*
	DEFINE_PRED_ARRAY( m_szObjectName, FIELD_CHARACTER, sizeof( m_szObjectName ) ),
	DEFINE_PRED_FIELD( m_vecBuildMins, FIELD_VECTOR, 0 ),
	DEFINE_PRED_FIELD( m_vecBuildMaxs, FIELD_VECTOR, 0 ),
*/

END_PREDICTION_DATA()


CWeaponBaseCombatObject::CWeaponBaseCombatObject()
{
}

//-----------------------------------------------------------------------------
// Purpose: Place the combat object
//-----------------------------------------------------------------------------
void CWeaponBaseCombatObject::PrimaryAttack( void )
{
	CBaseTFPlayer *pPlayer = dynamic_cast<CBaseTFPlayer*>((CBaseEntity*)GetOwner());
	if ( !pPlayer )
		return;
	if ( pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
		return;

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();

	Vector vecPlaceOrigin;
	QAngle angPlaceAngles;
	if ( GetPlacePosition( pPlayer, &vecPlaceOrigin, &angPlaceAngles ) == false )
	{
		WeaponSound( WPN_DOUBLE );
		return;
	}

	// Place the combat object
	PlaceCombatObject( pPlayer, vecPlaceOrigin, angPlaceAngles );

	WeaponSound( SINGLE );
	pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );

	// If I'm now out of ammo, switch away
	if ( !HasPrimaryAmmo() )
	{
		pPlayer->SelectLastItem();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if we found a valid placement point
//-----------------------------------------------------------------------------
bool CWeaponBaseCombatObject::GetPlacePosition( CBaseTFPlayer *pBuilder, Vector *vecPlaceOrigin, QAngle *angPlaceAngles )
{
	Vector vecForward;
	QAngle vecAngles = vec3_angle;
	vecAngles.y = pBuilder->EyeAngles().y;
	AngleVectors( vecAngles, &vecForward, NULL, NULL);
	*vecPlaceOrigin = pBuilder->WorldSpaceCenter() + (vecForward * ((m_vecBuildMaxs.x - m_vecBuildMins.x) + 32));
	if ( UTIL_PointContents( *vecPlaceOrigin ) != CONTENTS_EMPTY )
		return false;

	// Room to fit?
	trace_t tr;
	UTIL_TraceHull( *vecPlaceOrigin, *vecPlaceOrigin + Vector(0,0,-64), m_vecBuildMins, m_vecBuildMaxs, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.allsolid || tr.startsolid )
		return false;
	if ( tr.fraction == 1.0 )
		return false;

	*vecPlaceOrigin = tr.endpos;
	//VectorAngles( tr.plane.normal, *angPlaceAngles );
	*angPlaceAngles = QAngle(0,0,0);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Put the combat object for this weapon on the specified point
//-----------------------------------------------------------------------------
void CWeaponBaseCombatObject::PlaceCombatObject( CBaseTFPlayer *pBuilder, Vector vecOrigin, QAngle angles )
{
#if !defined( CLIENT_DLL )
	Assert( m_szObjectName != NULL );

	CBaseEntity *pEntity = CreateEntityByName( m_szObjectName );
	pEntity->SetLocalAngles( angles );
	pEntity->Spawn();
	pEntity->Teleport( &vecOrigin, &angles, &vec3_origin );

	// If it's an object, set it's builder & team
	CBaseObject *pObject = dynamic_cast< CBaseObject * >( pEntity );
	if ( pObject )
	{
		pObject->SetBuilder( pBuilder );
		pObject->ChangeTeam( pBuilder->GetTeamNumber() );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponBaseCombatObject::GetFireRate( void )
{
	return 0.5;
}
